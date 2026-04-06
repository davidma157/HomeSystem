#ifndef HOME_DEBUG_H
#define HOME_DEBUG_H

#include "Devicecontroller.h"
#include "NetworkManager.h"
#include "Sensor.h"
#include <Arduino.h>

class HomeDebug
{
private:
    uint8_t lgLine = 0;
    uint8_t lgLineMax = 60;
    long lastCheckAlive = 0;
    unsigned long lastCheckSleep = 0;
    unsigned long currentTime = 0;

    bool noSleepMsgSent = false;
    NetworkManager *network;
    // ConfigManager *configMgr;
    DeviceController *deviceCtrl;

    void connectNetwork();
    void simulSleepPeriod();
    void simulAlivePeriod();
    void printDot();

public:
    HomeDebug(NetworkManager *network, DeviceController *deviceCtrl);
    void exec();
};

#endif