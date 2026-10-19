#include "bande-led.h"

static void BandeLED_maj_materielle(BandeLED_t* bande_led)
{
    // TODO
}

void BandeLED_Init(BandeLED_t* bande_led, __TIM_HandleTypeDef *htim, uint32_t *dma_buffer, uint32_t freq_timer, uint8_t nb_leds)
{
    bande_led->PWM_timer = htim;
    bande_led->dma_buffer = dma_buffer;
    bande_led->freq_timer = freq_timer;
    bande_led->nb_leds = nb_leds;
    bande_led->leds = malloc(nb_leds * sizeof(BL_RGB_t));
    bande_led->power = false;
    for (int i = 0; i < nb_leds; i++)
    {
        bande_led->leds[i].rouge = 0;
        bande_led->leds[i].vert = 0;
        bande_led->leds[i].bleu = 0;
        bande_led->leds[i].alpha = 0;
    }
    BandeLED_maj_materielle(bande_led);
}
void BandeLED_allumer(BandeLED_t* bande_led)
{
    bande_led->power = true;
    BandeLED_maj_materielle(bande_led);
}
void BandeLED_eteindre(BandeLED_t* bande_led)
{
    bande_led->power = false;
    BandeLED_maj_materielle(bande_led);
}
void BandeLED_set_led_state(BandeLED_t* bande_led, int* led_ids, bool state)
{
    for (int i = 0; i < sizeof(led_ids) / sizeof(led_ids[0]); i++)
    {
        if (led_ids[i] >= 0 && led_ids[i] < bande_led->nb_leds)
        {
            bande_led->leds[led_ids[i]].power = state;
        }
    }
}
void BandeLED_set_led_hexa(BandeLED_t* bande_led, int* led_ids, uint hexa)
{
    for (int i = 0; i < sizeof(led_ids) / sizeof(led_ids[0]); i++)
    {
        if (led_ids[i] >= 0 && led_ids[i] < bande_led->nb_leds)
        {
            bande_led->leds[led_ids[i]].rouge = (hexa >> 16) & 0xFF;
            bande_led->leds[led_ids[i]].vert = (hexa >> 8) & 0xFF;
            bande_led->leds[led_ids[i]].bleu = hexa & 0xFF;
        }
    }
}
void BandeLED_set_led_rgb(BandeLED_t* bande_led, int* led_ids, uint8_t rouge, uint8_t vert, uint8_t bleu)
{
    for (int i = 0; i < sizeof(led_ids) / sizeof(led_ids[0]); i++)
    {
        if (led_ids[i] >= 0 && led_ids[i] < bande_led->nb_leds)
        {
            bande_led->leds[led_ids[i]].rouge = rouge;
            bande_led->leds[led_ids[i]].vert = vert;
            bande_led->leds[led_ids[i]].bleu = bleu;
        }
    }
}
void BandeLED_set_led_luminosity(BandeLED_t* bande_led, int* led_ids, uint8_t luminosity)
{
    for (int i = 0; i < sizeof(led_ids) / sizeof(led_ids[0]); i++)
    {
        if (led_ids[i] >= 0 && led_ids[i] < bande_led->nb_leds)
        {
            bande_led->leds[led_ids[i]].alpha = luminosity;
        }
    }
}