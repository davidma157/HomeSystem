#include "DeviceController.h"
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
    Serial.println("\n=== Initialisation du Device Controller ===");

    // 1. Configuration LED
    configureLED();

    // 4. Initialisation du réseau
    initializeNetwork();

    // 2. Chargement de la configuration
    configMgr.load();

    // 3. Configuration du sleep manager
    sleepMgr.configureLowPowerMode();
    // sleepMgr.setTimerPeriod(configDevice.sleep_period);
    sleepMgr.printWakeupReason();

    // 5. Initialisation du capteur
    initializeSensor();

#ifdef DEBUG_MODE
    Serial.println("\n╔═══════════════════════════════════════╗");
    Serial.println("║     MODE DEBUG ACTIVÉ                 ║");
    Serial.println("║  • Deep sleep DÉSACTIVÉ               ║");
    Serial.println("║  • USB Serial reste actif             ║");
    Serial.println("╚═══════════════════════════════════════╝");
    // TODO Serial.printf("Sensor: %s\n\n", sensor->getDescription());
    Serial.println("═══════════════════════════════════════════\n");
#endif

    isInitialized = true;
    Serial.println("✓ Initialisation terminée avec succès");
}

void DeviceController::run()
{
    if (!isInitialized)
    {
        Serial.println("ERREUR: DeviceController non initialisé");
        return;
    }

    Serial.println("\n=== Démarrage du cycle principal ===");

    // 1. Connexion réseau
    if (!connectToWiFi() || !connectToMQTT())
    {
        handleConnectionFailure();
        return;
    }

    subscribeToTopics();

    // 2. Attendre l'ID device si nécessaire
    if (configDevice.id_device == 0)
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
    publishSleepMessage();

    Serial.println("✓ Cycle principal terminé");
}

void DeviceController::shutdown()
{
    Serial.println("\n=== Arrêt du système ===");

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
    bool needsSave = false;

    if (macVerify(configJson))
    {
        needsSave = setConfig(configJson);
    }

    const char *arrayPtr = ConfigManager::findArrayStart(configJson, "SS");
    if (arrayPtr != nullptr)
    {
        char sensorJson[256];

        ESP_LOGD(TAG, "%s", arrayPtr);
        while (arrayPtr && (arrayPtr = ConfigManager::getNextObjectInArray(arrayPtr, sensorJson, sizeof(sensorJson))))
        {
            ESP_LOGD(TAG, "SensorJson:%s", sensorJson);
            char role[32];
            ConfigManager::jsonExtractString(ConfigManager::jsonFindValue(sensorJson, "ROLE"), role, sizeof(role));
            ESP_LOGD(TAG, "Role:%s", role);
            Sensor *sensor = nullptr;
            if (strcmp(role, "TEMPERATURE") == 0)
                sensor = new TemperatureReader();
            // else if ... (autres types)
            if (sensor)
            {
                sensor->setup(network.get());
                sensor->updateConfig(sensorJson); // C'EST ICI : Le capteur se démerde avec son JSON
                sensor->begin();
                sensors.push_back(sensor);
            }
        }
    }
    else
    {
    }
    /*
    // Mettre à jour la config spécifique du capteur
    if (sensor != nullptr)
    {
        if (sensor->updateConfig(configJson))
        {
            needsSave = true;
        }
        Serial.printf("--- needsSave:%d\n");
    }
    */

    if (needsSave)
    {
        ConfigManager::save(configJson);

        char buffer[100];
        jsonPrintConfig(buffer, 100);
        Serial.println("✓ Configuration mise à jour:");
        Serial.println(buffer);

        // TODO -- Revoir le message
        network->publish(TopicType::CFG, "Config saved");
    }

    return needsSave;
}

// ========== Méthodes d'initialisation ==========

bool DeviceController::setConfig(char *configJson)
{
    bool needsSave = false;

    // Extraire et mettre à jour l'ID device
    const char *pos = ConfigManager::jsonFindValue(configJson, "ID");
    if (pos)
    {
        int idDevice = ConfigManager::jsonExtractInt(pos);
        if (idDevice != configDevice.id_device)
        {
            configDevice.id_device = idDevice;
            needsSave = true;
            ESP_LOGI(TAG,"✓ Nouveau ID device: %d\n", idDevice);

            network->setTopicParameters(idDevice);
            network->subscribeToTopics();
        }
    }
    // Extraire sleep duration
    pos = ConfigManager::jsonFindValue(configJson, "SLEEP");
    if (pos)
    {
        int sleepPeriod = ConfigManager::jsonExtractInt(pos);
        if (configDevice.sleep_period != sleepPeriod)
        {
            configDevice.sleep_period = sleepPeriod;
            needsSave = true;
        }
    }

    pos = ConfigManager::jsonFindValue(configJson, "ALIVE");
    if (pos)
    {
        int alivePeriod = ConfigManager::jsonExtractInt(pos);
        if (configDevice.im_alive_period != alivePeriod)
        {
            configDevice.im_alive_period = alivePeriod;
            needsSave = true;
        }
    }
    return needsSave;
}

bool DeviceController::macVerify(char *configJson)
{
    const char *pos = ConfigManager::jsonFindValue(configJson, "MAC");
    if (!pos)
    {
        Serial.println("✗ Pas de MAC dans le message");
        return false;
    }

    char mac[20];
    ConfigManager::jsonExtractString(pos, mac, sizeof(mac));

    // Vérifier que la MAC correspond
    if (strcmp(mac, network->getMacAddress()) != 0)
    {
        Serial.println("✗ MAC ne correspond pas");
        return false;
    }
    Serial.println("---MAC OK");
    return true;
}

void DeviceController::configureLED()
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    Serial.println("✓ LED configurée");
}

void DeviceController::initializeNetwork()
{
    Serial.println("Initialisation du NetworkManager...");

    network = std::make_unique<NetworkManager>(
        WIFI_SSID, WIFI_PASSWORD,
        MQTT_BROKER, MQTT_PORT,
        MQTT_USERNAME, MQTT_PASSWORD);

    if (!network)
    {
        Serial.println("ERREUR: Allocation NetworkManager échouée");
        ESP.restart();
    }

    Serial.println("✓ NetworkManager créé");
}

void DeviceController::initializeSensor()
{
    Serial.println("Initialisation du capteur...");

    // TODO - Changer la logique
    //    sensor->setup(network.get());
    //  network->setTopicParameters(configDevice.id_device);
    // sensor->begin();

    Serial.println("✓ Capteur initialisé");
}

// ========== Méthodes de connexion ==========
/****
 * Connexion au WIFI.
 * Si la connexion ne fonctionne pas on augmente la puissance progressivement.
 */
bool DeviceController::connectToWiFi()
{
    Serial.println("\n--- Connexion WiFi ---");

    uint8_t attempt = 0;
    while (!network->isWiFiConnected() && attempt < MAX_WIFI_RETRIES)
    {
        network->connectWiFi();

        if (!network->isWiFiConnected())
        {
            attempt++;
            Serial.printf("Tentative %d/%d échouée\n", attempt, MAX_WIFI_RETRIES);

            if (attempt < MAX_WIFI_RETRIES)
            {
                delay(1000);
            }
        }
    }

    if (network->isWiFiConnected())
    {
        Serial.println("✓ WiFi connecté");
        return true;
    }

    Serial.println("✗ Échec connexion WiFi");
    return false;
}

bool DeviceController::connectToMQTT()
{
    Serial.println("--- Connexion MQTT ---");
    network->connectMQTT();

    if (network->isMQTTConnected())
    {
        Serial.println("✓ MQTT connecté");
        return true;
    }

    Serial.println("✗ Échec connexion MQTT");
    return false;
}

void DeviceController::subscribeToTopics()
{
    if (network->subscribeToTopics())
    {
        Serial.println("✓ Abonnement aux topics réussi");
    }
    else
    {
        Serial.println("⚠ Échec abonnement aux topics");
    }
}

// ========== Méthodes de cycle de vie ==========

void DeviceController::waitForDeviceId()
{
    Serial.println("\n--- Attente de l'ID device ---");

    snprintf(payloadBuffer, sizeof(payloadBuffer),
             "{\"id_device\":%d,\"mac\":\"%s\"}",
             configDevice.id_device, network->getMacAddress());

    if (!network->publish(TopicType::STATUS, payloadBuffer))
    {
        Serial.println("Échec publication demande ID");
        return;
    }

    unsigned long startTime = millis();

    while (configDevice.id_device == 0 &&
           (millis() - startTime) < DEVICE_ID_TIMEOUT_MS)
    {
        network->mqttLoop();
        delay(1000);
        Serial.print(".");
    }
    Serial.println();

    if (configDevice.id_device == 0)
    {
        Serial.println("⚠ Timeout: Aucun ID reçu");
        esp_restart();
    }
    else
    {
        Serial.printf("✓ ID reçu: %d\n", configDevice.id_device);
        network->setTopicParameters(configDevice.id_device);
        network->subscribeToTopics();
    }
}

void DeviceController::waitForMqttMessages()
{
    Serial.println("\n--- Attente messages MQTT ---");

    for (uint8_t i = 0; i < MAX_MQTT_WAIT_LOOPS; i++)
    {
        network->mqttLoop();
        delay(MQTT_LOOP_DELAY_MS);
    }

    Serial.println("✓ Période d'attente terminée");
}

void DeviceController::executeSensorJob()
{
    Serial.println("\n--- Exécution tâche capteur ---");
    // TODO
    /*
    if (sensor)
    {
        sensor->executeJob();
        Serial.println("✓ Tâche capteur terminée");
    }
    else
    {
        Serial.println("✗ Capteur non disponible");
    }
    */
}

// ========== Méthodes de publication ==========

void DeviceController::publishStatus(const char *status)
{
    snprintf(payloadBuffer, sizeof(payloadBuffer),
             "{\"id_device\":%d,\"status\":\"%s\"}",
             configDevice.id_device, status);

    network->publish(TopicType::STATUS, payloadBuffer);
}

void DeviceController::publishStartMessage()
{
    Serial.println("--- Publication message de démarrage ---");
    publishStatus("start");
}

void DeviceController::publishSleepMessage()
{
    Serial.println("--- Publication message de sleep ---");

    snprintf(payloadBuffer, sizeof(payloadBuffer),
             "{\"id_device\":%d,\"status\":\"go_to_sleep\",\"counter\":%d}",
             configDevice.id_device, configDevice.counter);

    configDevice.counter++;
    // TODO - Vérifier configMgr.save();

    network->publish(TopicType::STATUS, payloadBuffer);
}

// ========== Gestion des erreurs ==========

void DeviceController::handleConnectionFailure()
{
    Serial.println("\n⚠ ÉCHEC CONNEXION RÉSEAU");
    Serial.println("Le système va se rendormir pour réessayer plus tard");

    // Se rendormir pour 5 minutes avant de réessayer
    sleepMgr.setTimerPeriod(300); // 5 minutes
    prepareForSleep();
}

void DeviceController::prepareForSleep()
{
#ifdef DEBUG_MODE
    Serial.println("\n=== MODE DEBUG - SLEEP DÉSACTIVÉ ===");
    Serial.println("L'USB reste actif pour le debugging");
    Serial.printf("Sleep period: %d secondes\n", configDevice.sleep_period);
    Serial.printf("I'mAlive period: %d secondes\n", configDevice.im_alive_period);
#else
    Serial.println("\n=== Passage en deep sleep ===");
    delay(100);

    if (config.getSleepPeriod() > 0)
    {
        sleepMgr.goToDeepSleep();
    }
#endif
}