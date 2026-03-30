#ifndef DEVICE_CONTROLLER_H
#define DEVICE_CONTROLLER_H

#include "ConfigManager.h"
#include "NetworkManager.h"
#include "Sensor.h"
#include "SleepManager.h"
#include <Arduino.h>
#include <memory>

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
    DeviceController(Sensor *sensor);
    ~DeviceController() = default;

    // Cycle de vie principal
    void initialize();
    void run();
    void shutdown();

    // Accesseurs pour callbacks externes
    NetworkManager *getNetworkManager() { return network.get(); }
    ConfigManager *getConfigManager() { return &config; }
    //  Sensor *getSensor() { return sensor->get(); }
    Sensor *getSensor() { return sensor; }

private:
    // ===== Composants du système =====
    Sensor *sensor;

    ConfigManager config;
    SleepManager sleepMgr;

    std::unique_ptr<NetworkManager> network;

    // ===== État interne =====
    bool isInitialized;
    char payloadBuffer[256];

    // ===== Méthodes privées =====
    void configureLED();
    void initializeNetwork();
    void initializeSensor();

    bool connectToWiFi();
    bool connectToMQTT();
    void subscribeToTopics();

    void waitForDeviceId();
    void waitForMqttMessages();
    void executeSensorJob();

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