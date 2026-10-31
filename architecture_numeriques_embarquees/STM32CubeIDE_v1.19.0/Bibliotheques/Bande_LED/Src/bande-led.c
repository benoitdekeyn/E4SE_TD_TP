#include "bande-led.h"
#include <stdlib.h>

/**
 * @brief  Initialise la bande LED : enregistre le matériel, alloue les tableaux, met toutes les LEDs à noir
 *         (alpha à 255, power à true) puis envoie une première trame et allume la bande.
 *         // cf rapport : Scénario 9 – 5.1. Calcul du nombre de périodes de reset
 *         // cf rapport : Scénario 9 – 5.2. Enregistrement du matériel
 *         // cf rapport : Scénario 9 – 5.3. Allocation du tableau pour le DMA
 *         // cf rapport : Scénario 9 – 5.4. Valeurs par défaut
 *         // cf rapport : Scénario 9 – 5.5. Envoi de la première trame
 * @param  bande_led     Pointeur vers l'objet bande LED
 * @param  htim          Timer générant le PWM
 * @param  timer_channel Canal du timer utilisé
 * @param  freq_timer    Fréquence de l'horloge du timer (Hz)
 * @param  nb_leds       Nombre de LEDs de la bande
 * @return None
 */
void BandeLED_Init(BandeLED_t* bande_led, TIM_HandleTypeDef *htim, uint32_t timer_channel, uint32_t freq_timer, uint8_t nb_leds)
{
    // cf rapport : Scénario 9 – 5.1. Calcul du nombre de périodes de reset (+1 : arrondi par excès)
    bande_led->cycles_delai_maj = (BL_DELAI_MAJ_nS / (BL_NSEC_IN_1_SEC / BL_FREQ_CYCLE_PWM)) + 1;
    // cf rapport : Scénario 9 – 5.2. Enregistrement du matériel
    bande_led->PWM_timer        = htim;
    bande_led->PWM_channel      = timer_channel;
    // cf rapport : Scénario 9 – 5.3. Allocation du tableau pour le DMA (24 bits par LED + reset, 4 octets par case)
    bande_led->dma_buffer       = malloc((nb_leds * 3 * 8 + bande_led->cycles_delai_maj) * sizeof(uint32_t));
    bande_led->freq_timer       = freq_timer;
    bande_led->nb_leds          = nb_leds;
    bande_led->leds             = malloc(nb_leds * sizeof(BL_RGB_t));
    bande_led->power            = false;

    // cf rapport : Scénario 9 – 5.4. Valeurs par défaut
    for (int i = 0; i < nb_leds; i++)
    {
        bande_led->leds[i].rouge = 0;
        bande_led->leds[i].vert  = 0;
        bande_led->leds[i].bleu  = 0;
        bande_led->leds[i].alpha = 255;
        bande_led->leds[i].power = true;
    }
    // cf rapport : Scénario 9 – 5.5. Envoi de la première trame
    BandeLED_show(bande_led);
    BandeLED_allumer(bande_led);
}

/**
 * @brief  Encode l'état de toutes les LEDs dans le tableau DMA, puis lance l'envoi de la trame.
 *         Ne bloque pas : la transmission se déroule en arrière-plan.
 *         // cf rapport : Scénario 9 – 6. Encodage et envoi de la trame
 * @param  bande_led Pointeur vers l'objet bande LED
 * @return true si le DMA a bien démarré, false sinon
 */
bool BandeLED_show(BandeLED_t* bande_led)
{
    // cf rapport : Scénario 9 – 6.1. Conversion des durées en tics du timer
    int nS_per_timer_tic    = BL_NSEC_IN_1_SEC  / bande_led->freq_timer;
    int pulse_bit_0_tics    = BL_PULSE_BIT_0_nS / nS_per_timer_tic;
    int pulse_bit_1_tics    = BL_PULSE_BIT_1_nS / nS_per_timer_tic;

    // cf rapport : Scénario 9 – 6.6. Encodage de l'octet de chaque composante (index d'écriture dans le buffer DMA)
    uint32_t dma_index = 0;

    // Pour chaque NeoPixel de la bande LED
    // cf rapport : Scénario 9 – 6.2. Boucles for imbriquées
    for (int led_index = 0; led_index < bande_led->nb_leds; led_index++)
    {
        // Stockage temporaire de l'état de la LED courante pour simplifier le code ensuite
        BL_RGB_t* led_courante = &bande_led->leds[led_index];

        // Parcours des composantes dans l'ordre G, R, B (vert, rouge, bleu)
        // cf rapport : Scénario 9 – 6.3. Parcours des trois composantes
        for (int composante = 1; composante <= 3; composante++)
        {
            uint8_t valeur_composante = 0;
            switch (composante) 
            {   
                // Application de la luminosité sur la composante courante
                // cf rapport : Scénario 9 – 6.4. Application de la luminosité
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

            // On vérifie si la LED doit être allumée ou éteinte (power local et global)
            // cf rapport : Scénario 9 – 6.5. Application de l'interrupteur
            uint8_t valeur_a_envoyer = (bande_led->power && led_courante->power) ? valeur_composante : 0;
            
            // Encodage de l'octet de chaque composante bit à bit en valeurs de Pulse
            // puis écriture dans le buffer du DMA.
            // cf rapport : Scénario 9 – 6.6. Encodage de l'octet de chaque composante
            for (int bit = 8-1; bit >= 0; bit--)
            {
                uint32_t valeur_pulse = (valeur_a_envoyer & (1 << bit)) ? pulse_bit_1_tics : pulse_bit_0_tics;
                bande_led->dma_buffer[dma_index] = valeur_pulse;
                dma_index++;
            }
        }
    }
    
    // Ajout du délai de reset au buffer DMA (41 periodes PWM avec Pulse à 0)
    // cf rapport : Scénario 9 – 6.7. Ajout du délai de reset
    for (int i = 0; i < bande_led->cycles_delai_maj; i++) {
        bande_led->dma_buffer[dma_index] = 0;
        dma_index++;
    }
    
    // Arrêt propre d'un éventuel transfert précédent avant de lancer le nouveau
    // Puis lancement du nouveau transfert DMA avec le buffer mis à jour.
    // cf rapport : Scénario 9 – 6.8. Lancement de l'envoi par DMA
    HAL_TIM_PWM_Stop_DMA(bande_led->PWM_timer, bande_led->PWM_channel);
    
    __HAL_TIM_SET_COUNTER(bande_led->PWM_timer, 0);

    bool status = (HAL_TIM_PWM_Start_DMA(bande_led->PWM_timer, bande_led->PWM_channel, bande_led->dma_buffer, dma_index) == HAL_OK);

    return status; // Succès ou échec
}

/**
 * @brief  Active l'interrupteur général de la bande et applique immédiatement le changement.
 *         // cf rapport : Scénario 9 – 7. Modification de l'état des LEDs
 * @param  bande_led Pointeur vers l'objet bande LED
 * @return None
 */
void BandeLED_allumer(BandeLED_t* bande_led)
{
    bande_led->power = true;
    BandeLED_show(bande_led);
}

/**
 * @brief  Désactive l'interrupteur général de la bande et applique immédiatement le changement.
 *         // cf rapport : Scénario 9 – 7. Modification de l'état des LEDs
 * @param  bande_led Pointeur vers l'objet bande LED
 * @return None
 */
void BandeLED_eteindre(BandeLED_t* bande_led)
{
    bande_led->power = false;
    BandeLED_show(bande_led);
}

/**
 * @brief  Modifie l'interrupteur d'une LED en mémoire 
 *         (sans envoi : il faut appeler BandeLED_show() pour appliquer le changement).
 *         // cf rapport : Scénario 9 – 7. Modification de l'état des LEDs
 * @param  bande_led Pointeur vers l'objet bande LED
 * @param  led_id    Indice de la LED (ignoré s'il est invalide)
 * @param  state     true : LED allumée, false : LED éteinte
 * @return None
 */
void BandeLED_set_led_state(BandeLED_t* bande_led, int led_id, bool state)
{
    // Vérification de l'indice pour ne jamais écrire en dehors du tableau
    if (led_id >= 0 && led_id < bande_led->nb_leds)
    {
        bande_led->leds[led_id].power = state;
    }
}

/**
 * @brief  Modifie la couleur d'une LED en mémoire à partir d'une couleur 0xRRGGBB (sans envoi : il faut appeler BandeLED_show()).
 *         // cf rapport : Scénario 9 – 7. Modification de l'état des LEDs
 * @param  bande_led Pointeur vers l'objet bande LED
 * @param  led_id    Indice de la LED (ignoré s'il est invalide)
 * @param  hexa      Couleur au format 0xRRGGBB (ex : BL_HEXA_ROUGE)
 * @return None
 */
void BandeLED_set_led_hexa(BandeLED_t* bande_led, int led_id, uint32_t hexa)
{
    if (led_id >= 0 && led_id < bande_led->nb_leds)
    {
        // Extraction de chaque composante par décalage et masquage
        bande_led->leds[led_id].rouge = (hexa >> 16) & 0xFF;
        bande_led->leds[led_id].vert  = (hexa >> 8) & 0xFF;
        bande_led->leds[led_id].bleu  = hexa & 0xFF;
    }
}

/**
 * @brief  Modifie la couleur d'une LED en mémoire à partir de ses trois composantes (sans envoi : il faut appeler BandeLED_show()).
 *         // cf rapport : Scénario 9 – 7. Modification de l'état des LEDs
 * @param  bande_led Pointeur vers l'objet bande LED
 * @param  led_id    Indice de la LED (ignoré s'il est invalide)
 * @param  rouge     Composante rouge (0-255)
 * @param  vert      Composante verte (0-255)
 * @param  bleu      Composante bleue (0-255)
 * @return None
 */
void BandeLED_set_led_rgb(BandeLED_t* bande_led, int led_id, uint8_t rouge, uint8_t vert, uint8_t bleu)
{
    if (led_id >= 0 && led_id < bande_led->nb_leds)
    {
        bande_led->leds[led_id].rouge = rouge;
        bande_led->leds[led_id].vert  = vert;
        bande_led->leds[led_id].bleu  = bleu;
    }
}

/**
 * @brief  Modifie la luminosité (alpha) d'une LED en mémoire, sans changer sa couleur (sans envoi : il faut appeler BandeLED_show()).
 *         // cf rapport : Scénario 9 – 6.4. Application de la luminosité
 *         // cf rapport : Scénario 9 – 7. Modification de l'état des LEDs
 * @param  bande_led  Pointeur vers l'objet bande LED
 * @param  led_id     Indice de la LED (ignoré s'il est invalide)
 * @param  luminosity Luminosité (0 = nulle, 255 = couleur inchangée)
 * @return None
 */
void BandeLED_set_led_luminosity(BandeLED_t* bande_led, int led_id, uint8_t luminosity)
{
    if (led_id >= 0 && led_id < bande_led->nb_leds)
    {
        bande_led->leds[led_id].alpha = luminosity;
    }
}

/**
 * @brief  Fonction utilitaire de la démonstration : 
 *         convertit une position de 0 à 255 en couleur sur la roue chromatique.
 *         La position 0 correspond à 0° sur la roue chromatique, et la position 255 correspond à 180°.
 *         Générée par IA, son fonctionnement interne n'est pas détaillé dans le rapport.
 *         // cf rapport : Scénario 9 – 7.1. Fonction de démonstration
 * @param  position Position sur la roue chromatique (0-255)
 * @return Couleur au format 0xRRGGBB
 */
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

// Fonctions gardées et non utilisées dans le main.c final :

/**
 * @brief  Démonstration non bloquante, conçue pour être appelée à 20 Hz : 
 *         Fait défiler les LEDs 0 et 1 sur la roue chromatique à des vitesses différentes, 
 *         Fait clignoter les LEDs 2 et 3 en blanc à des fréquences différentes, 
 *         Puis envoie la trame.
 *         // cf rapport : Scénario 9 – 7.1. Fonction de démonstration
 * @param  bande Pointeur vers l'objet bande LED
 * @return true si la trame a bien été lancée, false sinon
 */
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
    // cf rapport : Scénario 9 – 6.8. Lancement de l'envoi par DMA
    return BandeLED_show(bande);
}