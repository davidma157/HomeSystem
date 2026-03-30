#include "ConfigManager.h"
#include "DeviceController.h"
#include "Sensor.h"
#include "TemperatureReader.h"

#ifdef DEBUG_MODE
#include "HomeDebug.h"
#endif

// ============= CONFIGURATION GLOBALE =============
extern const int bufferSize = 256;
char payload_buffer[bufferSize];

// ============= OBJETS GLOBAUX =============
std::unique_ptr<DeviceController> controller;
std::unique_ptr<TemperatureReader> sensor;

#ifdef DEBUG_MODE
std::unique_ptr<HomeDebug> homeDebug;
#endif

// ============= CLASSE MQTT HANDLER =============
/**
 * @brief Gère les callbacks MQTT avec accès au DeviceController
 */
class MqttHandler
{
public:
    static void setController(DeviceController *ctrl)
    {
        instance = ctrl;
    }

    static void callback(char *topic, byte *payload, unsigned int length)
    {
        if (!instance)
        {
            Serial.println("ERREUR: Controller non initialisé dans callback");
            return;
        }

        Serial.println("\n----- MQTT Message Reçu -----");

        // Copier le payload dans un buffer sécurisé
        char message[128];
        size_t copyLength = (length >= sizeof(message)) ? sizeof(message) - 1 : length;
        memcpy(message, payload, copyLength);
        message[copyLength] = '\0';

        Serial.printf("Topic: %s\nMessage: %s\n", topic, message);
        Serial.println("-----------------------------");

        // Dispatcher les commandes
        if (strstr(topic, "config") != nullptr)
        {
            handleConfigUpdate(message);
        }
        else if (strstr(topic, "restart") != nullptr)
        {
            handleRestart();
        }
        else if (strstr(topic, "get_mac") != nullptr)
        {
            handleMacRequest();
        }

        // Boucle MQTT pour lire les messages suivants
        instance->getNetworkManager()->mqttLoop();
    }

private:
    static DeviceController *instance;

    static void handleConfigUpdate(char *configJson)
    {
        Serial.println("→ Mise à jour configuration");

        NetworkManager *network = instance->getNetworkManager();
        ConfigManager *config = instance->getConfigManager();
        Sensor *sensor = instance->getSensor();

        if (!network || !config || !sensor)
        {
            Serial.println("✗ Composants non disponibles");
            return;
        }

        const char *pos = ConfigManager::jsonFindValue(configJson, "mac");
        if (!pos)
        {
            Serial.println("✗ Pas de MAC dans le message");
            return;
        }

        char mac[20];
        ConfigManager::jsonExtractString(pos, mac, sizeof(mac));

        // Vérifier que la MAC correspond
        if (strcmp(mac, network->getMacAddress()) != 0)
        {
            Serial.println("✗ MAC ne correspond pas");
            return;
        }

        bool needsSave = false;

        // Extraire et mettre à jour l'ID device
        pos = ConfigManager::jsonFindValue(configJson, "id_device");
        if (pos)
        {
            int idDevice = ConfigManager::jsonExtractInt(pos);
            if (idDevice != config->getDeviceId())
            {
                config->setDeviceId(idDevice);
                needsSave = true;
                Serial.printf("✓ Nouveau ID device: %d\n", idDevice);

                network->setTopicParameters(idDevice, sensor->getTopicDomain());
                network->subscribeToTopics();
            }
        }

        // Mettre à jour la config spécifique du capteur
        if (sensor->updateConfig(configJson))
        {
            needsSave = true;
        }

        // Sauvegarder et publier si nécessaire
        if (needsSave)
        {
            config->save();

            config->jsonPrintConfig(payload_buffer, bufferSize);
            Serial.println("✓ Configuration mise à jour:");
            Serial.println(payload_buffer);

            network->publish(TopicType::CFG, payload_buffer);
        }
    }

    static void handleRestart()
    {
        Serial.println("→ Commande de redémarrage reçue");
        delay(500);
        ESP.restart();
    }

    static void handleMacRequest()
    {
        Serial.println("→ Demande MAC/IP");

        NetworkManager *network = instance->getNetworkManager();
        ConfigManager *config = instance->getConfigManager();

        if (network && config)
        {
            snprintf(payload_buffer, bufferSize,
                     "{\"id_device\":%d,\"ip\":\"%s\",\"mac\":\"%s\"}",
                     config->getDeviceId(),
                     network->getIPAddress(),
                     network->getMacAddress());

            network->publish(TopicType::MAC_IP, payload_buffer);
            Serial.println("✓ MAC/IP envoyé");
        }
    }
};

// Initialiser le pointeur statique
DeviceController *MqttHandler::instance = nullptr;

// ============= SETUP =============
void setup()
{
    Serial.begin(115200);
    delay(3000); // Attendre stabilisation USB

    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║  ESP32-C3 Temperature Reader v2.0      ║");
    Serial.println("║  Architecture refactorisée avec C++14  ║");
    Serial.println("╚════════════════════════════════════════╝\n");

    try
    {
        // 1. Créer le contrôleur principal
        sensor = std::make_unique<TemperatureReader>();
        if (!sensor)
        {
            Serial.println("ERREUR: Allocation Sensor échouée");
            ESP.restart();
        }
        controller = std::make_unique<DeviceController>(sensor.get());

        // 2. Initialiser tous les composants
        controller->initialize();

        // 3. Configurer le callback MQTT
        MqttHandler::setController(controller.get());
        controller->getNetworkManager()->setMessageCallback(MqttHandler::callback);

        // 4. Exécuter le cycle principal
        controller->run();

        // 5. Arrêt propre
        controller->shutdown();
    }
    catch (const std::exception &e)
    {
        Serial.printf("EXCEPTION CRITIQUE: %s\n", e.what());
        delay(1000);
        ESP.restart();
    }
    catch (...)
    {
        Serial.println("ERREUR CRITIQUE INCONNUE");
        delay(1000);
        ESP.restart();
    }
}

// ============= LOOP =============
void loop()
{
#ifdef DEBUG_MODE
    // Mode debug: simulation sans deep sleep
    if (!homeDebug && controller)
    {
        Serial.println("\n========================================");
        Serial.println("Initialisation HomeDebug");
        Serial.println("========================================");

        homeDebug = std::make_unique<HomeDebug>(
            controller->getNetworkManager(),
            controller->getConfigManager(),
            controller->getSensor());

        controller->getConfigManager()->jsonPrintConfig(payload_buffer, bufferSize);
        Serial.printf("Configuration: %s\n", payload_buffer);
    }

    if (homeDebug)
    {
        homeDebug->exec();
    }
#else
    // Mode production: loop non utilisé car deep sleep
    delay(10000);
#endif
}