#include "TemperatureReader.h"
#include "esp_log.h"

const char TAG[] = "TEMP_SENSOR";

extern const int bufferSize;
extern char payload_buffer[];

TemperatureReader::TemperatureReader(NetworkManager *network, int deviceId, SensorData *sd)
    : Sensor(network, deviceId, sd)
{
    ESP_LOGD(TAG, "Initialisation TemperatureReader");
    this->deviceId = deviceId;
    this->config.id = sd->id_sensor;

    // Recherche des attributs
    for (size_t i = 0; i < sd->num_attributes; i++)
    {
        AttributeData *ad = &sd->attributes[i];

        if (strcmp(ad->key, this->config.pinNameDHT) == 0)
        {
            this->config.pinDHT = ad->value;
        }
    }

    ESP_LOGD(TAG, "Temperature config: ID:%d, pin:%d", this->config.id, this->config.pinDHT);

    dht.setup(DHTPIN, DHTesp::DHT22); // Connect DHT
}

TemperatureReader::~TemperatureReader()
{
}

void TemperatureReader::executeJob()
{
    ESP_LOGD(TAG, "------- TemperatureReader executeJob() -------");

    delay(dht.getMinimumSamplingPeriod());
    float humidity = dht.getHumidity();
    float temperature = dht.getTemperature();

    ESP_LOGD(TAG, "DHT Status:%s", dht.getStatusString());
    ESP_LOGD(TAG, "T:%.1f, H:%.1f", temperature, humidity);

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
             "{\"DEV\":{\"ID\":%d},\"SS\":{\"ID\":%d,\"V\":%d,\"SN\":%d,\"TMP\":{\"T\":%.1f,\"H\":%.1f}}}",
             this->config.id, this->config.id,
             this->config.version, this->config.sequenceNumber,
             temperature * 10, humidity * 10);

    network->publish(TopicType::DATA, payload_buffer);
    ESP_LOGD(TAG, "Data:%s", payload_buffer);

    this->config.sequenceNumber++;
    /*
     */

    ESP_LOGD(TAG, "------- FIN -------");
}