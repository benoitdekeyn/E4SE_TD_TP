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
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    bool state;
} LED_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// Liste des pins configurés
//  BTN_FENTR | D2  | PA_10 | EXTI 10 - Rising Edge | PULLUP   | 3V3
//  LED_JAUNE | D3  | PB_3  | GPIO OUTPUT           | NO       | 3V3
//  BUZZER    | D4  | PB_5  | TIMER 3 - CHANNEL 2   | NO       | 3V3
//  I2C_SDA   | D5  | PB_4  | I2C3 - SDA            | NO       | LCD+TEMP -> 5V
//  LED_BLEUE | D6  | PB_10 | GPIO OUTPUT           | NO       | 3V3
//  I2C_SCL   | D7  | PA_8  | I2C3 - SCL            | NO       | LCD+TEMP -> 5V
//  SERVO     | D8  | PA_9  | TIMER 1 - CHANNEL 2   | NO       | 5V
//  PIR_MVMT  | D9  | PC_7  | EXTI 7 - Rising Edge  | PULLDOWN | 3V3
//  RGB_BANDE | D10 | PB_6  | TIMER 4 - CHANNEL 1   | NO       | 5V
//  RGB_ROUGE | D11 | PA_7  | GPIO OUTPUT           | NO       | 3V3
//  BTN_HOME  | D12 | PA_6  | EXTI 6 - Rising Edge  | PULLDOWN | 3V3
//  GAZ       | A2  | PA_4  | ADC 1 - CHANNEL 4     | NO       | 5V
//  PLUIE     | A3  | PB_0  | ADC 1 - CHANNEL 8     | NO       | 5V

#define ADC_CAPTEURS    (&hadc1)
#define BUS_I2C         (&hi2c3)
#define TIMER_CAPTEURS  (&htim2)
#define TIMER_DEBOUNCE  (&htim5)
#define TIMER_SERVO     (&htim1)
#define CANAL_SERVO     TIM_CHANNEL_2
#define TIMER_BUZZER    (&htim3)
#define CANAL_BUZZER    TIM_CHANNEL_2
#define TIMER_BANDE_LED (&htim4)
#define CANAL_BANDE_LED TIM_CHANNEL_1

#define SEUIL_GAZ         2500
#define SEUIL_PLUIE       4000
#define SEUIL_CHALEUR     26
#define SEUIL_FROID       24
#define NBR_CAPTEURS_ADC  2
#define GAZ_ADC_RANK      0
#define PLUIE_ADC_RANK    1

#define FREQ_CYCLE_CAPTEURS   0.5
#define FREQ_CYCLE_DEBOUNCE   20

#define FREQ_TIMER_SERVO  2500000
#define FREQ_CYCLE_SERVO  50
#define SERVO_0_DEG_MS    0.5
#define SERVO_180_DEG_MS  2.5

#define FREQ_TIMER_BUZZER             25000000
#define FREQ_TIMER_BANDE_LED          100000000
#define FREQ_CYCLE_ALARME             FREQ_CYCLE_DEBOUNCE
#define FREQ_BLINK_ALARME_GAZ         10
#define FREQ_BLINK_ALARME_INTRUSION   2
#define FREQ_BUZZER_ALARME_GAZ        440
#define FREQ_BUZZER_ALARME_INTRUSION  384

#define NB_LEDS_BANDE_RGB 4
#define LED_BAS_DROITE    0
#define LED_HAUT_DROITE   1
#define LED_HAUT_GAUCHE   2
#define LED_BAS_GAUCHE    3
#define LUMINOSITE_BASE   10 // 0-255

// --- Couleurs du Tableau de bord LED ---
#define COULEUR_CAPTEURS         BL_HEXA_VIOLET
#define COULEUR_ALARME_GAZ       BL_HEXA_ROUGE
#define COULEUR_ALARME_INTRUSION BL_HEXA_ORANGE
#define COULEUR_TEMP_FROID       BL_HEXA_BLEU
#define COULEUR_TEMP_TEMPERE     BL_HEXA_VERT
#define COULEUR_TEMP_CHAUD       BL_HEXA_ORANGE
#define COULEUR_ETEINTE          BL_HEXA_NOIR

// Maison
typedef enum{
  HOME,
  AWAY
} maison_mode_t;
// Fenetre
typedef enum {INIT, FERMEE, OUVERTE} fenetre_t;
#define ANGLE_SERVO_OUVERT  120 // ATTENTION maximum 120° sinon ça casse
#define ANGLE_SERVO_FERME   0   // Montage effectué avec 0° fenetre fermee
// Temperature
typedef enum {FROID, CHAUD, TEMEPERE} temperature_t;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile LED_t  led_capteurs                = {LED_BLEUE_GPIO_Port, LED_BLEUE_Pin, false};
volatile LED_t  led_alarme_gaz              = {RGB_ROUGE_GPIO_Port, RGB_ROUGE_Pin, false};
volatile LED_t  led_alarme_intrusion        = {LED_JAUNE_GPIO_Port, LED_JAUNE_Pin, false};
volatile bool   buzzer                      = false;
volatile fenetre_t fenetre                  = OUVERTE;

volatile bool   debouncing_bouton_home      = false;
volatile bool   debouncing_bouton_fenetre   = false;
volatile bool   bouton_home_appuye          = false;
volatile bool   bouton_fenetre_appuye       = false;

volatile bool   timer_debounce_flag         = false;
volatile bool   timer_capteurs_flag         = false;
volatile bool   fin_mesure_ADC_flag         = false;
volatile int    blink_counter_alarm         = 0;

volatile maison_mode_t mode                 = HOME;
volatile bool   presence_detectee           = false;
volatile bool   pluie_detectee              = false;
volatile bool   alarme_intrusion            = false;
volatile bool   alarme_gaz                  = false;

volatile temperature_t temperature          = TEMEPERE;
volatile float temperature_valeur           = 0.0;
volatile int   pluie_valeur                 = 0;
volatile int   gaz_valeur                   = 0;

volatile uint16_t adc_valeurs[NBR_CAPTEURS_ADC];
TMP102_t thermometre;
BandeLED_t bande_led;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void afficher_capteurs_ecran() 
{
    char ligne1[8+1];
    char ligne2[16+1];

    // Ligne 1 2nde moitié : " 18,38°C"
    snprintf(ligne1, sizeof(ligne1), " %5s\xDF" "C",
             TMP102_temperature_to_string(temperature_valeur, 2));

    // Ligne 2 : "Hu 1234  Ga 1234"
    snprintf(ligne2, sizeof(ligne2), "Hu %4d  Ga %4d", 
             pluie_valeur, 
             gaz_valeur);

    lcd_put_cur(0, 8);
    lcd_send_string(ligne1);

    lcd_put_cur(1, 0);
    lcd_send_string(ligne2);
}

void afficher_mode_ecran () 
{ 
  char ligne_mode[8];
  snprintf(ligne_mode, sizeof(ligne_mode), "%4s",
  (mode == HOME) ? "HOME" : "AWAY");

  lcd_put_cur(0, 0);
  lcd_send_string(ligne_mode);
}

void initialiser_ecran()
{
    lcd_init(BUS_I2C);
    afficher_mode_ecran();
    lcd_put_cur(1, 0);
    lcd_send_string("Hello Abir (^w^)");
}

void initialiser_thermometre() 
{
    TMP102_init_default(&thermometre);

    thermometre.busI2C                = BUS_I2C;
    thermometre.address               = TMP102_I2C_ADDR_GND;
    thermometre.config.Mode           = TMP102_MODE_SHUTDOWN;
    thermometre.config.ExtendedMode   = TMP102_EXTENDED_OFF;

    if (TMP102_apply_registers(&thermometre)) {
        printf("Capteur TMP102 configuré\r\n");
    } else {
        printf("Erreur d'initialisation du capteur TMP102\r\n");
    }
}

void initialiser_bande_led()
{
  BandeLED_Init(&bande_led, TIMER_BANDE_LED, CANAL_BANDE_LED, FREQ_TIMER_BANDE_LED, NB_LEDS_BANDE_RGB);
  for (int i = 0; i < NB_LEDS_BANDE_RGB; i++) {
      BandeLED_set_led_luminosity(&bande_led, i, LUMINOSITE_BASE);
  }

  // Affectation des couleurs fixes une seule fois en mémoire
  BandeLED_set_led_hexa(&bande_led, LED_HAUT_GAUCHE, COULEUR_CAPTEURS);
  BandeLED_set_led_hexa(&bande_led, LED_HAUT_DROITE, COULEUR_ALARME_GAZ);
  BandeLED_set_led_hexa(&bande_led, LED_BAS_DROITE, COULEUR_ALARME_INTRUSION);

  // Extinction de toutes les LEDs pour commencer proprement
  BandeLED_set_led_state(&bande_led, LED_HAUT_GAUCHE, false);
  BandeLED_set_led_state(&bande_led, LED_HAUT_DROITE, false);
  BandeLED_set_led_state(&bande_led, LED_BAS_DROITE, false);
  BandeLED_set_led_state(&bande_led, LED_BAS_GAUCHE, false);

  BandeLED_show(&bande_led);
}

void update_bande_led()
{
    // LED Capteurs (Couleur en mémoire, on gère juste l'état)
    BandeLED_set_led_state(&bande_led, LED_HAUT_GAUCHE, led_capteurs.state);

    // LED Alarme Gaz (Couleur en mémoire, on gère juste l'état)
    BandeLED_set_led_state(&bande_led, LED_HAUT_DROITE, led_alarme_gaz.state);

    // LED Alarme Intrusion (Couleur en mémoire, on gère juste l'état)
    BandeLED_set_led_state(&bande_led, LED_BAS_DROITE, led_alarme_intrusion.state);

    // LED Température (Couleur dynamique, on applique la couleur et on allume/éteint)
    switch (temperature) {
        case FROID:
            BandeLED_set_led_state(&bande_led, LED_BAS_GAUCHE, true);
            BandeLED_set_led_hexa(&bande_led, LED_BAS_GAUCHE, COULEUR_TEMP_FROID);
            break;
        case TEMEPERE:
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

    // Envoi du signal physique
    BandeLED_show(&bande_led);
}

void angle_servo(int angle)
{
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

	__HAL_TIM_SET_COMPARE(TIMER_SERVO, CANAL_SERVO, pulse_tics);
}

void commande_fenetre()
{ static fenetre_t etat_precedent = INIT;
  if (fenetre != etat_precedent) {
  //printf("Commande fenetre: %s\r\n", (fenetre == OUVERTE) ? "OUVERTE" : "FERMEE");
  switch(fenetre) {
    case OUVERTE :
      angle_servo(ANGLE_SERVO_OUVERT);
      break;
    case FERMEE :
      angle_servo(ANGLE_SERVO_FERME);
      break;
    case INIT :
      break;
    }
  etat_precedent = fenetre;
  }
}

void commande_led()
{
  if (led_capteurs.state) {
    HAL_GPIO_WritePin(led_capteurs.port, led_capteurs.pin, 1);
  } else {
    HAL_GPIO_WritePin(led_capteurs.port, led_capteurs.pin, 0);
  }
  if (led_alarme_gaz.state) {
    HAL_GPIO_WritePin(led_alarme_gaz.port, led_alarme_gaz.pin, 1);
  } else {
    HAL_GPIO_WritePin(led_alarme_gaz.port, led_alarme_gaz.pin, 0);
  }
  if (led_alarme_intrusion.state) {
    HAL_GPIO_WritePin(led_alarme_intrusion.port, led_alarme_intrusion.pin, 1);
  } else {
    HAL_GPIO_WritePin(led_alarme_intrusion.port, led_alarme_intrusion.pin, 0);
  }
}

void commande_buzzer() 
{
	if (buzzer) {
	  HAL_TIM_PWM_Start(TIMER_BUZZER, CANAL_BUZZER);
	} else {
	  HAL_TIM_PWM_Stop(TIMER_BUZZER, CANAL_BUZZER);
	}
}

void frequence_buzzer(int frequence) 
{
	int periode_tics = (double)FREQ_TIMER_BUZZER/frequence;
	int pulse_tics   = periode_tics/2;
	__HAL_TIM_SET_AUTORELOAD(TIMER_BUZZER, periode_tics);
	__HAL_TIM_SET_COMPARE(TIMER_BUZZER, CANAL_BUZZER, pulse_tics);
}

void evaluer_gaz() 
{
  gaz_valeur = adc_valeurs[GAZ_ADC_RANK];
	if (gaz_valeur >= SEUIL_GAZ) {
    printf("Gaz detecte !\r\n");
		alarme_gaz = true;
	} else {
		alarme_gaz = false;
	}
  printf("                                      GAZ   = %d\r\n", gaz_valeur);
}

void evaluer_pluie() 
{
  pluie_valeur = adc_valeurs[PLUIE_ADC_RANK];
	if (pluie_valeur >= SEUIL_PLUIE) {
		printf("Pluie detecte !\r\n");
		pluie_detectee = true;
	} else {
		pluie_detectee = false;
	}
	printf("                                      PLUIE = %d\r\n", pluie_valeur);
}

void evaluer_temperature() 
{
  float temperature_local;

  if (TMP102_get_temperature(&thermometre, &temperature_local)) 
    {
      temperature_valeur = temperature_local;
      printf("                                      TEMP  = %s°C\r\n", TMP102_temperature_to_string(temperature_valeur, 2));
    } 
    else 
    {
      printf("                                      TEMP  = ERREUR LECTURE\r\n");
    }
    
    // Mise à jour de l'état de la température
    if (mode == HOME && !pluie_detectee) 
    {
      switch(temperature) 
      {
        case CHAUD:
          if (temperature_valeur < SEUIL_CHALEUR)
            {
              temperature = TEMEPERE;
              break;
            }
          else
            {
              break;
            }
        case FROID:
          if (temperature_valeur > SEUIL_FROID)
            {
              temperature = TEMEPERE;
              break;
            }
          else
            {
            break;
            }
        case TEMEPERE:
            if (temperature_valeur >= SEUIL_CHALEUR && temperature != CHAUD)
              {
                temperature = CHAUD;
                fenetre = OUVERTE;
                break;
              }
            else if (temperature_valeur <= SEUIL_FROID && temperature != FROID)
              {
                temperature = FROID;
                fenetre = FERMEE;
                break;
              }
            else
              {
                break;
              }
        }
      }
}

void debounce_filter() 
{
	if (debouncing_bouton_home && !HAL_GPIO_ReadPin(BTN_HOME_GPIO_Port, BTN_HOME_Pin)) {
		bouton_home_appuye = true;
		debouncing_bouton_home = false;
	}

	if (debouncing_bouton_fenetre && !HAL_GPIO_ReadPin(BTN_FENTR_GPIO_Port, BTN_FENTR_Pin)) {
		bouton_fenetre_appuye = true;
		debouncing_bouton_fenetre = false;
	}
}
void update_cycle_alarmes_leds() 
{
  if (!alarme_gaz && !alarme_intrusion) {
    led_alarme_intrusion.state = false;
    led_alarme_gaz.state = false;
    return;
  }

  if (alarme_gaz) {
    int demi_periode_gaz = FREQ_CYCLE_ALARME / (2 * FREQ_BLINK_ALARME_GAZ);
    if (blink_counter_alarm % demi_periode_gaz == 0) {
      led_alarme_gaz.state = !led_alarme_gaz.state;
    }
  } else {
    led_alarme_gaz.state = false;
  }

  if (alarme_intrusion) {
    int demi_periode_intrusion = FREQ_CYCLE_ALARME / (2 * FREQ_BLINK_ALARME_INTRUSION);
    if (blink_counter_alarm % demi_periode_intrusion == 0) {
      led_alarme_intrusion.state = !led_alarme_intrusion.state;
    }
  } else {
    led_alarme_intrusion.state = false;
  }
}

void update_cycle_alarmes_buzzer() 
{
  if (!alarme_gaz && !alarme_intrusion) {
    buzzer = false;
    return;
  }

  // Uniquement Gaz
  if (alarme_gaz && !alarme_intrusion) {
    frequence_buzzer(FREQ_BUZZER_ALARME_GAZ);
    buzzer = led_alarme_gaz.state; // Synchronisé avec la LED Gaz
  }
  // Uniquement Intrusion
  else if (!alarme_gaz && alarme_intrusion) {
    frequence_buzzer(FREQ_BUZZER_ALARME_INTRUSION);
    buzzer = led_alarme_intrusion.state; // Synchronisé avec la LED Intrusion
  }
  // Gaz ET Intrusion (Alternance)
  else if (alarme_gaz && alarme_intrusion) {
    int full_period_alternance = FREQ_CYCLE_ALARME / FREQ_BLINK_ALARME_INTRUSION;
    int half_period_alternance = full_period_alternance / 2;
    int phase_actuelle = blink_counter_alarm % full_period_alternance;

    if (phase_actuelle < half_period_alternance) {
      // Phase 1 : Son Intrusion
      frequence_buzzer(FREQ_BUZZER_ALARME_INTRUSION);
      buzzer = true; // Son continu pendant cette phase
    } else {
      // Phase 2 : Son Gaz
      frequence_buzzer(FREQ_BUZZER_ALARME_GAZ);
      buzzer = led_alarme_gaz.state; // Suit le clignotement rapide pendant cette phase
    }
  }
}

// Fonction globale à appeler dans le main
void update_cycle_alarmes() 
{
  // Gestion du compteur partagé
  if (!alarme_gaz && !alarme_intrusion) {
    blink_counter_alarm = 0;
  } else {
    blink_counter_alarm++;
  }

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
  HAL_TIM_Base_Start_IT(TIMER_DEBOUNCE);
  HAL_TIM_Base_Start_IT(TIMER_CAPTEURS);
  HAL_TIM_PWM_Start(TIMER_BUZZER, CANAL_BUZZER);
  HAL_TIM_PWM_Start(TIMER_SERVO, CANAL_SERVO);
  HAL_ADC_Start_DMA(ADC_CAPTEURS, (uint32_t*)adc_valeurs, NBR_CAPTEURS_ADC);

  initialiser_thermometre();
  initialiser_ecran();
  initialiser_bande_led();


  printf("\r\n--- Demarrage Systeme STM32 ---\r\n\n");
  printf("Seuil gaz configure: %d\r\n", SEUIL_GAZ);
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

    // Machine à état avec HOME et AWAY
    switch(mode) {
    case  HOME: 
    {
      if      (bouton_home_appuye) 
      {
        bouton_home_appuye = false;
        mode = AWAY;
        fenetre = FERMEE;
        printf("Je sors de la maison\r\n");
        afficher_mode_ecran();
      }
      else if (pluie_detectee)
      {
        fenetre = FERMEE;
    	  bouton_fenetre_appuye = false;
      } 
      else 
      {
        alarme_intrusion = false;
        if (bouton_fenetre_appuye) 
        {
          bouton_fenetre_appuye = false;
          switch(fenetre){
          case OUVERTE: 
          {
            fenetre = FERMEE;
            printf("Je ferme la fenetre\r\n");
            break;
          } 
          case FERMEE: 
          {
            fenetre = OUVERTE;
            printf("J'ouvre la fenetre\r\n");
            break;
          }
          case INIT:
            // pris en charge au premier appel de commande_fenetre()
            break;
        }
      }
    }
    break;
    }
    case AWAY:
    {
    	bouton_fenetre_appuye = false;
      if      (bouton_home_appuye) 
      {
        bouton_home_appuye = false;
        mode = HOME;
        printf("Je rentre à la maison\r\n");
        afficher_mode_ecran();
      } 
      else if (presence_detectee) 
      {
        alarme_intrusion = true;
        presence_detectee = false;
        printf("Intrusion detectee !\r\n");
      }
      break;
      }
    }

    // en continu
    if (timer_debounce_flag) {
    	timer_debounce_flag = false;
      debounce_filter();
      update_cycle_alarmes();
      update_bande_led();
    }
    if (timer_capteurs_flag) {
      timer_capteurs_flag = false;
      led_capteurs.state = !led_capteurs.state;
      if (!TMP102_start_measure(&thermometre)) {
        printf("Erreur lors du demarrage de la mesure TMP102 en mode One-Shot\r\n");
      }
      // + Lancement des mesures ADC automatique (TGO event TIMER)
    }
    if (fin_mesure_ADC_flag){
      fin_mesure_ADC_flag = false;
      evaluer_pluie();
      evaluer_gaz();
    }
    if (TMP102_is_measure_ready(&thermometre)) {
      evaluer_temperature();
      // derniere mesure a etre evaluee
      printf("\r\n");
      afficher_capteurs_ecran();
    }
    commande_buzzer();
    commande_led();
    commande_fenetre();
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

// Redirection du printf vers USART2
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

// INterruption des GPIO
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == PIR_MVMT_Pin)
  {
    if (mode == AWAY){
      presence_detectee = true;
		}
	}
  if (GPIO_Pin == BTN_HOME_Pin)
  {
    if (!debouncing_bouton_home) {
      debouncing_bouton_home = true;
      __HAL_TIM_SET_COUNTER(TIMER_DEBOUNCE, 0);
    }
  }
	if (GPIO_Pin == BTN_FENTR_Pin)
  {
		if (!debouncing_bouton_fenetre) {
			debouncing_bouton_fenetre = true;
			__HAL_TIM_SET_COUNTER(TIMER_DEBOUNCE, 0);
		}
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim == TIMER_DEBOUNCE)
  {
	  timer_debounce_flag = true;
  }
  if (htim == TIMER_CAPTEURS)
  {
    timer_capteurs_flag = true;
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc == ADC_CAPTEURS)
    {
        fin_mesure_ADC_flag = true;
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