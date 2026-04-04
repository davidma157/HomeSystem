#include "ConfigManager.h"
#include "DeviceController.h"
#include "esp_log.h"

const char TAG[] = "MAIN";

#ifdef DEBUG_MODE
#include "HomeDebug.h"
#endif

// ============= CONFIGURATION GLOBALE =============
extern const int bufferSize = 256;
char payload_buffer[bufferSize];

// ============= OBJETS GLOBAUX =============
std::unique_ptr<DeviceController> deviceCtrl;

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
            ESP_LOGE(TAG, "ERREUR: Controller non initialisé dans callback");
            return;
        }

        ESP_LOGD(TAG, "\n------- MQTT Message Reçu -----");

        // Copier le payload dans un buffer sécurisé
        int msgLength = 512;
        char message[msgLength];
        if (length > msgLength)
        {
            // TODO - Publier un message.
            ESP_LOGE(TAG, "Message reçu dépasse la longueur du buffer.");
            ESP_LOGE(TAG, "Topic: %s\nMessage: %s\n", topic, message);
        }

        size_t copyLength = (length >= sizeof(message)) ? sizeof(message) - 1 : length;
        memcpy(message, payload, copyLength);
        message[copyLength] = '\0';

        ESP_LOGD(TAG, "Topic: %s\nMessage: %s\n", topic, message);
        ESP_LOGD(TAG, "-----------------------------");

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
        ESP_LOGD(TAG, "→ Mise à jour configuration");

        NetworkManager *network = instance->getNetworkManager();
        ConfigManager *config = instance->getConfigManager();
        // Sensor *sensor = instance->getSensor();

        if (!network || !config)
        {
            ESP_LOGD(TAG, "✗ Composants non disponibles");
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
            ESP_LOGD(TAG,"✓ Configuration mise à jour:");
            ESP_LOGD(TAG,payload_buffer);

            network->publish(TopicType::CFG, payload_buffer);
        }
        */
    }

    static void handleRestart()
    {
        ESP_LOGD(TAG, "→ Commande de redémarrage reçue");
        delay(500);
        ESP.restart();
    }

    static void handleMacRequest()
    {
        ESP_LOGD(TAG, "→ Demande MAC/IP");

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
            ESP_LOGD(TAG, "✓ MAC/IP envoyé");
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
            ESP_LOGE(TAG,"ERREUR: Allocation Sensor échouée");
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
        ESP_LOGD(TAG, "EXCEPTION CRITIQUE: %s\n", e.what());
        delay(1000);
        ESP.restart();
    }
    catch (...)
    {
        ESP_LOGE(TAG, "ERREUR CRITIQUE INCONNUE");
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
        ESP_LOGD(TAG, "\n========================================");
        ESP_LOGD(TAG, "Initialisation HomeDebug");
        ESP_LOGD(TAG, "========================================");

        homeDebug = std::make_unique<HomeDebug>(
            deviceCtrl->getNetworkManager(),
            deviceCtrl.get());

        // controller->jsonPrintConfig(payload_buffer, bufferSize);
        // ESP_LOGD(TAG,"Configuration: %s\n", payload_buffer);
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