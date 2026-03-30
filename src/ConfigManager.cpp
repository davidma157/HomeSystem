#include "ConfigManager.h"

ConfigManager::ConfigManager() {
    // Constructor
    config.counter = 0;
}

bool ConfigManager::load() {
    preferences.begin(namespace_name, true);
    
    bool exists = preferences.getBool("initialized", false);
    
    if (exists) {
        config.id_device = preferences.getInt("id_device", 0);
        config.id_sensor = preferences.getInt("id_sensor", 0);
        config.sleep_period = preferences.getULong64("sleep_period", 0);
        config.im_alive_period = preferences.getULong64("alive_period", 0);
        preferences.end();
        Serial.println("Configuration chargée depuis la mémoire");
        return true;
    } else {
        preferences.end();
        Serial.println("Aucune configuration trouvée, valeurs par défaut utilisées");
        return false;
    }
}

void ConfigManager::save() {
    preferences.begin(namespace_name, false);
    preferences.clear();
    preferences.putBool("initialized", true);
    preferences.putInt("id_device", config.id_device);
    preferences.putInt("id_sensor", config.id_sensor);
    preferences.putULong64("sleep_period", config.sleep_period);    
    preferences.putULong64("alive_period", config.im_alive_period);
    preferences.end();
    Serial.println("Configuration sauvegardée");
}

void ConfigManager::reset() {
    preferences.begin(namespace_name, false);
    preferences.clear();
    preferences.end();
    config.id_device = 0;
    config.id_sensor = 0;
    config.im_alive_period = 0;
    config.sleep_period = 0;
    config.counter = 0;
    Serial.println("Configuration réinitialisée");
}

const char* ConfigManager::jsonFindValue(const char* json, const char* key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    
    const char* pos = strstr(json, search);
    if (!pos)
        return nullptr;
    
    pos += strlen(search);
    
    while (*pos == ' ' || *pos == '\t')
        pos++;
    
    return pos;
}

void ConfigManager::jsonExtractString(const char* src, char* dest, int maxLen) {
    if (*src == '"')
        src++;
    
    int i = 0;
    while (*src && *src != '"' && i < maxLen - 1) {
        dest[i++] = *src++;
    }
    dest[i] = '\0';
}

int ConfigManager::jsonExtractInt(const char* src) {
    while (*src == ' ' || *src == '\t')
        src++;
    return atoi(src);
}