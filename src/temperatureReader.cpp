#include "TemperatureReader.h"

extern const int bufferSize;
extern char payload_buffer[];

TemperatureReader::TemperatureReader()
{
}

TemperatureReader::~TemperatureReader()
{
}

void TemperatureReader::begin()
{
    dht.setup(DHTPIN, DHTesp::DHT22); // Connect DHT 
    Serial.println("TemperatureReader initialisé");
}

bool TemperatureReader::updateConfig(char *configJson)
{
    bool doSave = false;
    const char *pos;

    // Extraire ID sensor
    pos = ConfigManager::jsonFindValue(configJson, "temp_sensor_id");
    if (pos)
    {
        int idSensor = ConfigManager::jsonExtractInt(pos);
        if (idSensor != config->getSensorId())
        {
            config->setSensorId(idSensor);
            doSave = true;
        }
    }

    // Extraire sleep duration
    pos = ConfigManager::jsonFindValue(configJson, "sleep_duration");
    if (pos)
    {
        int sleepPeriod = ConfigManager::jsonExtractInt(pos);
        if (config->getSleepPeriod() != sleepPeriod)
        {
            config->setSleepPeriod(sleepPeriod);
            doSave = true;
        }
    }

    return doSave;
}

void TemperatureReader::executeJob()
{
    Serial.println("\n------- TemperatureReader executeJob()");

    delay(dht.getMinimumSamplingPeriod());
    float humidity = dht.getHumidity();
    float temperature = dht.getTemperature();
    Serial.println(dht.getStatusString());

    // Reconnecter si nécessaire
    if (!network->isMQTTConnected())
    {
        network->connectWiFi();
        network->connectMQTT();
        network->subscribeToTopics();
    }

    //{"DEV":{"ID":25,"V":2,"SN":0},"SS":{"ID":42,"TMP":{"T":225,"H":273}}}

    // DEV: Device ----
    // SS: Sensor data -- ID: sensorId -- V: version -- SN: sequenceNumber -- TMP: TemperatureData -- T:Temperature h:humidity
    snprintf(payload_buffer, bufferSize,
             "{\"DEV\":{\"ID\":%d,\"V\":%d,\"SN\":%d},\"SS\":{\"ID\":%d,\"TMP\":{\"T\":%.1f,\"H\":%.1f}}}",
             config->getDeviceId(), version, sequenceNumber,
             config->getSensorId(), temperature * 10, humidity * 10);

    network->publish(TopicType::DATA, payload_buffer);

    sequenceNumber++;
    Serial.println("\n------- Fin executeJob()-------------------");
}