#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

#define TAG_CONFIG "tagConfig"
#define NAME_SPACE_CONFIG "nameSpaceConfig"

class ConfigManager
{
private:
    static Preferences preferences;
    static char configJson[];

public:
    // Persistence
    static char *load();
    static void save(char *json);
    static void reset();

    static const char *jsonFindValue(const char *json, const char *key);
    static void jsonExtractString(const char *src, char *dest, int maxLen);
    static int jsonExtractInt(const char *src);
    static const char *findArrayStart(const char *json, const char *key);
    static const char *getNextObjectInArray(const char *currentPos, char *dest, int maxLen);
};

#endif