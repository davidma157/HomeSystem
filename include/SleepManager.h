#ifndef SLEEP_MANAGER_H
#define SLEEP_MANAGER_H

#include <esp_sleep.h>
#include <esp_pm.h>
#include <Arduino.h>

class SleepManager {
private:
    gpio_num_t wakeup_pin;
    uint64_t timer_period_us;
    void setWakeupPin(gpio_num_t pin);
    
public:
    SleepManager(gpio_num_t pin = GPIO_NUM_4);
    
    // Configuration
    void setTimerPeriod(uint64_t period_seconds);
    void configureLowPowerMode();
    
    // Sleep control
    void goToDeepSleep();
    
    // Wakeup info
    esp_sleep_wakeup_cause_t getWakeupCause();
    bool isWakeupByGPIO();
    bool isWakeupByTimer();
    void printWakeupReason();
};

#endif