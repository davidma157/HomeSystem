#include "DeviceController.h"
#include "ConfigManager.h"
#include "Sensor.h"
#include "TemperatureReader.h"
#include "esp_log.h"
#include "secretConfig.h"

const char TAG[] = "DEV_CTRL";

DeviceController::DeviceController()
    : sleepMgr(GPIO_NUM_4), isInitialized(false)
{
}

void DeviceController::initialize()
{
    ESP_LOGD(TAG, "═══ Initialisation du Device Controller ═══");

    ESP_LOGV(TAG, "1. Initialisation du réseau");
    initializeNetwork();

    ESP_LOGV(TAG, "2. Load config");
    if (ConfigManager::loadConfig(&this->deviceConfig))
    {
        ConfigManager::printConfig(&this->deviceConfig);
        configureLED();

        if (this->deviceConfig.initialized)
        {
            network->setTopicIdentifiant(deviceConfig.id_device);
        }

        ESP_LOGV(TAG, "3. Configuration sleep manager");
        sleepMgr.configureLowPowerMode();
        sleepMgr.setTimerPeriod(deviceConfig.sleep_period);
        ESP_LOGV(TAG, "Print Wakeup reason");
        sleepMgr.printWakeupReason();

        // 5. Initialisation du capteur
        if (deviceConfig.initialized)
        {
            ESP_LOGV(TAG, "Initialisation des sensors");
            initializeSensors();
        }

#ifdef DEBUG_MODE
        ESP_LOGD(TAG, "╔═══════════════════════════════════════╗");
        ESP_LOGD(TAG, "║     MODE DEBUG ACTIVÉ                 ║");
        ESP_LOGD(TAG, "║  • Deep sleep DÉSACTIVÉ               ║");
        ESP_LOGD(TAG, "║  • USB Serial reste actif             ║");
        ESP_LOGD(TAG, "╚═══════════════════════════════════════╝\n");
#endif

        isInitialized = true;
        ESP_LOGD(TAG, "✓ Initialisation terminée avec succès");
    }
}

void DeviceController::run()
{
    ESP_LOGD(TAG, "════════════ Démarrage du cycle principal ════════════");
    if (!isInitialized)
    {
        ESP_LOGE(TAG, "ERREUR: DeviceController non initialisé");
        return;
    }

    // 1. Connexion réseau
    if (!connectToWiFi() || !connectToMQTT())
    {
        handleConnectionFailure();
        return;
    }

    // TODO - revoir la séquence
    if (this->deviceConfig.initialized)
    {
        ESP_LOGD(TAG, "Config Initialized");
        /* code */
        subscribeToTopics();

        // 2. Attendre l'ID device si nécessaire
        if (deviceConfig.id_device == 0)
        {
            waitForDeviceId();
        }

        // 3. Publier le message de démarrage
        publishStartMessage();

        // 4. Attendre les messages de configuration
        waitForMqttMessages();

        // 5. Exécuter la tâche du capteur
        executeSensorJob();

        // 6. Message de fin
        //        publishSleepMessage();
    }
    else
    {
        ESP_LOGD(TAG, "Config NOT Initialized");
        waitForDeviceId();
    }

    ESP_LOGI(TAG, "✓ Cycle principal terminé");
}

void DeviceController::shutdown()
{
    ESP_LOGD(TAG, "══════ Arrêt du système ══════");

    if (network)
    {
        network->disconnectMqtt();
        network->disconnectWiFi();
    }

    prepareForSleep();
}

bool DeviceController::update(char *configJson)
{
    ESP_LOGD(TAG, "-----------------------------------------------------------------------------------");
    bool needsRestart = false;

    DeviceConfig config;
    ConfigManager::extractConfig(configJson, &config);
    ESP_LOGD(TAG, "Print config reçue");
    ConfigManager::printConfig(&config);

    if (strcmp(config.mac, network->getMacAddress()) != 0)
    {
        ESP_LOGD(TAG, "Mac:'%s' -- NetMAC:'%s'", config.mac, network->getMacAddress());
        ESP_LOGD(TAG, "✗ MAC ne correspond pas ");
        return false;
    }

    if (this->deviceConfig == config)
    {
        ESP_LOGI(TAG, "✓ Configuration identique");
        return needsRestart;
    }

    ESP_LOGD(TAG, "Print config avant et après mise à jour");
    ConfigManager::printConfig(&this->deviceConfig);
    this->deviceConfig = config;
    ConfigManager::printConfig(&this->deviceConfig);

    // TODO - Vérifier s'il y a des changements avant de sauvegarder
    ConfigManager::save(&this->deviceConfig);

    ESP_LOGI(TAG, "✓ Configuration mise à jour:");
    network->publish(TopicType::CFG, "Config saved");
    needsRestart = true;

    return needsRestart;
}

// ========== Méthodes d'initialisation ==========

bool DeviceController::macVerify(char *configJson)
{
    // TODO n'est plus utilisée
    char mac[20];
    ConfigManager::getMac(configJson, mac, sizeof(mac));

    if (strcmp(mac, network->getMacAddress()) != 0)
    {
        ESP_LOGD(TAG, "✗ MAC ne correspond pas");
        return false;
    }

    ESP_LOGD(TAG, "MAC:%s", mac);

    return true;
}

void DeviceController::configureLED()
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    ESP_LOGD(TAG, "✓ LED configurée");
}

void DeviceController::initializeNetwork()
{
    ESP_LOGD(TAG, "Initialisation du NetworkManager...");

    network = std::make_unique<NetworkManager>(
        WIFI_SSID, WIFI_PASSWORD,
        MQTT_BROKER, MQTT_PORT,
        MQTT_USERNAME, MQTT_PASSWORD);

    if (!network)
    {
        ESP_LOGE(TAG, "ERREUR: Allocation NetworkManager échouée");
        ESP.restart();
    }

    ESP_LOGI(TAG, "✓ NetworkManager créé");
}

void DeviceController::initializeSensors()
{
    ESP_LOGD(TAG, "Initialisation des Capteurs ...");

    Sensor *sensor = nullptr;
    for (size_t i = 0; i < deviceConfig.num_sensors; i++)
    {
        SensorData *sd = &deviceConfig.sensors[i];
        switch (sd->role)
        {
        case SensorRole::TEMPERATURE:
            ESP_LOGD(TAG, "New TemperatureReader()");
            sensor = new TemperatureReader(network.get(), deviceConfig.id_device, sd);
            sensors[i] = sensor;
            nbSensors++;
            break;

        case SensorRole::WATER_DETECTION:
            ESP_LOGD(TAG, "New WATER_DETECTION");
            break;
        case SensorRole::UNDEFINED:
            ESP_LOGE(TAG, "UNDEFINED sensor");
            break;
        default:
            break;
        }
    }

    ESP_LOGD(TAG, "✓ Capteurs initialisés");
}

// ========== Méthodes de connexion ==========
/****
 * Connexion au WIFI.
 * Si la connexion ne fonctionne pas on augmente la puissance progressivement.
 */
bool DeviceController::connectToWiFi()
{
    ESP_LOGD(TAG, "--- Connexion WiFi ---");

    uint8_t attempt = 0;
    while (!network->isWiFiConnected() && attempt < MAX_WIFI_RETRIES)
    {
        network->connectWiFi();

        if (!network->isWiFiConnected())
        {
            attempt++;
            ESP_LOGE(TAG, "Tentative %d/%d échouée\n", attempt, MAX_WIFI_RETRIES);

            if (attempt < MAX_WIFI_RETRIES)
            {
                delay(1000);
            }
        }
    }

    if (network->isWiFiConnected())
    {
        ESP_LOGD(TAG, "✓ WiFi connecté");
        return true;
    }

    ESP_LOGE(TAG, "✗ Échec connexion WiFi");
    return false;
}

bool DeviceController::connectToMQTT()
{
    ESP_LOGD(TAG, "--- Connexion MQTT ---");
    network->connectMQTT();

    if (network->isMQTTConnected())
    {
        ESP_LOGD(TAG, "✓ MQTT connecté");
        return true;
    }

    ESP_LOGE(TAG, "✗ Échec connexion MQTT");
    return false;
}

void DeviceController::subscribeToTopics()
{
    if (network->subscribeToTopics())
    {
        ESP_LOGD(TAG, "✓ Abonnement aux topics réussi");
    }
    else
    {
        ESP_LOGE(TAG, "⚠ Échec abonnement aux topics");
    }
}

// ========== Méthodes de cycle de vie ==========

void DeviceController::waitForDeviceId()
{
    ESP_LOGD(TAG, "════════ Attente de l'ID device ════════");

    snprintf(payloadBuffer, sizeof(payloadBuffer),
             "{\"id_device\":%d,\"mac\":\"%s\"}",
             deviceConfig.id_device, network->getMacAddress());

    if (!network->publish(TopicType::STATUS, payloadBuffer))
    {
        ESP_LOGE(TAG, "Échec publication demande ID");
        return;
    }

    unsigned long startTime = millis();

    while (deviceConfig.id_device == 0 &&
           (millis() - startTime) < DEVICE_ID_TIMEOUT_MS)
    {
        network->mqttLoop();
        delay(1000);
        Serial.print(".");
    }
    Serial.println();

    if (deviceConfig.id_device == 0)
    {
        ESP_LOGE(TAG, "⚠ Timeout: Aucun ID reçu");
        esp_restart();
    }
    else
    {
        ESP_LOGD(TAG, "✓ ID reçu: %d", deviceConfig.id_device);
        network->setTopicIdentifiant(deviceConfig.id_device);
        network->subscribeToTopics();
    }
}

void DeviceController::waitForMqttMessages()
{
    ESP_LOGD(TAG, "\n--- Attente messages MQTT ---");

    for (uint8_t i = 0; i < MAX_MQTT_WAIT_LOOPS; i++)
    {
        network->mqttLoop();
        delay(MQTT_LOOP_DELAY_MS);
    }

    ESP_LOGD(TAG, "✓ Période d'attente terminée");
}

void DeviceController::executeSensorJob()
{
    ESP_LOGV(TAG, "================= Exécution tâche capteur. Nb Sensors:%d ===========", nbSensors);
    for (size_t i = 0; i < nbSensors; i++)
    {
        sensors[i]->executeJob();
    }
}

// ========== Méthodes de publication ==========

void DeviceController::publishStatus(const char *status)
{
    snprintf(payloadBuffer, sizeof(payloadBuffer),
             "{\"id_device\":%d,\"status\":\"%s\"}",
             deviceConfig.id_device, status);

    network->publish(TopicType::STATUS, payloadBuffer);
}

void DeviceController::publishStartMessage()
{
    ESP_LOGD(TAG, "--- Démarrage ---");
    publishStatus("start");
}

void DeviceController::publishSleepMessage()
{
    ESP_LOGD(TAG, "--- Publication message de sleep ---");

    snprintf(payloadBuffer, sizeof(payloadBuffer),
             "{\"id_device\":%d,\"status\":\"go_to_sleep\"}",
             deviceConfig.id_device);

    // TODO rtcConfig->counter++;
    // TODO - Vérifier configMgr.save();

    network->publish(TopicType::STATUS, payloadBuffer);
}

// ========== Gestion des erreurs ==========

void DeviceController::handleConnectionFailure()
{
    ESP_LOGE(TAG, "\n⚠ ÉCHEC CONNEXION RÉSEAU");
    ESP_LOGE(TAG, "Le système va se rendormir pour réessayer plus tard");

    // Se rendormir pour 5 minutes avant de réessayer
    sleepMgr.setTimerPeriod(300); // 5 minutes
    prepareForSleep();
}

void DeviceController::prepareForSleep()
{
#ifdef DEBUG_MODE
    ESP_LOGD(TAG, "=============== MODE DEBUG - SLEEP DÉSACTIVÉ ===============");
    ESP_LOGD(TAG, "L'USB reste actif pour le debugging");
    ESP_LOGD(TAG, "Sleep period: %d secondes", deviceConfig.sleep_period);
    ESP_LOGD(TAG, "I'mAlive period: %d secondes", deviceConfig.im_alive_period);
    ESP_LOGD(TAG, "============================================================\n");

#else
    ESP_LOGD(TAG, "\n=== Passage en deep sleep ===");
    delay(100);

    if (config.getSleepPeriod() > 0)
    {
        sleepMgr.goToDeepSleep();
    }
#endif
}