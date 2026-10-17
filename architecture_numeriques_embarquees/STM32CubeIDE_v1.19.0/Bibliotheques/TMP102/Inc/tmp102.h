#ifndef TMP102_H
#define TMP102_H

#include "main.h"
#include <stdio.h>
#include "stdbool.h"

#define TMP102_NBR_CONFIG_REG_BYTES 2
#define TMP102_I2C_TIMEOUT          100

// ADRESSES I2C
#define TMP102_I2C_ADDR_GND         (0x48 << 1)
#define TMP102_I2C_ADDR_VCC         (0x49 << 1)
#define TMP102_I2C_ADDR_SDA         (0x4A << 1)
#define TMP102_I2C_ADDR_SCL         (0x4B << 1)

// REGISTRES INTERNES
#define TMP102_REG_TEMP             0x00
#define TMP102_REG_CONFIG           0x01

// VALEURS DES REGLAGES
#define TMP102_MODE_CONTINUOUS      0x00
#define TMP102_MODE_SHUTDOWN        0x01

#define TMP102_TM_COMPARATOR        0x00
#define TMP102_TM_INTERRUPT         0x01

#define TMP102_POL_ACTIVE_LOW       0x00
#define TMP102_POL_ACTIVE_HIGH      0x01

#define TMP102_FAULT_1              0x00
#define TMP102_FAULT_2              0x01
#define TMP102_FAULT_4              0x02
#define TMP102_FAULT_6              0x03

#define TMP102_RATE_0_25HZ          0x00
#define TMP102_RATE_1HZ             0x01
#define TMP102_RATE_4HZ             0x02
#define TMP102_RATE_8HZ             0x03

#define TMP102_EXTENDED_OFF         0x00 
#define TMP102_EXTENDED_ON          0x01 

// VALEURS PAR DEFAUT DES REGISTRES INTERNES
#define TMP102_OS_DEFAULT           0x00  
#define TMP102_R_DEFAULT            0x03  
#define TMP102_AL_DEFAULT           0x01  
#define TMP102_PADD_DEFAULT         0x00  

// INTERPRETATION REGISTRE TEMPERATURE
#define TMP102_TEMP_RESOLUTION      0.0625f
#define TMP102_NORM_BIT_LEN         12
#define TMP102_EXT_BIT_LEN          13


typedef struct {
    uint8_t Mode;
    uint8_t ConversionRate;
    uint8_t ExtendedMode;
    uint8_t ThermostatMode;
    uint8_t AlertPolarity;
    uint8_t FaultQueue;
} TMP102_config_t;

typedef struct {
    I2C_HandleTypeDef   *busI2C;
    uint8_t             address;
    TMP102_config_t     config;
} TMP102_t;

bool TMP102_init(TMP102_t *htmp);
bool TMP102_read_temperature(TMP102_t *htmp, float *Temperature);
char* TMP102_temperature_to_string(float temperature, uint8_t precision);

#endif
