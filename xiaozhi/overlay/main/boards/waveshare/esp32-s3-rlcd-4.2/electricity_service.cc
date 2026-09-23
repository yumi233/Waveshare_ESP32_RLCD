#include "electricity_service.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <cJSON.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>

#include "settings.h"

namespace {
constexpr char kTag[] = "Electricity";
constexpr char kUrl[] = "http://202.192.240.231/scp-api/electricity-recharge/getCurrentRemaining_v2";
constexpr size_t kMaximumResponseBytes = 2048;
constexpr uint32_t kRefreshMs = 15U * 60U * 1000U;
constexpr uint32_t kRetryMs = 60U * 1000U;

struct ResponseContext {
    std::string body;
    bool overflow = false;
};

esp_err_t CollectResponse(esp_http_client_event_t* event) {
    if (event->event_id != HTTP_EVENT_ON_DATA || event->data == nullptr ||
        event->data_len <= 0 || event->user_data == nullptr) return ESP_OK;
    auto* context = static_cast<ResponseContext*>(event->user_data);
    if (context->body.size() + static_cast<size_t>(event->data_len) >
        kMaximumResponseBytes) {
        context->overflow = true;
        return ESP_FAIL;
    }
    context->body.append(static_cast<const char*>(event->data), event->data_len);
    return ESP_OK;
}

bool ReadNumber(const cJSON* object, const char* key, float& output) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (cJSON_IsNumber(item)) output = static_cast<float>(item->valuedouble);
    else if (cJSON_IsString(item) && item->valuestring != nullptr) {
        char* end = nullptr;
        output = strtof(item->valuestring, &end);
        if (end == item->valuestring || *end != '\0') return false;
    } else return false;
    return std::isfinite(output) && output >= 0.0f && output < 1000000.0f;
}
}  // namespace

ElectricityService& ElectricityService::GetInstance() {
    static ElectricityService instance;
    return instance;
}

bool ElectricityService::Start() {
    if (task_ != nullptr) return true;
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) return false;
    return xTaskCreatePinnedToCore(TaskEntry, "electricity", 6144, this, 1,
                                   &task_, 0) == pdPASS;
}

void ElectricityService::TaskEntry(void* context) {
    static_cast<ElectricityService*>(context)->Run();
}

void ElectricityService::Run() {
    while (true) {
        Settings settings("wifi-config");
        const int building = settings.GetInt("elec-building", 0);
        const int room = settings.GetInt("elec-room", 0);
        PanelElectricitySnapshot result = {};
        GetSnapshot(result);
        const bool changed_room = result.building != building || result.room != room;
        if (changed_room) result = {};
        result.building = building;
        result.room = room;
        result.configured = building > 0 && building <= 999 && room > 0 && room <= 9999;
        if (!result.configured) {
            result.available = false;
            result.stale = false;
            if (changed_room || result.generation == 0) Publish(result);
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
            continue;
        }
        if (!WifiReady()) {
            if (result.available && !result.stale) {
                result.stale = true;
                Publish(result);
            } else if (changed_room) Publish(result);
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
            continue;
        }
        float remaining = 0, used = 0;
        const bool success = Fetch(building, room, remaining, used);
        if (success) {
            result.remaining_kwh = remaining;
            result.used_kwh = used;
            result.available = true;
            result.stale = false;
        } else {
            result.stale = result.available;
        }
        Publish(result);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(success ? kRefreshMs : kRetryMs));
    }
}

void ElectricityService::RequestRefresh() {
    if (task_ != nullptr) xTaskNotifyGive(task_);
}

bool ElectricityService::GetSnapshot(PanelElectricitySnapshot& result) {
    if (mutex_ == nullptr || xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) != pdTRUE)
        return false;
    result = snapshot_;
    xSemaphoreGive(mutex_);
    return true;
}

void ElectricityService::Publish(const PanelElectricitySnapshot& result) {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) return;
    PanelElectricitySnapshot next = result;
    next.generation = snapshot_.generation + 1;
    snapshot_ = next;
    xSemaphoreGive(mutex_);
}

bool ElectricityService::WifiReady() {
    wifi_ap_record_t ap = {};
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return false;
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip = {};
    return netif != nullptr && esp_netif_get_ip_info(netif, &ip) == ESP_OK &&
           ip.ip.addr != 0;
}

bool ElectricityService::Fetch(int building, int room, float& remaining, float& used) {
    char form[80];
    snprintf(form, sizeof(form), "userTypeID=1&building=%d&room=%d", building, room);
    ResponseContext response;
    esp_http_client_config_t config = {};
    config.url = kUrl;
    config.event_handler = CollectResponse;
    config.user_data = &response;
    config.timeout_ms = 10000;
    config.buffer_size = 1024;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) return false;
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_header(client, "Referer", "http://202.192.240.231/recharge.html");
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_post_field(client, form, strlen(form));
    const esp_err_t error = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (error != ESP_OK || status != 200 || response.overflow) {
        ESP_LOGW(kTag, "Query failed: transport=%s HTTP=%d", esp_err_to_name(error), status);
        return false;
    }
    cJSON* root = cJSON_ParseWithLength(response.body.c_str(), response.body.size());
    if (root == nullptr) return false;
    const cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
    const bool valid = cJSON_IsObject(data) &&
                       ReadNumber(data, "resamp", remaining) &&
                       ReadNumber(data, "usedamp", used);
    cJSON_Delete(root);
    return valid;
}
