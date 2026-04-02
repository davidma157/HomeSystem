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
std::unique_ptr<DeviceController> deviceCtrl;
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

        Serial.println("\n------- MQTT Message Reçu -----");

        // Copier le payload dans un buffer sécurisé
        int msgLength = 512;
        char message[msgLength];
        if (length > msgLength)
        {
            // TODO - Publier un message.
            Serial.println("Message reçu dépasse la longueur du buffer.");
            Serial.printf("Topic: %s\nMessage: %s\n", topic, message);
            Serial.println("-----------------------------");
        }

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
        // Sensor *sensor = instance->getSensor();

        if (!network || !config)
        {
            Serial.println("✗ Composants non disponibles");
            return;
        }

        instance->update(configJson);
        // Sauvegarder et publier si nécessaire
        /*
        //TODO -- Ménage
        if (needsSave)
        {
            config->save();
            instance->jsonPrintConfig(payload_buffer, bufferSize);
            Serial.println("✓ Configuration mise à jour:");
            Serial.println(payload_buffer);

            network->publish(TopicType::CFG, payload_buffer);
        }
        */
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
                     instance->getDeviceId(),
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
        /*
        sensor = std::make_unique<TemperatureReader>();
        if (!sensor)
        {
            Serial.println("ERREUR: Allocation Sensor échouée");
            ESP.restart();
        }
        deviceCtrl = std::make_unique<DeviceController>(sensor.get());
        */

        deviceCtrl = std::make_unique<DeviceController>();
        // 2. Initialiser tous les composants
        deviceCtrl->initialize();

        // 3. Configurer le callback MQTT
        MqttHandler::setController(deviceCtrl.get());
        deviceCtrl->getNetworkManager()->setMessageCallback(MqttHandler::callback);

        // 4. Exécuter le cycle principal
        deviceCtrl->run();

        // 5. Arrêt propre
        deviceCtrl->shutdown();
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
    if (!homeDebug && deviceCtrl)
    {
        Serial.println("\n========================================");
        Serial.println("Initialisation HomeDebug");
        Serial.println("========================================");

        homeDebug = std::make_unique<HomeDebug>(
            deviceCtrl->getNetworkManager(),
            deviceCtrl.get());

        // controller->jsonPrintConfig(payload_buffer, bufferSize);
        // Serial.printf("Configuration: %s\n", payload_buffer);
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