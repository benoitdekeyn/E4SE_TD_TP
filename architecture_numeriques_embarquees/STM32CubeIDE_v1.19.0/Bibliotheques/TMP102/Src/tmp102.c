#include "tmp102.h"

typedef struct {
    uint8_t byte_idx;
    uint8_t position;
    uint8_t length;
    uint8_t temp_absue;
} TMP102_Field_t;

bool TMP102_init(TMP102_t *tmp102) {
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
        |=              (    fields[i].temp_absue & mask) 
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

bool TMP102_read_temperature(TMP102_t *tmp102, float *Temperature) {
    uint8_t buffer_temp_reg[TMP102_NBR_CONFIG_REG_BYTES] = {0};
    int16_t raw_temp_absue;

    bool i2c_ok = (HAL_I2C_Mem_Read(tmp102->busI2C, 
                                     tmp102->address, 
                                     TMP102_REG_TEMP, 
                                     I2C_MEMADD_SIZE_8BIT, 
                                     buffer_temp_reg, 
                                     TMP102_NBR_CONFIG_REG_BYTES, 
                                     TMP102_I2C_TIMEOUT)
                    == HAL_OK);

    if (!i2c_ok) {
        *Temperature = -999.0f;
        return false;
    }

    // Sur combien de bits est codée la température
    uint8_t temp_length = (tmp102->config.ExtendedMode == TMP102_EXTENDED_ON) 
                        ? TMP102_EXT_BIT_LEN 
                        : TMP102_NORM_BIT_LEN;

    // Concaténation des 2 bytes reçus en type entier
    raw_temp_absue = (int16_t)((buffer_temp_reg[0] << 8) | buffer_temp_reg[1]);

    // Décalage à droite avec extension du signe
    raw_temp_absue >>= (16 - temp_length);

    // conversion de la temp_abseur du capteur en degres celsius
    *Temperature = (float)raw_temp_absue * TMP102_TEMP_RESOLUTION;

    return true;
}

char* TMP102_temperature_to_string(float temperature, uint8_t precision) 
{
    static char buffer[16];
    static const int multi_table[] = {1, 10, 100, 1000, 10000};

    // Precision maximale = 4
    if (precision > 4) {
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
    if (partie_decimale >= multiplicateur) {
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