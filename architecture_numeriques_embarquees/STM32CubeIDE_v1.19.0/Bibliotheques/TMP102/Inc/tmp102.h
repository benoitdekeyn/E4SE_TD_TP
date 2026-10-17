#ifndef TMP102_H
#define TMP102_H

#include "main.h"

// ==========================================
// CONSTANTES GLOBALES I2C
// ==========================================
#define TMP102_NBR_CONFIG_REG_BYTES 2
#define TMP102_I2C_TIMEOUT          100

// ADRESSES I2C (Décalées d'un bit pour HAL)
#define TMP102_I2C_ADDR_GND         (0x48 << 1)
#define TMP102_I2C_ADDR_VCC         (0x49 << 1)
#define TMP102_I2C_ADDR_SDA         (0x4A << 1)
#define TMP102_I2C_ADDR_SCL         (0x4B << 1)

// REGISTRES INTERNES
#define TMP102_REG_TEMP             0x00
#define TMP102_REG_CONFIG           0x01

// ==========================================
// MAPPAGE DU REGISTRE DE CONFIGURATION (16 bits)
// ==========================================
// Positions (Décalage par rapport au bit 0)
#define TMP102_OS_POS       15
#define TMP102_R_POS        13
#define TMP102_FAULT_POS    11
#define TMP102_POL_POS      10
#define TMP102_TM_POS       9
#define TMP102_SD_POS       8
#define TMP102_CR_POS       6
#define TMP102_AL_POS       5
#define TMP102_EM_POS       4
#define TMP102_PADD_POS     0

// Longueurs (Nombre de bits)
#define TMP102_OS_LEN       1
#define TMP102_R_LEN        2
#define TMP102_FAULT_LEN    2
#define TMP102_POL_LEN      1
#define TMP102_TM_LEN       1
#define TMP102_SD_LEN       1
#define TMP102_CR_LEN       2
#define TMP102_AL_LEN       1
#define TMP102_EM_LEN       1
#define TMP102_PADD_LEN     4

// ==========================================
// VALEURS DES RÉGLAGES
// ==========================================
// Valeurs fixes / lecture seule / par défaut
#define TMP102_OS_DEFAULT           0x00  // Pas de One-Shot au démarrage
#define TMP102_R_DEFAULT            0x03  // R1/R0 toujours à 11 (binaire)
#define TMP102_AL_DEFAULT           0x01  // Valeur par défaut de l'alerte
#define TMP102_PADD_DEFAULT         0x00  // Padding toujours à 0

// Mode de fonctionnement (SD)
#define TMP102_MODE_CONTINUOUS      0x00
#define TMP102_MODE_SHUTDOWN        0x01

// Mode Thermostat (TM)
#define TMP102_TM_COMPARATOR        0x00
#define TMP102_TM_INTERRUPT         0x01

// Polarité de la broche ALERT (POL)
#define TMP102_POL_ACTIVE_LOW       0x00
#define TMP102_POL_ACTIVE_HIGH      0x01

// Filtre anti-rebond (F1/F0)
#define TMP102_FAULT_1              0x00
#define TMP102_FAULT_2              0x01
#define TMP102_FAULT_4              0x02
#define TMP102_FAULT_6              0x03

// Cadence d'échantillonnage (CR1/CR0)
#define TMP102_RATE_0_25HZ          0x00
#define TMP102_RATE_1HZ             0x01
#define TMP102_RATE_4HZ             0x02
#define TMP102_RATE_8HZ             0x03

// Mode Étendu (EM)
#define TMP102_EXTENDED_OFF         0x00 
#define TMP102_EXTENDED_ON          0x01 

// ==========================================
// STRUCTURES "OBJET"
// ==========================================
typedef struct {
    uint8_t Mode;
    uint8_t ConversionRate;
    uint8_t ExtendedMode;
    uint8_t ThermostatMode;
    uint8_t AlertPolarity;
    uint8_t FaultQueue;
} TMP102_InitTypeDef;

typedef struct {
    I2C_HandleTypeDef   *hi2c;
    uint8_t             Address;
    TMP102_InitTypeDef  Init;
} TMP102_HandleTypeDef;

// ==========================================
// PROTOTYPES DES FONCTIONS
// ==========================================
HAL_StatusTypeDef TMP102_Init(TMP102_HandleTypeDef *htmp);
HAL_StatusTypeDef TMP102_ReadTemperature(TMP102_HandleTypeDef *htmp, float *Temperature);

#endif /* TMP102_H */