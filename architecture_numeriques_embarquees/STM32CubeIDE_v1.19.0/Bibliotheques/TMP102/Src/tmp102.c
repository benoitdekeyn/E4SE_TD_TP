#include "tmp102.h"

typedef struct {
    uint8_t byte_idx;
    uint8_t position;
    uint8_t length;
    uint8_t value;
} TMP102_Field_t;

bool TMP102_Init(TMP102_t *tmp102) {
    uint8_t config_reg_bytes[TMP102_NBR_CONFIG_REG_BYTES] = {0, 0};

    // Assignation des champs du registre de configuration
    TMP102_Field_t fields[] = {

    //   BYTE | POSITION | LENGTH | VALUE

        {   0 ,        7 ,      1 , TMP102_OS_DEFAULT               },
        {   0 ,        5 ,      2 , TMP102_R_DEFAULT                },
        {   0 ,        3 ,      2 , tmp102->config.FaultQueue       },
        {   0 ,        2 ,      1 , tmp102->config.AlertPolarity    },
        {   0 ,        1 ,      1 , tmp102->config.ThermostatMode   },
        {   0 ,        0 ,      1 , tmp102->config.Mode             },

        {   1 ,        6 ,      2 , tmp102->config.ConversionRate   },
        {   1 ,        5 ,      1 , TMP102_AL_DEFAULT               },
        {   1 ,        4 ,      1 , tmp102->config.ExtendedMode     },
        {   1 ,        0 ,      4 , TMP102_PADD_DEFAULT             }
    };

    // remplissage du registre
    uint8_t num_fields = sizeof(fields) / sizeof(fields[0]);
    for (uint8_t i = 0; i < num_fields; i++) 
    {
        uint8_t mask = (1 << fields[i].length) - 1; 
        config_reg_bytes[    fields[i].byte_idx]
        |=              (    fields[i].value & mask) 
        <<                   fields[i].position;
    }

    return (HAL_I2C_Mem_Write(tmp102->busI2C, 
                              tmp102->address, 
                              TMP102_REG_CONFIG, 
                              I2C_MEMADD_SIZE_8BIT, 
                              config_reg_bytes, 
                              TMP102_NBR_CONFIG_REG_BYTES, 
                              TMP102_I2C_TIMEOUT) 
            == HAL_OK);
}

bool TMP102_ReadTemperature(TMP102_t *tmp102, float *Temperature) {
    uint8_t buffer[TMP102_NBR_CONFIG_REG_BYTES] = {0};
    int16_t raw_temp;

    bool i2c_ok = (HAL_I2C_Mem_Read(tmp102->busI2C, 
                                     tmp102->address, 
                                     TMP102_REG_TEMP, 
                                     I2C_MEMADD_SIZE_8BIT, 
                                     buffer, 
                                     TMP102_NBR_CONFIG_REG_BYTES, 
                                     TMP102_I2C_TIMEOUT)
                    == HAL_OK);

    if (!i2c_ok) {
        *Temperature = -999.0f;
        return false;
    }

    if (tmp102->config.ExtendedMode == TMP102_EXTENDED_ON) {
        raw_temp = (buffer[0] << 5) | (buffer[1] >> 3);
        if (raw_temp & 0x1000) raw_temp |= 0xE000; 
    } else {
        raw_temp = (buffer[0] << 4) | (buffer[1] >> 4);
        if (raw_temp & 0x0800) raw_temp |= 0xF000;
    }

    *Temperature = (float)raw_temp * 0.0625f;
    return true;
}