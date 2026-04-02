#include "ConfigManager.h"
#include "esp_log.h"

const char TAG[] = "CONFIG_MGR";

Preferences ConfigManager::preferences;
char ConfigManager::configJson[500] = "";

char *ConfigManager::load()
{
    if (preferences.begin(NAME_SPACE_CONFIG, true))
    {
        bool initialized = preferences.getBool("initialized", false);

        ConfigManager::configJson[0] = '\0';

        if (initialized)
        {
            const char *strConfig = (preferences.getString(TAG_CONFIG, "")).c_str();
            strncpy(ConfigManager::configJson, strConfig, strlen(strConfig));
            ESP_LOGD(TAG, "%s", ConfigManager::configJson);
        }
        else
        {
            ESP_LOGD(TAG, "Configuration inexistante.");
            ESP_LOGD(TAG, "%s", ConfigManager::configJson);
        }
        preferences.end();
    }

    return ConfigManager::configJson;
}

void ConfigManager::save(char *json)
{
    preferences.begin(NAME_SPACE_CONFIG, false);
    preferences.clear();

    preferences.putBool("initialized", true);
    preferences.putString(TAG_CONFIG, json);

    preferences.end();
    Serial.println("Configuration sauvegardée");
}

void ConfigManager::reset()
{
    preferences.begin(NAME_SPACE_CONFIG, false);
    preferences.clear();
    preferences.end();

    Serial.println("Configuration réinitialisée");
}

const char *ConfigManager::jsonFindValue(const char *json, const char *key)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);

    ESP_LOGD(TAG, "FindValue: <%s>\n", search);
    const char *pos = strstr(json, search);
    if (!pos)
        return nullptr;

    pos += strlen(search);

    while (*pos == ' ' || *pos == '\t')
        pos++;

    ESP_LOGD(TAG, "FindValue - pos-->%s\n", pos);
    return pos;
}

void ConfigManager::jsonExtractString(const char *src, char *dest, int maxLen)
{
    if (*src == '"')
        src++;

    int i = 0;
    while (*src && *src != '"' && i < maxLen - 1)
    {
        dest[i++] = *src++;
    }
    dest[i] = '\0';
}

int ConfigManager::jsonExtractInt(const char *src)
{
    while (*src == ' ' || *src == '\t')
        src++;
    ESP_LOGD(TAG, "Int-->%s", src);
    return atoi(src);
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