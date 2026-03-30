#ifndef BUZZER_MANAGER_H
#define BUZZER_MANAGER_H

#include <Arduino.h>

class BuzzerManager {
private:
    uint8_t pin;
    uint8_t channel;
    uint8_t resolution;
    bool is_initialized;
    
public:
    BuzzerManager(uint8_t buzzerPin = 8, uint8_t pwmChannel = 0, uint8_t pwmResolution = 8);
    
    // Initialisation
    void begin();
    
    // Contrôle du son
    void playTone(int frequency, int volume = 128);
    void stop();
    
    // Patterns d'alarme prédéfinis
    void playAlarmPattern();
    void playBeep(int duration = 100);
    void playDoubleBeep();
    void playStartupSound();
    
    // Getters
    bool isInitialized() const { return is_initialized; }
};

#endif