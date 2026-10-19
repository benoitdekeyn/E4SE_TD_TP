#include "bande-led.h"
#include <stdlib.h>

void BandeLED_Init(BandeLED_t* bande_led, TIM_HandleTypeDef *htim, uint32_t timer_channel, uint32_t freq_timer, uint8_t nb_leds)
{
    bande_led->cycles_delai_maj = (BL_DELAI_MAJ_nS / (BL_NSEC_IN_1_SEC / BL_FREQ_CYCLE_PWM)) + 1;
    bande_led->PWM_timer        = htim;
    bande_led->PWM_channel      = timer_channel;
    bande_led->dma_buffer       = malloc((nb_leds * 3 * 8 + bande_led->cycles_delai_maj) * sizeof(uint32_t));
    bande_led->freq_timer       = freq_timer;
    bande_led->nb_leds          = nb_leds;
    bande_led->leds             = malloc(nb_leds * sizeof(BL_RGB_t));
    bande_led->power            = false;

    for (int i = 0; i < nb_leds; i++)
    {
        bande_led->leds[i].rouge = 0;
        bande_led->leds[i].vert  = 0;
        bande_led->leds[i].bleu  = 0;
        bande_led->leds[i].alpha = 255;
        bande_led->leds[i].power = true;
    }
    BandeLED_show(bande_led);
    BandeLED_allumer(bande_led);

}

bool BandeLED_show(BandeLED_t* bande_led)
{
    int nS_per_timer_tic     = BL_NSEC_IN_1_SEC  / bande_led->freq_timer;
    int pulse_bit_0_tics     = BL_PULSE_BIT_0_nS / nS_per_timer_tic;
    int pulse_bit_1_tics     = BL_PULSE_BIT_1_nS / nS_per_timer_tic;
    int nb_cycles_delai_maj = BL_DELAI_MAJ_nS / (BL_NSEC_IN_1_SEC / BL_FREQ_CYCLE_PWM) + 1; // +1 car le led attend >= BL_DELAI_MAJ_nS

    uint32_t dma_index = 0;

    for (int led_index = 0; led_index < bande_led->nb_leds; led_index++)
    {
        BL_RGB_t* led_courante = &bande_led->leds[led_index];
        for (int composante = 1; composante <= 3; composante++)
        {
            uint8_t valeur_composante = 0;
            switch (composante) 
            {
                case BL_ROUGE_SEQ_RANG :
                    valeur_composante = led_courante->rouge * led_courante->alpha / 255;
                    break;
                case BL_VERT_SEQ_RANG :
                    valeur_composante = led_courante->vert * led_courante->alpha / 255;
                    break;
                case BL_BLEU_SEQ_RANG :
                    valeur_composante = led_courante->bleu * led_courante->alpha / 255;
                    break;
            }
            uint8_t valeur_a_envoyer = (bande_led->power && led_courante->power) ? valeur_composante : 0;
            
            for (int bit = 8-1; bit >= 0; bit--)
            {
                uint32_t valeur_pulse = (valeur_a_envoyer & (1 << bit)) ? pulse_bit_1_tics : pulse_bit_0_tics;
                bande_led->dma_buffer[dma_index] = valeur_pulse;
                dma_index++;
            }
        }
    }
    
    for (int i = 0; i < nb_cycles_delai_maj; i++) {
        bande_led->dma_buffer[dma_index] = 0;
        dma_index++;
    }
    
    HAL_StatusTypeDef status = HAL_TIM_PWM_Start_DMA(bande_led->PWM_timer, bande_led->PWM_channel, bande_led->dma_buffer, dma_index);

    return (status == HAL_OK);
}

void BandeLED_allumer(BandeLED_t* bande_led)
{
    bande_led->power = true;
    BandeLED_show(bande_led);
}

void BandeLED_eteindre(BandeLED_t* bande_led)
{
    bande_led->power = false;
    BandeLED_show(bande_led);
}

void BandeLED_set_led_state(BandeLED_t* bande_led, int led_id, bool state)
{
    if (led_id >= 0 && led_id < bande_led->nb_leds)
    {
        bande_led->leds[led_id].power = state;
    }
}

void BandeLED_set_led_hexa(BandeLED_t* bande_led, int led_id, uint32_t hexa)
{
    if (led_id >= 0 && led_id < bande_led->nb_leds)
    {
        bande_led->leds[led_id].rouge = (hexa >> 16) & 0xFF;
        bande_led->leds[led_id].vert  = (hexa >> 8) & 0xFF;
        bande_led->leds[led_id].bleu  = hexa & 0xFF;
    }
}

void BandeLED_set_led_rgb(BandeLED_t* bande_led, int led_id, uint8_t rouge, uint8_t vert, uint8_t bleu)
{
    if (led_id >= 0 && led_id < bande_led->nb_leds)
    {
        bande_led->leds[led_id].rouge = rouge;
        bande_led->leds[led_id].vert  = vert;
        bande_led->leds[led_id].bleu  = bleu;
    }
}

void BandeLED_set_led_luminosity(BandeLED_t* bande_led, int led_id, uint8_t luminosity)
{
    if (led_id >= 0 && led_id < bande_led->nb_leds)
    {
        bande_led->leds[led_id].alpha = luminosity;
    }
}

// Fonction utilitaire : convertit une valeur de 0 à 255 en couleur RGB
static uint32_t Roue_Chromatique(uint8_t position) {
    position = 255 - position;
    if(position < 85) {
        return ((255 - position * 3) << 16) | (0 << 8) | (position * 3);
    }
    if(position < 170) {
        position -= 85;
        return (0 << 16) | ((position * 3) << 8) | (255 - position * 3);
    }
    position -= 170;
    return ((position * 3) << 16) | ((255 - position * 3) << 8) | 0;
}

// Fonction de démo non bloquante appelée à 20 Hz
bool BandeLED_demo(BandeLED_t* bande) {
    static uint32_t tick = 0;
    tick++;

    // --- 1. LEDs 0 et 1 : Arc-en-ciel à vitesses différentes ---
    uint8_t pos_led0 = (tick * 2) % 256; // Tourne doucement
    uint8_t pos_led1 = (tick * 6) % 256; // Tourne 3x plus vite

    BandeLED_set_led_hexa(bande, 0, Roue_Chromatique(pos_led0));
    BandeLED_set_led_hexa(bande, 1, Roue_Chromatique(pos_led1));

    // --- 2. LEDs 2 et 3 : Flash Blanc à fréquences différentes ---
    // À 20 Hz : 10 ticks = 0.5s. 
    
    // LED 2 : Clignote à ~2 Hz (5 ticks allumée, 5 ticks éteinte)
    if ((tick % 10) < 5) {
        BandeLED_set_led_hexa(bande, 2, BL_HEXA_BLANC);
    } else {
        BandeLED_set_led_hexa(bande, 2, BL_HEXA_NOIR);
    }

    // LED 3 : Clignote à ~5 Hz (2 ticks allumée, 2 ticks éteinte)
    if ((tick % 4) < 2) {
        BandeLED_set_led_hexa(bande, 3, BL_HEXA_BLANC);
    } else {
        BandeLED_set_led_hexa(bande, 3, BL_HEXA_NOIR);
    }

    // --- 3. Envoi physique vers le DMA ---
    return BandeLED_show(bande);
}