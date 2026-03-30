#include "SleepManager.h"

SleepManager::SleepManager(gpio_num_t pin)
    : wakeup_pin(pin), timer_period_us(3000000000) { // 3000 secondes par défaut
}

void SleepManager::setWakeupPin(gpio_num_t pin) {
    wakeup_pin = pin;
}

void SleepManager::setTimerPeriod(uint64_t period_seconds) {
    timer_period_us = period_seconds * 1000ULL * 1000ULL; // Conversion en microsecondes
    Serial.printf("Sleep period configuré: %llu secondes\n", period_seconds);
}

void SleepManager::configureLowPowerMode() {
    esp_pm_config_esp32c3_t pm_config = {
        .max_freq_mhz = 80,
        .min_freq_mhz = 10,
        .light_sleep_enable = true
    };
    
    esp_err_t result = esp_pm_configure(&pm_config);
    if (result == ESP_OK) {
        Serial.println("Mode économie d'énergie configuré");
    } else {
        Serial.printf("Erreur configuration power management: %d\n", result);
        Serial.printf("Normal en mode DEBUG\n");
    }
}

void SleepManager::goToDeepSleep() {
    // Configuration du GPIO wakeup
    uint64_t pin_mask = 1ULL << wakeup_pin;
    esp_deep_sleep_enable_gpio_wakeup(pin_mask, ESP_GPIO_WAKEUP_GPIO_LOW);
    
    // Configuration du timer wakeup
    esp_sleep_enable_timer_wakeup(timer_period_us);
    
    Serial.printf("Deep sleep activé:\n");
    Serial.printf("  - GPIO wakeup: GPIO%d (LOW)\n", wakeup_pin);
    Serial.printf("  - Timer wakeup: %llu secondes\n", timer_period_us / 1000000);
    Serial.println("Passage en deep sleep...");
    
    delay(100);
    esp_deep_sleep_start();
}

esp_sleep_wakeup_cause_t SleepManager::getWakeupCause() {
    return esp_sleep_get_wakeup_cause();
}

bool SleepManager::isWakeupByGPIO() {
    return getWakeupCause() == ESP_SLEEP_WAKEUP_GPIO;
}

bool SleepManager::isWakeupByTimer() {
    return getWakeupCause() == ESP_SLEEP_WAKEUP_TIMER;
}

void SleepManager::printWakeupReason() {
    esp_sleep_wakeup_cause_t cause = getWakeupCause();
    
    switch (cause) {
        case ESP_SLEEP_WAKEUP_GPIO:
            Serial.println(">>> ALERTE: Réveil par GPIO (eau détectée)!");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println(">>> Réveil par timer (I'mAlive)");
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            Serial.println(">>> Premier démarrage ou reset");
            break;
        default:
            Serial.printf(">>> Réveil par cause inconnue: %d\n", cause);
            break;
    }
}