#include "DeviceController.h"
#include "secretConfig.h"

DeviceController::DeviceController(Sensor *sensor)
    : sensor(sensor), sleepMgr(GPIO_NUM_4), isInitialized(false)
{
}

void DeviceController::initialize()
{
    Serial.println("\n=== Initialisation du Device Controller ===");

    // 1. Configuration LED
    configureLED();

    // 2. Chargement de la configuration
    config.load();

    // 3. Configuration du sleep manager
    sleepMgr.configureLowPowerMode();
    sleepMgr.setTimerPeriod(config.getSleepPeriod());
    sleepMgr.printWakeupReason();

    // 4. Initialisation du réseau
    initializeNetwork();

    // 5. Initialisation du capteur
    initializeSensor();

#ifdef DEBUG_MODE
    Serial.println("\n╔═══════════════════════════════════════╗");
    Serial.println("║     MODE DEBUG ACTIVÉ                 ║");
    Serial.println("║  • Deep sleep DÉSACTIVÉ               ║");
    Serial.println("║  • USB Serial reste actif             ║");
    Serial.println("╚═══════════════════════════════════════╝");
    Serial.printf("Sensor: %s\n\n", sensor->getDescription());
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
    if (config.getDeviceId() == 0)
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

// ========== Méthodes d'initialisation ==========

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

    sensor->setup(network.get(), // Pointeur brut via .get()
                  &config);
    network->setTopicParameters(config.getDeviceId(), sensor->getTopicDomain());
    sensor->begin();

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
             config.getDeviceId(), network->getMacAddress());

    if (!network->publish(TopicType::STATUS, payloadBuffer))
    {
        Serial.println("Échec publication demande ID");
        return;
    }

    unsigned long startTime = millis();

    while (config.getDeviceId() == 0 &&
           (millis() - startTime) < DEVICE_ID_TIMEOUT_MS)
    {
        network->mqttLoop();
        delay(1000);
        Serial.print(".");
    }
    Serial.println();

    if (config.getDeviceId() == 0)
    {
        Serial.println("⚠ Timeout: Aucun ID reçu");
        esp_restart();
    }
    else
    {
        Serial.printf("✓ ID reçu: %d\n", config.getDeviceId());
        network->setTopicParameters(config.getDeviceId(), sensor->getTopicDomain());
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

    if (sensor)
    {
        sensor->executeJob();
        Serial.println("✓ Tâche capteur terminée");
    }
    else
    {
        Serial.println("✗ Capteur non disponible");
    }
}

// ========== Méthodes de publication ==========

void DeviceController::publishStatus(const char *status)
{
    snprintf(payloadBuffer, sizeof(payloadBuffer),
             "{\"id_device\":%d,\"status\":\"%s\"}",
             config.getDeviceId(), status);

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
             config.getDeviceId(), config.getCounter());

    config.incrementCounter();
    config.save();

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
    Serial.printf("Sleep period: %d secondes\n", config.getSleepPeriod());
    Serial.printf("I'mAlive period: %d secondes\n", config.getAlivePeriod());
#else
    Serial.println("\n=== Passage en deep sleep ===");
    delay(100);

    if (config.getSleepPeriod() > 0)
    {
        sleepMgr.goToDeepSleep();
    }
#endif
}