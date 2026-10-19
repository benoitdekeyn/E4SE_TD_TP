#include "bande-led.h"

static void BandeLED_maj_materielle(BandeLED_t* bande_led)
{
    // Prise en comptes des differents parametres de luminosité en mettant alpha à 255 apres avoir evalué la luminosité directement dans les composantes RGB
    BL_RGB_t leds_alpha_applied[bande_led->nb_leds];
    for (int i = 0; i < bande_led->nb_leds; i++)
    {
        leds_alpha_applied[i].rouge = bande_led->leds[i].rouge * bande_led->leds[i].alpha / 255;
        leds_alpha_applied[i].vert = bande_led->leds[i].vert * bande_led->leds[i].alpha / 255;
        leds_alpha_applied[i].bleu = bande_led->leds[i].bleu * bande_led->leds[i].alpha / 255;
        leds_alpha_applied[i].alpha = 255;
    }

    // ecriture du DMA
    int nS_per_pulse_tic = NSEC_IN_1_SEC / bande_led->freq_timer;
    int pulse_bit_0_tics = PULSE_BIT_0_nS / nS_per_pulse_tic;
    int pulse_bit_1_tics = PULSE_BIT_1_nS / nS_per_pulse_tic;

    uint8_t rouge = 0;
    uint8_t vert = 0;
    uint8_t bleu = 0;

    uint32_t dma_index = 0;
    for (int led_index = 0; led_index < bande_led->nb_leds; led_index++)
    {
        // Ordre des composantes attendu par les LED : vert, rouge, bleu.
        for (int couleur = 0; couleur < 3; couleur++)
        {
            uint8_t composante = 0;
            if (bande_led->power && bande_led->leds[led_index].power)
            {
                
            }

            for (int bit = 7; bit >= 0; bit--)
            {
                bande_led->dma_buffer[dma_index++] =
                    (composante & (1U << bit)) ? pulse_bit_1_tics : pulse_bit_0_tics;
            }
        }
    }
    
    for (int i = 0; i < DELAI_MAJ_nS/nS_per_pulse_tic; i++)
    {
        bande_led->dma_buffer[dma_index++] = 0;
    }

    // TODO : lancer le transfert DMA avec le buffer rempli
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