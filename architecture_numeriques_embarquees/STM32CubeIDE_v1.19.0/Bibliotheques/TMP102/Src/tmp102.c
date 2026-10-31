#include "tmp102.h"

/**
 * @brief  Initialise l'objet TMP102 avec les réglages par défaut.
 *         Le bus I2C reste à NULL : il doit être renseigné par l'appelant avant d'appeler TMP102_apply_registers().
 *         // cf rapport : Scénario 6 – 6.2. Valeurs par défaut
 * @param  tmp102 Pointeur vers l'objet TMP102 à initialiser
 * @return None
 */
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

/**
 * @brief  Synchronise le buffer du registre de configuration (2 octets) avec la structure TMP102_Config_t.
 *         Une seule boucle générique parcourt la table de mapping des champs.
 *         En écriture, le buffer doit être initialisé à 0 car l'encodage se fait par OU binaire.
 *         // cf rapport : Scénario 6 – 4.4. Organisation des champs du registre de configuration
 *         // cf rapport : Scénario 6 – 4.5. Synchronisation entre le registre de configuration et l'objet TMP102
 * @param  tmp102   Pointeur vers l'objet TMP102
 * @param  buffer   Buffer du registre de configuration (2 octets)
 * @param  is_write WRITE : structure -> buffer, READ : buffer -> structure
 * @return None
 */
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

    // Parcours de tous les champs pour la synchronisation entre la structure et le buffer
    for (uint8_t i = 0; i < num_fields; i++) 
    {
        // Masque de la taille du champ (ex : 2 bits -> 0b11)
        uint8_t mask = (1U << fields[i].length) - 1;
        
        if (is_write) 
        {
            // Encode : Structure -> Buffer (masque, décalage à la position du champ, puis OU binaire)
            // Cf rapport : Scénario 6 – 4.5. Synchronisation entre le registre de configuration et l'objet TMP102
            buffer[fields[i].byte_idx] |= (*fields[i].value_ptr & mask) << fields[i].position;
        } 
        else 
        {
            // Décode : Buffer -> Structure (décalage vers le LSB, puis masque)
            // Cf rapport : Scénario 6 – 4.5. Synchronisation entre le registre de configuration et l'objet TMP102
            *fields[i].value_ptr = (buffer[fields[i].byte_idx] >> fields[i].position) & mask;
        }
    }
}

/**
 * @brief  Convertit une température en °C vers la valeur brute du registre (2 octets, aligné à gauche).
 *         // cf rapport : Scénario 6 – 8.2. Température en °C → Registre brut
 * @param  temperature   Température en °C
 * @param  reg_value     Buffer de sortie (2 octets : octet de poids fort puis de poids faible)
 * @param  extended_mode true : mode étendu (13 bits), false : mode normal (12 bits)
 * @return None
 */
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

/**
 * @brief  Convertit la valeur brute du registre de température (complément à 2) en °C.
 *         // cf rapport : Scénario 6 – 8.1. Registre brut → température en °C
 * @param  reg_value   Octets lus dans le registre de température (2 octets)
 * @param  temperature Pointeur vers la température en °C (sortie)
 * @return None
 */
static void TMP102_register_to_celsius(uint8_t *reg_value, float *temperature) 
{
    // Sur combien de bits est codée la température
    // En mode étendu, le bit D0 (LSB) du second octet vaut 1
    bool extended_mode = (reg_value[1] & 0x01) != 0;
    uint8_t temp_length = (extended_mode) 
                        ? TMP102_EXT_BIT_LEN 
                        : TMP102_NORM_BIT_LEN;
    
    // Concaténation des 2 bytes reçus en type entier
    uint16_t raw_reg = (uint16_t)((reg_value[0] << 8) | reg_value[1]);
    
    // Décalage à droite avec extension du signe
    int16_t reg_val = (int16_t)raw_reg >> (16 - temp_length);
    
    // Conversion de la valeur du capteur en degrés Celsius (1 LSB = 0,0625 °C)
    *temperature = (float)reg_val * TMP102_TEMP_RESOLUTION;
}

/**
 * @brief  Écrit le registre de configuration du capteur d'après les réglages de l'objet TMP102.
 *         // cf rapport : Scénario 6 – 5.1. Application du registre de configuration
 * @param  tmp102 Pointeur vers l'objet TMP102
 * @return true si la transaction I2C a réussi, false sinon
 */
static bool TMP102_write_config_reg(TMP102_t *tmp102) 
{
    // Initialisation du buffer de configuration à zéro
    // (important pour les bitwise operations de la fonction TMP102_SyncConfigBuffer)
    uint8_t config_reg_bytes[TMP102_NBR_CONFIG_REG_BYTES] = {0};

    // Synchronisation du buffer de configuration avec l'objet TMP102 avant l'écriture
    TMP102_SyncConfigBuffer(tmp102, config_reg_bytes, WRITE);

    // Écriture du buffer de configuration sur le capteur via I2C
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
    // Si l'écriture I2C a réussi, on retourne true
    return true;
}

/**
 * @brief  Lit le registre de configuration du capteur et met à jour les réglages de l'objet TMP102.
 *         Utilisée pour lire le bit OS lors d'une mesure One-Shot.
 *         // cf rapport : Scénario 6 – 5.1. Application du registre de configuration
 *         // cf rapport : Scénario 6 – 9.3. Attendre la fin de la mesure
 * @param  tmp102 Pointeur vers l'objet TMP102
 * @return true si la transaction I2C a réussi, false sinon
 */
static bool TMP102_read_config_reg(TMP102_t *tmp102) 
{
    // Initialisation du buffer de configuration pour la lecture
    uint8_t config_reg_bytes[TMP102_NBR_CONFIG_REG_BYTES] = {0};

    // Lecture du registre de configuration du capteur via I2C
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

    // Synchronisation du buffer de configuration avec l'objet TMP102 après la lecture
    TMP102_SyncConfigBuffer(tmp102, config_reg_bytes, READ);
    return true;
}

/**
 * @brief  Écrit les registres de seuil T_LOW puis T_HIGH, convertis en valeurs brutes.
 *         // cf rapport : Scénario 6 – 5.2. Application des registres de seuil de température
 * @param  tmp102 Pointeur vers l'objet TMP102
 * @return true si les deux écritures I2C ont réussi, false sinon
 */
static  bool TMP102_write_T_LOW_T_HIGH_reg(TMP102_t *tmp102) 
{
    // Registre T_LOW

    // Conversion de la température T_LOW en valeurs brutes pour le registre
    uint8_t t_low_reg_bytes[TMP102_NBR_REG_TEMP_BYTES];
    TMP102_celsius_to_register(tmp102->T_Low, t_low_reg_bytes, tmp102->config.ExtendedMode);
    
    // Écriture du registre T_LOW sur le capteur via I2C
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

    // Conversion de la température T_HIGH en valeurs brutes pour le registre
    uint8_t t_high_reg_bytes[TMP102_NBR_REG_TEMP_BYTES];
    TMP102_celsius_to_register(tmp102->T_High, t_high_reg_bytes, tmp102->config.ExtendedMode);
    
    // Écriture du registre T_HIGH sur le capteur via I2C
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

/**
 * @brief  Applique tous les réglages de l'objet TMP102 au capteur (registre de configuration, puis T_LOW et T_HIGH).
 *         // cf rapport : Scénario 6 – 5.1. Application du registre de configuration
 *         // cf rapport : Scénario 6 – 5.2. Application des registres de seuil de température
 * @param  tmp102 Pointeur vers l'objet TMP102
 * @return true si toutes les écritures ont réussi, false sinon
 */
bool TMP102_apply_registers(TMP102_t *tmp102) 
{
    // Écriture du registre de configuration d'après les valeurs stockées dans la structure
    if (!TMP102_write_config_reg(tmp102)) 
    {
        return false;
    }

    // Écriture des registres T_LOW et T_HIGH
    if (!TMP102_write_T_LOW_T_HIGH_reg(tmp102)) 
    {
        return false;
    }

    return true;
}

/**
 * @brief  Lit le registre de température et le convertit en °C.
 *         En cas d'erreur I2C, *Temperature vaut -999.0f.
 *         // cf rapport : Scénario 6 – 7.1. Fonction de lecture
 * @param  tmp102      Pointeur vers l'objet TMP102
 * @param  Temperature Pointeur vers la température en °C (sortie)
 * @return true si la lecture I2C a réussi, false sinon
 */
static bool TMP102_read_temperature_register(TMP102_t *tmp102, float *Temperature) 
{
    // Initialisation du buffer de lecture du registre de température
    // Mise à zéro du buffer avant la lecture pour la fonction TMP102_register_to_celsius
    uint8_t buffer_temp_reg[TMP102_NBR_REG_TEMP_BYTES] = {0};

    // Lecture du registre de température via I2C
    bool i2c_ok = (HAL_I2C_Mem_Read(tmp102->busI2C, 
                                    tmp102->address, 
                                    TMP102_REG_TEMP, 
                                    I2C_MEMADD_SIZE_8BIT, 
                                    buffer_temp_reg, 
                                    TMP102_NBR_REG_TEMP_BYTES, 
                                    TMP102_I2C_TIMEOUT)
                   == HAL_OK);

    // Vérification du succès de la lecture I2C
    if (!i2c_ok) 
    {
        // En cas d'erreur I2C, on écrit une valeur aberrante et retourne false
        *Temperature = -999.0f;
        return false;
    }

    // Conversion des valeurs brutes du registre en température en °C
    TMP102_register_to_celsius(buffer_temp_reg, Temperature);

    return true;
}

/**
 * @brief  Démarre une conversion One-Shot, uniquement si le capteur est en mode Shutdown.
 *         Positionne le flag interne de mesure en cours, puis écrit le bit OS à 1.
 *         // cf rapport : Scénario 6 – 9.1. Mode Continuous vs Shutdown
 *         // cf rapport : Scénario 6 – 9.2. Déclencher une mesure One-Shot
 * @param  tmp102 Pointeur vers l'objet TMP102
 * @return true si la mesure a été déclenchée, false sinon
 */
bool TMP102_start_measure(TMP102_t *tmp102) 
{
    // Vérification que le capteur est en mode Shutdown avant de démarrer une mesure One-Shot
    if (tmp102->config.Mode != TMP102_MODE_SHUTDOWN) 
    {
        return false;
    }

    // Positionnement du flag interne de mesure en cours
    tmp102->is_measuring_one_shot = true;

    // Écriture du bit OS à 1 pour démarrer la mesure One-Shot
    tmp102->config.OneShot = TMP102_OS_START_MEASURE;
    return (TMP102_write_config_reg(tmp102));
}

/**
 * @brief  Teste (sans bloquer) si la mesure One-Shot est terminée : le bit OS vaut 0 pendant la mesure et 1 une fois terminée.
 *         À appeler périodiquement (polling) jusqu'à obtenir true.
 *         // cf rapport : Scénario 6 – 9.3. Attendre la fin de la mesure
 * @param  tmp102 Pointeur vers l'objet TMP102
 * @return true si la mesure est terminée, false si elle est en cours ou en cas d'erreur
 */
bool TMP102_is_measure_ready(TMP102_t *tmp102) 
{
    // Vérification que la mesure One-Shot est en cours
    if (!tmp102->is_measuring_one_shot) 
    {
        return false;
    }
    // Vérification que le capteur est toujours en mode Shutdown
    if (tmp102->config.Mode != TMP102_MODE_SHUTDOWN) 
    {
        return false;
    }
    // Lecture du registre de configuration pour pouvoir vérifier le bit OS
    if (!TMP102_read_config_reg(tmp102)) 
    {
        return false;
    }
    // Vérification de si la mesure est toujours en cours (bit OS = 0)
    if (tmp102->config.OneShot == TMP102_OS_MEASURING) 
    {
        return false;
    }
    // La mesure est terminée (bit OS = 1)
    // donc on met à jour le flag interne et retourne true
    tmp102->is_measuring_one_shot = false;
    return true;
}

/**
 * @brief  Récupère la température mesurée par le capteur, en °C.
 *         // cf rapport : Scénario 6 – 7.2. Appel externe
 *         // cf rapport : Scénario 6 – 9.4. Lire la température actualisée
 * @param  tmp102      Pointeur vers l'objet TMP102
 * @param  Temperature Pointeur vers la température en °C (sortie, -999.0f en cas d'erreur)
 * @return true si la lecture a réussi, false sinon
 */
bool TMP102_get_temperature(TMP102_t *tmp102, float *Temperature) 
{
    // Lecture de la température depuis le registre du capteur
    return TMP102_read_temperature_register(tmp102, Temperature);
}

/**
 * @brief  Convertit une température float en chaîne de caractères (séparateur décimal : virgule), sans utiliser %f.
 *         Le buffer est statique : le résultat est écrasé à l'appel suivant.
 *         // cf rapport : Scénario 6 – 8.3. Affichage de la température de type float
 * @param  temperature Température en °C
 * @param  precision   Nombre de décimales (limité à 4)
 * @return Pointeur vers la chaîne statique (16 caractères maximum)
 */
char* TMP102_temperature_to_string(float temperature, uint8_t precision) 
{
    // Création du buffer statique pour la chaîne de caractères de la température
    // Au pire la string ressemble à ça : +-XXX,XXXX ce qui donne un nombre max de caractères 
    // = signe + max 3 chiffres pour la partie entière + virgule + max 4 chiffres pour la partie décimale + '\0' 
    // = max 14 caractères
    static char buffer[14];
    // Table des puissances de 10 pour décaler la partie décimale
    static const int multi_table[] = {1, 10, 100, 1000, 10000};

    // Précision maximale = 4
    // Cf. rapport : Scénario 6 – 8.3. Affichage de la température de type float
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

    // Propagation de la retenue si l'arrondi déborde (Exemple : 18,997 arrondi à 19,00)
    if (partie_decimale >= multiplicateur) 
    {
        partie_entiere++;
        partie_decimale = 0;
    }
    
    // Formatage de la chaîne de caractères avec snprintf
    // %s : signe
    // %d : partie entière
    // %0*d : partie décimale avec zéro-padding pour une précision variable
    snprintf(buffer, sizeof(buffer), "%s%d,%0*d", 
             negatif ? "-" : "", 
             partie_entiere, 
             precision, 
             partie_decimale);

    return buffer;
}