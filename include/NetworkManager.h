#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#define TOPIC_TEMPLATE_PUBLISH "home/sensor/ID-%d%s"
#define TOPIC_TEMPLATE_SUBSCRIBE "home/to-sensor/ID-%d/#"

// Seuils RSSI pour évaluer la qualité du signal
#define RSSI_EXCELLENT -50 // Signal excellent, pas besoin d'augmenter la puissance
#define RSSI_GOOD -67      // Signal bon, acceptable pour un usage normal
#define RSSI_POOR -80      // Signal faible, qualité insuffisante

struct TopicInfo
{
    int id_device = 0;
    char domain[20];
};

enum class TopicType
{
    STATUS,
    CFG,
    DATA,
    ALERT,
    MAC_IP,
    RECEIVER,
};

struct WiFiQuality
{
    int rssi;
    const char *quality;
    const char *description;
    uint8_t signalPercent;
};

class NetworkManager
{
private:
    // WiFi
    const char *wifi_ssid;
    const char *wifi_password;
    uint32_t wifi_timeout;
    char ipAddress[15];
    char macAddress[20];

    // MQTT
    const char *mqtt_server;
    int mqtt_port;
    const char *mqtt_user;
    const char *mqtt_password;
    uint32_t mqtt_timeout;

    WiFiClient espClient;
    PubSubClient client;

    // Topics
    int idDevice = 0;
    char identifiant[15] = "SENSOR_0";

    // Callback
    void (*messageCallback)(char *, byte *, unsigned int);

    // Gestion de la puissance
    wifi_power_t currentPowerLevel;

    // Puissance optimale mémorisée entre les connexions.
    // Vaut WIFI_POWER_MINUS_1dBm tant qu'aucune valeur n'a encore été déterminée.
    wifi_power_t optimalPowerLevel;
    bool optimalPowerFound; // true dès qu'une valeur optimale a été établie

    // ── Helpers topic ────────────────────────────────────────────────────────
    const char *topicTypeToString(TopicType c)
    {
        switch (c)
        {
        case TopicType::ALERT:
            return "/alert";
        case TopicType::CFG:
            return "/cfg-updated";
        case TopicType::DATA:
            return "/data";
        case TopicType::MAC_IP:
            return "/mac-ip";
        case TopicType::STATUS:
            return "/status";
        case TopicType::RECEIVER:
            return "";
        default:
            return "error";
        }
    }

    // ── Gestion de la puissance (privé) ──────────────────────────────────────
    void setWiFiPower(wifi_power_t power);
    wifi_power_t getNextPowerLevel(wifi_power_t current);     // monte d'un cran
    wifi_power_t getPreviousPowerLevel(wifi_power_t current); // descend d'un cran
    wifi_power_t getCurrentPowerLevel() const;
    const char *getPowerLevelName(wifi_power_t power);
    void incrementPowerLevel();
    void decrementPowerLevel();
    int getRSSI() const;
    WiFiQuality getWiFiQuality() const;
    void afficherQualiteSignal();
    void setLowPowerMode();
    void setHighPowerMode();

    // ── Logique de connexion interne ─────────────────────────────────────────

    // Tente une connexion WiFi à la puissance courante ; retourne true si connecté
    bool tryConnectAtCurrentPower();

    // Après une connexion réussie, cherche la puissance minimale donnant un bon signal.
    // Met à jour optimalPowerLevel.
    void calibrateOptimalPower();

    // Parcourt les puissances du max vers le min jusqu'à connexion + signal correct.
    // Met à jour optimalPowerLevel.
    bool connectWithPowerSweep();

public:
    NetworkManager(const char *ssid, const char *password,
                   const char *server, int port,
                   const char *user = "", const char *pwd = "");

    ~NetworkManager();

    // ── Configuration ────────────────────────────────────────────────────────
    void setTimeouts(uint32_t wifiTimeout, uint32_t mqttTimeout);
    void setMessageCallback(void (*callback)(char *, byte *, unsigned int));

    // ── WiFi ─────────────────────────────────────────────────────────────────
    void connectWiFi();
    void disconnectWiFi();
    bool isWiFiConnected();
    const char *getIPAddress();
    const char *getMacAddress();

    // ── MQTT ─────────────────────────────────────────────────────────────────
    void connectMQTT();
    bool isMQTTConnected();
    void disconnectMqtt();
    bool mqttLoop();

    // ── Publishing ───────────────────────────────────────────────────────────
    void setTopicIdentifiant(int idDevice);
    bool publish(TopicType topicDomain, const char *payload);

    // ── Subscription ─────────────────────────────────────────────────────────
    bool subscribeToTopics();
};

#endif