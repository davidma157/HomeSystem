#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "esp_log.h"
#include <Arduino.h>
#include <Preferences.h>

#define TAG_CONFIG "tagConfig"
#define NAME_SPACE_CONFIG "nameSpaceConfig"

#define ATTR_KEY_SIZE 8
#define MAX_SENSORS 4
#define MAX_ATTRIBUTES 4

enum class SensorRole
{
    UNDEFINED,
    TEMPERATURE,
    WATER_DETECTION
};

struct AttributeData
{
    char key[ATTR_KEY_SIZE];
    int value;
};

struct SensorData
{
    int id_sensor;
    SensorRole role;
    int num_attributes;
    AttributeData attributes[MAX_ATTRIBUTES];
};

struct DeviceConfig
{
    int id_device;
    int sleep_period;
    int im_alive_period;
    int num_sensors;
    SensorData sensors[MAX_SENSORS];
};

class ConfigManager
{
private:
    static Preferences preferences;

    static int jsonExtractInt(const char *json, const char *key);
    static void jsonExtractString(const char *src, const char *key, char *dest, int maxLen);

    static const char *jsonFindValue(const char *json, const char *key);
    static const char *findArrayStart(const char *json, const char *key);
    static const char *getNextObjectInArray(const char *currentPos, char *dest, int maxLen);

    static SensorRole getRole(char *sensorJson);

public:
    static DeviceConfig *extractConfig(char *configJson);
    // Persistence
    static void load();
    static void save(char *json);
    static void reset();
    static DeviceConfig *getConfig();
    static void printConfig();
};

#endif