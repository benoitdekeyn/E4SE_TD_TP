#include "tmp102.h"

// Reglages par defaut du capteur TMP102
void TMP102_init_default(TMP102_t *tmp102) 
{
    if (tmp102 == NULL) 
    {
        return;
    }

    tmp102->busI2C                  = NULL;
    tmp102->address                 = TMP102_I2C_ADDR_GND;
    tmp102->T_Low                   = 0.0f;
    tmp102->T_High                  = 100.0f;
    tmp102->is_measuring_one_shot   = false;

    tmp102->config.Mode             = TMP102_MODE_CONTINUOUS;
    tmp102->config.ConversionRate   = TMP102_RATE_8HZ;
    tmp102->config.ExtendedMode     = TMP102_EXTENDED_OFF;
    tmp102->config.ThermostatMode   = TMP102_TM_INTERRUPT;
    tmp102->config.AlertPolarity    = TMP102_POL_ACTIVE_HIGH;
    tmp102->config.FaultQueue       = TMP102_FAULT_1;
    tmp102->config.OneShot          = TMP102_OS_DEFAULT;
    tmp102->config.Resolution       = TMP102_R_DEFAULT;
    tmp102->config.Alert            = TMP102_AL_DEFAULT;
    tmp102->config.Padding          = TMP102_PADD_DEFAULT;
}

// Mapping des champs du registre de configuration
static void TMP102_SyncConfigBuffer(TMP102_t *tmp102, uint8_t *buffer, bool is_write) 
{
    TMP102_FieldMap_t fields[] = {
    //   Byte | Position | Length | Field Name
        {   0 ,        7 ,      1 , &tmp102->config.OneShot         },
        {   0 ,        5 ,      2 , &tmp102->config.Resolution      },
        {   0 ,        3 ,      2 , &tmp102->config.FaultQueue      },
        {   0 ,        2 ,      1 , &tmp102->config.AlertPolarity   },
        {   0 ,        1 ,      1 , &tmp102->config.ThermostatMode  },
        {   0 ,        0 ,      1 , &tmp102->config.Mode            },

        {   1 ,        6 ,      2 , &tmp102->config.ConversionRate  },
        {   1 ,        5 ,      1 , &tmp102->config.Alert           },
        {   1 ,        4 ,      1 , &tmp102->config.ExtendedMode    },
        {   1 ,        0 ,      4 , &tmp102->config.Padding         }
    };

    uint8_t num_fields = sizeof(fields) / sizeof(fields[0]);

    for (uint8_t i = 0; i < num_fields; i++) 
    {
        uint8_t mask = (1U << fields[i].length) - 1;
        
        if (is_write) 
        {
            // Encode : Structure -> Buffer
            buffer[fields[i].byte_idx] |= (*fields[i].value_ptr & mask) << fields[i].position;
        } 
        else 
        {
            // Décode : Buffer -> Structure
            *fields[i].value_ptr = (buffer[fields[i].byte_idx] >> fields[i].position) & mask;
        }
    }
}

static void TMP102_celsius_to_register(float temperature, uint8_t *reg_value, bool extended_mode) 
{
    // Sur combien de bits est codée la température
    uint8_t temp_length = (extended_mode) 
                        ? TMP102_EXT_BIT_LEN 
                        : TMP102_NORM_BIT_LEN;
    
    // Conversion de la température en valeur de registre
    float reg_value_float = temperature / TMP102_TEMP_RESOLUTION;
    
    // Arrondi de la valeur flottante pour obtenir la valeur entière du registre
    int16_t reg_val = (int16_t)(reg_value_float >= 0.0f ? (reg_value_float + 0.5f) : (reg_value_float - 0.5f));
    
    // Décalage pour aligner la valeur sur le nombre de bits utilisés par le capteur
    uint16_t raw_reg = (uint16_t)reg_val << (16 - temp_length);
    
    // Mise en bytes de la valeur brute du registre sur 16 bits
    reg_value[0] = (uint8_t)(raw_reg >> 8);
    reg_value[1] = (uint8_t)(raw_reg & 0xFF);
}

static void TMP102_register_to_celsius(uint8_t *reg_value, float *temperature, bool extended_mode) 
{
    // Sur combien de bits est codée la température
    uint8_t temp_length = (extended_mode) 
                        ? TMP102_EXT_BIT_LEN 
                        : TMP102_NORM_BIT_LEN;
    
    // Concaténation des 2 bytes reçus en type entier
    uint16_t raw_reg = (uint16_t)((reg_value[0] << 8) | reg_value[1]);
    
    // Décalage à droite avec extension du signe
    int16_t reg_val = (int16_t)raw_reg >> (16 - temp_length);
    
    // conversion de la valeur du capteur en degres celsius
    *temperature = (float)reg_val * TMP102_TEMP_RESOLUTION;
}

static bool TMP102_write_config_reg(TMP102_t *tmp102) 
{
    uint8_t config_reg_bytes[TMP102_NBR_CONFIG_REG_BYTES] = {0};
    TMP102_SyncConfigBuffer(tmp102, config_reg_bytes, WRITE);

    bool i2c_ok = (HAL_I2C_Mem_Write(tmp102->busI2C, 
                                     tmp102->address, 
                                     TMP102_REG_CONFIG, 
                                     I2C_MEMADD_SIZE_8BIT, 
                                     config_reg_bytes, 
                                     TMP102_NBR_CONFIG_REG_BYTES, 
                                     TMP102_I2C_TIMEOUT) 
                   == HAL_OK);
                   
    if (!i2c_ok) 
    {
        return false;
    }
    return true;
}

static bool TMP102_read_config_reg(TMP102_t *tmp102) 
{
    uint8_t config_reg_bytes[TMP102_NBR_CONFIG_REG_BYTES] = {0};

    bool i2c_ok = (HAL_I2C_Mem_Read(tmp102->busI2C, 
                                    tmp102->address, 
                                    TMP102_REG_CONFIG, 
                                    I2C_MEMADD_SIZE_8BIT, 
                                    config_reg_bytes, 
                                    TMP102_NBR_CONFIG_REG_BYTES, 
                                    TMP102_I2C_TIMEOUT)
                   == HAL_OK);

    if (!i2c_ok) 
    {
        return false;
    }

    TMP102_SyncConfigBuffer(tmp102, config_reg_bytes, READ);
    return true;
}

static  bool TMP102_write_T_LOW_T_HIGH_reg(TMP102_t *tmp102) 
{
    // Registre T_LOW
    uint8_t t_low_reg_bytes[TMP102_NBR_REG_TEMP_BYTES];
    TMP102_celsius_to_register(tmp102->T_Low, t_low_reg_bytes, tmp102->config.ExtendedMode);
    
    bool i2c_ok = (HAL_I2C_Mem_Write(tmp102->busI2C, 
                                     tmp102->address, 
                                     TMP102_REG_TLOW, 
                                     I2C_MEMADD_SIZE_8BIT, 
                                     t_low_reg_bytes,
                                     TMP102_NBR_REG_TEMP_BYTES,
                                     TMP102_I2C_TIMEOUT) 
                   == HAL_OK);
                   
    if (!i2c_ok) 
    {
        return false;
    }

    // Registre T_HIGH
    uint8_t t_high_reg_bytes[TMP102_NBR_REG_TEMP_BYTES];
    TMP102_celsius_to_register(tmp102->T_High, t_high_reg_bytes, tmp102->config.ExtendedMode);
    
    i2c_ok = (HAL_I2C_Mem_Write(tmp102->busI2C, 
                                tmp102->address, 
                                TMP102_REG_THIGH, 
                                I2C_MEMADD_SIZE_8BIT, 
                                t_high_reg_bytes,
                                TMP102_NBR_REG_TEMP_BYTES,
                                TMP102_I2C_TIMEOUT) 
              == HAL_OK);
              
    if (!i2c_ok) 
    {
        return false;
    }

    return true;
}

bool TMP102_apply_config(TMP102_t *tmp102) 
{
    // Écriture du registre de configuration d'après les valeurs stockées dans la structure
    if (!TMP102_write_config_reg(tmp102)) 
    {
        return false;
    }

    // Ecriture des registres T_LOW et T_HIGH
    if (!TMP102_write_T_LOW_T_HIGH_reg(tmp102)) 
    {
        return false;
    }

    return true;
}

static bool TMP102_read_temperature_register(TMP102_t *tmp102, float *Temperature) 
{
    uint8_t buffer_temp_reg[TMP102_NBR_REG_TEMP_BYTES] = {0};

    bool i2c_ok = (HAL_I2C_Mem_Read(tmp102->busI2C, 
                                    tmp102->address, 
                                    TMP102_REG_TEMP, 
                                    I2C_MEMADD_SIZE_8BIT, 
                                    buffer_temp_reg, 
                                    TMP102_NBR_REG_TEMP_BYTES, 
                                    TMP102_I2C_TIMEOUT)
                   == HAL_OK);

    if (!i2c_ok) 
    {
        *Temperature = -999.0f;
        return false;
    }

    TMP102_register_to_celsius(buffer_temp_reg, Temperature, tmp102->config.ExtendedMode);

    return true;
}

// Démarre une conversion One-Shot, uniquement si en mode shutdown
bool TMP102_start_measure(TMP102_t *tmp102) 
{
    if (tmp102->config.Mode != TMP102_MODE_SHUTDOWN) 
    {
        return false;
    }

    tmp102->is_measuring_one_shot = true;

    tmp102->config.OneShot = TMP102_OS_START_MEASURE;
    return (TMP102_write_config_reg(tmp102));
}

bool TMP102_is_measure_ready(TMP102_t *tmp102) 
{
    if (!tmp102->is_measuring_one_shot) 
    {
        return false;
    }
    if (tmp102->config.Mode != TMP102_MODE_SHUTDOWN) 
    {
        return false;
    }
    if (!TMP102_read_config_reg(tmp102)) 
    {
        return false;
    }
    if (tmp102->config.OneShot == TMP102_OS_MEASURING) 
    {
        return false;
    }
    tmp102->is_measuring_one_shot = false;
    return true;
}

bool TMP102_get_temperature(TMP102_t *tmp102, float *Temperature) 
{
    // Si appel en mode shutdown : 10ms bloquantes le temps de la mesure
    if (tmp102->config.Mode == TMP102_MODE_SHUTDOWN) 
    {
        if (!TMP102_start_measure(tmp102)) 
        {
            return false;
        }

        while (!TMP102_is_measure_ready(tmp102))
        {
            HAL_Delay(1);
        }
    }

    return TMP102_read_temperature_register(tmp102, Temperature);
}

char* TMP102_temperature_to_string(float temperature, uint8_t precision) 
{
    static char buffer[16];
    static const int multi_table[] = {1, 10, 100, 1000, 10000};

    // Precision maximale = 4
    if (precision > 4) 
    {
        precision = 4;
    }

    // Gestion du signe
    bool negatif = (temperature < 0.0f);
    float temp_abs = negatif ? -temperature : temperature;

    int multiplicateur = multi_table[precision];
    
    // Décomposition en partie entière et décimale arrondie
    int partie_entiere = (int)temp_abs;
    int partie_decimale = (int)((temp_abs - (float)partie_entiere) * (float)multiplicateur + 0.5f);

    // Propagation de la retenue si l'arrondi déborde
    if (partie_decimale >= multiplicateur) 
    {
        partie_entiere++;
        partie_decimale = 0;
    }

    snprintf(buffer, sizeof(buffer), "%s%d,%0*d", 
             negatif ? "-" : "", 
             partie_entiere, 
             precision, 
             partie_decimale);

    return buffer;
}