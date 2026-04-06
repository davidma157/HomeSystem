#ifndef TEMPERATURE_READER_H
#define TEMPERATURE_READER_H

#include "ConfigManager.h"
#include "DHTesp.h"
#include "Sensor.h"

#ifdef ESP32
// #pragma message("THIS EXAMPLE IS FOR ESP8266 ONLY!")
// #error Select ESP8266 board.
#endif

// Configuration du DHT22
#define DHTPIN D2     // Pin GPIO (ajustez selon votre branchement)
#define DHTTYPE DHT22 // Type de capteur DHT22

struct Config
{
    int id;
    const char *pinNameDHT = "pin"; // TODO renommer pour  "DHTPIN";
    int pinDHT;
    uint8_t version = 2;
    uint8_t sequenceNumber = 0;
};

class TemperatureReader : public Sensor
{
public:
    TemperatureReader(NetworkManager *network, int deviceId, SensorData *sd);
    ~TemperatureReader();

    // const char* getTopicDomain() const override {return topicDomain;};
    // Implémentation des méthodes virtuelles
    void executeJob() override;

    const char *getTopicDomain() const override { return TemperatureReader::topicDomain; };
    const char *getDescription() const override { return TemperatureReader::description; };

#ifdef DEBUG_MODE
    void debugMode() {};
#endif
private:
    Config config;

    static constexpr const char *topicDomain = "temp";
    static constexpr const char *description = "TODO";

    DHTesp dht;
};

#endif