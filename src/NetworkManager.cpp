#include "NetworkManager.h"
#include "esp_log.h"

const char TAG[] = "NETWORK_MGR";

// ─────────────────────────────────────────────────────────────────────────────
//  Constructeur / Destructeur
// ─────────────────────────────────────────────────────────────────────────────

NetworkManager::NetworkManager(const char *ssid, const char *password,
                               const char *server, int port,
                               const char *user, const char *pwd)
    : wifi_ssid(ssid), wifi_password(password),
      wifi_timeout((uint32_t)10000),
      mqtt_server(server), mqtt_port(port),
      mqtt_user(user), mqtt_password(pwd),
      mqtt_timeout((uint32_t)15000),
      client(espClient), messageCallback(nullptr),
      currentPowerLevel(WIFI_POWER_MINUS_1dBm),
      optimalPowerLevel(WIFI_POWER_MINUS_1dBm),
      optimalPowerFound(false)
{
}

NetworkManager::~NetworkManager()
{
    if (client.connected())
    {
        client.disconnect();
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        WiFi.disconnect(true);
        delay(100);
    }

    ESP_LOGD(TAG, "%s", "NetworkManager détruit proprement");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Configuration
// ─────────────────────────────────────────────────────────────────────────────

void NetworkManager::setTimeouts(uint32_t wifiTimeout, uint32_t mqttTimeout)
{
    wifi_timeout = wifiTimeout;
    mqtt_timeout = mqttTimeout;
}

[[deprecated]]

// TODO À revoir
void NetworkManager::setTopicIdentifiant(int idDevice)
{
    this->idDevice = idDevice;
    snprintf(identifiant, sizeof(identifiant), "ID-%d", idDevice);
}

void NetworkManager::setMessageCallback(void (*callback)(char *, byte *, unsigned int))
{
    messageCallback = callback;
    client.setCallback(messageCallback);
}

// ─────────────────────────────────────────────────────────────────────────────
//  WiFi – helpers internes
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Tente une connexion WiFi à la puissance déjà configurée (currentPowerLevel).
 * Respecte wifi_timeout. Retourne true si la connexion est établie.
 */
bool NetworkManager::tryConnectAtCurrentPower()
{
    WiFi.begin(wifi_ssid, wifi_password);

    uint32_t startTime = millis();
    int dotCount = 0;

    while (WiFi.status() != WL_CONNECTED && millis() - startTime < wifi_timeout)
    {
        delay(500);
        Serial.print(".");
        if (++dotCount > 10)
        {
            Serial.println();
            dotCount = 0;
        }
    }
    Serial.println();

    return WiFi.status() == WL_CONNECTED;
}

/**
 * Cherche la puissance minimale permettant d'obtenir un signal de qualité
 * suffisante (RSSI >= RSSI_GOOD).
 *
 * Algorithme :
 *   - Partir de la puissance actuelle (connexion déjà établie).
 *   - Descendre cran par cran ; à chaque cran, laisser le signal se stabiliser
 *     puis mesurer le RSSI.
 *   - S'arrêter dès que le RSSI passe sous RSSI_GOOD et revenir au cran précédent.
 *   - Mémoriser le résultat dans optimalPowerLevel.
 */
void NetworkManager::calibrateOptimalPower()
{
    ESP_LOGD(TAG, "%s", "Recherche de la puissance minimale optimale...");

    wifi_power_t bestLevel = currentPowerLevel;

    while (true)
    {
        wifi_power_t candidate = getPreviousPowerLevel(currentPowerLevel);

        // On est déjà au minimum absolu
        if (candidate == currentPowerLevel)
            break;

        setWiFiPower(candidate);
        delay(1500); // laisser le signal se stabiliser

        int rssi = getRSSI();
        ESP_LOGD(TAG, "[Calibration] %s → RSSI %d dBm",
                 getPowerLevelName(candidate), rssi);

        if (rssi >= RSSI_GOOD)
        {
            // Ce niveau convient, on peut encore descendre
            bestLevel = candidate;
        }
        else
        {
            // Trop faible : revenir au cran précédent et s'arrêter
            ESP_LOGD(TAG, "[Calibration] RSSI insuffisant, niveau retenu : %s",
                     getPowerLevelName(bestLevel));
            setWiFiPower(bestLevel);
            break;
        }
    }

    optimalPowerLevel = currentPowerLevel;
    optimalPowerFound = true;

    ESP_LOGD(TAG, "Puissance optimale : %s\n",
             getPowerLevelName(optimalPowerLevel));
}

/**
 * Parcourt les niveaux de puissance du maximum vers le minimum.
 * Pour chaque niveau, tente une connexion puis vérifie la qualité du signal.
 * Mémorise le premier niveau donnant un RSSI >= RSSI_POOR.
 * Retourne true si une connexion acceptable est trouvée.
 */
bool NetworkManager::connectWithPowerSweep()
{
    ESP_LOGD(TAG, "[Sweep] Balayage des puissances du max vers le min...");

    // Construire la liste de puissances décroissante
    const wifi_power_t levels[] = {
        WIFI_POWER_19_5dBm, WIFI_POWER_19dBm, WIFI_POWER_18_5dBm,
        WIFI_POWER_17dBm, WIFI_POWER_15dBm, WIFI_POWER_13dBm,
        WIFI_POWER_11dBm, WIFI_POWER_8_5dBm, WIFI_POWER_7dBm,
        WIFI_POWER_5dBm, WIFI_POWER_2dBm, WIFI_POWER_MINUS_1dBm};
    const int nbLevels = sizeof(levels) / sizeof(levels[0]);

    for (int i = 0; i < nbLevels; i++)
    {
        // S'assurer d'être déconnecté avant chaque tentative
        if (WiFi.status() == WL_CONNECTED)
        {
            WiFi.disconnect(true);
            delay(300);
        }

        setWiFiPower(levels[i]);
        ESP_LOGD(TAG, "[Sweep] Tentative à %s\n", getPowerLevelName(levels[i]));

        if (!tryConnectAtCurrentPower())
        {
            ESP_LOGE(TAG, "[Sweep] Connexion échouée à ce niveau, on continue...");
            continue;
        }

        // Connexion réussie – vérifier la qualité
        delay(500);
        int rssi = getRSSI();
        ESP_LOGD(TAG, "[Sweep] Connecté ! RSSI = %d dBm\n", rssi);

        if (rssi >= RSSI_POOR)
        {
            // Signal acceptable ; affiner vers le bas pour trouver l'optimum
            ESP_LOGD(TAG, "[Sweep] Signal acceptable, lancement de la calibration.");
            calibrateOptimalPower();
            return true;
        }
        else
        {
            // Signal trop faible même à cette puissance : rester dessus ne
            // sert à rien, mais aucun niveau plus haut n'était disponible.
            // On conserve quand même la connexion si c'est le mieux possible.
            if (i == 0)
            {
                ESP_LOGD(TAG, "[Sweep] Puissance maximale atteinte, signal faible mais connexion conservée.");
                optimalPowerLevel = levels[0];
                optimalPowerFound = true;
                return true;
            }
            // Sinon on se déconnecte et on essaie le niveau suivant (plus haut)
            // – normalement impossible car on part du max, donc cette branche
            // ne sera jamais atteinte dans la boucle décroissante.
        }
    }

    ESP_LOGD(TAG, "[Sweep] Aucune connexion trouvée après balayage complet.");
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  WiFi – API publique
// ─────────────────────────────────────────────────────────────────────────────

void NetworkManager::connectWiFi()
{
    ESP_LOGD(TAG, "Connexion à WiFi: %s", wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);

    // ── 1. Utiliser la puissance optimale mémorisée si disponible ────────────
    if (optimalPowerFound)
    {
        ESP_LOGD(TAG, "[connectWiFi] Réutilisation de la puissance optimale mémorisée : %s\n",
                 getPowerLevelName(optimalPowerLevel));
        setWiFiPower(optimalPowerLevel);

        if (tryConnectAtCurrentPower())
        {
            int rssi = getRSSI();
            ESP_LOGD(TAG, "[connectWiFi] Connecté avec la puissance mémorisée. RSSI = %d dBm", rssi);

            // La puissance mémorisée reste valide si le signal est encore bon.
            if (rssi >= RSSI_GOOD)
            {
                ESP_LOGD(TAG, "[connectWiFi] Qualité suffisante, pas de recalibration.");
                afficherQualiteSignal();
                return;
            }

            // Signal dégradé depuis la dernière fois → recalibrer
            ESP_LOGD(TAG, "[connectWiFi] Signal dégradé, recalibration en cours...");
            calibrateOptimalPower();
            afficherQualiteSignal();
            return;
        }

        // La puissance mémorisée ne permet plus de se connecter → sweep complet
        ESP_LOGD(TAG, "[connectWiFi] Connexion échouée avec la puissance mémorisée, balayage complet.");
    }

    // ── 2. Première connexion (ou puissance mémorisée devenue inopérante) ────
    //    Partir d'une puissance basse et essayer d'abord normalement.
    setWiFiPower(WIFI_POWER_2dBm);

    if (tryConnectAtCurrentPower())
    {
        ESP_LOGD(TAG, "Connecté à puissance basse. RSSI = %d dBm", getRSSI());

        WiFiQuality q = getWiFiQuality();
        if (q.rssi >= RSSI_GOOD)
        {
            // Signal déjà bon à faible puissance : chercher le minimum optimal.
            ESP_LOGD(TAG, "Bon signal, calibration de la puissance optimale.");
            calibrateOptimalPower();
        }
        else
        {
            // Signal insuffisant même à 2 dBm → chercher une puissance plus haute.
            ESP_LOGD(TAG, "Signal insuffisant à faible puissance, calibration vers le haut.");
            // Monter jusqu'à obtenir RSSI_GOOD
            while (q.rssi < RSSI_GOOD)
            {
                wifi_power_t next = getNextPowerLevel(currentPowerLevel);
                if (next == currentPowerLevel)
                    break; // déjà au maximum
                setWiFiPower(next);
                delay(1500);
                q = getWiFiQuality();
                ESP_LOGD(TAG, "[connectWiFi] %s → RSSI %d dBm\n",
                         getPowerLevelName(currentPowerLevel), q.rssi);
            }
            optimalPowerLevel = currentPowerLevel;
            optimalPowerFound = true;
            ESP_LOGD(TAG, "[connectWiFi] Puissance optimale retenue : %s\n",
                     getPowerLevelName(optimalPowerLevel));
        }

        afficherQualiteSignal();
        return;
    }

    // ── 3. Connexion initiale échouée → balayage du max vers le min ──────────
    ESP_LOGE(TAG, "[connectWiFi] Connexion initiale échouée, balayage de puissance.");
    if (!connectWithPowerSweep())
    {
        ESP_LOGE(TAG, "[connectWiFi] ERREUR : Impossible de se connecter au WiFi.");
        return;
    }

    afficherQualiteSignal();
}

void NetworkManager::disconnectWiFi()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        if (client.connected())
            disconnectMqtt();

        WiFi.disconnect(true);
        delay(100);

        uint32_t startTime = millis();
        while (WiFi.status() == WL_CONNECTED && millis() - startTime < 2000)
            delay(50);

        ESP_LOGE(TAG, "WiFi déconnecté");
    }
}

bool NetworkManager::isWiFiConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

const char *NetworkManager::getIPAddress()
{
    strcpy(ipAddress, WiFi.localIP().toString().c_str());
    Serial.println(ipAddress);
    return ipAddress;
}

const char *NetworkManager::getMacAddress()
{
    strcpy(macAddress, WiFi.macAddress().c_str());
    return macAddress;
}

// ─────────────────────────────────────────────────────────────────────────────
//  MQTT
// ─────────────────────────────────────────────────────────────────────────────

void NetworkManager::connectMQTT()
{
    client.setServer(mqtt_server, mqtt_port);
    ESP_LOGD(TAG, "Tentative connexion MQTT:Identifiant:%s - %s:%d", identifiant, mqtt_server, mqtt_port);

    uint32_t startTime = millis();

    while (!client.connected() && millis() - startTime < mqtt_timeout)
    {
        bool clean_session = false;

        if (client.connect(identifiant, mqtt_user, mqtt_password, 0, 0, 0, 0, clean_session))
        {
            ESP_LOGI(TAG, "%s", "MQTT connecté!");
        }
        else
        {
            ESP_LOGE(TAG, "Erreur MQTT, code: %d\n", client.state());
            delay(2000);
        }
    }
}

bool NetworkManager::isMQTTConnected()
{
    bool connected = client.connected();
    ESP_LOGD(TAG, "Statut:%d", connected);

    return connected;
}

void NetworkManager::disconnectMqtt()
{
    if (client.connected())
    {
        client.disconnect();
        delay(500);
        ESP_LOGE(TAG, "MQTT déconnecté");
    }
}

bool NetworkManager::mqttLoop()
{
    return client.loop();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Publishing / Subscription
// ─────────────────────────────────────────────────────────────────────────────

bool NetworkManager::publish(TopicType topicType, const char *payload)
{
    char topic[64];
    snprintf(topic, sizeof(topic), TOPIC_TEMPLATE_PUBLISH,
             this->idDevice, topicTypeToString(topicType));

    if (!client.connected())
    {
        ESP_LOGE(TAG, "MQTT non connecté, impossible de publier");
        return false;
    }

    if (client.publish(topic, payload))
    {
        ESP_LOGD(TAG, "[%s]: %s", topic, payload);
        return true;
    }
    else
    {
        ESP_LOGE(TAG, "Échec publication [%s]", topic);
        return false;
    }
}

bool NetworkManager::subscribeToTopics()
{
    char topic_receiver[64];
    snprintf(topic_receiver, sizeof(topic_receiver), TOPIC_TEMPLATE_SUBSCRIBE, // "home/to-sensor/ID-%d/#",
             this->idDevice);

    if (client.subscribe(topic_receiver, 1))
    {
        ESP_LOGD(TAG, "%s", topic_receiver);
        return true;
    }
    else
    {
        ESP_LOGE(TAG, "Échec abonnement à : %s\n", topic_receiver);
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Gestion de la puissance – implémentations
// ─────────────────────────────────────────────────────────────────────────────

void NetworkManager::setLowPowerMode()
{
    setWiFiPower(WIFI_POWER_2dBm);
    ESP_LOGD(TAG, "WiFi: Mode basse consommation");
}

void NetworkManager::setHighPowerMode()
{
    setWiFiPower(WIFI_POWER_19_5dBm);
    ESP_LOGI(TAG, "WiFi: Mode haute consommation");
}

void NetworkManager::setWiFiPower(wifi_power_t power)
{
    currentPowerLevel = power;
    WiFi.setTxPower(power);
    ESP_LOGI(TAG, "Puissance WiFi réglée : %s", getPowerLevelName(power));
}

wifi_power_t NetworkManager::getNextPowerLevel(wifi_power_t current)
{
    switch (current)
    {
    case WIFI_POWER_MINUS_1dBm:
        return WIFI_POWER_2dBm;
    case WIFI_POWER_2dBm:
        return WIFI_POWER_5dBm;
    case WIFI_POWER_5dBm:
        return WIFI_POWER_7dBm;
    case WIFI_POWER_7dBm:
        return WIFI_POWER_8_5dBm;
    case WIFI_POWER_8_5dBm:
        return WIFI_POWER_11dBm;
    case WIFI_POWER_11dBm:
        return WIFI_POWER_13dBm;
    case WIFI_POWER_13dBm:
        return WIFI_POWER_15dBm;
    case WIFI_POWER_15dBm:
        return WIFI_POWER_17dBm;
    case WIFI_POWER_17dBm:
        return WIFI_POWER_18_5dBm;
    case WIFI_POWER_18_5dBm:
        return WIFI_POWER_19dBm;
    case WIFI_POWER_19dBm:
        return WIFI_POWER_19_5dBm;
    case WIFI_POWER_19_5dBm:
        return WIFI_POWER_19_5dBm; // déjà au max
    default:
        return WIFI_POWER_MINUS_1dBm;
    }
}

wifi_power_t NetworkManager::getPreviousPowerLevel(wifi_power_t current)
{
    switch (current)
    {
    case WIFI_POWER_19_5dBm:
        return WIFI_POWER_19dBm;
    case WIFI_POWER_19dBm:
        return WIFI_POWER_18_5dBm;
    case WIFI_POWER_18_5dBm:
        return WIFI_POWER_17dBm;
    case WIFI_POWER_17dBm:
        return WIFI_POWER_15dBm;
    case WIFI_POWER_15dBm:
        return WIFI_POWER_13dBm;
    case WIFI_POWER_13dBm:
        return WIFI_POWER_11dBm;
    case WIFI_POWER_11dBm:
        return WIFI_POWER_8_5dBm;
    case WIFI_POWER_8_5dBm:
        return WIFI_POWER_7dBm;
    case WIFI_POWER_7dBm:
        return WIFI_POWER_5dBm;
    case WIFI_POWER_5dBm:
        return WIFI_POWER_2dBm;
    case WIFI_POWER_2dBm:
        return WIFI_POWER_MINUS_1dBm;
    case WIFI_POWER_MINUS_1dBm:
        return WIFI_POWER_MINUS_1dBm; // déjà au min
    default:
        return WIFI_POWER_MINUS_1dBm;
    }
}

const char *NetworkManager::getPowerLevelName(wifi_power_t power)
{
    switch (power)
    {
    case WIFI_POWER_MINUS_1dBm:
        return "-1 dBm";
    case WIFI_POWER_2dBm:
        return "2 dBm";
    case WIFI_POWER_5dBm:
        return "5 dBm";
    case WIFI_POWER_7dBm:
        return "7 dBm";
    case WIFI_POWER_8_5dBm:
        return "8.5 dBm";
    case WIFI_POWER_11dBm:
        return "11 dBm";
    case WIFI_POWER_13dBm:
        return "13 dBm";
    case WIFI_POWER_15dBm:
        return "15 dBm";
    case WIFI_POWER_17dBm:
        return "17 dBm";
    case WIFI_POWER_18_5dBm:
        return "18.5 dBm";
    case WIFI_POWER_19dBm:
        return "19 dBm";
    case WIFI_POWER_19_5dBm:
        return "19.5 dBm";
    default:
        return "Inconnu";
    }
}

wifi_power_t NetworkManager::getCurrentPowerLevel() const
{
    return currentPowerLevel;
}

void NetworkManager::incrementPowerLevel()
{
    wifi_power_t newLevel = getNextPowerLevel(currentPowerLevel);
    if (newLevel != currentPowerLevel)
    {
        setWiFiPower(newLevel);
        ESP_LOGD(TAG, "%s", "Puissance WiFi augmentée");
    }
    else
    {
        ESP_LOGD(TAG, "%s", "Puissance WiFi déjà au maximum");
    }
}

void NetworkManager::decrementPowerLevel()
{
    wifi_power_t newLevel = getPreviousPowerLevel(currentPowerLevel);
    if (newLevel != currentPowerLevel)
    {
        setWiFiPower(newLevel);
        ESP_LOGD(TAG, "%s", "Puissance WiFi diminuée");
    }
    else
    {
        ESP_LOGD(TAG, "%s", "Puissance WiFi déjà au minimum");
    }
}

int NetworkManager::getRSSI() const
{
    return WiFi.RSSI();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Qualité du signal
// ─────────────────────────────────────────────────────────────────────────────

/*********************************************************************************************
 *  RSSI(dBm)      Qualité        Description
 *  ─────────────────────────────────────────────────────────
 *   -30 à -50     Excellent      Signal très fort, connexion parfaite
 *   -50 à -60     Très bon       Signal fort, performances optimales
 *   -60 à -70     Bon            Signal correct, connexion stable
 *   -70 à -80     Moyen          Signal faible, peut avoir des pertes
 *   -80 à -90     Faible         Signal très faible, connexion instable
 *   -90 et moins  Très faible    Connexion difficile ou impossible
 ********************************************************************************************/
WiFiQuality NetworkManager::getWiFiQuality() const
{
    WiFiQuality result;
    result.rssi = 0;

    // Attendre un RSSI valide (jusqu'à 10 tentatives)
    for (int i = 0; i < 10 && result.rssi == 0; i++)
    {
        result.rssi = WiFi.RSSI();
        if (result.rssi == 0)
        {
            ESP_LOGD(TAG, "%s", "--- Attente d'un RSSI valide ---");
            delay(1000);
        }
    }

    // Conversion RSSI en pourcentage
    if (result.rssi == 0 || result.rssi <= -100)
        result.signalPercent = 0;
    else if (result.rssi >= -50)
        result.signalPercent = 100;
    else
        result.signalPercent = (uint8_t)(2 * (result.rssi + 100));

    // Évaluation qualitative
    if (result.rssi == 0)
    {
        result.quality = "indéterminé";
        result.description = "RSSI non reçu";
    }
    else if (result.rssi >= -50)
    {
        result.quality = "Excellent";
        result.description = "Signal très fort";
    }
    else if (result.rssi >= -60)
    {
        result.quality = "Très bon";
        result.description = "Connexion optimale";
    }
    else if (result.rssi >= -67)
    {
        result.quality = "Bon";
        result.description = "VoIP et streaming OK";
    }
    else if (result.rssi >= -70)
    {
        result.quality = "Correct";
        result.description = "Navigation stable";
    }
    else if (result.rssi >= -80)
    {
        result.quality = "Faible";
        result.description = "Connexion lente";
    }
    else if (result.rssi >= -90)
    {
        result.quality = "Très faible";
        result.description = "Connexion instable";
    }
    else
    {
        result.quality = "Critique";
        result.description = "Déconnexions fréquentes";
    }

    return result;
}

void NetworkManager::afficherQualiteSignal()
{
    WiFiQuality quality = getWiFiQuality();

    ESP_LOGD(TAG, "%s", "═══════════════════════════════════");
    ESP_LOGD(TAG, "RSSI: %d dBm", quality.rssi);
    ESP_LOGD(TAG, "Qualité: %s (%d%%)", quality.quality, quality.signalPercent);
    ESP_LOGD(TAG, "Description: %s", quality.description);
    ESP_LOGD(TAG, "Puissance TX: %s", getPowerLevelName(getCurrentPowerLevel()));
    ESP_LOGD(TAG, "%s", "═══════════════════════════════════\n");
}