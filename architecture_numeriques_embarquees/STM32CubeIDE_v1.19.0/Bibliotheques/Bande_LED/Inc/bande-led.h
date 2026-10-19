#ifndef _BANDE_LED_H
#define _BANDE_LED_H

#include <main.h>
#include <stdbool.h>

// Couleurs predefinies
#define HEXA_BLANC  0xFFFFFF
#define HEXA_NOIR   0x000000

#define HEXA_BLEU   0x0000FF
#define HEXA_ROUGE  0xFF0000
#define HEXA_VERT   0x00FF00

#define HEXA_JAUNE  0xFFFF00
#define HEXA_CYAN   0x00FFFF
#define HEXA_MAGENTA 0xFF00FF

#define HEXA_ORANGE 0xFF8000
#define HEXA_VIOLET 0x8000FF
#define HEXA_ROSE   0xFF0080
#define HEXA_MARRON 0x804000
#define HEXA_CIEL   0x800000

// Paramtres datasheet bande led
#define FREQ_CYCLE_PWM 800000
#define PULSE_BIT_0_nS 400
#define PULSE_BIT_1_nS 800
#define DELAI_MAJ_nS   500000
#define NSEC_IN_1_SEC  1000000000

typedef struct {
    uint8_t rouge;
    uint8_t vert;
    uint8_t bleu;
    uint8_t alpha;
    bool    power;
} BL_RGB_t;

typedef struct {
    bool                    power;
    int                     nb_leds;
    BL_RGB_t*               leds;
    __TIM_HandleTypeDef*    PWM_timer;
    uint32_t*               dma_buffer;
    int                     freq_timer;
} BandeLED_t;

void BandeLED_Init(BandeLED_t* bande_led, __TIM_HandleTypeDef *htim, uint32_t *dma_buffer, uint32_t freq_timer, uint8_t nb_leds);
void BandeLED_allumer(BandeLED_t* bande_led);
void BandeLED_eteindre(BandeLED_t* bande_led);
void BandeLED_set_led_state(BandeLED_t* bande_led, int* led_ids, bool state);
void BandeLED_set_led_hexa(BandeLED_t* bande_led, int* led_ids, uint32_t hexa);
void BandeLED_set_led_rgb(BandeLED_t* bande_led, int* led_ids, uint8_t r, uint8_t g, uint8_t b);
void BandeLED_set_led_luminosity(BandeLED_t* bande_led, int* led_ids, uint8_t luminosity);

#endif