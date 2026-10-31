#ifndef _BANDE_LED_H
#define _BANDE_LED_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

// ==============================================================================
// Couleurs prédéfinies au format 0xRRGGBB, utilisables avec BandeLED_set_led_hexa()
// cf rapport : Scénario 9 – 7. Modification de l'état des LEDs
// ==============================================================================
#define BL_HEXA_BLANC  0xFFFFFF
#define BL_HEXA_NOIR   0x000000 // (LED éteinte)

#define BL_HEXA_BLEU   0x0000FF
#define BL_HEXA_ROUGE  0xFF0000
#define BL_HEXA_VERT   0x00FF00

#define BL_HEXA_JAUNE  0xFFFF00
#define BL_HEXA_CYAN   0x00FFFF
#define BL_HEXA_MAGENTA 0xFF00FF

#define BL_HEXA_ORANGE 0xFF8000
#define BL_HEXA_VIOLET 0x8000FF
#define BL_HEXA_ROSE   0xFF0080
#define BL_HEXA_MARRON 0x804000
#define BL_HEXA_CIEL   0x800000

// ==============================================================================
// Paramètres de la datasheet de la bande LED (protocole one-wire)
// cf rapport : Scénario 9 – 1. Analyse de la datasheet (protocole one-wire)
// cf rapport : Scénario 9 – 4.3. Define
// ==============================================================================
#define BL_FREQ_CYCLE_PWM 800000     /**< Fréquence d'un cycle de bit (Hz) */
#define BL_PULSE_BIT_0_nS 400        /**< Durée de l'état haut pour un bit 0 (ns) */
#define BL_PULSE_BIT_1_nS 800        /**< Durée de l'état haut pour un bit 1 (ns) */
#define BL_DELAI_MAJ_nS   50000      /**< Durée minimale du reset entre deux trames (ns) */
#define BL_NSEC_IN_1_SEC  1000000000 /**< Nombre de ns dans 1 s (évite le hardcoding) */
// Ordre d'envoi des composantes : G, R, B
// cf rapport : Scénario 9 – 6.2. Parcours des trois composantes
#define BL_VERT_SEQ_RANG  1 /**< Rang d'envoi de la composante verte */
#define BL_ROUGE_SEQ_RANG 2 /**< Rang d'envoi de la composante rouge */
#define BL_BLEU_SEQ_RANG  3 /**< Rang d'envoi de la composante bleue */

/**
 * @brief État mémorisé d'une LED : la couleur est séparée de la luminosité (alpha) et de l'interrupteur (power).
 *        // cf rapport : Scénario 9 – 4.2. Les structures `BL_RGB_t` et `BandeLED_t`
 */
typedef struct {
    uint8_t rouge; /**< Composante rouge (0-255) */
    uint8_t vert;  /**< Composante verte (0-255) */
    uint8_t bleu;  /**< Composante bleue (0-255) */
    uint8_t alpha; /**< Luminosité (0 = nulle, 255 = couleur inchangée) */
    bool    power; /**< Interrupteur individuel de la LED */
} BL_RGB_t;

/**
 * @brief Objet bande LED : regroupe l'état des LEDs et le matériel utilisé pour envoyer la trame.
 *        // cf rapport : Scénario 9 – 4.2. Les structures `BL_RGB_t` et `BandeLED_t`
 */
typedef struct {
    bool                power;            /**< Interrupteur général de la bande */
    int                 nb_leds;          /**< Nombre de LEDs de la bande */
    BL_RGB_t*           leds;             /**< Tableau de l'état de chaque LED (alloué dynamiquement) */
    TIM_HandleTypeDef*  PWM_timer;        /**< Timer générant le PWM */
    uint32_t            PWM_channel;      /**< Canal du timer utilisé */
    uint32_t*           dma_buffer;       /**< Tableau des CCR envoyés par DMA (alloué dynamiquement) */
    int                 freq_timer;       /**< Fréquence de l'horloge du timer (Hz) */
    int                 cycles_delai_maj; /**< Nombre de périodes de reset ajoutées à la fin de la trame */
} BandeLED_t;

// API PUBLIQUE
// cf rapport : Scénario 9 – 4.1. Vue d'ensemble
void BandeLED_Init(BandeLED_t* bande_led, TIM_HandleTypeDef *htim, uint32_t timer_channel, uint32_t freq_timer, uint8_t nb_leds);
void BandeLED_allumer(BandeLED_t* bande_led);
void BandeLED_eteindre(BandeLED_t* bande_led);
void BandeLED_set_led_state(BandeLED_t* bande_led, int led_id, bool state);
void BandeLED_set_led_hexa(BandeLED_t* bande_led, int led_id, uint32_t hexa);
void BandeLED_set_led_rgb(BandeLED_t* bande_led, int led_id, uint8_t r, uint8_t g, uint8_t b);
void BandeLED_set_led_luminosity(BandeLED_t* bande_led, int led_id, uint8_t luminosity);
bool BandeLED_show(BandeLED_t* bande_led);
bool BandeLED_demo(BandeLED_t* bande);

#endif