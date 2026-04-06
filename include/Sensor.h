#ifndef SENSOR_H
#define SENSOR_H

#include "ConfigManager.h"
#include "NetworkManager.h"

/**
 * @brief Classe abstraite de base pour tous les capteurs
 *
 * Définit l'interface commune pour tous les types de capteurs.
 * Utilise des pointeurs bruts car la classe ne possède pas les objets.
 */
class Sensor
{
public:
    /**
     * @brief Constructeur
     */
    Sensor(NetworkManager *network, int deviceId, SensorData *sd)
    {
        this->network = network;
        this->deviceId = deviceId;
    };
    virtual ~Sensor() = default;

    // Méthodes purement virtuelles à implémenter
    // virtual void setup(int deviceId, SensorData *sd) = 0;
    virtual void executeJob() = 0;
    virtual const char *getTopicDomain() const = 0;
    virtual const char *getDescription() const = 0;

#ifdef DEBUG_MODE
    virtual void debugMode();
#endif

protected:
    int deviceId = 0;
    int sensorId = 0;
    NetworkManager *network = nullptr; // Pointeur non-possédé
                                       // ConfigManager *config;   // Pointeur non-possédé

    /**
     * @brief Vérifie la validité des pointeurs
     * @return true si les pointeurs sont valides
     */
    bool isValid() const
    {
        return network != nullptr; // && config != nullptr;
    }
};

#endif // SENSOR_H