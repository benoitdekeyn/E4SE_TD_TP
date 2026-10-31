/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "stdbool.h"
#include "tmp102.h"
#include "i2c-lcd.h"
#include "bande-led.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/**
 * @brief Structure d'encapsulation pour la gestion unifiée d'une LED matérielle.
 *        // cf rapport : Architecture globale – 6. Structure pour les LEDs
 */
typedef struct {
    GPIO_TypeDef *port;   /**< Pointeur vers le port GPIO (ex: GPIOA, GPIOB) */
    uint16_t pin;         /**< Numéro de broche GPIO (ex: GPIO_PIN_10) */
    bool state;           /**< État logique souhaité (true: allumée, false: éteinte) */
} LED_t;

/**
 * @brief Modes de fonctionnement et de surveillance globale de la maison.
 *        //  cf rapport : Scénario 2 – 3. Machine à états HOME / AWAY
 */
typedef enum {
  HOME,                   /**< Mode Présent : alarme intrusion désactivée, contrôle manuel fenêtre actif */
  AWAY                    /**< Mode Absent  : surveillance intrusion armée, fenêtre fermée et verrouillée */
} maison_mode_t;

/**
 * @brief États discrets de la fenêtre motorisée par servomoteur.
 *        // cf rapport : Scénario 5 – 2.1. États de la fenêtre
 */
typedef enum {
  INIT,                   /**< État initial au démarrage avant positionnement */
  FERMEE,                 /**< Fenêtre fermée (servomoteur à 0°) */
  OUVERTE                 /**< Fenêtre ouverte (servomoteur à 120°) */
} fenetre_t;

/**
 * @brief Plages de confort thermique pour la régulation automatique de la fenêtre.
 *        // cf rapport : Scénario 6 – 12.1. Seuils et états de température
 */
typedef enum {
  FROID,                  /**< Température <= 24°C : force la fermeture de la fenêtre */
  CHAUD,                  /**< Température >= 26°C : déclenche l'aération de la fenêtre */
  TEMPEREE                /**< Zone de confort (24°C - 26°C) formant une hystérésis naturelle */
} temperature_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// ==============================================================================
// Brochage matériel Arduino Header (NUCLEO-F411RE)
// cf rapport : Architecture globale – 1.2. Tableau de brochage final
// ==============================================================================
//  BTN_FENTR | D2  | PA_10 | EXTI 10 - Falling Edge | PULLUP   | 3V3
//  LED_JAUNE | D3  | PB_3  | GPIO OUTPUT            | NO       | 3V3 (Shield)
//  BUZZER    | D4  | PB_5  | TIMER 3 - CHANNEL 2    | NO       | 3V3 (Passif PWM)
//  I2C_SDA   | D5  | PB_4  | I2C3 - SDA             | NO       | LCD + TMP102 (5V pullup)
//  LED_BLEUE | D6  | PB_10 | GPIO OUTPUT            | NO       | 3V3 (Cycle capteurs)
//  I2C_SCL   | D7  | PA_8  | I2C3 - SCL             | NO       | LCD + TMP102 (5V pullup)
//  SERVO     | D8  | PA_9  | TIMER 1 - CHANNEL 2    | NO       | 5V (PWM 50 Hz)
//  PIR_MVMT  | D9  | PC_7  | EXTI 7 - Rising Edge   | PULLDOWN | 3V3 (Détecteur mouvement)
//  RGB_BANDE | D10 | PB_6  | TIMER 4 - CHANNEL 1    | NO       | 5V (WS2812B PWM + DMA)
//  RGB_ROUGE | D11 | PA_7  | GPIO OUTPUT            | NO       | 3V3 (Alarme gaz Shield)
//  BTN_HOME  | D12 | PA_6  | EXTI 6 - Falling Edge  | PULLUP   | 3V3 (Bouton mode)
//  GAZ       | A2  | PA_4  | ADC 1 - CHANNEL 4      | NO       | 5V (Capteur MQ-2)
//  PLUIE     | A3  | PB_0  | ADC 1 - CHANNEL 8      | NO       | 5V (Capteur résistif)

// ==============================================================================
// Abstraction matérielle (évite le hardcoding des handles HAL)
// cf rapport : Architecture globale – 5.1. Defines matériels
// ==============================================================================
#define ADC_CAPTEURS    (&hadc1)    /**< Instance ADC1 pour mesures gaz et pluie */
#define BUS_I2C         (&hi2c3)    /**< Bus I2C3 partagé (TMP102 + Afficheur LCD) */
#define TIMER_CAPTEURS  (&htim2)    /**< TIM2 : base temporelle échantillonnage (2 s) */
#define TIMER_DEBOUNCE  (&htim5)    /**< TIM5 : base temporelle anti-rebond & alarmes (50 ms) */
#define TIMER_SERVO     (&htim1)    /**< TIM1 : génération PWM 50 Hz pour servomoteur */
#define CANAL_SERVO     TIM_CHANNEL_2 /**< Canal 2 de TIM1 (broche PA9 / D8) */
#define TIMER_BUZZER    (&htim3)    /**< TIM3 : génération PWM audio pour buzzer passif */
#define CANAL_BUZZER    TIM_CHANNEL_2 /**< Canal 2 de TIM3 (broche PB5 / D4) */
#define TIMER_BANDE_LED (&htim4)    /**< TIM4 : génération PWM 800 kHz DMA WS2812B */
#define CANAL_BANDE_LED TIM_CHANNEL_1 /**< Canal 1 de TIM4 (broche PB6 / D10) */

// ==============================================================================
// Seuils de décision et configuration ADC
// cf rapport : Scénario 3 – 3.2. Seuil de détection (Gaz),
//              Scénario 5 – 4.6. Tests du capteur de pluie et seuil (Pluie),
//              Scénario 6 – 12.1. Seuils et états de température (Température)
// ==============================================================================
#define SEUIL_GAZ         2500      /**< Seuil détection gaz MQ-2 (0-4095) // cf rapport : Scénario 3 – 3.2. Seuil de détection */
#define SEUIL_PLUIE       4000      /**< Seuil détection eau/pluie (0-4095) // cf rapport : Scénario 5 – 4.6. Tests du capteur de pluie et seuil */
#define SEUIL_CHALEUR     26        /**< Seuil haut de température en °C // cf rapport : Scénario 6 – 12.1. Seuils et états de température */
#define SEUIL_FROID       24        /**< Seuil bas de température en °C // cf rapport : Scénario 6 – 12.1. Seuils et états de température */
#define NBR_CAPTEURS_ADC  2         /**< Nombre de canaux convertis en séquence Scan */
#define GAZ_ADC_RANK      0         /**< Index Rank 1 dans buffer DMA : Capteur MQ-2 */
#define PLUIE_ADC_RANK    1         /**< Index Rank 2 dans buffer DMA : Capteur pluie */

// ==============================================================================
// Fréquences d'échantillonnage et bases temporelles
// cf rapport : Scénario 1 – 1. Configuration du Timer (Timer 2),
//              Scénario 2 – 2.1. Prescaler du Timer 5 (Timer 5)
// ==============================================================================
#define FREQ_CYCLE_CAPTEURS   0.5   /**< Fréquence d'échantillonnage (0.5 Hz => Période = 2 s) */
#define FREQ_CYCLE_DEBOUNCE   20    /**< Fréquence de scrutation anti-rebond (20 Hz => 50 ms) */

// ==============================================================================
// Paramètres PWM Servomoteur (Timer 1)
// cf rapport : Scénario 5 – 1. Commande du servomoteur
// ==============================================================================
#define FREQ_TIMER_SERVO  2500000   /**< Fréquence de comptage TIM1 (2.5 MHz => 0.4 µs/tic) */
#define FREQ_CYCLE_SERVO  50        /**< Fréquence PWM servomoteur standard (50 Hz => 20 ms) */
#define SERVO_0_DEG_MS    0.5       /**< Largeur d'impulsion à 0° (0.5 ms => 1250 tics) */
#define SERVO_180_DEG_MS  2.5       /**< Largeur d'impulsion à 180° (2.5 ms => 6250 tics) */

// ==============================================================================
// Paramètres Audio et Alarmes (Timer 3 & Timer 5)
// cf rapport : Scénario 8 – Gestion globale des alarmes
// ==============================================================================
#define FREQ_TIMER_BUZZER             25000000 /**< Fréquence d'horloge TIM3 (25 MHz) */
#define FREQ_TIMER_BANDE_LED          100000000/**< Fréquence d'horloge TIM4 (100 MHz) */
#define FREQ_CYCLE_ALARME             FREQ_CYCLE_DEBOUNCE /**< Base de comptage alarme (20 Hz) */
#define FREQ_BLINK_ALARME_GAZ         10       /**< Fréquence clignotement alarme gaz (10 Hz) */
#define FREQ_BLINK_ALARME_INTRUSION   2        /**< Fréquence clignotement alarme intrusion (2 Hz) */
#define FREQ_BUZZER_ALARME_GAZ        440      /**< Fréquence note alarme gaz (La4 / 440 Hz) */
#define FREQ_BUZZER_ALARME_INTRUSION  384      /**< Fréquence note alarme intrusion (384 Hz) */

// ==============================================================================
// Tableau de bord NeoPixel WS2812B (Timer 4 + DMA)
// cf rapport : Scénario 9 – Bande de LED NeoPixel
// ==============================================================================
#define NB_LEDS_BANDE_RGB 4         /**< Nombre total de LEDs sur le bandeau */
#define LED_BAS_DROITE    0         /**< Index LED 0 : Alarme intrusion (orange) */
#define LED_HAUT_DROITE   1         /**< Index LED 1 : Alarme gaz (rouge) */
#define LED_HAUT_GAUCHE   2         /**< Index LED 2 : Témoin cycle capteurs (violet) */
#define LED_BAS_GAUCHE    3         /**< Index LED 3 : Indicateur température (Bleu/Vert/Orange) */
#define LUMINOSITE_BASE   10        /**< Luminosité réduite (échelle de 0-255) */

// --- Nuancier hexadécimal pour le tableau de bord ---
#define COULEUR_CAPTEURS         BL_HEXA_VIOLET /**< Violet : pulsation cycle capteurs */
#define COULEUR_ALARME_GAZ       BL_HEXA_ROUGE  /**< Rouge : alerte fuite de gaz */
#define COULEUR_ALARME_INTRUSION BL_HEXA_ORANGE /**< Orange : alerte détection intrusion */
#define COULEUR_TEMP_FROID       BL_HEXA_BLEU   /**< Bleu : ambiance froide (< 24°C) */
#define COULEUR_TEMP_TEMPERE     BL_HEXA_VERT   /**< Vert : confort tempéré (24°C - 26°C) */
#define COULEUR_TEMP_CHAUD       BL_HEXA_ORANGE /**< Orange : ambiance chaude (> 26°C) */
#define COULEUR_ETEINTE          BL_HEXA_NOIR   /**< Noir : LED éteinte */

// ==============================================================================
// Butées angulaires de la fenêtre
// cf rapport : Scénario 5 – 2. Gestion de la fenêtre
// ==============================================================================
#define ANGLE_SERVO_OUVERT  120     /**< Butée mécanique maximale d'ouverture (120°) */
#define ANGLE_SERVO_FERME   0       /**< Position de fermeture (0°) */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// ==============================================================================
// Instances et états des voyants LED physiques
// cf rapport : Architecture globale – 6. Structure pour les LEDs
// ==============================================================================
LED_t           led_capteurs                = {LED_BLEUE_GPIO_Port, LED_BLEUE_Pin, false};
LED_t           led_alarme_gaz              = {RGB_ROUGE_GPIO_Port, RGB_ROUGE_Pin, false};
LED_t           led_alarme_intrusion        = {LED_JAUNE_GPIO_Port, LED_JAUNE_Pin, false};
bool            buzzer                      = false; /**< État logique ON/OFF du buzzer passif */
fenetre_t       fenetre                     = OUVERTE; /**< État courant de la fenêtre (ici OUVERTE au démarrage) */

// ==============================================================================
// Signaux de contrôle et drapeaux anti-rebond (partagés avec EXTI Callback)
// cf rapport : Scénario 2 – 2. Gestion de l'anti-rebond & Scénario 5 – 3.1. Implémentation et anti-rebond
// ==============================================================================
volatile bool   debouncing_bouton_home      = false; /**< Verrou anti-rebond en cours (Bouton HOME) */
volatile bool   debouncing_bouton_fenetre   = false; /**< Verrou anti-rebond en cours (Bouton Fenêtre) */
bool            bouton_home_appuye          = false; /**< Impulsion validée d'appui sur bouton HOME */
bool            bouton_fenetre_appuye       = false; /**< Impulsion validée d'appui sur bouton Fenêtre */

// ==============================================================================
// Drapeaux d'interruptions périodiques et DMA (partagés avec ISR)
// cf rapport : Architecture globale – 3.1. Boucle principale et gestion des interruptions
// ==============================================================================
volatile bool   timer_debounce_flag         = false; /**< Tick périodique TIM5 (20 Hz / 50 ms) */
volatile bool   timer_capteurs_flag         = false; /**< Tick périodique TIM2 (0.5 Hz / 2 s) */
volatile bool   fin_mesure_ADC_flag         = false; /**< Notification fin de transfert circulaire DMA ADC */
int             blink_counter_alarm         = 0;     /**< Compteur de tics pour modulation des alarmes */

// ==============================================================================
// Variables d'état métier et sécurité
// cf rapport : Scénario 2 – 3.2. Gestion des modes, Scénario 2 – 4. Capteur PIR et alarme intrusion,
//              Scénario 3 – 4. Alarme gaz, Scénario 5 – 4. Détection de pluie avec ADC DMA multi-canaux,
//              Scénario 6 – 12.1. Seuils et états de température
// ==============================================================================
maison_mode_t   mode                        = HOME;  /**< Mode courant de la maison (HOME / AWAY) */
volatile bool   presence_detectee           = false; /**< Détection brute PIR (EXTI 7) */
bool            pluie_detectee              = false; /**< Alerte pluie active */
bool            alarme_intrusion            = false; /**< Alarme intrusion armée et sonnante */
bool            alarme_gaz                  = false; /**< Alarme fuite de gaz active */

temperature_t   temperature                 = TEMPEREE; /**< Tranche de confort thermique */
float           temperature_valeur          = 0.0;      /**< Dernière température mesurée en °C */
int             pluie_valeur                = 0;        /**< Valeur brute ADC du capteur de pluie */
int             gaz_valeur                  = 0;        /**< Valeur brute ADC du capteur MQ-2 */

// ==============================================================================
// Buffers matériels et descripteurs de périphériques
// cf rapport : Scénario 5 – 4.3. Configuration DMA, Scénario 6 – 4. Structure de la bibliothèque,
//              Scénario 9 – 4. Structure de la bibliothèque
// ==============================================================================
volatile uint16_t adc_valeurs[NBR_CAPTEURS_ADC]; /**< Buffer de destination du DMA circulaire ADC1 */
TMP102_t        thermometre;                     /**< Contexte et configuration du capteur TMP102 */
BandeLED_t      bande_led;                       /**< Descripteur de la bande de 4 LEDs WS2812B */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void afficher_capteurs_ecran(void);
void afficher_mode_ecran(void);
void initialiser_ecran(void);
void initialiser_thermometre(void);
void initialiser_bande_led(void);
void commande_bande_led(void);
void angle_servo(int angle);
void commande_fenetre(void);
void commande_led(void);
void commande_buzzer(void);
void frequence_buzzer(int frequence);
void evaluer_gaz(void);
void evaluer_pluie(void);
void evaluer_temperature(void);
void debounce_filter(void);
void update_cycle_alarmes_leds(void);
void update_cycle_alarmes_buzzer(void);
void update_cycle_alarmes(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief  Met à jour l'affichage des valeurs des capteurs sur l'écran LCD 16x2.
 *         Formate et affiche la température (°C) sur la seconde moitié de la première ligne,
 *         et les valeurs brutes ADC de pluie (Hu) et de gaz (Ga) sur la deuxième ligne.
 *         // cf rapport :   Scénario 7 – 3.3. Valeurs capteurs
 * @param  None
 * @return None
 */
void afficher_capteurs_ecran() 
{
    char ligne1[8+1];
    char ligne2[16+1];

    // Ligne 1 (2nde moitié) : formatage de la température sur 5 caractères avec le symbole degré
    // Le code hexadécimal \xDF correspond au symbole '°' dans la table CGROM du contrôleur LCD
    // cf rapport : Scénario 7 – 3.3. Valeurs capteurs
    snprintf(ligne1, sizeof(ligne1), " %5s\xDF" "C",
             TMP102_temperature_to_string(temperature_valeur, 2));

    // Ligne 2 : formatage à largeur fixe (%4d) pour écraser entièrement l'ancienne mesure
    // et éviter les reliquats visuels d'affichage sur les capteurs de pluie et de gaz
    // cf rapport : Scénario 7 – 3.3. Valeurs capteurs
    snprintf(ligne2, sizeof(ligne2), "Hu %4d  Ga %4d", 
             pluie_valeur, 
             gaz_valeur);

    // Positionnement du curseur en colonne 8 de la ligne 0 pour préserver le mode HOME/AWAY (colonnes 0-7)
    lcd_put_cur(0, 8);
    lcd_send_string(ligne1);

    // Positionnement du curseur au début de la ligne 1 pour l'affichage complet des mesures analogiques
    lcd_put_cur(1, 0);
    lcd_send_string(ligne2);
}

/**
 * @brief  Affiche le mode de présence de la maison (HOME ou AWAY) sur le LCD.
 *         Écrit sur les 4 premières colonnes de la première ligne de l'afficheur.
 *          // cf rapport : Scénario 7 – 3.2. HOME / AWAY
 * @param  None
 * @return None
 */
void afficher_mode_ecran() 
{ 
  char ligne_mode[8];
  // Formatage à largeur fixe (%4s) pour occuper exactement 4 caractères (HOME ou AWAY)
  // sans déborder sur l'affichage de la température situé en colonne 8
  // cf rapport : Scénario 7 – 3.2. HOME / AWAY
  snprintf(ligne_mode, sizeof(ligne_mode), "%4s",
           (mode == HOME) ? "HOME" : "AWAY");

  // Positionnement au début de la ligne 0
  lcd_put_cur(0, 0);
  lcd_send_string(ligne_mode);
}

/**
 * @brief  Initialise le module d'affichage LCD 16x2 sur le bus I2C.
 *         Configure le contrôleur LCD HD44780 (via extenseur PCF8574), affiche le mode
 *         initial et un message d'accueil temporaire sur la ligne 2.
 *         // cf rapport : Scénario 7 – 2. Intégration de la bibliothèque LCD I²C & Scénario 7 – 3.1. Initialisation
 * @param  None
 * @return None
 */
void initialiser_ecran()
{
    // Initialisation matérielle du contrôleur LCD avec le bus I2C3
    // cf rapport : Scénario 7 – 2.1. Modification de l'initialisation
    lcd_init(BUS_I2C);

    // Affichage initial du mode de la maison (HOME par défaut)
    afficher_mode_ecran();

    // Message de bienvenue temporaire (écrasé au premier cycle capteurs après 2 s)
    // cf rapport : Scénario 7 – 3.1. Initialisation
    lcd_put_cur(1, 0);
    lcd_send_string("Hello Abir (^w^)");
}

/**
 * @brief  Initialise et configure le capteur de température I2C TMP102.
 *         Charge la configuration par défaut puis active le mode Shutdown basse consommation
 *         pour opérer en mode One-Shot cadencé par le timer d'échantillonnage.
 *         // cf rapport : Scénario 6 – 6. Initialisation, Scénario 6 – 9. Mesure One-Shot en mode Shutdown, Scénario 6 – 10. Utilisation de la bibliothèque dans le code
 * @param  None
 * @return None
 */
void initialiser_thermometre() 
{
    // Chargement des valeurs nominales par défaut dans la structure
    // cf rapport : Scénario 6 – 6.2. Valeurs par défaut
    TMP102_init_default(&thermometre);

    // Configuration des paramètres matériels et applicatifs
    thermometre.busI2C                = BUS_I2C;             // Bus I2C3
    thermometre.address               = TMP102_I2C_ADDR_GND; // Adresse 0x48 (ADDR0 relié à GND)
    thermometre.config.Mode           = TMP102_MODE_SHUTDOWN;// Mode veille (<0.15 µA) avec One-Shot
    thermometre.config.ExtendedMode   = TMP102_EXTENDED_OFF; // Résolution standard 12 bits

    // Transmission de la configuration vers les registres du capteur physique via I2C
    // cf rapport : Scénario 6 – 5.1. Application du registre de configuration
    if (TMP102_apply_registers(&thermometre)) {
        printf("Capteur TMP102 configuré\r\n");
    } else {
        printf("Erreur d'initialisation du capteur TMP102\r\n");
    }
}

/**
 * @brief  Initialise le pilote du ruban de 4 LEDs adressables RGB (WS2812B NeoPixel).
 *         Configure le timer PWM et DMA associé (TIM4 CH1, 800 kHz), règle la luminosité de base
 *         et précharge les teintes de repos pour le tableau de bord.
 *         // cf rapport : Scénario 9 – 3. Branchement et configuration CubeMX & Scénario 9 – 8. Utilisation dans le code principal
 * @param  None
 * @return None
 */
void initialiser_bande_led()
{
  // Initialisation du descripteur : TIM4 CH1 cadencé à 100 MHz, protocole one-wire 800 kHz
  // cf rapport : Scénario 9 – 5. Initialisation
  BandeLED_Init(&bande_led, TIMER_BANDE_LED, CANAL_BANDE_LED, FREQ_TIMER_BANDE_LED, NB_LEDS_BANDE_RGB);

  // Luminosité réduite (10/255) sur chaque LED pour limiter le courant et éviter l'éblouissement
  // cf rapport : Scénario 9 – 6.3. Application de la luminosité
  for (int i = 0; i < NB_LEDS_BANDE_RGB; i++) {
      BandeLED_set_led_luminosity(&bande_led, i, LUMINOSITE_BASE);
  }

  // Affectation des couleurs fonctionnelles fixes en mémoire
  // LED 2 (Haut Gauche) : Témoin capteurs (Violet)
  // LED 1 (Haut Droite) : Alarme gaz (Rouge)
  // LED 0 (Bas Droite)  : Alarme intrusion (Orange)
  BandeLED_set_led_hexa(&bande_led, LED_HAUT_GAUCHE, COULEUR_CAPTEURS);
  BandeLED_set_led_hexa(&bande_led, LED_HAUT_DROITE, COULEUR_ALARME_GAZ);
  BandeLED_set_led_hexa(&bande_led, LED_BAS_DROITE, COULEUR_ALARME_INTRUSION);

  // Extinction initiale de toutes les LEDs pour un démarrage propre
  BandeLED_set_led_state(&bande_led, LED_HAUT_GAUCHE, false);
  BandeLED_set_led_state(&bande_led, LED_HAUT_DROITE, false);
  BandeLED_set_led_state(&bande_led, LED_BAS_DROITE, false);
  BandeLED_set_led_state(&bande_led, LED_BAS_GAUCHE, false);

  // Émission de la première trame via DMA pour actualiser le ruban physique
  BandeLED_show(&bande_led);
}

/**
 * @brief  Met à jour l'affichage dynamique du tableau de bord sur les 4 NeoPixels.
 *         Reflète l'état des cycles capteurs, des alarmes gaz et intrusion, ainsi que
 *         la couleur représentative de la température, puis déclenche le transfert DMA.
 *          // cf rapport : Scénario 9 – 8.4. Mise à jour périodique
 * @param  None
 * @return None
 */
void commande_bande_led()
{
    // LED Haut Gauche : pulsation de cycle capteurs, synchronisée sur la LED bleue physique
    BandeLED_set_led_state(&bande_led, LED_HAUT_GAUCHE, led_capteurs.state);

    // LED Haut Droite : alerte fuite de gaz, synchronisée sur la LED rouge d'alarme
    BandeLED_set_led_state(&bande_led, LED_HAUT_DROITE, led_alarme_gaz.state);

    // LED Bas Droite : alerte intrusion, synchronisée sur la LED jaune d'alarme
    BandeLED_set_led_state(&bande_led, LED_BAS_DROITE, led_alarme_intrusion.state);

    // LED Bas Gauche : indicateur chromatique de la tranche de température
    // Bleu = Froid (<= 24°C), Vert = Tempéré (24-26°C), Orange = Chaud (>= 26°C)
    // cf rapport : Scénario 9 – 8.4. Mise à jour périodique & Scénario 6 – 12.1. Seuils et états de température
    switch (temperature) {
        case FROID:
            BandeLED_set_led_state(&bande_led, LED_BAS_GAUCHE, true);
            BandeLED_set_led_hexa(&bande_led, LED_BAS_GAUCHE, COULEUR_TEMP_FROID);
            break;
        case TEMPEREE:
            BandeLED_set_led_state(&bande_led, LED_BAS_GAUCHE, true);
            BandeLED_set_led_hexa(&bande_led, LED_BAS_GAUCHE, COULEUR_TEMP_TEMPERE);
            break;
        case CHAUD:
            BandeLED_set_led_state(&bande_led, LED_BAS_GAUCHE, true);
            BandeLED_set_led_hexa(&bande_led, LED_BAS_GAUCHE, COULEUR_TEMP_CHAUD);
            break;
        default:
            BandeLED_set_led_state(&bande_led, LED_BAS_GAUCHE, false);
            break;
    }

    // Mise à jour matérielle des NeoPixels
    // cf rapport : Scénario 9 – 6.7. Lancement de l'envoi par DMA
    BandeLED_show(&bande_led);
}

/**
 * @brief  Positionne le servomoteur de la fenêtre à l'angle angulaire spécifié (0° à 180°).
 *         Convertit l'angle cible en durée d'impulsion PWM (0.5 ms à 2.5 ms),
 *         calcule le nombre de tics d'horloge du Timer 1 et met à jour le registre de comparaison CCR.
 *         //  cf rapport : Scénario 5 – 1. Commande du servomoteur
 * @param  angle Angle cible en degrés compris entre 0 et 180 (ex: 0° fermé, 120° ouvert).
 * @return None
 */
void angle_servo(int angle)
{
  // Calcul mathématique de la largeur d'impulsion :
  // Impulsion (tics) = [ [(angle / 180.0) * (T_180 - T_0)] + T_0 ] * Freq_timer_servo
  // Résolution angulaire ≈ 27.8 tics/degré
  // cf rapport : Scénario 5 – 1.4. Calcul de la résolution angulaire
	int pulse_tics = 
  (
    ( 
      (angle / 180.0) 
      * 
      (SERVO_180_DEG_MS - SERVO_0_DEG_MS) / 1000.0
    ) 
    + 
    (SERVO_0_DEG_MS / 1000.0)
  ) 
  * FREQ_TIMER_SERVO;

  // Mise à jour du registre de comparaison CCR (TIM1 Channel 2, broche PA9)
  // cf rapport : Scénario 5 – 1.5. Appliquer un angle au servomoteur
	__HAL_TIM_SET_COMPARE(TIMER_SERVO, CANAL_SERVO, pulse_tics);
}

/**
 * @brief  Gère l'actionnement mécanique de la fenêtre via le servomoteur.
 *         N'applique la consigne d'angle que lors d'un changement d'état effectif
 *         (OUVERTE -> 120°, FERMEE -> 0°) pour éviter les écritures redondantes.
 *          // cf rapport : Scénario 5 – 2. Gestion de la fenêtre
 * @param  None
 * @return None
 */
void commande_fenetre()
{ 
  // Variable statique pour mémoriser l'état précédent entre deux itérations
  // Initialisée à INIT pour rendre possible la première transition depuis un état indéfini.
  // cf rapport : Scénario 5 – 2.2. Application de l'etat de la fenêtre
  static fenetre_t etat_precedent = INIT;

  if (fenetre != etat_precedent) {
    switch(fenetre) {
      case OUVERTE :
        // Angle d'ouverture fixé empiriquement à 120° (au-delà, risque de casse mécanique contre le mur)
        // cf rapport : Scénario 5 – 2.1. États de la fenêtre
        angle_servo(ANGLE_SERVO_OUVERT);
        break;
      case FERMEE :
        // Angle de fermeture étanche à 0° (position initiale lors du vissage du servomoteur)
        angle_servo(ANGLE_SERVO_FERME);
        break;
      case INIT :
        break;
    }
    // Mémorisation du nouvel état validé
    etat_precedent = fenetre;
  }
}

/**
 * @brief  Applique l'état logique mémorisé des structures LED_t aux sorties GPIO physiques.
 *          // cf rapport : Architecture globale – 6. Structure pour les LEDs
 * @param  None
 * @return None
 */
void commande_led()
{
  // LED témoin de cycle capteurs (LED bleue du shield, PB10)
  if (led_capteurs.state) {
    HAL_GPIO_WritePin(led_capteurs.port, led_capteurs.pin, GPIO_PIN_SET);
  } else {
    HAL_GPIO_WritePin(led_capteurs.port, led_capteurs.pin, GPIO_PIN_RESET);
  }

  // LED témoin d'alarme gaz (composante rouge de la LED RGB du shield, PA7)
  if (led_alarme_gaz.state) {
    HAL_GPIO_WritePin(led_alarme_gaz.port, led_alarme_gaz.pin, GPIO_PIN_SET);
  } else {
    HAL_GPIO_WritePin(led_alarme_gaz.port, led_alarme_gaz.pin, GPIO_PIN_RESET);
  }

  // LED témoin d'alarme intrusion (LED jaune, PB3)
  if (led_alarme_intrusion.state) {
    HAL_GPIO_WritePin(led_alarme_intrusion.port, led_alarme_intrusion.pin, GPIO_PIN_SET);
  } else {
    HAL_GPIO_WritePin(led_alarme_intrusion.port, led_alarme_intrusion.pin, GPIO_PIN_RESET);
  }
}

/**
 * @brief  Pilote l'activation ou l'extinction du signal PWM alimentant le buzzer passif.
 *         Démarre ou arrête la génération PWM sur le Canal 2 du Timer 3 selon le drapeau 'buzzer'.
 *         //  cf rapport : Scénario 8 – 2. Gestion du buzzer avec fréquences distinctes
 * @param  None
 * @return None
 */
void commande_buzzer() 
{
	if (buzzer) {
    // Démarrage de la génération PWM matérielle (TIM3 Channel 2, broche PB5)
	  HAL_TIM_PWM_Start(TIMER_BUZZER, CANAL_BUZZER);
	} else {
    // Arrêt complet du signal pour couper l'émission acoustique
	  HAL_TIM_PWM_Stop(TIMER_BUZZER, CANAL_BUZZER);
	}
}

/**
 * @brief  Configure dynamiquement la fréquence sonore émise par le buzzer passif.
 *         Calcule et programme en temps réel la période de comptage (registre ARR)
 *         et la durée à l'état haut (registre CCR) pour obtenir un rapport cyclique de 50%.
 *         // cf rapport : Scénario 8 – 2.1. Configuration du Timer PWM & Scénario 8 – 2.3. Fréquences distinctes pour chaque alarme
 * @param  frequence Fréquence acoustique cible en Hertz (ex: 440 Hz pour gaz, 384 Hz pour intrusion).
 * @return None
 */
void frequence_buzzer(int frequence) 
{
  // Calcul du nombre de tics pour la période (registre ARR) :
  // Horloge du Timer 3 cadencée à 25 MHz (PSC = 4 - 1 depuis horloge source 100 MHz)
  // ARR = (FREQ_TIMER_BUZZER / frequence) - 1
  // Exemple Gaz       (440 Hz) : 25 000 000 / 440 ≈ 56818 tics -1
  // Exemple Intrusion (384 Hz) : 25 000 000 / 384 ≈ 65104 tics -1
  // cf rapport : Scénario 8 – 2.1. Configuration du Timer PWM
	int periode_tics = (double)FREQ_TIMER_BUZZER / frequence - 1;

  // Calcul du temps haut pour un rapport cyclique de 50% (onde carrée acoustique optimale) :
  // pulse_tics = periode_tics / 2
  // CCR        = ARR + 1      / 2
	int pulse_tics   = (periode_tics + 1) / 2;

  // Rechargement immédiat des registres matériels du Timer 3
	__HAL_TIM_SET_AUTORELOAD(TIMER_BUZZER, periode_tics);
	__HAL_TIM_SET_COMPARE(TIMER_BUZZER, CANAL_BUZZER, pulse_tics);
}

/**
 * @brief  Évalue la concentration de gaz mesurée par le capteur analogique MQ-2.
 *         Lit la valeur convertie dans le buffer DMA (Canal ADC 4, Rank 1),
 *         compare la mesure au seuil critique (SEUIL_GAZ) pour armer l'alarme fuite de gaz,
 *         et affiche la valeur sur le moniteur série USART2.
 *         // cf rapport : Scénario 3 – 3.2. Seuil de détection, Scénario 3 – 4. Alarme gaz, Scénario 5 – 4. Détection de pluie avec ADC DMA multi-canaux
 * @param  None
 * @return None
 */
void evaluer_gaz() 
{
  // Lecture de la conversion ADC Rank 1 directement en mémoire via le buffer DMA
  // cf rapport : Scénario 3 – 3.2. Seuil de détection & Scénario 5 – 4.5. Lecture et évaluation des capteurs
  gaz_valeur = adc_valeurs[GAZ_ADC_RANK];

  // Évaluation du dépassement du seuil de détection de gaz
  if (gaz_valeur >= SEUIL_GAZ) {
    printf("Gaz detecte !\r\n");
    alarme_gaz = true; // Déclenchement de l'alarme (prioritaire et indépendante du mode HOME/AWAY)
  } else {
    alarme_gaz = false;
  }

  // Affichage périodique sur le moniteur série UART
  // cf rapport : Scénario 4 – 4.1. Mesures périodiques (toutes les 2 s)
  printf("                                      GAZ   = %d\r\n", gaz_valeur);
}

/**
 * @brief  Évalue la présence d'eau/pluie mesurée par le capteur de pluie.
 *         Lit la conversion ADC stockée dans le buffer DMA (Canal ADC 8, Rank 2),
 *         la compare au seuil de pluie (SEUIL_PLUIE) pour commander la fermeture de sécurité
 *         de la fenêtre, et transmet la mesure sur l'UART.
 *         // cf rapport : Scénario 5 – 4. Détection de pluie avec ADC DMA multi-canaux & Scénario 5 – 5. Logique de décision de la fenêtre
 * @param  None
 * @return None
 */
void evaluer_pluie() 
{
  // Lecture de la conversion ADC Rank 2 issue du transfert DMA
  // Surface mouillée : monte immédiatement vers 4095
  // cf rapport : Scénario 5 – 4.6. Tests du capteur de pluie et seuil
  pluie_valeur = adc_valeurs[PLUIE_ADC_RANK];

  // Seuil fixé à 4000 pour éviter tout faux positif dû à l'humidité résiduelle
  if (pluie_valeur >= SEUIL_PLUIE) {
    printf("Pluie detecte !\r\n");
    pluie_detectee = true; // Détection de pluie active
  } else {
    pluie_detectee = false;
  }

  // Affichage périodique sur le moniteur série UART
  printf("                                      PLUIE = %d\r\n", pluie_valeur);
}

/**
 * @brief  Acquiert la température via le capteur I2C TMP102 et gère la machine à états thermique.
 *         Convertit le registre brut en degrés Celsius, applique une hystérésis (FROID <= 24°C,
 *         TEMPEREE, CHAUD >= 26°C) et déclenche l'ouverture ou la fermeture automatique de la fenêtre
 *          (si l'on n'est pas bloqué par le mode AWAY ou par la pluie).
  *         // cf rapport : Scénario 6 – 10. Utilisation de la bibliothèque dans le code & Scénario 6 – 12. Gestion automatique de la fenêtre par la température
 * @param  None
 * @return None
 */
void evaluer_temperature() 
{
  float temperature_local;

  // Lecture du registre de température du capteur TMP102 (communication I2C, en mode One-Shot)
  // cf rapport : Scénario 6 – 7. Lecture du registre de température & Scénario 6 – 8.1. Registre brut → température en °C
  if (TMP102_get_temperature(&thermometre, &temperature_local)) 
  {
    temperature_valeur = temperature_local;
    // Formatage personnalisé du float avec précision 2 décimales sans bibliothèque math lourde
    // cf rapport : Scénario 6 – 8.3. Affichage de la température de type float
    printf("                                      TEMP  = %s°C\r\n", TMP102_temperature_to_string(temperature_valeur, 2));
  } 
  else 
  {
    printf("                                      TEMP  = ERREUR LECTURE\r\n");
  }
    
  // Machine à états de régulation thermique avec une hystérésis [24°C - 26°C]
  // Logique asymétrique : action unique lors de l'entrée dans CHAUD ou FROID, 
  // aucune action au retour en zone TEMPEREE
  // cf rapport : Scénario 6 – 12.1. Seuils et états de température & Scénario 6 – 12.2. Logique de décision de la fenêtre
  switch(temperature)
  {
    case CHAUD:
      // Sortie de l'état CHAUD : la température repasse sous 26°C
      if (temperature_valeur < SEUIL_CHALEUR)
      {
        temperature = TEMPEREE; // Retour neutre, l'utilisateur ou la météo reprend la main
        break;
      }
      else
      {
        break;
      }

    case FROID:
      // Sortie de l'état FROID : la température repasse au-dessus de 24°C
      if (temperature_valeur > SEUIL_FROID)
      {
        temperature = TEMPEREE; // Retour neutre, aucune modification forcée de la fenêtre
        break;
      }
      else
      {
        break;
      }

    case TEMPEREE:
      // Transition vers CHAUD : dépassement du seuil de chaleur (>= 26°C)
      if (temperature_valeur >= SEUIL_CHALEUR && temperature != CHAUD)
      {
        temperature = CHAUD;
        // Protection : en cas de pluie ou si le mode est AWAY, on ne touche à rien.
        if (mode == HOME && !pluie_detectee) {
          fenetre = OUVERTE; // On ouvre la fenêtre pour aérer la maison
        }
        break;
      }
      // Transition vers FROID : chute sous le seuil de froid (<= 24°C)
      else if (temperature_valeur <= SEUIL_FROID && temperature != FROID)
      {
        temperature = FROID;
        // Protection : en cas de pluie ou si le mode est AWAY, on ne touche à rien.
        if (mode == HOME && !pluie_detectee) {
          fenetre = FERMEE; // On ferme la fenêtre pour se protéger du froid
        }
        break;
      }
      else
      {
        break;
      }
  }
}

/**
 * @brief  Filtre anti-rebond logiciel non bloquant pour les boutons poussoirs (MODE/HOME et Fenêtre).
 *         Appelé périodiquement toutes les 50 ms par le Timer 5. Confirme l'appui uniquement
 *         si le signal est maintenu à l'état bas après la temporisation de rebond mécanique.
 *          // cf rapport : Scénario 2 – 2. Gestion de l'anti-rebond & Scénario 5 – 3.1. Implémentation et anti-rebond
 * @param  None
 * @return None
 */
void debounce_filter() 
{
  // Bouton HOME (PA6 / D12) : configuré avec pull-up interne (actif à 0 V)
  // L'interruption EXTI6 a levé debouncing_bouton_home et remis TIM5 à 0 il y a 50 ms
  // cf rapport : Scénario 2 – 2.4. Détection d'appui
	if (debouncing_bouton_home && !HAL_GPIO_ReadPin(BTN_HOME_GPIO_Port, BTN_HOME_Pin)) {
		debouncing_bouton_home = false; // Réarmement du verrou anti-rebond
		bouton_home_appuye = true;      // Validation de l'appui pour la gestion du mode HOME
	}

  // Bouton Fenêtre (PA10 / D2) : configuré avec pull-up interne (actif à 0 V)
  // Même logique d'évaluation 50 ms après le front descendant initial
  // cf rapport : Scénario 5 – 3.1. Implémentation et anti-rebond
	if (debouncing_bouton_fenetre && !HAL_GPIO_ReadPin(BTN_FENTR_GPIO_Port, BTN_FENTR_Pin)) {
		debouncing_bouton_fenetre = false;// Réarmement du verrou anti-rebond
		bouton_fenetre_appuye = true;     // Validation de l'appui pour la gestion de la fenêtre
	}
}

/**
 * @brief  Module de clignotement asynchrone non bloquant des voyants lumineux d'alarmes (LEDs).
 *         Évalue un compteur de tics partagé cadencé à 20 Hz par le Timer 5 pour faire clignoter
 *         la LED gaz à 10 Hz et la LED intrusion à 2 Hz par division de fréquence entière.
 *          // cf rapport : Scénario 8 – 1. Clignotement non bloquant par compteur partagé
 * @param  None
 * @return None
 */
void update_cycle_alarmes_leds() 
{
  // Si aucune alarme n'est en cours, extinction immédiate des deux voyants
  if (!alarme_gaz && !alarme_intrusion) {
    led_alarme_intrusion.state = false;
    led_alarme_gaz.state = false;
    return;
  }

  // Gestion du clignotement de l'alarme Gaz (10 Hz souhaité)
  // Horloge de base = 20 Hz (période = 50 ms). Demi-période = 20 / (2 * 10) = 1 tic.
  // La LED bascule d'état logique à chaque tic de 50 ms (50 ms ON, 50 ms OFF => Période = 100 ms => 10 Hz)
  // cf rapport : Scénario 8 – 1. Clignotement non bloquant par compteur partagé
  if (alarme_gaz) {
    int demi_periode_gaz = FREQ_CYCLE_ALARME / (2 * FREQ_BLINK_ALARME_GAZ);
    if (blink_counter_alarm % demi_periode_gaz == 0) {
      led_alarme_gaz.state = !led_alarme_gaz.state;
    }
  } else {
    led_alarme_gaz.state = false;
  }

  // Gestion du clignotement de l'alarme Intrusion (2 Hz souhaité)
  // Horloge de base = 20 Hz. Demi-période = 20 / (2 * 2) = 5 tics.
  // La LED bascule d'état tous les 5 tics de 50 ms, soit 250 ms ON, 250 ms OFF => Période = 500 ms => 2 Hz
  // cf rapport : Scénario 8 – 1. Clignotement non bloquant par compteur partagé
  if (alarme_intrusion) {
    int demi_periode_intrusion = FREQ_CYCLE_ALARME / (2 * FREQ_BLINK_ALARME_INTRUSION);
    if (blink_counter_alarm % demi_periode_intrusion == 0) {
      led_alarme_intrusion.state = !led_alarme_intrusion.state;
    }
  } else {
    led_alarme_intrusion.state = false;
  }
}

/**
 * @brief  Module de commande acoustique non bloquante du buzzer selon les alarmes en cours.
 *         Génère les fréquences distinctes (440 Hz pour le gaz, 384 Hz pour l'intrusion) et
 *         gère l'alternance temporelle par tranches de 250 ms en cas de double alarme simultanée.
 *         // cf rapport : Scénario 8 – 2. Gestion du buzzer avec fréquences distinctes & Scénario 8 – 3. Cas simultané : alternance gaz / intrusion
 * @param  None
 * @return None
 */
void update_cycle_alarmes_buzzer() 
{
  // Silencieux immédiat si aucune condition d'alerte n'est remplie
  if (!alarme_gaz && !alarme_intrusion) {
    buzzer = false;
    return;
  }

  // Cas 1 : Alarme Gaz seule
  // Note de 440 Hz (La4), hachée en synchronisation parfaite avec la LED rouge (10 Hz)
  // cf rapport : Scénario 8 – 2.3. Fréquences distinctes pour chaque alarme
  if (alarme_gaz && !alarme_intrusion) {
    frequence_buzzer(FREQ_BUZZER_ALARME_GAZ);
    buzzer = led_alarme_gaz.state; // Le buzzer retentit quand la LED s'allume
  }
  // Cas 2 : Alarme Intrusion seule
  // Note de 384 Hz (plus grave), hachée en synchronisation avec la LED jaune (2 Hz)
  // cf rapport : Scénario 8 – 2.3. Fréquences distinctes pour chaque alarme
  else if (!alarme_gaz && alarme_intrusion) {
    frequence_buzzer(FREQ_BUZZER_ALARME_INTRUSION);
    buzzer = led_alarme_intrusion.state; // Le buzzer retentit quand la LED s'allume
  }
  // Cas 3 : Alarme Gaz ET Alarme Intrusion simultanées
  // Le buzzer ne pouvant jouer qu'une note à la fois, le système alterne cycliquement toutes les 250 ms :
  // Période complète du motif = 20 Hz / 2 Hz = 10 tics (500 ms)
  // Phase 1 (tics 0 à 4 = 250 ms) : Son Intrusion continu à 384 Hz
  // Phase 2 (tics 5 à 9 = 250 ms) : Son Gaz à 440 Hz suivant le clignotement rapide de la LED rouge
  // cf rapport : Scénario 8 – 3. Cas simultané : alternance gaz / intrusion
  else if (alarme_gaz && alarme_intrusion) {
    int full_period_alternance = FREQ_CYCLE_ALARME / FREQ_BLINK_ALARME_INTRUSION;
    int half_period_alternance = full_period_alternance / 2;
    int phase_actuelle = blink_counter_alarm % full_period_alternance;

    if (phase_actuelle < half_period_alternance) {
      // Phase 1 (0 à 250 ms) : Signature sonore Intrusion
      frequence_buzzer(FREQ_BUZZER_ALARME_INTRUSION);
      buzzer = true; // Émission acoustique continue pendant le créneau
    } else {
      // Phase 2 (250 à 500 ms) : Signature sonore Gaz
      frequence_buzzer(FREQ_BUZZER_ALARME_GAZ);
      buzzer = led_alarme_gaz.state; // Modulation rapide synchronisée sur la LED gaz
    }
  }
}

/**
 * @brief  Fonction unique à appeler en boucle pour l'animation cyclique des alarmes.
 *         Incrémente le compteur de base de temps partagé (cadencé à 20 Hz par le Timer 5)
 *         et appelle les fonctions qui gèrent le clignotement des LEDs et du Buzzer.
 *         // cf rapport : Scénario 8 – Gestion globale des alarmes
 * @param  None
 * @return None
 */
void update_cycle_alarmes() 
{
  // Réinitialisation du compteur lors du retour au calme pour démarrer le prochain cycle en phase 0
  if (!alarme_gaz && !alarme_intrusion) {
    blink_counter_alarm = 0;
  } else {
    blink_counter_alarm++;
  }

  // Actualisation des clignotements lumineux et sonores
  update_cycle_alarmes_leds();
  update_cycle_alarmes_buzzer();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM5_Init();
  MX_ADC1_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_I2C3_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */

  // Démarrage des bases temporelles en interruption :
  // - TIMER_DEBOUNCE (TIM5) : cadence l'anti-rebond et les alarmes à 20 Hz (période = 50 ms)
  //   En mode base temporelle (_BASE) car simple comptage cyclique.
  //   En mode interruption (_IT) pour déclencher debounce_filter() et la mise à jour des alarmes.
  //   cf rapport : Scénario 2 – 2.1. Prescaler du Timer 5 & Scénario 2 – 2.2. Période du Timer 5
  HAL_TIM_Base_Start_IT(TIMER_DEBOUNCE);

  // - TIMER_CAPTEURS (TIM2) : cadence l'échantillonnage à 0.5 Hz (période = 2 s)
  //   En mode base temporelle (_BASE) car simple comptage cyclique.
  //   En mode interruption (_IT) pour déclencher le cycle des capteurs.
  //   Émet automatiquement un signal TRGO matériel à chaque update pour déclencher l'ADC
  //   cf rapport : Scénario 1 – 1. Configuration du Timer & Scénario 3 – 1. Déclenchement de l'ADC par le Timer 2
  HAL_TIM_Base_Start_IT(TIMER_CAPTEURS);

  // Démarrage des canaux PWM pour les actionneurs :
  // - TIMER_BUZZER (TIM3 CH2)
  //   En mode PWM (_PWM) pour générer l'onde carrée sur le buzzer passif.
  //   cf rapport : Scénario 8 – 2. Gestion du buzzer avec fréquences distinctes
  HAL_TIM_PWM_Start(TIMER_BUZZER, CANAL_BUZZER);

  // - TIMER_SERVO (TIM1 CH2) : PWM 50 Hz pour pilotage angulaire de la fenêtre
  //   En mode PWM (_PWM) pour générer le signal de commande angulaire du servo.
  //   cf rapport : Scénario 5 – 1.3. Configuration du Timer 1 dans CubeMX
  HAL_TIM_PWM_Start(TIMER_SERVO, CANAL_SERVO);

  // Démarrage de la chaîne d'acquisition analogique multi-canaux (Gaz + Pluie)
  // - ADC_CAPTEURS (ADC1)
  //   En mode DMA (_DMA) pour stocker les mesures acquises en mémoire.
  //   Utilise le Scan circulaire pour acquérir plusieurs canaux successivement.
  // En paramètres : 
  //   - adc_valeurs : tableau de stockage des mesures
  //   - NBR_CAPTEURS_ADC : nombre de canaux à acquérir
  // cf rapport : Scénario 5 – 4.3. Configuration DMA
  HAL_ADC_Start_DMA(ADC_CAPTEURS, (uint32_t*)adc_valeurs, NBR_CAPTEURS_ADC);

  // Initialisation des bibliothèques utilisées
  // cf rapport : Scénario 6 – 6. Initialisation, Scénario 7 – 2. Intégration de la bibliothèque LCD I²C,
  //              Scénario 9 – 5. Initialisation
  initialiser_thermometre();
  initialiser_ecran();
  initialiser_bande_led();

  // Affichage des messages d'accueil et récapitulatif des paramètres système sur l'UART
  // cf rapport : Scénario 4 – 3. Message de bienvenue au démarrage
  printf("\r\n--- Demarrage Systeme STM32 ---\r\n\n");
  printf("Seuil gaz configure: %d\r\n", SEUIL_GAZ);
  printf("Seuil pluie configure: %d\r\n", SEUIL_PLUIE);
  printf("Frequence clignotement alarme gaz: %d Hz\r\n", FREQ_BLINK_ALARME_GAZ);
  printf("Frequence clignotement alarme intrusion: %d Hz\r\n", FREQ_BLINK_ALARME_INTRUSION);
  printf("Periode echantillonnage capteurs: %d s\r\n", (int)(1/FREQ_CYCLE_CAPTEURS));
  printf("Periode debounce bouton: %d ms\r\n", (int)(1000/FREQ_CYCLE_DEBOUNCE));
  printf("\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // ==========================================================================
    // Machine à états principale : Modes de présence HOME et AWAY
    // cf rapport : Scénario 2 – 3. Machine à états HOME / AWAY & Scénario 2 – 5. Comportement selon le mode (AWAY vs HOME)
    // ==========================================================================
    switch(mode) 
    {
      case HOME: 
      {
        // --- Événement de transition HOME -> AWAY ---
        // L'utilisateur appuie sur le bouton HOME pour quitter la maison
        if (bouton_home_appuye) 
        {
          bouton_home_appuye = false; // Acquittement du flag de bouton
          mode = AWAY; // Changement de mode vers AWAY
          fenetre = FERMEE; // Sécurité départ : fermeture systématique de la fenêtre
          printf("Je sors de la maison\r\n");
          afficher_mode_ecran(); // Actualisation immédiate du libellé LCD
        }
        else 
        {
          // --- Actions continues en mode HOME (Présence normale) ---
          // On éteint l'alarme si jamais elle était armée
          alarme_intrusion = false;
          // Réinitialisation permanente des détections intrusion pour éviter 
          // que l'alarme se déclenche sur la transition suite à un appui en mode HOME
          presence_detectee = false;

          // Priorité 1 fenêtre : La pluie maintient la fermeture 
          // cf rapport : Scénario 5 – 5. Logique de décision de la fenêtre
          if (pluie_detectee)
          {
            fenetre = FERMEE;
            bouton_fenetre_appuye = false; // Ignore tout appui utilisateur pendant l'intempérie
          } 
          else 
          {
            // Priorité 2 : Commande manuelle de la fenêtre occasionnelle par bouton poussoir
            // cf rapport : Scénario 5 – 3.2. Comportement et priorités selon le mode (HOME vs AWAY)
            if (bouton_fenetre_appuye) 
            {
              bouton_fenetre_appuye = false; // Acquittement du flag d'appui
              switch(fenetre)
              {
                // Si elle est ouverte, on la ferme
                case OUVERTE: 
                {
                  fenetre = FERMEE;
                  printf("Je ferme la fenetre\r\n");
                  break;
                }
                // Si elle est fermée, on l'ouvre
                case FERMEE: 
                {
                  fenetre = OUVERTE;
                  printf("J'ouvre la fenetre\r\n");
                  break;
                }
                // État initial utile uniquement pour la fonction commande_fenetre()
                // On a mis ça juste pour éviter les warnings du compilateur (switch incomplet)
                case INIT:
                  break;
              }
            }
          }
        }
        break;
      }

      case AWAY: 
      {
        // --- Événement de transition AWAY -> HOME ---
        // L'utilisateur appuie sur le bouton HOME à son retour
        if (bouton_home_appuye) 
        {
          bouton_home_appuye = false; // Acquittement du flag 
          mode = HOME; // Changement de mode vers HOME
          printf("Je rentre à la maison\r\n");
          afficher_mode_ecran(); // Actualisation immédiate du libellé LCD
        } 
        else 
        {
          // --- Actions continues en mode AWAY ---
          // Verrouillage de la commande manuelle : les appuis fenêtre sont ignorés
          // cf rapport : Scénario 5 – 3.2. Comportement et priorités selon le mode (HOME vs AWAY)
          bouton_fenetre_appuye = false;
          
          // Surveillance active du détecteur de mouvement PIR
          // cf rapport : Scénario 2 – 5.3. Alarme intrusion
          if (presence_detectee) 
          {
            alarme_intrusion = true;    // Armement et déclenchement de l'alarme intrusion
            presence_detectee = false;  // Acquittement du flag de détection de présence
            printf("Intrusion detectee !\r\n");
          }

          // N.B. L'alarme reste active tant qu'on ne rentre pas à la maison
        }
        break;
      }
    }

    // ==========================================================================
    // Traitement des tâches périodiques non bloquantes cadencées par interruptions
        // cf rapport : Architecture globale – 3.1. Boucle principale et gestion des interruptions
    // ==========================================================================
    
    // Tâche 1 : Base de temps 50 ms (TIM5) - Anti-rebond, modulation alarmes, NeoPixels
    // cf rapport : Scénario 2 – 2. Gestion de l'anti-rebond, Scénario 9 – 8.4. Mise à jour périodique,
    //              Scénario 8 – 1. Clignotement non bloquant par compteur partagé
    if (timer_debounce_flag) {
      timer_debounce_flag = false; // Acquittement du flag d'interruption
      debounce_filter();           // Évaluation de la stabilité des boutons (50 ms après front)
      update_cycle_alarmes();      // Cadencement du clignotement des LEDs et alternance buzzer
      commande_bande_led();          // Actualisation de l'état des NeoPixels en fonction des variables
    }
    
    // Tâche 2 : Base de temps 2 s (TIM2) - Déclenchement One-Shot capteur température
    // N.B. L'ADC est déclenché en arrière-plan par le signal matériel TRGO du TIM2
    // cf rapport : Scénario 1 – 2. Clignotement de la LED témoin & Scénario 6 – 9.2. Déclencher une mesure One-Shot
    if (timer_capteurs_flag) {
      timer_capteurs_flag = false; // Acquittement du flag d'interruption
      led_capteurs.state = !led_capteurs.state; // Clignotement témoin d'activité (LED bleue)
      if (!TMP102_start_measure(&thermometre)) {
        printf("Erreur lors du demarrage de la mesure TMP102 en mode One-Shot\r\n");
      }
    }
    
    // Tâche 3 : Fin de conversion ADC multi-canaux notifiée par le DMA (Gaz + Pluie)
    // cf rapport : Scénario 5 – 4.5. Lecture et évaluation des capteurs
    if (fin_mesure_ADC_flag) {
      fin_mesure_ADC_flag = false; // Acquittement du flag d'interruption
      evaluer_pluie(); // Mise à jour de la valeur du capteur et vérification du seuil de pluie
      evaluer_gaz();   // Mise à jour de la valeur du capteur et vérification du seuil de gaz
    }
    
    // Tâche 4 : Fin de conversion du thermomètre TMP102 prête (polling non bloquant ~10 ms après le lancement)
    // La température étant la dernière mesure disponible, l'afficheur LCD est rafraîchi ici
    // cf rapport : Scénario 6 – 9.3. Attendre la fin de la mesure & Scénario 7 – 3.3. Valeurs capteurs
    if (TMP102_is_measure_ready(&thermometre)) {
      evaluer_temperature();     // Mise à jour de la température et de l'état thermique de la maison
      printf("\r\n");            // Saut de ligne pour différencier les séries de 3 mesures successives
      afficher_capteurs_ecran(); // Rafraîchissement global synchronisé de l'écran LCD
    }
    
    // ==========================================================================
    // Pilotage des actionneurs physiques (fonctions non bloquantes)
    // ==========================================================================
    commande_buzzer();   // Actualisation de l'état ON/OFF du signal PWM sonore
    commande_led();      // Actualisation des broches GPIO des LEDs du shield
    commande_fenetre();  // Actualisation de la position PWM du servomoteur sur changement d'état
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
 * @brief  Redirige le flux de sortie standard C (utilisé par printf) vers l'USART2.
 *         Permet l'envoi caractère par caractère vers l'interface ST-LINK branchée au PC.
 *         //  cf rapport : Scénario 4 – 2. Redirection de printf vers l'UART
 * @param  ch Caractère ASCII à transmettre.
 * @return int Caractère émis ou EOF en cas d'erreur (non utilisé)
 */
int __io_putchar(int ch) 
{
    // Émission synchrone bloquante de 1 octet sur USART2 avec timeout maximal
    // cf rapport : Scénario 4 – 2. Redirection de printf vers l'UART
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

/**
 * @brief  Callback appelée par la routine d'interruption pour les interruptions externes sur broches GPIO (EXTI).
 *         Capture les événements asynchrones en temps réel.
 *         cf rapport : Scénario 2 – 2.3. Routine d'interruption du GPIO et du Timer,
 *                      Scénario 2 – 4.2. Lecture du capteur PIR, Scénario 5 – 3.1. Implémentation et anti-rebond
 * @param  GPIO_Pin Numéro du pin GPIO ayant déclenché l'interruption.
 * @return None
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  // 1. Détection de mouvement par le capteur PIR (broche PC7 / EXTI 7)
  // Signal actif sur front montant avec pulldown interne
  // cf rapport : Scénario 2 – 4.2. Lecture du capteur PIR
  if (GPIO_Pin == PIR_MVMT_Pin)
  {
    presence_detectee = true; // Activation du flag de détection de présence
  }

  // 2. Appui sur le bouton HOME (broche PA6 / EXTI 6)
  // Déclenché sur front descendant (pull-up interne, passage de 1 à 0)
  // cf rapport : Scénario 2 – 2.3. Routine d'interruption du GPIO et du Timer
  if (GPIO_Pin == BTN_HOME_Pin)
  {
    if (!debouncing_bouton_home) {
      debouncing_bouton_home = true; // Active la détection d'appui pour le bouton HOME
      // Amorce le délai anti-rebond de 50 ms en réinitialisant le compteur de TIM5 à zéro
      __HAL_TIM_SET_COUNTER(TIMER_DEBOUNCE, 0);
    }
  }

  // 3. Appui sur le bouton FENÊTRE (broche PA10 / EXTI 10)
  // Déclenché sur front descendant (pull-up interne)
  // cf rapport : Scénario 5 – 3.1. Implémentation et anti-rebond
  if (GPIO_Pin == BTN_FENTR_Pin)
  {
    if (!debouncing_bouton_fenetre) {
      debouncing_bouton_fenetre = true; // Active la détection d'appui pour le bouton FENÊTRE
      // Amorce le délai anti-rebond de 50 ms en réinitialisant le compteur de TIM5 à zéro
      __HAL_TIM_SET_COUNTER(TIMER_DEBOUNCE, 0);
    }
  }
}

/**
 * @brief  Callback déclenchée à l'atteinte de la période de n'importe quel timer démarré en interruption.
 *         Lève les drapeaux logiciels asynchrones correspondants afin de déporter les traitements
 *         dans la boucle principale.
 *         // cf rapport : Architecture globale – 3.1. Boucle principale et gestion des interruptions, Scénario 1 – 1. Configuration du Timer, Scénario 2 – 2.1. Prescaler du Timer 5
 * @param  htim Pointeur vers la structure du timer ayant généré l'interruption.
 * @return None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  // Timer d'anti-rebond et de clignotement des alarmes (TIM5, 20 Hz / 50 ms)
  // cf rapport : Scénario 2 – 2.3. Routine d'interruption du GPIO et du Timer
  //             Scénario 8 – 1. Clignotement non bloquant par compteur partagé
  if (htim == TIMER_DEBOUNCE)
  {
    timer_debounce_flag = true; // Activation du flag du timer d'anti-rebond
  }

  // Timer d'échantillonnage (TIM2, 0.5 Hz / 2 s)
  // cf rapport : Scénario 1 – 1. Configuration du Timer
  if (htim == TIMER_CAPTEURS)
  {
    timer_capteurs_flag = true; // Activation du flag du timer d'échantillonnage
  }
}

/**
 * @brief  Callback appelée par le contrôleur DMA à la fin de chaque cycle de conversion ADC.
 *         Confirme que les mesures des canaux Gaz (Rank 1) et Pluie (Rank 2) 
 *         ont été transférées dans le buffer en mémoire.
 *         //  cf rapport : Scénario 5 – 4.4. Interruptions ADC et DMA activées
 * @param  hadc Pointeur vers la structure du convertisseur ADC (ADC_CAPTEURS / hadc1).
 * @return None
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc == ADC_CAPTEURS)
    {
        // cf rapport : Scénario 3 – 2. Lecture de la valeur ADC au Scénario 3 et problème rencontré & Scénario 5 – 4.4. Interruptions ADC et DMA activées
        fin_mesure_ADC_flag = true; // Activation du flag de fin de mesure ADC
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
