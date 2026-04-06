#include "ConfigManager.h"

const char TAG[] = "CONFIG_MGR";

// Stockage en mémoire RTC
// RTC_DATA_ATTR DeviceConfig rtcConfig;
RTC_DATA_ATTR bool rtcConfigValid = false;

Preferences ConfigManager::preferences;

/*
//TODO ménage
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

        // TODO - Déplacer lors de la réception d'une nouvelle configuration.
        // TODO Pour la durée du développement
        ConfigManager::extractConfig(buffer);
        ConfigManager::save(&rtcConfig);
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
*/

bool ConfigManager::loadConfig(DeviceConfig *config)
{
    preferences.begin("system", true);
    size_t schLen = preferences.getBytesLength("config_bin");

    if (schLen != sizeof(DeviceConfig))
    {
        ESP_LOGE(TAG, "Configuration invalide - N'a pas été chargée.");
        preferences.end();
        return false; // Configuration absente ou version différente
    }

    preferences.getBytes("config_bin", config, sizeof(DeviceConfig));
    preferences.end();
    return true;
}

void ConfigManager::save(DeviceConfig *config)
{

    preferences.begin("system", false);
    // On sauvegarde toute la structure d'un coup comme un bloc de bytes
    preferences.putBytes("config_bin", config, sizeof(DeviceConfig));
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

/*
DeviceConfig *ConfigManager::getConfig()
{
    return &rtcConfig;
}
*/

void ConfigManager::extractConfig(char *configJson, DeviceConfig *config)
{
    config->id_device = ConfigManager::jsonExtractInt(configJson, "ID");
    ConfigManager::jsonExtractString(configJson, "MAC", config->mac, sizeof(config->mac));
    config->sleep_period = ConfigManager::jsonExtractInt(configJson, "SLEEP");
    config->im_alive_period = ConfigManager::jsonExtractInt(configJson, "ALIVE");

    const char *arrayPtr = ConfigManager::findArrayStart(configJson, "SS");
    if (arrayPtr != nullptr)
    {
        char sensorJson[512];
        int8_t indxSensor = 0;
        config->num_sensors = 0;
        ESP_LOGV(TAG, "%s", arrayPtr);
        while (arrayPtr && (arrayPtr = ConfigManager::getNextObjectInArray(arrayPtr, sensorJson, sizeof(sensorJson))))
        {
            SensorData *sensorData = &(config->sensors[indxSensor]);
            config->num_sensors = indxSensor + 1;
            indxSensor++;
            if (indxSensor == MAX_SENSORS)
            {
                ESP_LOGE(TAG, "Nombre de sensors(%d) dépasse la limite(%d)", config->num_sensors, MAX_SENSORS);
                return;
            }

            ESP_LOGV(TAG, "SensorJson:%s", sensorJson);

            int id = ConfigManager::jsonExtractInt(sensorJson, "ID");
            ESP_LOGV(TAG, "ID:%d", id);
            sensorData->id_sensor = id;

            sensorData->role = ConfigManager::getRole(sensorJson);
            sensorData->num_attributes = 0;

            // 2. Extraction des attributs (ex: pins)
            const char *attrPtr = ConfigManager::findArrayStart(sensorJson, "ATTS");
            ESP_LOGV(TAG, "attrPtr:%s", attrPtr);
            if (attrPtr != nullptr)
            {
                char attrBuf[128];
                uint8_t indxAttr = 0;
                while (attrPtr && (attrPtr = ConfigManager::getNextObjectInArray(attrPtr, attrBuf, sizeof(attrBuf))))
                {
                    ESP_LOGV(TAG, "attrPtr:%s", attrPtr);
                    AttributeData *data = &sensorData->attributes[indxAttr];
                    sensorData->num_attributes = indxAttr + 1;
                    if (sensorData->num_attributes == MAX_ATTRIBUTES)
                    {
                        ESP_LOGE(TAG, "Nombre d'attributs(%d) dépasse la limite(%d)", sensorData->num_attributes, MAX_ATTRIBUTES);
                        return;
                    }
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
    ESP_LOGV(TAG, "Terminé");
    config->initialized = true;
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

void ConfigManager::printConfig(DeviceConfig *config)
{
    int bufferSize = 512;
    char buffer[bufferSize];
    char *indxBuffer = buffer;

    indxBuffer += snprintf(indxBuffer, bufferSize, "\n\tInitialisé:%d\n\tDEV(ID:%d, MAC:%s, Sleep:%d, Alive:%d, Sensors(%d))",
                           config->initialized, config->id_device, config->mac,
                           config->sleep_period, config->im_alive_period,
                           config->num_sensors);
    bufferSize -= strlen(buffer);

    for (size_t i = 0; i < config->num_sensors; i++)
    {
        char *sensorIndex = indxBuffer;
        SensorData *sd = &config->sensors[i];

        indxBuffer += snprintf(indxBuffer, bufferSize, "\n\t\tSensor(ID:%d, Role:%d, Attr(%d))", sd->id_sensor, sd->role, sd->num_attributes);
        bufferSize -= strlen(buffer);

        for (size_t j = 0; j < sd->num_attributes; j++)
        {
            AttributeData *ad = &sd->attributes[j];
            indxBuffer += snprintf(indxBuffer, bufferSize, "\n\t\t\tAttr:(Key:%s, Value:%d)", ad->key, ad->value);
            bufferSize -= strlen(buffer);
        }
        //      ESP_LOGI(TAG, "%s", sensorIndex);
    }

    ESP_LOGI(TAG, "%s", buffer);
}

void ConfigManager::getMac(char *configJson, char *dest, int destLen)
{
    ConfigManager::jsonExtractString(configJson, "MAC", dest, destLen);
}
