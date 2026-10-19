#ifndef _BANDE_LED_H
#define _BANDE_LED_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

// Couleurs predefinies
#define BL_HEXA_BLANC  0xFFFFFF
#define BL_HEXA_NOIR   0x000000

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

// Paramtres datasheet bande led
#define BL_FREQ_CYCLE_PWM 800000
#define BL_PULSE_BIT_0_nS 400
#define BL_PULSE_BIT_1_nS 800
#define BL_DELAI_MAJ_nS   500000
#define BL_NSEC_IN_1_SEC  1000000000
#define BL_VERT_SEQ_RANG  1
#define BL_ROUGE_SEQ_RANG 2
#define BL_BLEU_SEQ_RANG  3

typedef struct {
    uint8_t rouge;
    uint8_t vert;
    uint8_t bleu;
    uint8_t alpha;
    bool    power;
} BL_RGB_t;

typedef struct {
    bool                power;
    int                 nb_leds;
    BL_RGB_t*           leds;
    TIM_HandleTypeDef*  PWM_timer;
    uint32_t            PWM_channel;
    uint32_t*           dma_buffer;
    int                 freq_timer;
} BandeLED_t;

void BandeLED_Init(BandeLED_t* bande_led, uint32_t *dma_buffer, TIM_HandleTypeDef *htim, uint32_t timer_channel, uint32_t freq_timer, uint8_t nb_leds);
void BandeLED_allumer(BandeLED_t* bande_led);
void BandeLED_eteindre(BandeLED_t* bande_led);
void BandeLED_set_led_state(BandeLED_t* bande_led, int* led_ids, bool state);
void BandeLED_set_led_hexa(BandeLED_t* bande_led, int* led_ids, uint32_t hexa);
void BandeLED_set_led_rgb(BandeLED_t* bande_led, int* led_ids, uint8_t r, uint8_t g, uint8_t b);
void BandeLED_set_led_luminosity(BandeLED_t* bande_led, int* led_ids, uint8_t luminosity);
void BandeLED_show(BandeLED_t* bande_led);

#endif