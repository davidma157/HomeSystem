#include "HomeDebug.h"

extern const int bufferSize;
extern char payload_buffer[];

HomeDebug::HomeDebug(NetworkManager *network, ConfigManager *config, Sensor *sensor)
    : network(network), config(config), sensor(sensor)
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
    if (config->getSleepPeriod() > 0)
    {
        if (currentTime - lastCheckSleep >= config->getSleepPeriod() * 1000)
        {
            lastCheckSleep = currentTime;
            sensor->executeJob();
            Serial.printf("SleepPeriod : Attente de %llu sec\n\n", config->getSleepPeriod());
        }
        else
        {
#ifdef DEBUG_MODE
            sensor->debugMode();
#endif
        }
    }
}
void HomeDebug::simulAlivePeriod()
{
    if (config->getAlivePeriod() > 0)
    {
        // Simulation Cycle I'm Alive
        if (currentTime - lastCheckAlive >= config->getAlivePeriod() * 1000)
        {
            lastCheckAlive = currentTime;
            Serial.println("\n--- Cycle I'm Alive ---");

            snprintf(payload_buffer, bufferSize,
                     "{\"id_device\":\"%d\",\"status\":\"alive\",\"counter\":%d}",
                     config->getDeviceId(), config->getCounter());
            config->incrementCounter();
            network->publish(TopicType::STATUS, payload_buffer);
            Serial.printf("AlivePeriod : Attente de %llu sec\n\n", config->getAlivePeriod());
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
        config->jsonPrintConfig(payload_buffer, bufferSize);
        Serial.printf("\nConfig:%s\n", payload_buffer);
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