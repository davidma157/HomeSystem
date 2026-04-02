#ifndef TEMPERATURE_READER_H
#define TEMPERATURE_READER_H

#include "DHTesp.h"
#include "Sensor.h"
// #include <vector>

#ifdef ESP32
// #pragma message("THIS EXAMPLE IS FOR ESP8266 ONLY!")
// #error Select ESP8266 board.
#endif

// Configuration du DHT22
#define DHTPIN D2     // Pin GPIO (ajustez selon votre branchement)
#define DHTTYPE DHT22 // Type de capteur DHT22

class TemperatureReader : public Sensor
{
public:
    TemperatureReader();
    ~TemperatureReader();

    // const char* getTopicDomain() const override {return topicDomain;};
    // Implémentation des méthodes virtuelles
    void begin() override;
    bool updateConfig(char *config) override; // Pas de configuration particulière.
    void executeJob() override;

    const char *getTopicDomain() const override { return TemperatureReader::topicDomain; };
    const char *getDescription() const override { return TemperatureReader::description; };

#ifdef DEBUG_MODE
    void debugMode() {};
#endif
private:
    // std::vector<Sensor *> attributs;
    uint8_t version = 2;
    uint8_t sequenceNumber = 0;

    static constexpr const char *topicDomain = "temp";
    static constexpr const char *description = "TODO";

    // DHT dht(DHTPIN, DHTTYPE);
    //  DHT_Unified dht(DHTPIN, DHTTYPE);
    DHTesp dht;

    // Méthodes spécifiques au détecteur d'eau
};

#endif