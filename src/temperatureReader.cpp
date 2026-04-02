#include "TemperatureReader.h"
#include "esp_log.h"

const char TAG[] = "TEMP_SENSOR";

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

bool TemperatureReader::updateConfig(char *json)
{
    bool changed = false;
    ESP_LOGD(TAG, "Message:%s", json);

    // 1. Extraction de l'ID spécifique
    const char *pos = ConfigManager::jsonFindValue(json, "ID");
    ESP_LOGD(TAG, "pos:%s", pos);

    if (pos)
    {
        int id = ConfigManager::jsonExtractInt(pos);
        ESP_LOGD(TAG, "ID:%d", id);

        if (this->sensorId != id)
        {
            this->sensorId = id;
            changed = true;
        }
    }

    // 2. Extraction des attributs (ex: pins)
    const char *attrPtr = ConfigManager::findArrayStart(json, "ATTS");
    ESP_LOGD(TAG, "attrPtr:%s", attrPtr);
    if (attrPtr != nullptr)
    {
        char attrBuf[128];
        while (attrPtr && (attrPtr = ConfigManager::getNextObjectInArray(attrPtr, attrBuf, sizeof(attrBuf))))
        {
            char key[8];
            int value = 0;
            ConfigManager::jsonExtractString(ConfigManager::jsonFindValue(attrBuf, "KEY"), key, sizeof(key));
            ESP_LOGD(TAG, "KEY:%s", key);
            if (attrBuf)
            {
                value = ConfigManager::jsonExtractInt(ConfigManager::jsonFindValue(attrBuf, "VAL"));
                ESP_LOGD(TAG, "value:%d", value);
            }
        }
    }

    return changed;
}

void TemperatureReader::executeJob()
{
    Serial.println("\n------- TemperatureReader executeJob()");

    delay(dht.getMinimumSamplingPeriod());
    //   float humidity = dht.getHumidity();
    //   float temperature = dht.getTemperature();
    Serial.println(dht.getStatusString());

    // Reconnecter si nécessaire
    if (!network->isMQTTConnected())
    {
        network->connectWiFi();
        network->connectMQTT();
        network->subscribeToTopics();
    }

    /*
    //{"DEV":{"ID":25,"V":2,"SN":0},"SS":{"ID":42,"TMP":{"T":225,"H":273}}}
    // DEV: Device ----
    // SS: Sensor data -- ID: sensorId -- V: version -- SN: sequenceNumber -- TMP: TemperatureData -- T:Temperature h:humidity
    snprintf(payload_buffer, bufferSize,
    "{\"DEV\":{\"ID\":%d,\"V\":%d,\"SN\":%d},\"SS\":{\"ID\":%d,\"TMP\":{\"T\":%.1f,\"H\":%.1f}}}",
    config->getDeviceId(), version, sequenceNumber,
    config->getSensorId(), temperature * 10, humidity * 10);

    network->publish(TopicType::DATA, payload_buffer);
    */

    sequenceNumber++;
    Serial.println("\n------- Fin executeJob()-------------------");
}