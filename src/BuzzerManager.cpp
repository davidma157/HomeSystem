#include "BuzzerManager.h"
#include "esp_log.h"

const char TAG[] = "BUZZER";

BuzzerManager::BuzzerManager(uint8_t buzzerPin, uint8_t pwmChannel, uint8_t pwmResolution)
    : pin(buzzerPin), channel(pwmChannel), resolution(pwmResolution), is_initialized(false)
{
}

void BuzzerManager::begin()
{
    ledcSetup(channel, 2000, resolution);
    ledcAttachPin(pin, channel);
    is_initialized = true;
    ESP_LOGD(TAG, "Buzzer initialisé sur GPIO%d, channel %d\n", pin, channel);
}

void BuzzerManager::playTone(int frequency, int volume)
{
    if (!is_initialized)
    {
        ESP_LOGD(TAG, "Erreur: Buzzer non initialisé");
        return;
    }

    // Limiter le volume entre 0 et 255
    volume = constrain(volume, 0, 255);

    ledcWriteTone(channel, frequency);
    ledcWrite(channel, volume);
}

void BuzzerManager::stop()
{
    if (!is_initialized)
    {
        return;
    }

    ledcWriteTone(channel, 0);
    ledcWrite(channel, 0);
}

void BuzzerManager::playAlarmPattern()
{
    if (!is_initialized)
    {
        return;
    }

    // Pattern d'alarme: son aigu intense
    playTone(2500, 200);
    delay(200);
    stop();
    delay(100);
}

void BuzzerManager::playBeep(int duration)
{
    if (!is_initialized)
    {
        return;
    }

    playTone(2000, 150);
    delay(duration);
    stop();
}

void BuzzerManager::playDoubleBeep()
{
    if (!is_initialized)
    {
        return;
    }

    playBeep(100);
    delay(100);
    playBeep(100);
}

void BuzzerManager::playStartupSound()
{
    if (!is_initialized)
    {
        return;
    }

    // Son de démarrage ascendant
    playTone(1000, 100);
    delay(100);
    playTone(1500, 120);
    delay(100);
    playTone(2000, 150);
    delay(100);
    stop();
}