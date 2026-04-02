#ifndef DEVICE_CONTROLLER_H
#define DEVICE_CONTROLLER_H

#include "ConfigManager.h"
#include "NetworkManager.h"
#include "Sensor.h"
#include "SleepManager.h"
#include <Arduino.h>
#include <memory>
#include <vector>

struct ConfigDevice
{
    int id_device = 0;
    int id_sensor = 0;
    int im_alive_period = 0;
    int sleep_period = 60;
    uint16_t counter = 0;
    const char *version = "2.0";
};

/**
 * @brief Contrôleur principal du dispositif IoT
 *
 * Centralise la logique métier et gère le cycle de vie complet:
 * - Initialisation des composants
 * - Connexion réseau
 * - Exécution des tâches
 * - Gestion du deep sleep
 */
class DeviceController
{
public:
    DeviceController();
    ~DeviceController() = default;

    // Cycle de vie principal
    void initialize();
    void run();
    void shutdown();
    bool update(char *configJson);

    // Accesseurs pour callbacks externes
    NetworkManager *getNetworkManager() { return network.get(); }
    ConfigManager *getConfigManager() { return &configMgr; }
    // TODO Sensor *getSensor() { return sensor; }
    void executeSensorJob();

    int getDeviceId() const { return configDevice.id_device; }
    int getSleepPeriod() const { return configDevice.sleep_period; }
    int getAlivePeriod() const { return configDevice.im_alive_period; }

#ifdef DEBUG_MODE
    void debugMode() {};
#endif

    // JSON parsing helpers
    void jsonPrintConfig(char *buffer, int bufferSize)
    {
        snprintf(buffer, bufferSize,
                 "{\"id_device\":%d, \"alive\":%d,\"sleep\":%d,\"version\":%s}",
                 configDevice.id_device, configDevice.im_alive_period, configDevice.sleep_period, configDevice.version);
    };

private:
    // ===== Composants du système =====
    //  Sensor *sensor;
    std::vector<Sensor *> sensors;

    ConfigDevice configDevice;
    ConfigManager configMgr;
    SleepManager sleepMgr;

    std::unique_ptr<NetworkManager> network;

    // ===== État interne =====
    bool isInitialized;
    char payloadBuffer[256];

    // ===== Méthodes privées =====
    void addSensor(Sensor *sensor) { sensors.push_back(sensor); }
    bool setConfig(char *configJson);
    bool macVerify(char *configJson);

    void configureLED();
    void initializeNetwork();
    void initializeSensor();

    bool connectToWiFi();
    bool connectToMQTT();
    void subscribeToTopics();

    void waitForDeviceId();
    void waitForMqttMessages();

    void publishStatus(const char *status);
    void publishStartMessage();
    void publishSleepMessage();

    void handleConnectionFailure();
    void prepareForSleep();

    // Constantes
    static constexpr int LED_PIN = 8;
    static constexpr uint8_t MAX_WIFI_RETRIES = 5;
    static constexpr uint8_t MAX_MQTT_WAIT_LOOPS = 20;
    static constexpr unsigned long DEVICE_ID_TIMEOUT_MS = 30000;
    static constexpr uint16_t MQTT_LOOP_DELAY_MS = 100;
};

#endif // DEVICE_CONTROLLER_H