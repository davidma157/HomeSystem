#ifndef JSON_HELPER_H
#define JSON_HELPER_H

#include "stdio.h"

/**********Topics exemple ********************************************
    - Capteur publie ses données
    home/v2/devices/ID-32/telemetry          -- device → serveur
    - Capteur publie son état
    home/v2/devices/ID-32/status             -- device → serveur
    - Serveur envoie une commande
    home/v2/devices/ID-32/command/config/set -- serveur → device
    - Capteur répond à la commande
    home/v2/devices/ID-32/response/config/ok -- device → serveur
*********************************************************************/
#define TOPIC_PUB_TEMPLATE "home/V2/devices/ID-%d/%s"
#define TOPIC_PUB_CFG_OK "config/ok"
#define TOPIC_PUB_ERROR "error"
#define TOPIC_PUB_MAC_IP "mac-ip" // Réponse MAC + IP
#define TOPIC_PUB_STATUS "status"
#define TOPIC_PUB_TELEMETRY "telemetry" // Données d'un capteur

#define TOPIC_SUB_TEMPLATE "home/V2/devices/ID-%d/%s"
#define TOPIC_SUB_CFG_SET "config/set"
#define TOPIC_SUB_GET_MAC_IP "get/mac-ip" // Demande du MAC + IP
#define TOPIC_SUB_RESTART "restart"

/*═════ CONFIGURATION JSON TAGS ═══════════════════════════════════════════════════════════════════════════════
    -------------- Message de configuration --------------
    {"ID":32,"MAC":"58:8C:81:B0:D8:9C","SL":120,"IMA":0,
        "SES":[
            {"ID":51,"ROLE":"TEMPERATURE","ATTS":[{"KEY":"pin","VAL":11},{"KEY":"pin2","VAL":2}]},
            {"ID":52,"ROLE":"VALVE","ATTS":[[{"KEY":"name","VAL":(int value)}]}
        ]
    }
*/

#define JTAG_ID "ID"
#define JTAG_MAC "MAC"
#define JTAG_SLEEP "SL"
#define JTAG_IM_ALIVE "IMA"
#define JTAG_SENSORS "SES"
#define JTAG_ROLE "ROLE"
#define JTAG_ATTRIBUTS "ATTS"
#define JTAG_KEY "KEY"
#define JTAG_VALUE "VAL"

#define ROLE_TEMPERATURE "TEMPERATURE"
#define ROLE_WATER_DETECTION "WD"
#define ROLE_VALVE "VALVE"

/*═════ CONFIGURATION JSON TAGS FIN ═════════════════════════════════════════════════════════════════════════════*/

/*═════ MESSAGES JSON ═════════════════════════════════════════════════════════════════════════════*/
// Gabarit pour les messages publiés
#define JMSG_STATUS "{\"ID\":%d,\"status\":\"%s\"}"
#define JMSG_START "{\"ID\":%d,\"status\":\"Start\"}"
#define JMSG_DATA_TEMPERATURE "{\"ID\":%d,\"SE\":{\"IDS\":%d,\"V\":%d,\"SN\":%d,\"TMP\":{\"T\":%.1f,\"H\":%.1f}}}"

class JsonHelper
{
public:
    static void getMessageStatus(char *dest, size_t size, int id, const char *status)
    {
        snprintf(dest, size, JMSG_STATUS,
                 id, status);
    }
};
/*═════ MESSAGES FIN ═════════════════════════════════════════════════════════════════════════════*/

#endif