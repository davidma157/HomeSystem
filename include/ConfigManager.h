#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Preferences.h>
#include <Arduino.h>

struct Config {
    int id_device = 0;
    int id_sensor = 0;
    int im_alive_period = 0;
    int sleep_period = 0;
    uint16_t counter = 0;
    const char* version = "2.0";
};

class ConfigManager {
private:
    Preferences preferences;
    Config config;
    const char* namespace_name = "config";

public:
    ConfigManager();
    
    // Getters
    int getDeviceId() const { return config.id_device; }
    int getSensorId() const { return config.id_sensor; }
    int getAlivePeriod() const { return config.im_alive_period; }
    int getSleepPeriod() const { return config.sleep_period; }
    const char* getVersion() const { return config.version; }

    uint16_t getCounter() const { return config.counter; }
    Config& getConfig() { return config; }
    

    // Setters
    void setDeviceId(int id){config.id_device = id;};
    void setSensorId(int id){config.id_sensor = id;};
    void setAlivePeriod(int period){config.im_alive_period = period;};
    void setSleepPeriod(int period){config.sleep_period = period;};
    void incrementCounter(){config.counter++;};
    
    // Persistence
    bool load();
    void save();
    void reset();
    
    // JSON parsing helpers
    void jsonPrintConfig(char* buffer, int bufferSize){
        snprintf(buffer, bufferSize,
        "{\"id_device\":%d, \"id_sensor\":%d,\"alive\":%d,\"sleep\":%d,\"version\":%s}",
        config.id_device, config.id_sensor, config.im_alive_period, config.sleep_period, config.version);
    };
    static const char* jsonFindValue(const char* json, const char* key);
    static void jsonExtractString(const char* src, char* dest, int maxLen);
    static int jsonExtractInt(const char* src);
};

#endif