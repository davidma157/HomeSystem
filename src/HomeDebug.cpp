#include "HomeDebug.h"
#include "esp_log.h"

const char TAG[] = "HomeDebug";

extern const int bufferSize;
extern char payload_buffer[];

HomeDebug::HomeDebug(NetworkManager *network, DeviceController *deviceCtrl)
    : network(network), deviceCtrl(deviceCtrl)
{
    currentTime = millis();
    connectNetwork();
}

void HomeDebug::connectNetwork()
{
    if (!network->isWiFiConnected())
    {
        network->connectWiFi();
    }
    if (!network->isMQTTConnected())
    {
        network->connectMQTT();
        network->subscribeToTopics();
    }
}

void HomeDebug::simulSleepPeriod()
{
    if (deviceCtrl->getSleepPeriod() > 0)
    {
        if (currentTime - lastCheckSleep >= deviceCtrl->getSleepPeriod() * 1000)
        {
            lastCheckSleep = currentTime;
            deviceCtrl->executeSensorJob();
            ESP_LOGD(TAG, "SleepPeriod : Attente de %llu sec\n\n", deviceCtrl->getSleepPeriod());
        }
        else
        {
            deviceCtrl->debugMode();
        }
    }
}
void HomeDebug::simulAlivePeriod()
{
    if (deviceCtrl->getAlivePeriod() > 0)
    {
        // Simulation Cycle I'm Alive
        if (currentTime - lastCheckAlive >= deviceCtrl->getAlivePeriod() * 1000)
        {
            lastCheckAlive = currentTime;
            ESP_LOGD(TAG, "\n--- Cycle I'm Alive ---");

            snprintf(payload_buffer, bufferSize,
                     "{\"id_device\":\"%d\",\"status\":\"alive\"}",
                     deviceCtrl->getDeviceId());
            // config->incrementCounter();
            network->publish(TopicType::STATUS, payload_buffer);
            ESP_LOGD(TAG, "AlivePeriod : Attente de %llu sec\n\n", deviceCtrl->getAlivePeriod());
        }
    }
}

void HomeDebug::printDot()
{
    Serial.print(".");
    lgLine++;
    if (lgLine >= lgLineMax)
    {
        Serial.println(".");
        lgLine = 0;
    }
}

void HomeDebug::exec()
{
    connectNetwork();
    // Get message from MQTT
    network->mqttLoop();
    currentTime = millis();
    simulSleepPeriod();
    simulAlivePeriod();
    printDot();
    delay(1000);
}