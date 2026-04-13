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
            // Ajout du linefeed après les ......
            Serial.println();
            lastCheckSleep = currentTime;
            deviceCtrl->executeSensorJob();
            ESP_LOGD(TAG, "SleepPeriod : Attente de %d sec", deviceCtrl->getSleepPeriod());
        }
        else
        {
            deviceCtrl->debugMode();
        }
    }
    else
    {
        // Affichage du message une seule fois.
        if (!noSleepMsgSent)
        {
            ESP_LOGD(TAG, "SleepPeriod ==0 --> executeJob() n'est jamais appellé\n", deviceCtrl->getSleepPeriod());
            noSleepMsgSent = true;
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
            network->publish(TOPIC_PUB_STATUS, payload_buffer);
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

    // Get message from MQTT
    network->mqttLoop();
    currentTime = millis();
    simulSleepPeriod();
    simulAlivePeriod();
    printDot();
    delay(1000);
}