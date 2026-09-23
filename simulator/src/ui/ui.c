/* Project-specific additions Copyright (c) 2026 黑沐. MIT; upstream notices retained. */
#include "ui/ui.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lvgl.h"
#include "assets/assets.h"
#include "ui/ui_fonts.h"

typedef enum {
    PAGE_DASHBOARD,
    PAGE_PERFORMANCE,
    PAGE_SYNA,
    PAGE_ELECTRICITY,
    PAGE_COMPUTERS,
    PAGE_ABOUT,
} page_t;

typedef struct {
    const char *name;
    const char *connection;
    const char *agent_state;
    const char *task;
    const char *elapsed;
    const char *deepseek_balance;
    int codex_short_percent;
    int codex_week_percent;
} computer_t;

typedef struct {
    float cpu_percent;
    float memory_percent;
    float gpu_percent;
    float disk_percent;
    float cpu_temperature_c;
    float gpu_temperature_c;
    float upload_mb_per_second;
    float download_mb_per_second;
    int latency_ms;
    bool gpu_valid;
    bool cpu_temperature_valid;
    bool gpu_temperature_valid;
    bool connected;
} performance_state_t;

static computer_t computers[] = {
    {
        .name = "OFFICE-PC",
        .connection = "ONLINE",
        .agent_state = "WORKING",
        .task = "Build visual simulator",
        .elapsed = "00:08:25",
        .deepseek_balance = "CNY 86.42",
        .codex_short_percent = -1,
        .codex_week_percent = -1,
    },
    {
        .name = "LAPTOP",
        .connection = "ONLINE",
        .agent_state = "IDLE",
        .task = "No active task",
        .elapsed = "--:--:--",
        .deepseek_balance = "CNY 120.00",
        .codex_short_percent = 94,
        .codex_week_percent = 85,
    },
    {
        .name = "HOME-PC",
        .connection = "OFFLINE",
        .agent_state = "OFFLINE",
        .task = "Last seen 2h ago",
        .elapsed = "--:--:--",
        .deepseek_balance = "--",
        .codex_short_percent = -1,
        .codex_week_percent = -1,
    },
};

#define MOCK_COMPUTER_COUNT ((int)(sizeof(computers) / sizeof(computers[0])))
#define COMPUTER_ROWS_VISIBLE 3

#define COLOR_BLACK lv_color_black()
#define COLOR_WHITE lv_color_white()

static page_t current_page = PAGE_DASHBOARD;
static int current_computer = 0;
static int current_list_computer = 0;
static int selected_computer = 0;
static int computer_list_count = 3;
static int computer_list_offset = 0;
static ui_computer_info_t computer_list[UI_MAX_COMPUTERS] = {
    {.name = "OFFICE-PC", .agent_state = "WORKING", .online = true},
    {.name = "LAPTOP", .agent_state = "IDLE", .online = true},
    {.name = "HOME-PC", .agent_state = "OFFLINE", .online = false},
};

static lv_obj_t *clock_label;
static lv_obj_t *dashboard_clock_label;
static lv_obj_t *performance_clock_label;
static lv_obj_t *dashboard_temperature_label;
static lv_obj_t *dashboard_humidity_label;
static lv_obj_t *dashboard_electricity_label;
static lv_obj_t *dashboard_electricity_title;
static lv_obj_t *dashboard_battery_label;
static lv_obj_t *performance_battery_label;
static lv_obj_t *syna_battery_label;
static lv_obj_t *electricity_battery_label;
static lv_obj_t *electricity_screen;
static lv_obj_t *electricity_room_label;
static lv_obj_t *electricity_balance_label;
static lv_obj_t *electricity_usage_label;
static lv_obj_t *electricity_status_label;
static lv_obj_t *dashboard_wifi_status_image;
static lv_obj_t *performance_wifi_status_image;
static lv_obj_t *syna_wifi_status_image;
static lv_obj_t *dashboard_pc_status_image;
static lv_obj_t *performance_pc_status_image;
static lv_obj_t *syna_pc_status_image;
static lv_obj_t *dashboard_agent_status_image;
static lv_obj_t *dashboard_agent_login_label;
static lv_obj_t *dashboard_short_quota_title;
static lv_obj_t *dashboard_week_quota_title;
static lv_obj_t *dashboard_short_quota_track;
static lv_obj_t *dashboard_week_quota_track;
static lv_obj_t *dashboard_agent_task_title;
static lv_obj_t *dashboard_agent_task_label;
static lv_obj_t *dashboard_short_quota_label;
static lv_obj_t *dashboard_week_quota_label;
static lv_obj_t *dashboard_short_quota_fill;
static lv_obj_t *dashboard_week_quota_fill;
static lv_obj_t *dashboard_media_status_label;
static lv_obj_t *dashboard_media_title_label;
static lv_obj_t *dashboard_media_artist_label;
static lv_obj_t *dashboard_media_position_label;
static lv_obj_t *dashboard_media_duration_label;
static lv_obj_t *dashboard_media_lyric_label;
static lv_obj_t *dashboard_media_progress_knob;
static lv_timer_t *agent_done_blink_timer;
static lv_obj_t *performance_temperature_labels[2];
static lv_obj_t *performance_usage_labels[4];
static lv_obj_t *performance_usage_fills[4];
static lv_obj_t *performance_latency_label;
static lv_obj_t *performance_upload_label;
static lv_obj_t *performance_download_label;
static lv_obj_t *performance_network_chart;
static lv_chart_series_t *performance_network_series;
static lv_obj_t *dashboard_screen;
static lv_obj_t *performance_screen;
static lv_obj_t *syna_screen;
static lv_obj_t *syna_clock_label;
static lv_obj_t *syna_api_provider_label;
static lv_obj_t *syna_api_balance_label;
static lv_obj_t *syna_user_label;
static lv_obj_t *syna_assistant_label;
static lv_obj_t *syna_todo_count_label;
static lv_obj_t *syna_todo_images[UI_MAX_TODOS];
static lv_obj_t *syna_todo_labels[UI_MAX_TODOS];
static lv_obj_t *computers_screen;
static lv_obj_t *computer_rows[COMPUTER_ROWS_VISIBLE];
static lv_obj_t *computer_name_labels[COMPUTER_ROWS_VISIBLE];
static lv_obj_t *computer_state_labels[COMPUTER_ROWS_VISIBLE];
static lv_obj_t *computer_current_labels[COMPUTER_ROWS_VISIBLE];
static lv_obj_t *computer_selection_markers[COMPUTER_ROWS_VISIBLE];
static lv_obj_t *computer_found_label;
static lv_obj_t *assistant_overlay;
static lv_obj_t *assistant_overlay_label;
static bool assistant_active = false;
static char assistant_state[32] = "夏柠";
static char assistant_text[192] = "正在聆听…";
static char current_syna_user[160] = "";
static char current_syna_assistant[192] = "";
static char current_api_provider[32] = "API";
static char current_api_balance[48] = "CNY 86.42";
static ui_todo_item_t current_todos[UI_MAX_TODOS];
static int current_todo_count = 0;
static ui_wifi_state_t current_wifi_state = UI_WIFI_UNCONFIGURED;
static bool current_pc_connected = false;
static int current_battery_percent = -1;
static bool current_battery_valid = false;
static char current_agent_state[16] = "WORKING";
static char current_agent_task[96] = "Build visual simulator";
static int agent_home_mode = 1;
static bool quota_fill_valid[2] = {false, false};
static performance_state_t current_performance_state;
static int current_electricity_building;
static int current_electricity_room;
static float current_electricity_remaining;
static float current_electricity_used;
static bool current_electricity_configured;
static bool current_electricity_available;
static bool current_electricity_stale;

static void style_screen(lv_obj_t *screen);
static lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                            int32_t x, int32_t y);
static lv_obj_t *make_value_label(lv_obj_t *parent, const lv_font_t *font,
                                  int32_t x, int32_t y, int32_t width,
                                  int32_t height);
static lv_obj_t *make_panel(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t width, int32_t height);
static lv_obj_t *make_progress_fill(lv_obj_t *parent, int32_t y, int percent);
static lv_obj_t *make_performance_fill(lv_obj_t *parent, int32_t y, int percent);
static const lv_image_dsc_t *status_asset_for(const char *state);
static const lv_image_dsc_t *wifi_asset_for(ui_wifi_state_t state);
static const lv_image_dsc_t *pc_asset_for(bool connected);
static void agent_done_blink_cb(lv_timer_t *timer);
static void update_computer_rows(void);
static void computer_row_clicked(lv_event_t *event);
static void sync_assistant_visibility(void);
static void refresh_syna_conversation(void);
static void refresh_syna_todos(void);
static void refresh_agent_home(void);

static void update_battery_labels(void)
{
    char value[8];
    if(current_battery_valid) {
        int percent = current_battery_percent;
        if(percent < 0) percent = 0;
        if(percent > 100) percent = 100;
        snprintf(value, sizeof(value), "%d%%", percent);
    }
    else {
        snprintf(value, sizeof(value), "--");
    }

    lv_obj_t *labels[] = {dashboard_battery_label, performance_battery_label,
                          syna_battery_label, electricity_battery_label};
    for(size_t index = 0; index < sizeof(labels) / sizeof(labels[0]); ++index) {
        if(labels[index] != NULL && lv_obj_is_valid(labels[index]) &&
           strcmp(lv_label_get_text(labels[index]), value) != 0) {
            lv_label_set_text(labels[index], value);
        }
    }
}

static int network_chart_value(float upload_mb_per_second,
                               float download_mb_per_second)
{
    float kb_per_second = (upload_mb_per_second + download_mb_per_second) * 1024.0f;
    int value;
    if(kb_per_second < 1.0f) value = 0;
    else if(kb_per_second < 10.0f) value = 8 + (int)(kb_per_second * 2.0f);
    else if(kb_per_second < 100.0f) value = 28 + (int)(kb_per_second / 5.0f);
    else if(kb_per_second < 1000.0f) value = 48 + (int)(kb_per_second / 30.0f);
    else value = 78 + (int)(kb_per_second / 500.0f);
    if(value > 100) value = 100;
    return value;
}

static void ensure_assistant_overlay(void)
{
    if(assistant_overlay != NULL && lv_obj_is_valid(assistant_overlay)) return;
    assistant_overlay = make_panel(lv_layer_top(), 18, 231, 364, 52);
    lv_obj_set_style_border_width(assistant_overlay, 1, 0);
    lv_obj_set_style_radius(assistant_overlay, 0, 0);
    assistant_overlay_label = make_label(
        assistant_overlay, "夏柠  正在聆听…", &ui_font_14_cjk, 10, 14);
    lv_obj_set_size(assistant_overlay_label, 342, 22);
    lv_label_set_long_mode(assistant_overlay_label, LV_LABEL_LONG_CLIP);
    lv_obj_add_flag(assistant_overlay, LV_OBJ_FLAG_HIDDEN);
}

void ui_show_assistant_overlay(const char *state, const char *text)
{
    ensure_assistant_overlay();
    assistant_active = true;
    char line[256];
    const char *prefix = (state != NULL && state[0] != '\0') ? state : "夏柠";
    const char *message = (text != NULL && text[0] != '\0') ? text : "正在聆听…";
    snprintf(assistant_state, sizeof(assistant_state), "%s", prefix);
    snprintf(assistant_text, sizeof(assistant_text), "%s", message);
    snprintf(line, sizeof(line), "%s  %s", prefix, message);
    lv_label_set_text(assistant_overlay_label, line);
    if(current_page == PAGE_SYNA) {
        snprintf(current_syna_assistant, sizeof(current_syna_assistant), "%s", message);
        refresh_syna_conversation();
    }
    sync_assistant_visibility();
}

void ui_hide_assistant_overlay(void)
{
    assistant_active = false;
    if(assistant_overlay != NULL && lv_obj_is_valid(assistant_overlay)) {
        lv_obj_add_flag(assistant_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void sync_assistant_visibility(void)
{
    if(assistant_overlay == NULL || !lv_obj_is_valid(assistant_overlay)) return;
    if(assistant_active && current_page != PAGE_SYNA) {
        lv_obj_remove_flag(assistant_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(assistant_overlay);
    }
    else {
        lv_obj_add_flag(assistant_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void style_screen(lv_obj_t *screen)
{
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen, COLOR_BLACK, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                            int32_t x, int32_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, COLOR_BLACK, 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

static lv_obj_t *make_value_label(lv_obj_t *parent, const lv_font_t *font,
                                  int32_t x, int32_t y, int32_t width,
                                  int32_t height)
{
    lv_obj_t *label = make_label(parent, "--", font, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_bg_color(label, COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static lv_obj_t *make_panel(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t width, int32_t height)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_bg_color(panel, COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, COLOR_BLACK, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    return panel;
}

static lv_obj_t *make_progress_fill(lv_obj_t *parent, int32_t y, int percent)
{
    if(percent < 0) percent = 0;
    if(percent > 100) percent = 100;

    lv_obj_t *fill = lv_obj_create(parent);
    lv_obj_remove_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(fill, 19, y);
    lv_obj_set_size(fill, (142 * percent) / 100, 11);
    lv_obj_set_style_radius(fill, 3, 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_set_style_pad_all(fill, 0, 0);
    lv_obj_set_style_bg_color(fill, COLOR_BLACK, 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
    if(percent <= 0) lv_obj_add_flag(fill, LV_OBJ_FLAG_HIDDEN);
    return fill;
}

static lv_obj_t *make_performance_fill(lv_obj_t *parent, int32_t y, int percent)
{
    if(percent < 0) percent = 0;
    if(percent > 100) percent = 100;

    lv_obj_t *fill = lv_obj_create(parent);
    lv_obj_remove_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(fill, 18, y);
    lv_obj_set_size(fill, (150 * percent) / 100, 9);
    lv_obj_set_style_radius(fill, 3, 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_set_style_pad_all(fill, 0, 0);
    lv_obj_set_style_bg_color(fill, COLOR_BLACK, 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
    return fill;
}

static const lv_image_dsc_t *status_asset_for(const char *state)
{
    if(strcmp(state, "WAITING") == 0) return &ui_status_waiting;
    if(strcmp(state, "DONE") == 0) return &ui_status_done;
    if(strcmp(state, "IDLE") == 0) return &ui_status_idle;
    if(strcmp(state, "OFFLINE") == 0) return &ui_status_offline;
    return &ui_status_working;
}

static void agent_done_blink_cb(lv_timer_t *timer)
{
    (void)timer;
    if(strcmp(current_agent_state, "DONE") != 0 ||
       dashboard_agent_status_image == NULL ||
       !lv_obj_is_valid(dashboard_agent_status_image)) return;

    if(lv_obj_has_flag(dashboard_agent_status_image, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_remove_flag(dashboard_agent_status_image, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(dashboard_agent_status_image, LV_OBJ_FLAG_HIDDEN);
    }
}

static const lv_image_dsc_t *wifi_asset_for(ui_wifi_state_t state)
{
    switch(state) {
        case UI_WIFI_CONNECTED: return &ui_wifi_connected;
        case UI_WIFI_CONNECTING: return &ui_wifi_connecting;
        case UI_WIFI_PROVISIONING: return &ui_wifi_provisioning;
        default: return &ui_wifi_unconfigured;
    }
}

static const lv_image_dsc_t *pc_asset_for(bool connected)
{
    return connected ? &ui_pc_connected : &ui_pc_disconnected;
}

static lv_obj_t *make_white_mask(lv_obj_t *parent, int x, int y, int width, int height)
{
    lv_obj_t *mask = lv_obj_create(parent);
    lv_obj_remove_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(mask, x, y);
    lv_obj_set_size(mask, width, height);
    lv_obj_set_style_radius(mask, 0, 0);
    lv_obj_set_style_border_width(mask, 0, 0);
    lv_obj_set_style_pad_all(mask, 0, 0);
    lv_obj_set_style_bg_color(mask, COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_COVER, 0);
    return mask;
}

static lv_obj_t *make_rule(lv_obj_t *parent, int x, int y, int width, int height)
{
    lv_obj_t *rule = make_white_mask(parent, x, y, width, height);
    lv_obj_set_style_bg_color(rule, COLOR_BLACK, 0);
    return rule;
}

static void make_sparkle(lv_obj_t *parent, int x, int y)
{
    make_rule(parent, x + 4, y, 1, 2);
    make_rule(parent, x + 2, y + 2, 1, 1);
    make_rule(parent, x + 6, y + 2, 1, 1);
    make_rule(parent, x, y + 4, 9, 1);
    make_rule(parent, x + 2, y + 6, 1, 1);
    make_rule(parent, x + 6, y + 6, 1, 1);
    make_rule(parent, x + 4, y + 7, 1, 2);
}

static void make_heart(lv_obj_t *parent, int x, int y)
{
    static const char *rows[] = {
        "01100110", "11111111", "11111111",
        "01111110", "00111100", "00011000",
    };
    for(int row = 0; row < 6; ++row) {
        int col = 0;
        while(col < 8) {
            if(rows[row][col] != '1') { ++col; continue; }
            const int start = col;
            while(col < 8 && rows[row][col] == '1') ++col;
            make_rule(parent, x + start * 2, y + row * 2,
                      (col - start) * 2, 2);
        }
    }
}

/* One-bit, original pixel ornaments: solid strokes stay crisp on e-paper. */
static void make_pixel_frame(lv_obj_t *parent, int x, int y, int w, int h)
{
    make_rule(parent, x + 7, y, w - 14, 1);
    make_rule(parent, x + 7, y + h - 1, w - 14, 1);
    make_rule(parent, x, y + 7, 1, h - 14);
    make_rule(parent, x + w - 1, y + 7, 1, h - 14);
    make_rule(parent, x + 3, y + 3, 5, 1);
    make_rule(parent, x + 3, y + 3, 1, 5);
    make_rule(parent, x + w - 8, y + 3, 5, 1);
    make_rule(parent, x + w - 4, y + 3, 1, 5);
    make_rule(parent, x + 3, y + h - 4, 5, 1);
    make_rule(parent, x + 3, y + h - 8, 1, 5);
    make_rule(parent, x + w - 8, y + h - 4, 5, 1);
    make_rule(parent, x + w - 4, y + h - 8, 1, 5);
    make_rule(parent, x + 11, y + 3, 3, 2);
    make_rule(parent, x + w - 14, y + 3, 3, 2);
    make_rule(parent, x + 11, y + h - 5, 3, 2);
    make_rule(parent, x + w - 14, y + h - 5, 3, 2);
}

static void make_pixel_sprig(lv_obj_t *parent, int x, int y)
{
    make_rule(parent, x + 7, y + 7, 2, 11);
    make_rule(parent, x + 2, y + 2, 5, 2);
    make_rule(parent, x, y + 4, 5, 2);
    make_rule(parent, x + 2, y + 6, 5, 2);
    make_rule(parent, x + 9, y, 5, 2);
    make_rule(parent, x + 11, y + 2, 5, 2);
    make_rule(parent, x + 9, y + 4, 5, 2);
    make_rule(parent, x + 5, y + 17, 6, 1);
}

static void make_pixel_ribbon(lv_obj_t *parent, const char *title)
{
    make_rule(parent, 13, 5, 185, 18);
    make_rule(parent, 10, 8, 3, 12);
    make_rule(parent, 198, 8, 3, 12);
    lv_obj_t *label = make_label(parent, title, &ui_font_14_cjk, 19, 6);
    lv_obj_set_style_text_color(label, COLOR_WHITE, 0);
    make_rule(parent, 21, 18, 5, 1);
    make_rule(parent, 185, 18, 5, 1);
}

static void make_pixel_footer(lv_obj_t *parent)
{
    make_rule(parent, 18, 258, 364, 1);
    make_rule(parent, 18, 261, 8, 1);
    make_rule(parent, 374, 261, 8, 1);
}

static void make_editorial_shell(lv_obj_t *screen, const char *section)
{
    make_pixel_ribbon(screen, section);
    make_rule(screen, 18, 70, 364, 1);
    make_pixel_footer(screen);
    make_label(screen, "BAT", &ui_font_11_regular, 322, 270);
}

void ui_update_electricity(int building, int room, float remaining_kwh,
                           float used_kwh, bool configured, bool available,
                           bool stale)
{
    current_electricity_building = building;
    current_electricity_room = room;
    current_electricity_remaining = remaining_kwh;
    current_electricity_used = used_kwh;
    current_electricity_configured = configured;
    current_electricity_available = available;
    current_electricity_stale = stale;
    if(dashboard_electricity_label != NULL) {
        char compact[24];
        if(available && remaining_kwh < 1000.0f)
            snprintf(compact, sizeof(compact), "%.1f", remaining_kwh);
        else if(available) snprintf(compact, sizeof(compact), "%.0f", remaining_kwh);
        else snprintf(compact, sizeof(compact), "--");
        lv_label_set_text(dashboard_electricity_label, compact);
        lv_label_set_text(dashboard_electricity_title, stale ? "余电*/度" : "余电/度");
    }
    if(electricity_screen == NULL) return;
    char text[80];
    if(configured) snprintf(text, sizeof(text), "%d 栋  /  %d 室", building, room);
    else snprintf(text, sizeof(text), "请先设置宿舍");
    lv_label_set_text(electricity_room_label, text);
    if(available) {
        snprintf(text, sizeof(text), "%.2f", remaining_kwh);
        lv_label_set_text(electricity_balance_label, text);
        snprintf(text, sizeof(text), "已用电量  %.1f 度", used_kwh);
        lv_label_set_text(electricity_usage_label, text);
    } else {
        lv_label_set_text(electricity_balance_label, "--.--");
        lv_label_set_text(electricity_usage_label,
                          configured ? "查询失败或需要校园网" : "在设置页填写楼栋和房间");
    }
    lv_label_set_text(electricity_status_label,
                      !configured ? "未配置" : stale ? "上次数据" :
                      available ? "已更新" : "等待查询");
}

void ui_show_electricity(void)
{
    current_page = PAGE_ELECTRICITY;
    sync_assistant_visibility();
    if(electricity_screen != NULL) {
        lv_screen_load(electricity_screen);
        return;
    }
    lv_obj_t *screen = lv_obj_create(NULL);
    electricity_screen = screen;
    style_screen(screen);
    make_editorial_shell(screen, "04 / 夏柠 · 用电");
    make_label(screen, "WUYI UNIVERSITY  /  DORM", &ui_font_11_regular, 18, 44);
    make_label(screen, "宿舍余电", &ui_font_14_cjk, 18, 81);
    make_sparkle(screen, 368, 86);
    electricity_room_label = make_label(screen, "", &ui_font_14_cjk, 18, 111);
    electricity_status_label = make_label(screen, "", &ui_font_14_cjk, 292, 110);
    lv_obj_set_size(electricity_status_label, 90, 22);
    lv_obj_set_style_text_align(electricity_status_label, LV_TEXT_ALIGN_RIGHT, 0);
    make_rule(screen, 18, 141, 364, 1);
    make_label(screen, "REMAINING", &ui_font_11_regular, 18, 153);
    electricity_balance_label = make_label(screen, "--.--", &lv_font_montserrat_42, 18, 175);
    lv_obj_set_size(electricity_balance_label, 267, 54);
    make_label(screen, "kWh / 度", &ui_font_14_cjk, 279, 205);
    electricity_usage_label = make_label(screen, "", &ui_font_14_cjk, 18, 235);
    lv_obj_set_size(electricity_usage_label, 340, 20);
    electricity_battery_label = make_value_label(screen, &ui_font_11_regular,
                                                  354, 268, 36, 18);
    update_battery_labels();
    ui_update_electricity(current_electricity_building, current_electricity_room,
                          current_electricity_remaining, current_electricity_used,
                          current_electricity_configured, current_electricity_available,
                          current_electricity_stale);
    lv_screen_load(screen);
}

void ui_show_dashboard(void)
{
    current_page = PAGE_DASHBOARD;
    sync_assistant_visibility();
    if(dashboard_screen != NULL) {
        clock_label = dashboard_clock_label;
        lv_screen_load(dashboard_screen);
        ui_update_clock();
        return;
    }

    lv_obj_t *screen = lv_obj_create(NULL);
    dashboard_screen = screen;
    style_screen(screen);
    const computer_t *computer = &computers[current_computer];

    make_editorial_shell(screen, "01 / 夏柠 · 首页");
    make_label(screen, "TEMP", &ui_font_11_regular, 180, 27);
    make_label(screen, "HUM", &ui_font_11_regular, 245, 27);
    dashboard_electricity_title = make_label(screen, "余电/度", &ui_font_14_cjk, 312, 26);
    make_rule(screen, 17, 165, 166, 1);
    make_rule(screen, 193, 103, 187, 1);
    make_rule(screen, 193, 181, 187, 1);
    make_label(screen, "NOW PLAYING", &ui_font_11_regular, 193, 81);
    make_sparkle(screen, 370, 81);
    lv_obj_t *portrait_frame = make_panel(screen, 16, 78, 82, 82);
    lv_obj_set_style_radius(portrait_frame, 0, 0);
    make_label(screen, "♪", &ui_font_14_cjk, 193, 113);
    make_heart(screen, 365, 237);

    clock_label = make_label(screen, "--:--", &lv_font_montserrat_42, 18, 24);
    dashboard_clock_label = clock_label;
    lv_obj_set_width(clock_label, 150);
    ui_update_clock();

    dashboard_temperature_label = make_value_label(screen, &ui_font_18_regular,
                                                    180, 44, 59, 22);
    dashboard_humidity_label = make_value_label(screen, &ui_font_18_regular,
                                                 245, 44, 57, 22);
    dashboard_electricity_label = make_value_label(screen, &ui_font_18_regular,
                                                    312, 44, 77, 22);
    lv_label_set_text(dashboard_electricity_label, "--");
    dashboard_battery_label = make_value_label(screen, &ui_font_11_regular,
                                                354, 268, 36, 18);
    lv_obj_set_style_text_align(dashboard_temperature_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_align(dashboard_humidity_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_align(dashboard_electricity_label, LV_TEXT_ALIGN_LEFT, 0);
    ui_update_electricity(current_electricity_building, current_electricity_room,
                          current_electricity_remaining, current_electricity_used,
                          current_electricity_configured, current_electricity_available,
                          current_electricity_stale);

    dashboard_wifi_status_image = lv_image_create(screen);
    lv_image_set_src(dashboard_wifi_status_image, wifi_asset_for(current_wifi_state));
    lv_obj_set_pos(dashboard_wifi_status_image, 18, 270);
    dashboard_pc_status_image = lv_image_create(screen);
    lv_image_set_src(dashboard_pc_status_image, pc_asset_for(current_pc_connected));
    lv_obj_set_pos(dashboard_pc_status_image, 205, 270);

    /* Keep all live Agent content inside the blank left card. */
    lv_obj_t *avatar = lv_image_create(screen);
    lv_image_set_src(avatar, &ui_character_avatar);
    lv_obj_set_pos(avatar, 17, 79);
    make_label(screen, "AI AGENT", &ui_font_14_regular, 101, 88);
    dashboard_agent_status_image = lv_image_create(screen);
    lv_image_set_src(dashboard_agent_status_image,
                     status_asset_for(current_agent_state));
    lv_obj_set_pos(dashboard_agent_status_image, 99, 115);
    dashboard_agent_login_label = make_label(
        screen, "请登录", &ui_font_14_cjk, 99, 120);
    lv_obj_set_size(dashboard_agent_login_label, 88, 20);
    lv_obj_set_style_text_align(dashboard_agent_login_label, LV_TEXT_ALIGN_CENTER, 0);
    if(strcmp(current_agent_state, "LOGIN_REQUIRED") == 0) {
        lv_obj_add_flag(dashboard_agent_status_image, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(dashboard_agent_login_label, LV_OBJ_FLAG_HIDDEN);
    }

    dashboard_media_status_label = make_label(
        screen, "未播放", &ui_font_14_cjk, 304, 80);
    lv_obj_set_size(dashboard_media_status_label, 77, 18);
    dashboard_media_title_label = make_label(
        screen, "网易云音乐", &ui_font_14_cjk, 216, 113);
    lv_label_set_long_mode(dashboard_media_title_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(dashboard_media_title_label, 165, 18);
    dashboard_media_artist_label = make_label(
        screen, "--", &ui_font_14_cjk, 216, 139);
    lv_label_set_long_mode(dashboard_media_artist_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(dashboard_media_artist_label, 165, 18);
    dashboard_media_position_label = make_label(
        screen, "--:--", &ui_font_11_regular, 193, 189);
    dashboard_media_duration_label = make_label(
        screen, "--:--", &ui_font_11_regular, 349, 189);
    lv_obj_set_width(dashboard_media_duration_label, 34);
    lv_obj_set_style_text_align(dashboard_media_duration_label, LV_TEXT_ALIGN_RIGHT, 0);
    dashboard_media_lyric_label = make_label(
        screen, "暂无歌词", &ui_font_14_cjk, 191, 216);
    lv_label_set_long_mode(dashboard_media_lyric_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(dashboard_media_lyric_label, 190, 20);
    lv_obj_set_style_text_align(dashboard_media_lyric_label, LV_TEXT_ALIGN_CENTER, 0);

    dashboard_media_progress_knob = lv_obj_create(screen);
    lv_obj_remove_flag(dashboard_media_progress_knob, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(dashboard_media_progress_knob, 191, 178);
    lv_obj_set_size(dashboard_media_progress_knob, 5, 5);
    lv_obj_set_style_radius(dashboard_media_progress_knob, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dashboard_media_progress_knob, 0, 0);
    lv_obj_set_style_pad_all(dashboard_media_progress_knob, 0, 0);
    lv_obj_set_style_bg_color(dashboard_media_progress_knob, COLOR_BLACK, 0);
    lv_obj_set_style_bg_opa(dashboard_media_progress_knob, LV_OPA_COVER, 0);

    char short_quota[24];
    char week_quota[24];
    if(computer->codex_short_percent >= 0) {
        snprintf(short_quota, sizeof(short_quota), "%d%%", computer->codex_short_percent);
        snprintf(week_quota, sizeof(week_quota), "%d%%", computer->codex_week_percent);
    }
    else {
        snprintf(short_quota, sizeof(short_quota), "--");
        snprintf(week_quota, sizeof(week_quota), "--");
    }

    /* All three home layouts are composed from live LVGL objects. */
    dashboard_short_quota_title = make_label(
        screen, "5小时额度", &ui_font_14_cjk, 17, 170);
    dashboard_week_quota_title = make_label(
        screen, "周额度", &ui_font_14_cjk, 17, 211);
    dashboard_short_quota_track = make_panel(screen, 17, 190, 146, 14);
    dashboard_week_quota_track = make_panel(screen, 17, 231, 146, 14);
    lv_obj_set_style_radius(dashboard_short_quota_track, 0, 0);
    lv_obj_set_style_radius(dashboard_week_quota_track, 0, 0);
    dashboard_short_quota_fill = make_progress_fill(
        screen, 192, computer->codex_short_percent);
    dashboard_week_quota_fill = make_progress_fill(
        screen, 233, computer->codex_week_percent);
    dashboard_short_quota_label = make_label(
        screen, short_quota, &ui_font_18_regular, 106, 169);
    lv_obj_set_size(dashboard_short_quota_label, 57, 22);
    lv_label_set_long_mode(dashboard_short_quota_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(dashboard_short_quota_label, LV_TEXT_ALIGN_RIGHT, 0);
    dashboard_week_quota_label = make_label(
        screen, week_quota, &ui_font_18_regular, 106, 210);
    lv_obj_set_size(dashboard_week_quota_label, 57, 22);
    lv_label_set_long_mode(dashboard_week_quota_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(dashboard_week_quota_label, LV_TEXT_ALIGN_RIGHT, 0);
    dashboard_agent_task_title = make_label(
        screen, "当前任务", &ui_font_14_cjk, 17, 170);
    dashboard_agent_task_label = make_label(
        screen, current_agent_task, &ui_font_14_cjk, 17, 190);
    lv_obj_set_size(dashboard_agent_task_label, 146, 35);
    lv_label_set_long_mode(dashboard_agent_task_label, LV_LABEL_LONG_WRAP);
    refresh_agent_home();
    lv_screen_load(screen);
}

void ui_show_performance(void)
{
    current_page = PAGE_PERFORMANCE;
    sync_assistant_visibility();
    if(performance_screen != NULL) {
        clock_label = performance_clock_label;
        lv_screen_load(performance_screen);
        ui_update_clock();
        return;
    }

    lv_obj_t *screen = lv_obj_create(NULL);
    performance_screen = screen;
    style_screen(screen);
    make_editorial_shell(screen, "02 / 夏柠 · 性能");
    make_label(screen, "CPU TEMP", &ui_font_11_regular, 200, 13);
    make_label(screen, "GPU TEMP", &ui_font_11_regular, 318, 13);
    make_label(screen, "RESOURCE LOAD", &ui_font_11_regular, 18, 80);
    make_label(screen, "NETWORK / LIVE", &ui_font_11_regular, 200, 80);
    make_sparkle(screen, 372, 82);
    make_rule(screen, 18, 101, 150, 1);
    make_rule(screen, 200, 101, 182, 1);
    make_rule(screen, 184, 80, 1, 168);
    make_label(screen, "PING", &ui_font_11_regular, 200, 124);
    make_label(screen, "UPLOAD", &ui_font_11_regular, 200, 146);
    make_label(screen, "DOWNLOAD", &ui_font_11_regular, 200, 168);
    make_rule(screen, 200, 185, 182, 1);

    clock_label = make_label(screen, "--:--", &lv_font_montserrat_38, 18, 24);
    performance_clock_label = clock_label;
    lv_label_set_long_mode(clock_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(clock_label, 150, 44);
    ui_update_clock();

    performance_temperature_labels[0] =
        make_value_label(screen, &ui_font_18_regular, 200, 31, 59, 22);
    performance_temperature_labels[1] =
        make_value_label(screen, &ui_font_18_regular, 318, 31, 60, 22);
    lv_obj_set_style_text_align(performance_temperature_labels[0], LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_align(performance_temperature_labels[1], LV_TEXT_ALIGN_LEFT, 0);

    const int usage[] = {38, 67, 42, 15};
    const int bar_y[] = {129, 168, 207, 246};
    const int label_y[] = {105, 144, 183, 222};
    const char *metric_names[] = {"CPU", "MEMORY", "GPU", "DISK"};
    for(int i = 0; i < 4; ++i) {
        make_label(screen, metric_names[i], &ui_font_11_regular, 18, label_y[i] + 5);
        lv_obj_t *track = make_panel(screen, 17, bar_y[i] - 2, 152, 13);
        lv_obj_set_style_radius(track, 3, 0);
        char percent[8];
        snprintf(percent, sizeof(percent), "%d%%", usage[i]);
        lv_obj_t *value = make_label(screen, percent, &ui_font_18_regular,
                                     116, label_y[i]);
        performance_usage_labels[i] = value;
        lv_obj_set_size(value, 52, 22);
        lv_label_set_long_mode(value, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
        performance_usage_fills[i] = make_performance_fill(screen, bar_y[i], usage[i]);
    }

    lv_obj_t *latency = make_label(screen, "12 ms", &ui_font_11_regular, 334, 124);
    performance_latency_label = latency;
    lv_obj_set_width(latency, 47);
    lv_obj_set_style_text_align(latency, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *upload = make_label(screen, "2.4 MB/s", &ui_font_11_regular, 321, 146);
    performance_upload_label = upload;
    lv_obj_set_width(upload, 60);
    lv_obj_set_style_text_align(upload, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *download = make_label(screen, "18.7 MB/s", &ui_font_11_regular, 314, 168);
    performance_download_label = download;
    lv_obj_set_width(download, 67);
    lv_obj_set_style_text_align(download, LV_TEXT_ALIGN_RIGHT, 0);

    performance_network_chart = lv_chart_create(screen);
    lv_obj_set_pos(performance_network_chart, 200, 191);
    lv_obj_set_size(performance_network_chart, 182, 48);
    lv_obj_set_style_bg_color(performance_network_chart, COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(performance_network_chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(performance_network_chart, 0, 0);
    lv_obj_set_style_radius(performance_network_chart, 0, 0);
    lv_obj_set_style_pad_all(performance_network_chart, 0, 0);
    lv_obj_set_style_line_width(performance_network_chart, 1, LV_PART_ITEMS);
    lv_obj_set_style_size(performance_network_chart, 0, 0, LV_PART_INDICATOR);
    lv_chart_set_type(performance_network_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(performance_network_chart, 0, 0);
    lv_chart_set_point_count(performance_network_chart, 32);
    lv_chart_set_range(performance_network_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    performance_network_series = lv_chart_add_series(
        performance_network_chart, COLOR_BLACK, LV_CHART_AXIS_PRIMARY_Y);
    for(int i = 0; i < 32; ++i) {
        lv_chart_set_next_value(performance_network_chart,
                                performance_network_series, 0);
    }

    make_white_mask(screen, 354, 268, 36, 18);
    performance_battery_label = make_value_label(
        screen, &ui_font_11_regular, 354, 268, 36, 18);
    update_battery_labels();
    performance_wifi_status_image = lv_image_create(screen);
    lv_image_set_src(performance_wifi_status_image, wifi_asset_for(current_wifi_state));
    lv_obj_set_pos(performance_wifi_status_image, 18, 270);
    performance_pc_status_image = lv_image_create(screen);
    lv_image_set_src(performance_pc_status_image, pc_asset_for(current_pc_connected));
    lv_obj_set_pos(performance_pc_status_image, 205, 270);
    ui_update_performance(
        current_performance_state.cpu_percent,
        current_performance_state.memory_percent,
        current_performance_state.gpu_percent,
        current_performance_state.gpu_valid,
        current_performance_state.disk_percent,
        current_performance_state.cpu_temperature_c,
        current_performance_state.cpu_temperature_valid,
        current_performance_state.gpu_temperature_c,
        current_performance_state.gpu_temperature_valid,
        current_performance_state.latency_ms,
        current_performance_state.upload_mb_per_second,
        current_performance_state.download_mb_per_second,
        current_performance_state.connected);
    lv_screen_load(screen);
}

static void refresh_syna_conversation(void)
{
    if(syna_user_label != NULL && lv_obj_is_valid(syna_user_label)) {
        char text[192];
        snprintf(text, sizeof(text), "你\n%s", current_syna_user[0] ? current_syna_user : "…");
        lv_label_set_text(syna_user_label, text);
    }
    if(syna_assistant_label != NULL && lv_obj_is_valid(syna_assistant_label)) {
        char text[224];
        snprintf(text, sizeof(text), "夏柠\n%s",
                 current_syna_assistant[0] ? current_syna_assistant : "…");
        lv_label_set_text(syna_assistant_label, text);
    }
}

static void refresh_syna_todos(void)
{
    int completed = 0;
    for(int index = 0; index < current_todo_count; ++index) {
        if(current_todos[index].completed) ++completed;
    }
    if(syna_todo_count_label != NULL && lv_obj_is_valid(syna_todo_count_label)) {
        char count[24];
        snprintf(count, sizeof(count), "%d/%d", completed, current_todo_count);
        lv_label_set_text(syna_todo_count_label, count);
    }
    for(int index = 0; index < UI_MAX_TODOS; ++index) {
        if(syna_todo_images[index] == NULL || syna_todo_labels[index] == NULL) continue;
        if(index < current_todo_count) {
            lv_image_set_src(syna_todo_images[index], current_todos[index].completed
                                 ? &ui_syna_todo_checked
                                 : &ui_syna_todo_unchecked);
            lv_label_set_text(syna_todo_labels[index], current_todos[index].text);
            lv_obj_remove_flag(syna_todo_images[index], LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(syna_todo_labels[index], LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(syna_todo_images[index], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(syna_todo_labels[index], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ui_show_syna(void)
{
    current_page = PAGE_SYNA;
    sync_assistant_visibility();
    if(syna_screen != NULL) {
        clock_label = syna_clock_label;
        refresh_syna_conversation();
        refresh_syna_todos();
        lv_screen_load(syna_screen);
        ui_update_clock();
        return;
    }

    lv_obj_t *screen = lv_obj_create(NULL);
    syna_screen = screen;
    style_screen(screen);
    make_editorial_shell(screen, "03 / 夏柠 · 对话");
    make_label(screen, "API BALANCE", &ui_font_11_regular, 208, 13);
    make_label(screen, "DIALOGUE / 夏柠", &ui_font_14_cjk, 18, 79);
    make_sparkle(screen, 186, 83);
    make_label(screen, "TODAY / TASKS", &ui_font_11_regular, 221, 81);
    make_rule(screen, 18, 103, 178, 1);
    make_rule(screen, 221, 103, 160, 1);
    make_rule(screen, 206, 79, 1, 168);

    clock_label = make_label(screen, "--:--", &lv_font_montserrat_42, 18, 24);
    syna_clock_label = clock_label;
    lv_obj_set_width(clock_label, 150);

    syna_api_provider_label = make_label(screen, current_api_provider,
                                         &ui_font_11_regular, 208, 35);
    lv_obj_set_size(syna_api_provider_label, 88, 17);
    lv_label_set_long_mode(syna_api_provider_label, LV_LABEL_LONG_CLIP);
    syna_api_balance_label = make_label(screen, current_api_balance,
                                        &ui_font_11_regular, 302, 35);
    lv_obj_set_size(syna_api_balance_label, 77, 17);
    lv_label_set_long_mode(syna_api_balance_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(syna_api_balance_label, LV_TEXT_ALIGN_RIGHT, 0);

    syna_todo_count_label = make_label(screen, "0/0", &ui_font_11_regular, 350, 81);
    lv_obj_set_size(syna_todo_count_label, 32, 17);
    lv_obj_set_style_text_align(syna_todo_count_label, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_t *user_bubble = make_panel(screen, 18, 111, 178, 48);
    lv_obj_set_style_radius(user_bubble, 0, 0);
    lv_obj_set_style_border_width(user_bubble, 0, 0);
    make_rule(screen, 18, 111, 178, 1);
    syna_user_label = make_label(user_bubble, "", &ui_font_14_cjk, 7, 3);
    lv_obj_set_size(syna_user_label, 164, 42);
    lv_label_set_long_mode(syna_user_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(syna_user_label, 0, 0);

    lv_obj_t *assistant_bubble = make_panel(screen, 18, 167, 178, 70);
    lv_obj_set_style_radius(assistant_bubble, 0, 0);
    lv_obj_set_style_border_width(assistant_bubble, 0, 0);
    make_rule(screen, 18, 167, 178, 1);
    syna_assistant_label = make_label(assistant_bubble, "", &ui_font_14_cjk, 7, 3);
    lv_obj_set_size(syna_assistant_label, 164, 62);
    lv_label_set_long_mode(syna_assistant_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(syna_assistant_label, 0, 0);

    for(int index = 0; index < UI_MAX_TODOS; ++index) {
        const int y = 114 + index * 31;
        syna_todo_images[index] = lv_image_create(screen);
        lv_image_set_src(syna_todo_images[index], &ui_syna_todo_unchecked);
        lv_obj_set_pos(syna_todo_images[index], 221, y);
        syna_todo_labels[index] = make_label(screen, "", &ui_font_14_cjk, 243, y - 2);
        lv_obj_set_size(syna_todo_labels[index], 138, 22);
        lv_label_set_long_mode(syna_todo_labels[index], LV_LABEL_LONG_CLIP);
    }

    make_white_mask(screen, 354, 268, 36, 18);
    syna_battery_label = make_value_label(screen, &ui_font_11_regular,
                                          354, 268, 36, 18);
    syna_wifi_status_image = lv_image_create(screen);
    lv_image_set_src(syna_wifi_status_image, wifi_asset_for(current_wifi_state));
    lv_obj_set_pos(syna_wifi_status_image, 18, 270);
    syna_pc_status_image = lv_image_create(screen);
    lv_image_set_src(syna_pc_status_image, pc_asset_for(current_pc_connected));
    lv_obj_set_pos(syna_pc_status_image, 205, 270);

    refresh_syna_conversation();
    refresh_syna_todos();
    update_battery_labels();
    ui_update_clock();
    lv_screen_load(screen);
}

void ui_show_computers(void)
{
    current_page = PAGE_COMPUTERS;
    sync_assistant_visibility();
    selected_computer = current_list_computer >= 0 ? current_list_computer : 0;
    clock_label = NULL;
    if(computers_screen != NULL) {
        update_computer_rows();
        lv_screen_load(computers_screen);
        return;
    }

    lv_obj_t *screen = lv_obj_create(NULL);
    computers_screen = screen;
    style_screen(screen);

    make_pixel_ribbon(screen, "04 / 夏柠 · 电脑");
    computer_found_label = make_label(screen, "3 FOUND", &ui_font_11_regular,
                                      300, 10);
    lv_obj_set_width(computer_found_label, 82);
    lv_obj_set_style_text_align(computer_found_label, LV_TEXT_ALIGN_RIGHT, 0);
    make_rule(screen, 18, 37, 364, 1);
    make_label(screen, "SELECT DEVICE", &ui_font_18_regular, 18, 51);
    make_rule(screen, 18, 79, 364, 1);

    for(int i = 0; i < COMPUTER_ROWS_VISIBLE; ++i) {
        int32_t row_y = 86 + i * 55;
        lv_obj_t *row = make_panel(screen, 18, row_y, 364, 50);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, computer_row_clicked, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);

        computer_rows[i] = row;
        computer_name_labels[i] = make_label(row, "",
                                             &ui_font_18_regular, 12, 3);
        computer_state_labels[i] = make_label(row, "", &ui_font_11_regular, 12, 29);

        computer_current_labels[i] = make_label(row, "",
                                                &ui_font_11_regular, 288, 10);
        computer_selection_markers[i] = make_rule(row, 4, 14, 3, 20);
    }

    make_pixel_footer(screen);
    make_label(screen, "UP/DOWN  SELECT", &ui_font_11_regular, 18, 272);
    make_label(screen, "ENTER  CONNECT", &ui_font_11_regular, 151, 272);
    make_label(screen, "ESC  BACK", &ui_font_11_regular, 309, 272);
    update_computer_rows();
    lv_screen_load(screen);
}

/* This page is cached like the other primary pages. */
static lv_obj_t *about_screen;
static void ui_show_about(void)
{
    current_page = PAGE_ABOUT;
    if(about_screen == NULL) {
        about_screen = lv_obj_create(NULL);
        style_screen(about_screen);
        make_pixel_ribbon(about_screen, "05 / 夏柠 · 关于");
        make_label(about_screen, "v1.0.0", &ui_font_11_regular, 338, 14);
        make_rule(about_screen, 18, 38, 364, 1);
        make_label(about_screen, "ABOUT", &ui_font_18_regular, 18, 51);
        make_label(about_screen, "关于", &ui_font_14_cjk, 111, 53);
        make_label(about_screen, "OPEN SOURCE CREDITS", &ui_font_11_regular, 18, 80);
        make_rule(about_screen, 18, 100, 364, 1);

        make_label(about_screen, "01 / ORIGINAL", &ui_font_11_regular, 18, 111);
        make_label(about_screen, "黑沐", &ui_font_14_cjk, 18, 132);
        make_label(about_screen, "github.com/heimumumu", &ui_font_11_regular, 18, 161);
        make_rule(about_screen, 194, 108, 1, 76);
        make_label(about_screen, "02 / UI REWORK", &ui_font_11_regular, 211, 111);
        make_label(about_screen, "玉米", &ui_font_14_cjk, 211, 132);
        make_label(about_screen, "github.com/yumi233", &ui_font_11_regular, 211, 161);
        make_heart(about_screen, 366, 135);

        make_label(about_screen, "SOURCE PROJECT", &ui_font_11_regular, 18, 204);
        make_label(about_screen, "heimumumu/Waveshare_ESP32_RLCD",
                   &ui_font_14_regular, 18, 222);
        make_rule(about_screen, 18, 248, 364, 1);
        make_label(about_screen, "MIT · 保留原作者版权与许可", &ui_font_14_cjk, 18, 258);
        make_label(about_screen, "Third-party assets retain their own licenses",
                   &ui_font_11_regular, 18, 281);
    }
    lv_screen_load(about_screen);
}

static void dismiss_signature(lv_timer_t *timer)
{
    lv_obj_t *panel = lv_timer_get_user_data(timer);
    lv_obj_delete(panel);
    lv_timer_delete(timer);
}

static void show_startup_signature(void)
{
    lv_obj_t *panel = lv_obj_create(lv_layer_top());
    lv_obj_set_size(panel, 400, 300);
    lv_obj_set_pos(panel, 0, 0);
    style_screen(panel);
    make_pixel_frame(panel, 18, 17, 364, 266);
    make_rule(panel, 130, 31, 140, 20);
    lv_obj_t *eyebrow = make_label(panel, "夏柠  /  欢迎", &ui_font_14_cjk, 130, 32);
    lv_obj_set_width(eyebrow, 140);
    lv_obj_set_style_text_align(eyebrow, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(eyebrow, COLOR_WHITE, 0);
    make_pixel_sprig(panel, 82, 113);
    make_pixel_sprig(panel, 303, 113);
    make_pixel_frame(panel, 154, 65, 92, 92);
    lv_obj_t *avatar = lv_image_create(panel);
    lv_image_set_src(avatar, &ui_character_avatar);
    lv_obj_set_pos(avatar, 160, 71);
    lv_obj_t *title = make_label(panel, "夏柠", &ui_font_28_brand, 0, 174);
    lv_obj_set_width(title, 400);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    make_rule(panel, 138, 213, 124, 1);
    lv_obj_t *status = make_label(panel, "正在启动...", &ui_font_14_cjk, 0, 223);
    lv_obj_set_width(status, 400);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *version = make_label(panel, "XIA NING  /  v1.0.0", &ui_font_11_regular, 0, 259);
    lv_obj_set_width(version, 400);
    lv_obj_set_style_text_align(version, LV_TEXT_ALIGN_CENTER, 0);
    lv_timer_create(dismiss_signature, 2000, panel);
}

void ui_toggle_page(void)
{
    if(current_page == PAGE_DASHBOARD) ui_show_performance();
    else if(current_page == PAGE_PERFORMANCE) ui_show_syna();
    else if(current_page == PAGE_SYNA) ui_show_electricity();
    else if(current_page == PAGE_ELECTRICITY) ui_show_about();
    else ui_show_dashboard();
}

void ui_select_computer(int direction)
{
    if(current_page != PAGE_COMPUTERS) {
        ui_show_computers();
        return;
    }

    if(computer_list_count <= 0) return;
    selected_computer =
        (selected_computer + direction + computer_list_count) % computer_list_count;
    if(selected_computer < computer_list_offset) computer_list_offset = selected_computer;
    if(selected_computer >= computer_list_offset + COMPUTER_ROWS_VISIBLE) {
        computer_list_offset = selected_computer - COMPUTER_ROWS_VISIBLE + 1;
    }
    update_computer_rows();
}

int ui_confirm_computer(void)
{
    if(current_page != PAGE_COMPUTERS || computer_list_count <= 0) return -1;

    current_list_computer = selected_computer;
    if(selected_computer < MOCK_COMPUTER_COUNT) current_computer = selected_computer;
    /* Dashboard values depend on the selected computer. Recreate this one
     * screen only when that selection actually changes. */
    lv_obj_t *old_dashboard = dashboard_screen;
    dashboard_screen = NULL;
    dashboard_clock_label = NULL;
    ui_show_dashboard();
    if(old_dashboard != NULL && old_dashboard != lv_screen_active()) {
        lv_obj_delete(old_dashboard);
    }
    return current_list_computer;
}

void ui_update_computer_list(const ui_computer_info_t *items, int count,
                             int current_index)
{
    if(count < 0) count = 0;
    if(count > UI_MAX_COMPUTERS) count = UI_MAX_COMPUTERS;
    computer_list_count = count;
    for(int i = 0; i < count; ++i) computer_list[i] = items[i];

    if(current_index >= 0 && current_index < count) current_list_computer = current_index;
    else if(count == 0) current_list_computer = -1;
    if(selected_computer < 0 || selected_computer >= count) {
        selected_computer = current_list_computer >= 0 ? current_list_computer : 0;
    }
    if(selected_computer < computer_list_offset) computer_list_offset = selected_computer;
    if(selected_computer >= computer_list_offset + COMPUTER_ROWS_VISIBLE) {
        computer_list_offset = selected_computer - COMPUTER_ROWS_VISIBLE + 1;
    }
    if(computer_list_count <= COMPUTER_ROWS_VISIBLE) computer_list_offset = 0;
    if(computers_screen != NULL) update_computer_rows();
}

void ui_cycle_mock_status(void)
{
    computer_t *computer = &computers[current_computer];
    if(strcmp(computer->connection, "ONLINE") != 0) return;

    if(strcmp(computer->agent_state, "WORKING") == 0) {
        computer->agent_state = "WAITING";
        computer->task = "Approval required";
    }
    else if(strcmp(computer->agent_state, "WAITING") == 0 ||
            strcmp(computer->agent_state, "IDLE") == 0) {
        computer->agent_state = "DONE";
        computer->task = "Task completed";
    }
    else {
        computer->agent_state = "WORKING";
        computer->task = "Build visual simulator";
    }

    lv_obj_t *old_dashboard = dashboard_screen;
    dashboard_screen = NULL;
    dashboard_clock_label = NULL;
    ui_show_dashboard();
    if(old_dashboard != NULL && old_dashboard != lv_screen_active()) {
        lv_obj_delete(old_dashboard);
    }
}

void ui_update_clock(void)
{
    if(clock_label == NULL || !lv_obj_is_valid(clock_label)) return;

    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if(local == NULL) return;

    char time_text[8];
    strftime(time_text, sizeof(time_text), "%H:%M", local);
    /* The screen only shows hours and minutes. Avoid invalidating a full-screen
     * monochrome render once per second when the visible text did not change. */
    if(strcmp(lv_label_get_text(clock_label), time_text) != 0) {
        lv_label_set_text(clock_label, time_text);
    }
}

void ui_update_environment(float temperature_c, float humidity_percent,
                           int battery_percent, bool environment_valid,
                           bool battery_valid)
{
    current_battery_percent = battery_percent;
    current_battery_valid = battery_valid;
    if(dashboard_temperature_label != NULL &&
       lv_obj_is_valid(dashboard_temperature_label)) {
        char value[12];
        if(environment_valid) snprintf(value, sizeof(value), "%.0f\xC2\xB0" "C", temperature_c);
        else snprintf(value, sizeof(value), "--");
        if(strcmp(lv_label_get_text(dashboard_temperature_label), value) != 0) {
            lv_label_set_text(dashboard_temperature_label, value);
        }
    }

    if(dashboard_humidity_label != NULL && lv_obj_is_valid(dashboard_humidity_label)) {
        char value[8];
        if(environment_valid) snprintf(value, sizeof(value), "%.0f%%", humidity_percent);
        else snprintf(value, sizeof(value), "--");
        if(strcmp(lv_label_get_text(dashboard_humidity_label), value) != 0) {
            lv_label_set_text(dashboard_humidity_label, value);
        }
    }

    update_battery_labels();
}

void ui_update_wifi_state(ui_wifi_state_t state)
{
    current_wifi_state = state;
    const lv_image_dsc_t *asset = wifi_asset_for(state);
    if(dashboard_wifi_status_image != NULL &&
       lv_obj_is_valid(dashboard_wifi_status_image)) {
        lv_image_set_src(dashboard_wifi_status_image, asset);
    }
    if(performance_wifi_status_image != NULL &&
       lv_obj_is_valid(performance_wifi_status_image)) {
        lv_image_set_src(performance_wifi_status_image, asset);
    }
    if(syna_wifi_status_image != NULL && lv_obj_is_valid(syna_wifi_status_image)) {
        lv_image_set_src(syna_wifi_status_image, asset);
    }
}

void ui_update_pc_connected(bool connected)
{
    current_pc_connected = connected;
    const lv_image_dsc_t *asset = pc_asset_for(connected);
    if(dashboard_pc_status_image != NULL && lv_obj_is_valid(dashboard_pc_status_image)) {
        lv_image_set_src(dashboard_pc_status_image, asset);
    }
    if(performance_pc_status_image != NULL && lv_obj_is_valid(performance_pc_status_image)) {
        lv_image_set_src(performance_pc_status_image, asset);
    }
    if(syna_pc_status_image != NULL && lv_obj_is_valid(syna_pc_status_image)) {
        lv_image_set_src(syna_pc_status_image, asset);
    }
}

void ui_update_syna_conversation(const char *user_text,
                                 const char *assistant_reply)
{
    if(user_text != NULL) {
        snprintf(current_syna_user, sizeof(current_syna_user), "%s", user_text);
    }
    if(assistant_reply != NULL) {
        snprintf(current_syna_assistant, sizeof(current_syna_assistant), "%s",
                 assistant_reply);
    }
    refresh_syna_conversation();
}

void ui_update_api_balance(const char *provider, const char *balance)
{
    snprintf(current_api_provider, sizeof(current_api_provider), "%s",
             provider != NULL && provider[0] ? provider : "API");
    snprintf(current_api_balance, sizeof(current_api_balance), "%s",
             balance != NULL && balance[0] ? balance : "--");
    if(syna_api_provider_label != NULL && lv_obj_is_valid(syna_api_provider_label)) {
        lv_label_set_text(syna_api_provider_label, current_api_provider);
    }
    if(syna_api_balance_label != NULL && lv_obj_is_valid(syna_api_balance_label)) {
        lv_label_set_text(syna_api_balance_label, current_api_balance);
    }
}

void ui_update_todos(const ui_todo_item_t *items, int count)
{
    if(count < 0) count = 0;
    if(count > UI_MAX_TODOS) count = UI_MAX_TODOS;
    if(items == NULL && count > 0) return;
    current_todo_count = count;
    for(int index = 0; index < count; ++index) {
        current_todos[index] = items[index];
        current_todos[index].text[sizeof(current_todos[index].text) - 1] = '\0';
    }
    refresh_syna_todos();
}

void ui_update_agent_state(const char *state)
{
    if(state == NULL || state[0] == '\0') state = "OFFLINE";
    const bool state_changed = strcmp(current_agent_state, state) != 0;
    if(!state_changed) return;
    snprintf(current_agent_state, sizeof(current_agent_state), "%s", state);
    if(dashboard_agent_status_image != NULL &&
       lv_obj_is_valid(dashboard_agent_status_image)) {
        lv_image_set_src(dashboard_agent_status_image,
                         status_asset_for(current_agent_state));
        if(strcmp(current_agent_state, "LOGIN_REQUIRED") == 0) {
            lv_obj_add_flag(dashboard_agent_status_image, LV_OBJ_FLAG_HIDDEN);
        }
        else if(state_changed) {
            lv_obj_remove_flag(dashboard_agent_status_image, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if(dashboard_agent_login_label != NULL &&
       lv_obj_is_valid(dashboard_agent_login_label)) {
        if(strcmp(current_agent_state, "LOGIN_REQUIRED") == 0) {
            lv_obj_remove_flag(dashboard_agent_login_label, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(dashboard_agent_login_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if(agent_done_blink_timer != NULL && state_changed) {
        if(strcmp(current_agent_state, "DONE") == 0) {
            lv_timer_reset(agent_done_blink_timer);
            lv_timer_resume(agent_done_blink_timer);
        }
        else {
            lv_timer_pause(agent_done_blink_timer);
        }
    }
}

static void set_agent_visible(lv_obj_t *object, bool visible)
{
    if(object == NULL || !lv_obj_is_valid(object)) return;
    if(visible) lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_agent_home(void)
{
    const bool show_short = agent_home_mode == 0;
    const bool show_week = agent_home_mode != 2;
    const bool show_task = agent_home_mode != 0;
    set_agent_visible(dashboard_short_quota_title, show_short);
    set_agent_visible(dashboard_short_quota_label, show_short);
    set_agent_visible(dashboard_short_quota_track, show_short);
    set_agent_visible(dashboard_short_quota_fill, show_short && quota_fill_valid[0]);
    set_agent_visible(dashboard_week_quota_title, show_week);
    set_agent_visible(dashboard_week_quota_label, show_week);
    set_agent_visible(dashboard_week_quota_track, show_week);
    set_agent_visible(dashboard_week_quota_fill, show_week && quota_fill_valid[1]);
    set_agent_visible(dashboard_agent_task_title, show_task);
    set_agent_visible(dashboard_agent_task_label, show_task);
    if(dashboard_agent_task_label != NULL && lv_obj_is_valid(dashboard_agent_task_label)) {
        lv_obj_set_height(dashboard_agent_task_label, show_week ? 22 : 56);
    }
}

void ui_set_agent_home_mode(int mode)
{
    if(mode < 0 || mode > 2) mode = 1;
    if(agent_home_mode == mode) return;
    agent_home_mode = mode;
    refresh_agent_home();
}

void ui_update_agent_task(const char *task)
{
    snprintf(current_agent_task, sizeof(current_agent_task), "%s",
             task != NULL && task[0] ? task : "暂无任务");
    if(dashboard_agent_task_label != NULL && lv_obj_is_valid(dashboard_agent_task_label)) {
        lv_label_set_text(dashboard_agent_task_label, current_agent_task);
    }
}

void ui_update_codex_quota(int short_remaining_percent,
                           int week_remaining_percent, bool connected, bool stale)
{
    const int values[2] = {short_remaining_percent, week_remaining_percent};
    lv_obj_t *labels[2] = {dashboard_short_quota_label,
                           dashboard_week_quota_label};
    lv_obj_t *fills[2] = {dashboard_short_quota_fill,
                          dashboard_week_quota_fill};
    for(int index = 0; index < 2; ++index) {
        const bool valid = connected && values[index] >= 0 && values[index] <= 100;
        quota_fill_valid[index] = valid && values[index] > 0;
        char text[8];
        if(valid) snprintf(text, sizeof(text), "%s%d%%", stale ? "~" : "", values[index]);
        else snprintf(text, sizeof(text), "--");
        if(labels[index] != NULL && lv_obj_is_valid(labels[index])) {
            lv_label_set_text(labels[index], text);
        }
        if(fills[index] != NULL && lv_obj_is_valid(fills[index])) {
            if(valid && values[index] > 0) {
                lv_obj_set_width(fills[index], (142 * values[index]) / 100);
                lv_obj_remove_flag(fills[index], LV_OBJ_FLAG_HIDDEN);
            }
            else {
                lv_obj_add_flag(fills[index], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    refresh_agent_home();
}

static void format_media_time(char *buffer, size_t size, int seconds, bool valid)
{
    if(!valid || seconds < 0) {
        snprintf(buffer, size, "--:--");
        return;
    }
    snprintf(buffer, size, "%02d:%02d", seconds / 60, seconds % 60);
}

void ui_update_media(bool available, const char *status, const char *title,
                     const char *artist, int position_seconds,
                     int duration_seconds, const char *lyric)
{
    const bool playing = available && status != NULL && strcmp(status, "playing") == 0;
    const bool paused = available && status != NULL && strcmp(status, "paused") == 0;
    if(dashboard_media_status_label != NULL) {
        lv_label_set_text(dashboard_media_status_label,
                          playing ? "正在播放" : (paused ? "已暂停" : "未播放"));
    }
    if(dashboard_media_title_label != NULL) {
        lv_label_set_text(dashboard_media_title_label,
                          available && title != NULL && title[0] ? title : "网易云音乐");
    }
    if(dashboard_media_artist_label != NULL) {
        lv_label_set_text(dashboard_media_artist_label,
                          available && artist != NULL && artist[0] ? artist : "--");
    }
    if(dashboard_media_lyric_label != NULL) {
        lv_label_set_text(dashboard_media_lyric_label,
                          available && lyric != NULL && lyric[0] ? lyric : "暂无歌词");
    }
    char position_text[12];
    char duration_text[12];
    const bool timeline_valid = available && duration_seconds > 0;
    format_media_time(position_text, sizeof(position_text), position_seconds, timeline_valid);
    format_media_time(duration_text, sizeof(duration_text), duration_seconds, timeline_valid);
    if(dashboard_media_position_label != NULL) {
        lv_label_set_text(dashboard_media_position_label, position_text);
    }
    if(dashboard_media_duration_label != NULL) {
        lv_label_set_text(dashboard_media_duration_label, duration_text);
    }
    if(dashboard_media_progress_knob != NULL) {
        int progress = timeline_valid ? (position_seconds * 182) / duration_seconds : 0;
        if(progress < 0) progress = 0;
        if(progress > 182) progress = 182;
        lv_obj_set_x(dashboard_media_progress_knob, 191 + progress);
    }
}

void ui_update_performance(float cpu_percent, float memory_percent,
                           float gpu_percent, bool gpu_valid,
                           float disk_percent, float cpu_temperature_c,
                           bool cpu_temperature_valid,
                           float gpu_temperature_c,
                           bool gpu_temperature_valid,
                           int latency_ms, float upload_mb_per_second,
                           float download_mb_per_second, bool connected)
{
    current_performance_state.cpu_percent = cpu_percent;
    current_performance_state.memory_percent = memory_percent;
    current_performance_state.gpu_percent = gpu_percent;
    current_performance_state.gpu_valid = gpu_valid;
    current_performance_state.disk_percent = disk_percent;
    current_performance_state.cpu_temperature_c = cpu_temperature_c;
    current_performance_state.cpu_temperature_valid = cpu_temperature_valid;
    current_performance_state.gpu_temperature_c = gpu_temperature_c;
    current_performance_state.gpu_temperature_valid = gpu_temperature_valid;
    current_performance_state.latency_ms = latency_ms;
    current_performance_state.upload_mb_per_second = upload_mb_per_second;
    current_performance_state.download_mb_per_second = download_mb_per_second;
    current_performance_state.connected = connected;
    if(performance_screen == NULL || !lv_obj_is_valid(performance_screen)) return;

    const float values[4] = {cpu_percent, memory_percent, gpu_percent, disk_percent};
    for(int index = 0; index < 4; ++index) {
        bool valid = connected && (index != 2 || gpu_valid);
        int percent = valid ? (int)(values[index] + 0.5f) : 0;
        if(percent < 0) percent = 0;
        if(percent > 100) percent = 100;
        char text[8];
        if(valid) snprintf(text, sizeof(text), "%d%%", percent);
        else snprintf(text, sizeof(text), "--");
        lv_label_set_text(performance_usage_labels[index], text);
        lv_obj_set_width(performance_usage_fills[index], (150 * percent) / 100);
    }

    char text[20];
    if(connected && cpu_temperature_valid)
        snprintf(text, sizeof(text), "%.0f\xC2\xB0" "C", cpu_temperature_c);
    else snprintf(text, sizeof(text), "--");
    lv_label_set_text(performance_temperature_labels[0], text);

    if(connected && gpu_temperature_valid)
        snprintf(text, sizeof(text), "%.0f\xC2\xB0" "C", gpu_temperature_c);
    else snprintf(text, sizeof(text), "--");
    lv_label_set_text(performance_temperature_labels[1], text);

    if(connected) snprintf(text, sizeof(text), "%d ms", latency_ms);
    else snprintf(text, sizeof(text), "--");
    lv_label_set_text(performance_latency_label, text);

    if(connected) snprintf(text, sizeof(text), "%.1f MB/s", upload_mb_per_second);
    else snprintf(text, sizeof(text), "--");
    lv_label_set_text(performance_upload_label, text);

    if(connected) snprintf(text, sizeof(text), "%.1f MB/s", download_mb_per_second);
    else snprintf(text, sizeof(text), "--");
    lv_label_set_text(performance_download_label, text);

    if(performance_network_chart != NULL && performance_network_series != NULL &&
       lv_obj_is_valid(performance_network_chart)) {
        lv_chart_set_next_value(
            performance_network_chart,
            performance_network_series,
            connected ? network_chart_value(upload_mb_per_second,
                                              download_mb_per_second) : 0);
    }
    ui_update_pc_connected(connected);
}

bool ui_is_computer_page(void)
{
    return current_page == PAGE_COMPUTERS;
}

bool ui_is_syna_page(void)
{
    return current_page == PAGE_SYNA;
}

void ui_init(void)
{
    lv_obj_t *startup_screen = lv_screen_active();
    ui_show_dashboard();
    show_startup_signature();
    agent_done_blink_timer = lv_timer_create(agent_done_blink_cb, 500, NULL);
    lv_timer_pause(agent_done_blink_timer);
    if(startup_screen != lv_screen_active()) lv_obj_delete(startup_screen);
}

static void update_computer_rows(void)
{
    if(computer_found_label != NULL) {
        char found[20];
        snprintf(found, sizeof(found), "%d FOUND", computer_list_count);
        lv_label_set_text(computer_found_label, found);
    }
    for(int i = 0; i < COMPUTER_ROWS_VISIBLE; ++i) {
        const int item_index = computer_list_offset + i;
        const bool visible = item_index < computer_list_count;
        if(!visible) {
            lv_obj_add_flag(computer_rows[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(computer_rows[i], LV_OBJ_FLAG_HIDDEN);
        const bool selected = item_index == selected_computer;
        const ui_computer_info_t *item = &computer_list[item_index];
        char state[48];
        snprintf(state, sizeof(state), "%s  AGENT %s",
                 item->online ? "ONLINE" : "OFFLINE", item->agent_state);
        lv_label_set_text(computer_name_labels[i], item->name);
        lv_label_set_text(computer_state_labels[i], state);
        lv_label_set_text(computer_current_labels[i],
                          item_index == current_list_computer ? "CURRENT" : "");
        set_agent_visible(computer_selection_markers[i], selected);

        /* White-on-black antialiased glyphs lose strokes after 1-bit
         * quantization. Keep the same rounded-card language as the dashboard,
         * and show selection with a bold black outline instead. */
        lv_obj_set_style_bg_color(computer_rows[i], COLOR_WHITE, 0);
        lv_obj_set_style_border_width(computer_rows[i], selected ? 2 : 1, 0);
        lv_obj_set_style_text_color(computer_rows[i], COLOR_BLACK, 0);
        lv_obj_set_style_text_color(computer_name_labels[i], COLOR_BLACK, 0);
        lv_obj_set_style_text_color(computer_state_labels[i], COLOR_BLACK, 0);
        lv_obj_set_style_text_color(computer_current_labels[i], COLOR_BLACK, 0);
    }
}

static void computer_row_clicked(lv_event_t *event)
{
    selected_computer = computer_list_offset +
                        (int)(uintptr_t)lv_event_get_user_data(event);
    update_computer_rows();
}
