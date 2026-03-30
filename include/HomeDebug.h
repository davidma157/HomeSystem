#ifndef HOME_DEBUG_H
#define HOME_DEBUG_H

#include <Arduino.h>
#include "Sensor.h"
#include "NetworkManager.h"
#include "ConfigManager.h"



class HomeDebug {
private:
    uint8_t lgLine = 0;
    uint8_t lgLineMax = 60;
    long lastCheckAlive = 0;
    unsigned long lastCheckSleep = 0;
    unsigned long currentTime = 0;


    NetworkManager* network;
    ConfigManager* config;
    Sensor* sensor;

    void connectNetwork();
    void simulSleepPeriod();
    void simulAlivePeriod();
    void printDot();

public:
    HomeDebug(NetworkManager* network, ConfigManager* config, Sensor* sensor);
    void exec();
};

#endif