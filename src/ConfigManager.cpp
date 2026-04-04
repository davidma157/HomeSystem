#include "ConfigManager.h"

const char TAG[] = "CONFIG_MGR";

// Stockage en mémoire RTC
RTC_DATA_ATTR DeviceConfig rtcConfig;
RTC_DATA_ATTR bool rtcConfigValid = false;

Preferences ConfigManager::preferences;

void ConfigManager::load()
{
    int bufferSize = 256;
    char buffer[bufferSize] = "\0";

    if (preferences.begin(NAME_SPACE_CONFIG, true))
    {
        bool initialized = preferences.getBool("initialized", false);

        if (initialized)
        {
            const char *strConfig = (preferences.getString(TAG_CONFIG, "")).c_str();
            strncpy(buffer, strConfig, bufferSize);
            ESP_LOGD(TAG, "%s", buffer);
        }
        else
        {
            ESP_LOGD(TAG, "Configuration inexistante.");
        }
        preferences.end();

        ConfigManager::extractConfig(buffer);
    }
}

void ConfigManager::save(char *json)
{
    preferences.begin(NAME_SPACE_CONFIG, false);
    preferences.clear();

    preferences.putBool("initialized", true);
    preferences.putString(TAG_CONFIG, json);

    preferences.end();
    ESP_LOGD(TAG, "Configuration sauvegardée");
}

void ConfigManager::reset()
{
    preferences.begin(NAME_SPACE_CONFIG, false);
    preferences.clear();
    preferences.end();

    ESP_LOGI(TAG, "Configuration réinitialisée");
}

DeviceConfig *ConfigManager::getConfig()
{
    return &rtcConfig;
}

DeviceConfig *ConfigManager::extractConfig(char *configJson)
{
    bool needsSave = false;

    // Extraire et mettre à jour l'ID device
    // const char *pos = ConfigManager::jsonFindValue(configJson, "ID");
    //  if (pos)
    //   {
    //  int idDevice = ConfigManager::jsonExtractInt(pos);
    int idDevice = ConfigManager::jsonExtractInt(configJson, "ID");
    if (rtcConfig.id_device == 0)
    {
        rtcConfig.id_device = idDevice;
        needsSave = true;
        ESP_LOGI(TAG, "✓ Nouveau ID device: %d\n", idDevice);
    }

    int sleepPeriod = ConfigManager::jsonExtractInt(configJson, "SLEEP");
    if (rtcConfig.sleep_period != sleepPeriod)
    {
        rtcConfig.sleep_period = sleepPeriod;
        needsSave = true;
    }

    int alivePeriod = ConfigManager::jsonExtractInt(configJson, "ALIVE");
    if (rtcConfig.im_alive_period != alivePeriod)
    {
        rtcConfig.im_alive_period = alivePeriod;
        needsSave = true;
    }
    //   }

    const char *arrayPtr = ConfigManager::findArrayStart(configJson, "SS");
    if (arrayPtr != nullptr)
    {
        char sensorJson[256];
        int8_t indxSensor = 0;
        ESP_LOGV(TAG, "%s", arrayPtr);
        while (arrayPtr && (arrayPtr = ConfigManager::getNextObjectInArray(arrayPtr, sensorJson, sizeof(sensorJson))))
        {
            SensorData *sensorData = &(rtcConfig.sensors[indxSensor]);
            rtcConfig.num_sensors = indxSensor + 1;
            indxSensor++;
            ESP_LOGV(TAG, "SensorJson:%s", sensorJson);

            int id = ConfigManager::jsonExtractInt(sensorJson, "ID");
            ESP_LOGV(TAG, "ID:%d", id);
            sensorData->id_sensor = id;

            sensorData->role = ConfigManager::getRole(sensorJson);

            // 2. Extraction des attributs (ex: pins)
            const char *attrPtr = ConfigManager::findArrayStart(sensorJson, "ATTS");
            ESP_LOGV(TAG, "attrPtr:%s", attrPtr);
            if (attrPtr != nullptr)
            {
                char attrBuf[128];
                uint8_t indxAttr = 0;
                while (attrPtr && (attrPtr = ConfigManager::getNextObjectInArray(attrPtr, attrBuf, sizeof(attrBuf))))
                {
                    AttributeData *data = &sensorData->attributes[indxAttr];
                    sensorData->num_attributes = indxAttr + 1;
                    indxAttr++;
                    char key[8];
                    ConfigManager::jsonExtractString(attrBuf, "KEY", key, sizeof(key));

                    strncpy(data->key, key, 8);
                    ESP_LOGV(TAG, "KEY:%s", key);
                    if (attrBuf)
                    {
                        data->value = ConfigManager::jsonExtractInt(attrBuf, "VAL");
                    }
                }
            }
        }
    }
    if (needsSave)
    {
        /* code */
    }
    ConfigManager::printConfig();
    return &rtcConfig;
}

const char *ConfigManager::jsonFindValue(const char *json, const char *key)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);

    ESP_LOGV(TAG, "FindValue: <%s>\n", search);
    const char *pos = strstr(json, search);
    if (!pos)
        return nullptr;

    pos += strlen(search);

    while (*pos == ' ' || *pos == '\t')
        pos++;

    ESP_LOGV(TAG, "FindValue - pos-->%s\n", pos);
    return pos;
}

void ConfigManager::jsonExtractString(const char *src, const char *key, char *dest, int maxLen)
{
    const char *pos = ConfigManager::jsonFindValue(src, key);

    if (*pos == '"')
        pos++;

    int i = 0;
    while (*pos && *pos != '"' && i < maxLen - 1)
    {
        dest[i++] = *pos++;
    }
    dest[i] = '\0';
}

int ConfigManager::jsonExtractInt(const char *json, const char *key)
{
    const char *pos = ConfigManager::jsonFindValue(json, key);

    if (pos)
    {
        while (*pos == ' ' || *pos == '\t')
            pos++;
        ESP_LOGV(TAG, "Int-->%s", pos);
        return atoi(pos);
    }
    return 0;
}

// Trouve le début du tableau "[" après une clé
const char *ConfigManager::findArrayStart(const char *json, const char *key)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *pos = strstr(json, search);
    if (!pos)
        return nullptr;
    return strchr(pos, '[');
}

// Ajoutez cette méthode pour extraire un objet JSON d'un tableau
const char *ConfigManager::getNextObjectInArray(const char *currentPos, char *dest, int maxLen)
{
    const char *start = strchr(currentPos, '{');
    if (!start)
        return nullptr;

    int depth = 0;
    const char *end = start;
    while (*end)
    {
        if (*end == '{')
            depth++;
        if (*end == '}')
            depth--;
        if (depth == 0)
            break;
        end++;
    }

    int len = end - start + 1;
    if (len >= maxLen)
        len = maxLen - 1;
    memcpy(dest, start, len);
    dest[len] = '\0';
    return end + 1;
}

SensorRole ConfigManager::getRole(char *sensorJson)
{
    char role[20];
    ConfigManager::jsonExtractString(sensorJson, "ROLE", role, sizeof(role));
    if (strcmp(role, "TEMPERATURE") == 0)
    {
        return SensorRole::TEMPERATURE;
    }
    return SensorRole::UNDEFINED;
}

void ConfigManager::printConfig()
{
    int bufferSize = 512;
    char buffer[bufferSize];
    char *indxBuffer = buffer;

    indxBuffer += snprintf(indxBuffer, bufferSize, "\n\tDEV(ID:%d, Sensors(%d))", rtcConfig.id_device, rtcConfig.num_sensors);
    bufferSize -= strlen(buffer);

    for (size_t i = 0; i < rtcConfig.num_sensors; i++)
    {
        SensorData *sd = &rtcConfig.sensors[i];

        indxBuffer += snprintf(indxBuffer, bufferSize, "\n\t\tSensor(ID:%d, Role:%d, Attr(%d))", sd->id_sensor, sd->role, sd->num_attributes);
        bufferSize -= strlen(buffer);

        for (size_t j = 0; j < sd->num_attributes; j++)
        {
            AttributeData *ad = &sd->attributes[j];
            indxBuffer += snprintf(indxBuffer, bufferSize, "\n\t\t\tAttr:(Key:%s, Value:%d)", ad->key, ad->value);
            bufferSize -= strlen(buffer);
        }
    }

    ESP_LOGI(TAG, "%s", buffer);
}
