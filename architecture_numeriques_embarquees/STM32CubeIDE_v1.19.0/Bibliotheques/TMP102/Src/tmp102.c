#include "tmp102.h"

// Petite structure locale pour faciliter la boucle de construction du registre
typedef struct {
    uint16_t Value;
    uint8_t  Position;
    uint8_t  Length;
} TMP102_RegField_t;

/**
 * @brief Construit et envoie la configuration au capteur
 */
HAL_StatusTypeDef TMP102_Init(TMP102_HandleTypeDef *htmp) {
    uint16_t config_reg_16bits = 0;
    uint8_t config_bytes[TMP102_NBR_CONFIG_REG_BYTES];

    // Tableau regroupant tous les champs du registre 16 bits
    TMP102_RegField_t fields[] = {
        {TMP102_OS_DEFAULT,         TMP102_OS_POS,    TMP102_OS_LEN},
        {TMP102_R_DEFAULT,          TMP102_R_POS,     TMP102_R_LEN},
        {htmp->Init.FaultQueue,     TMP102_FAULT_POS, TMP102_FAULT_LEN},
        {htmp->Init.AlertPolarity,  TMP102_POL_POS,   TMP102_POL_LEN},
        {htmp->Init.ThermostatMode, TMP102_TM_POS,    TMP102_TM_LEN},
        {htmp->Init.Mode,           TMP102_SD_POS,    TMP102_SD_LEN},
        {htmp->Init.ConversionRate, TMP102_CR_POS,    TMP102_CR_LEN},
        {TMP102_AL_DEFAULT,         TMP102_AL_POS,    TMP102_AL_LEN},
        {htmp->Init.ExtendedMode,   TMP102_EM_POS,    TMP102_EM_LEN},
        {TMP102_PADD_DEFAULT,       TMP102_PADD_POS,  TMP102_PADD_LEN}
    };

    // Calcul du nombre de champs dans le tableau
    uint8_t num_fields = sizeof(fields) / sizeof(fields[0]);

    // Boucle d'assemblage du registre 16 bits
    for (uint8_t i = 0; i < num_fields; i++) {
        // Crée un masque dynamique en fonction de la longueur (ex: len=2 -> masque=0x03)
        uint16_t mask = (1 << fields[i].Length) - 1; 
        
        // Applique le masque à la valeur pour la sécuriser, puis la décale à sa position
        config_reg_16bits |= (fields[i].Value & mask) << fields[i].Position;
    }

    // Découpage en 2 octets pour l'envoi I2C (MSB en premier)
    config_bytes[0] = (config_reg_16bits >> 8) & 0xFF;
    config_bytes[1] = config_reg_16bits & 0xFF;

    // Envoi de la configuration via HAL
    return HAL_I2C_Mem_Write(htmp->hi2c, 
                             htmp->Address, 
                             TMP102_REG_CONFIG, 
                             I2C_MEMADD_SIZE_8BIT, 
                             config_bytes, 
                             TMP102_NBR_CONFIG_REG_BYTES, 
                             TMP102_I2C_TIMEOUT);
}

/**
 * @brief Lit et convertit la température
 */
HAL_StatusTypeDef TMP102_ReadTemperature(TMP102_HandleTypeDef *htmp, float *Temperature) {
    uint8_t buffer[TMP102_NBR_CONFIG_REG_BYTES] = {0};
    int16_t raw_temp;
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Read(htmp->hi2c, 
                              htmp->Address, 
                              TMP102_REG_TEMP, 
                              I2C_MEMADD_SIZE_8BIT, 
                              buffer, 
                              TMP102_NBR_CONFIG_REG_BYTES, 
                              TMP102_I2C_TIMEOUT);
    
    if (status != HAL_OK) {
        *Temperature = -999.0f;
        return status;
    }

    // Le traitement dépend de la configuration (12 ou 13 bits)
    if (htmp->Init.ExtendedMode == TMP102_EXTENDED_ON) {
        raw_temp = (buffer[0] << 5) | (buffer[1] >> 3);
        if (raw_temp & 0x1000) raw_temp |= 0xE000; 
    } else {
        raw_temp = (buffer[0] << 4) | (buffer[1] >> 4);
        if (raw_temp & 0x0800) raw_temp |= 0xF000;
    }

    *Temperature = (float)raw_temp * 0.0625f;
    return HAL_OK;
}