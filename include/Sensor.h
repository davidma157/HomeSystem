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
    Sensor() {};
    virtual ~Sensor() = default;

    /**
     * @brief setup
     * @param network Pointeur vers le NetworkManager (non possédé)
     * @param config Pointeur vers le ConfigManager (non possédé)
     */
    void setup(NetworkManager *network, ConfigManager *config)
    {
        this->network = network;
        this->config = config;
    };

    // Méthodes purement virtuelles à implémenter
    virtual void begin() = 0;
    virtual void executeJob() = 0;
    virtual bool updateConfig(char *messageJson) = 0;
    virtual const char *getTopicDomain() const = 0;
    virtual const char *getDescription() const = 0;

#ifdef DEBUG_MODE
    virtual void debugMode();
#endif

protected:
    NetworkManager *network; // Pointeur non-possédé
    ConfigManager *config;   // Pointeur non-possédé

    /**
     * @brief Vérifie la validité des pointeurs
     * @return true si les pointeurs sont valides
     */
    bool isValid() const
    {
        return network != nullptr && config != nullptr;
    }
};

#endif // SENSOR_H