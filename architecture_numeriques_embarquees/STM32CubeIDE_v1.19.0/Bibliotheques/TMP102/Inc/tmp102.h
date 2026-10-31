#ifndef TMP102_H
#define TMP102_H

#include "main.h"
#include <stdio.h>
#include <stdbool.h>

// ==============================================================================
// Tailles et timeouts des transactions I2C (évite le hardcoding)
// cf rapport : Scénario 6 – 4.3. Define
// ==============================================================================
#define TMP102_NBR_CONFIG_REG_BYTES 2   /**< Taille du registre de configuration (octets) */
#define TMP102_NBR_REG_TEMP_BYTES   2   /**< Taille des registres de température, T_LOW et T_HIGH (octets) */
#define TMP102_I2C_TIMEOUT          100 /**< Timeout des transactions I2C de la HAL (ms) */

// ADRESSES I2C : dépendent du niveau de la broche ADD0, décalées de 1 bit vers la gauche (format HAL 8 bits)
// cf rapport : Scénario 6 – 1.3. Adresse I²C (ADD0)
#define TMP102_I2C_ADDR_GND         (0x48 << 1) /**< ADD0 relié à GND */
#define TMP102_I2C_ADDR_VCC         (0x49 << 1) /**< ADD0 relié à VCC */
#define TMP102_I2C_ADDR_SDA         (0x4A << 1) /**< ADD0 relié à SDA */
#define TMP102_I2C_ADDR_SCL         (0x4B << 1) /**< ADD0 relié à SCL */

// REGISTRES INTERNES : adresses à fournir à la HAL (MemAddress)
// cf rapport : Scénario 6 – 2.1. Registres accessibles
#define TMP102_REG_TEMP             0x00 /**< Registre de température (lecture seule) */
#define TMP102_REG_CONFIG           0x01 /**< Registre de configuration */
#define TMP102_REG_TLOW             0x02 /**< Seuil bas d'alerte T_LOW */
#define TMP102_REG_THIGH            0x03 /**< Seuil haut d'alerte T_HIGH */

// VALEURS DES REGLAGES : valeurs possibles de chaque champ de TMP102_Config_t
// cf rapport : Scénario 6 – 2.3. Registre de configuration
#define TMP102_MODE_CONTINUOUS      0x00 /**< Conversions continues (mode par défaut du capteur) */
#define TMP102_MODE_SHUTDOWN        0x01 /**< Veille (<0.15 µA), conversion uniquement en One-Shot */

#define TMP102_TM_COMPARATOR        0x00 /**< Alerte en mode comparateur */
#define TMP102_TM_INTERRUPT         0x01 /**< Alerte en mode interruption */

#define TMP102_POL_ACTIVE_LOW       0x00 /**< Broche ALERT active à l'état bas */
#define TMP102_POL_ACTIVE_HIGH      0x01 /**< Broche ALERT active à l'état haut */

#define TMP102_FAULT_1              0x00 /**< 1 faute consécutive avant alerte */
#define TMP102_FAULT_2              0x01 /**< 2 fautes consécutives avant alerte */
#define TMP102_FAULT_4              0x02 /**< 4 fautes consécutives avant alerte */
#define TMP102_FAULT_6              0x03 /**< 6 fautes consécutives avant alerte */

#define TMP102_RATE_0_25HZ          0x00 /**< Fréquence de conversion 0.25 Hz (mode Continuous) */
#define TMP102_RATE_1HZ             0x01 /**< Fréquence de conversion 1 Hz (mode Continuous) */
#define TMP102_RATE_4HZ             0x02 /**< Fréquence de conversion 4 Hz (mode Continuous) */
#define TMP102_RATE_8HZ             0x03 /**< Fréquence de conversion 8 Hz (mode Continuous) */

#define TMP102_EXTENDED_OFF         0x00 /**< Mode normal : température sur 12 bits */
#define TMP102_EXTENDED_ON          0x01 /**< Mode étendu : température sur 13 bits */

// Bit One-Shot (OS) : à 1 en écriture il déclenche une conversion ; en lecture il vaut 0 pendant la mesure
// cf rapport : Scénario 6 – 9.2. Déclencher une mesure One-Shot
#define TMP102_OS_DEFAULT           0x00 /**< Valeur d'initialisation du champ OneShot */
#define TMP102_OS_START_MEASURE     0x01 /**< Écriture : déclenche une mesure One-Shot */
#define TMP102_OS_MEASURING         0x00 /**< Lecture : conversion en cours */

// CHAMPS EN LECTURE SEULE / INUTILISES : valeurs par défaut de la datasheet
// cf rapport : Scénario 6 – 6.2. Valeurs par défaut
#define TMP102_R_DEFAULT            0x03 /**< Résolution du convertisseur (lecture seule, toujours 0x03) */
#define TMP102_AL_DEFAULT           0x01 /**< Bit d'alerte (lecture seule) */
#define TMP102_PADD_DEFAULT         0x00 /**< Bits inutilisés (toujours 0x00) */

// INTERPRETATION REGISTRE TEMPERATURE
// cf rapport : Scénario 6 – 8.1. Registre brut → température en °C
#define TMP102_TEMP_RESOLUTION      0.0625f /**< Valeur en °C d'un LSB du registre de température */
#define TMP102_NORM_BIT_LEN         12      /**< Longueur de la température en mode normal (bits) */
#define TMP102_EXT_BIT_LEN          13      /**< Longueur de la température en mode étendu (bits) */

// Sens de la synchronisation dans TMP102_SyncConfigBuffer()
#define WRITE true  /**< Encodage : structure -> buffer */
#define READ  false /**< Décodage : buffer -> structure */

// STRUCTURES DE MAPPING

/**
 * @brief Descripteur de la position d'un champ dans le registre de configuration (16 bits).
 *        Permet d'encoder et de décoder tous les champs avec une seule boucle générique.
 *        // cf rapport : Scénario 6 – 4.4. Organisation des champs du registre de configuration
 */
typedef struct 
{
    uint8_t byte_idx;   /**< Octet 0 ou 1 du registre 16 bits */
    uint8_t position;   /**< Position du LSB du champ dans l'octet */
    uint8_t length;     /**< Nombre de bits du champ */
    uint8_t *value_ptr; /**< Pointeur vers le champ correspondant dans TMP102_Config_t */
} TMP102_FieldMap_t;

/**
 * @brief Réglages du registre de configuration, un champ par réglage (pas de masques de bits à manipuler).
 *        // cf rapport : Scénario 6 – 4.2. Les structures `TMP102_Config_t` et `TMP102_t`
 */
typedef struct 
{
    uint8_t Mode;           /**< Shutdown ou Continuous */
    uint8_t ConversionRate; /**< 0.25 / 1 / 4 / 8 Hz */
    uint8_t ExtendedMode;   /**< 12 ou 13 bits */
    uint8_t ThermostatMode; /**< Comparateur ou Interruption */
    uint8_t AlertPolarity;  /**< Actif bas ou haut */
    uint8_t FaultQueue;     /**< 1 / 2 / 4 / 6 fautes */
    uint8_t OneShot;        /**< Déclenchement One-Shot */
    uint8_t Resolution;     /**< Lecture seule (toujours 0x03) */
    uint8_t Alert;          /**< Lecture seule */
    uint8_t Padding;        /**< Bits inutilisés (toujours 0x00) */
} TMP102_Config_t;

/**
 * @brief Objet TMP102 : regroupe tout le contexte d'une instance du capteur.
 *        Permet d'instancier plusieurs capteurs indépendants et de conserver les réglages entre deux écritures.
 *        // cf rapport : Scénario 6 – 4.2. Les structures `TMP102_Config_t` et `TMP102_t`
 */
typedef struct 
{
    bool                is_measuring_one_shot; /**< Flag interne : une mesure One-Shot est en cours */
    I2C_HandleTypeDef   *busI2C;               /**< Pointeur vers le bus I2C de la HAL */
    uint8_t             address;               /**< Adresse I2C décalée (<<1) */
    TMP102_Config_t     config;                /**< Tous les réglages du registre de configuration */
    float               T_High;                /**< Seuil haut d'alerte (°C) */
    float               T_Low;                 /**< Seuil bas d'alerte (°C) */
} TMP102_t;

// API PUBLIQUE : seules ces 6 fonctions sont utilisables hors de la bibliothèque, les autres sont static
// cf rapport : Scénario 6 – 4.1. Vue d'ensemble
void TMP102_init_default(TMP102_t *htmp);
bool TMP102_apply_registers(TMP102_t *htmp);
bool TMP102_get_temperature(TMP102_t *htmp, float *Temperature);
bool TMP102_start_measure(TMP102_t *htmp);
bool TMP102_is_measure_ready(TMP102_t *htmp);
char* TMP102_temperature_to_string(float temperature, uint8_t precision);

#endif