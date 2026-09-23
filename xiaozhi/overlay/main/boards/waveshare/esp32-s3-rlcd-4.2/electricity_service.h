#pragma once

#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

struct PanelElectricitySnapshot {
    uint32_t generation = 0;
    int building = 0;
    int room = 0;
    float remaining_kwh = 0;
    float used_kwh = 0;
    bool configured = false;
    bool available = false;
    bool stale = false;
};

class ElectricityService {
public:
    static ElectricityService& GetInstance();
    bool Start();
    bool GetSnapshot(PanelElectricitySnapshot& snapshot);
    void RequestRefresh();

private:
    ElectricityService() = default;
    static void TaskEntry(void* context);
    void Run();
    void Publish(const PanelElectricitySnapshot& snapshot);
    static bool WifiReady();
    static bool Fetch(int building, int room, float& remaining, float& used);

    SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t task_ = nullptr;
    PanelElectricitySnapshot snapshot_ = {};
};
