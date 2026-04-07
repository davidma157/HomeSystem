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

/*═════ JSON TAGS ═══════════════════════════════════════════════════════════════════════════════*/
// Message de configuration
//  {"ID":32,"MAC":"58:8C:81:B0:D8:9C","SLEEP":120,"ALIVE":0,
//   "SS":[ {"ID":51,"ROLE":"TEMPERATURE","ATTS":[{"KEY":"pin","VAL":11},{"KEY":"pin2","VAL":2}]},
//         {"ID":52,"ROLE":"VALVE","ATTS":[]}
//       ]
// }

#define JTAG_ID "ID"
#define JTAG_MAC "MAC"
#define JTAG_SLEEP "SLEEP"
#define JTAG_ALIVE "ALIVE"
#define JTAG_SENSORS "SS"
#define JTAG_ROLE "ROLE"
#define JTAG_ATTRIBUTS "ATTS"
#define JTAG_KEY "KEY"
#define JTAG_VALUE "VAL"

#define ROLE_TEMPERATURE "TEMPERATURE"
#define ROLE_WATER_DETECTION "WD"
#define ROLE_VALVE "VALVE"

/*═════ JSON TAGS FIN ═════════════════════════════════════════════════════════════════════════════*/

enum class SensorRole
{
    UNDEFINED,
    TEMPERATURE,
    VALVE,
    WATER_DETECTION
};

struct AttributeData
{
    char key[ATTR_KEY_SIZE];
    int value;

    bool operator==(const AttributeData &other) const
    {
        return value == other.value &&
               strncmp(key, other.key, ATTR_KEY_SIZE) == 0;
    }
};

struct SensorData
{
    int id_sensor;
    SensorRole role;
    int num_attributes;
    AttributeData attributes[MAX_ATTRIBUTES];

    bool operator==(const SensorData &other) const
    {
        if (id_sensor != other.id_sensor || role != other.role || num_attributes != other.num_attributes)
            return false;

        // On ne compare que les attributs réellement utilisés
        for (int i = 0; i < num_attributes; ++i)
        {
            if (!(attributes[i] == other.attributes[i]))
                return false;
        }
        return true;
    }
};

struct DeviceConfig
{
    bool initialized = false;
    int id_device;
    char mac[20];
    int sleep_period;
    int im_alive_period;
    int num_sensors;
    SensorData sensors[MAX_SENSORS];

    bool operator==(const DeviceConfig &other) const
    {
        if (initialized != other.initialized || id_device != other.id_device ||
            sleep_period != other.sleep_period || im_alive_period != other.im_alive_period ||
            num_sensors != other.num_sensors)
            return false;

        for (int i = 0; i < num_sensors; ++i)
        {
            if (!(sensors[i] == other.sensors[i]))
                return false;
        }
        return true;
    }
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
    static void extractConfig(char *configJson, DeviceConfig *config);
    // Persistence
    static bool loadConfig(DeviceConfig *config);
    static void save(DeviceConfig *config);
    static void reset();
    //  static DeviceConfig *getConfig();
    static void printConfig(DeviceConfig *config);
    static void getMac(char *configJson, char *dest, int destLen);
};

#endif