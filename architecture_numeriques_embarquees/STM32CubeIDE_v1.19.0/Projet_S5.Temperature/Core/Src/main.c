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
#include "stdbool.h"
#include <stdio.h>
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
//  RGB_STRIP | D10 | PB_6  | TIMER 4 - CHANNEL 1   | NO       | 5V
//  RGB_ROUGE | D11 | PA_7  | GPIO OUTPUT           | NO       | 3V3
//  BTN_HOME  | D12 | PA_6  | EXTI 6 - Rising Edge  | PULLDOWN | 3V3
//  GAZ       | A2  | PA_4  | ADC 1 - CHANNEL 4     | NO       | 5V
//  PLUIE     | A3  | PB_0  | ADC 1 - CHANNEL 8     | NO       | 5V

#define TIMER_CAPTEURS (&htim2)
#define TIMER_DEBOUNCE (&htim5)
#define TIMER_SERVO (&htim1)
#define CANAL_SERVO TIM_CHANNEL_2
#define TIMER_BUZZER (&htim3)
#define CANAL_BUZZER TIM_CHANNEL_2

#define SEUIL_GAZ 2200
#define SEUIL_PLUIE 3000
#define ADC_CAPTEURS (&hadc1)
#define NBR_CAPTEURS_ADC 2
#define GAZ_ADC_RANK 0
#define PLUIE_ADC_RANK 1

#define FREQ_CYCLE_CAPTEURS 0.5
#define FREQ_CYCLE_DEBOUNCE 20

#define FREQ_TIMER_SERVO 2500000
#define FREQ_CYCLE_SERVO 50
#define SERVO_0_DEG_MS 0.5
#define SERVO_180_DEG_MS 2.5

#define FREQ_TIMER_BUZZER 25000000
#define FREQ_CYCLE_ALARME FREQ_CYCLE_DEBOUNCE
#define FREQ_BLINK_ALARME_GAZ 10
#define FREQ_BLINK_ALARME_INTRUSION 2
#define FREQ_BUZZER_ALARME_GAZ 440
#define FREQ_BUZZER_ALARME_INTRUSION 384

#define HOME true
#define AWAY false
#define OUVERTE true
#define FERMEE false


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
volatile bool   fenetre                     = false;

volatile bool   debouncing_bouton_home      = false;
volatile bool   debouncing_bouton_fenetre   = false;
volatile bool   bouton_home_appuye          = false;
volatile bool   bouton_fenetre_appuye       = false;

volatile bool   timer_debounce_flag         = false;
volatile bool   timer_capteurs_flag         = false;
volatile bool   fin_mesure_ADC_flag         = false;
volatile int    blink_counter_alarm         = 0;

volatile bool   mode                        = HOME;
volatile bool   presence_detectee           = false;
volatile bool   pluie_detectee              = false;
volatile bool   alarme_intrusion            = false;
volatile bool   alarme_gaz                  = false;

volatile int    gaz_valeur                  = 0;
volatile int    pluie_valeur                = 0;
volatile float  temperature_celsius         = 0.0;

volatile uint16_t adc_valeurs[NBR_CAPTEURS_ADC];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
{
	if (fenetre == OUVERTE){
		angle_servo(120);
	}
	if (fenetre == FERMEE){
		angle_servo(0);
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

void update_cycle_alarme() 
{
  // Aucune alarme
  if (!alarme_gaz && !alarme_intrusion) {
    buzzer = false;
    led_alarme_intrusion.state = false;
    led_alarme_gaz.state = false;
    blink_counter_alarm = 0;
    return;
  }

  blink_counter_alarm++;

  // Alarme GAZ
  if (alarme_gaz && !alarme_intrusion) {
    frequence_buzzer(FREQ_BUZZER_ALARME_GAZ);
    led_alarme_intrusion.state = false;

    if (blink_counter_alarm >= FREQ_CYCLE_ALARME / (2 * FREQ_BLINK_ALARME_GAZ)) {
      led_alarme_gaz.state = !led_alarme_gaz.state;
      buzzer = led_alarme_gaz.state;
      blink_counter_alarm = 0;
    }
  }

  // Alarme INTRUSION
  else if (!alarme_gaz && alarme_intrusion) {
    frequence_buzzer(FREQ_BUZZER_ALARME_INTRUSION);
    led_alarme_gaz.state = false;

    if (blink_counter_alarm >= FREQ_CYCLE_ALARME / (2 * FREQ_BLINK_ALARME_INTRUSION)) {
      led_alarme_intrusion.state = !led_alarme_intrusion.state;
      buzzer = led_alarme_intrusion.state;
      blink_counter_alarm = 0;
    }
  }

  // Alarme GAZ + INTRUSION
  else if (alarme_gaz && alarme_intrusion) {
    int half_period_intrusion = FREQ_CYCLE_ALARME / (2 * FREQ_BLINK_ALARME_INTRUSION);
    int full_period_intrusion = FREQ_CYCLE_ALARME / FREQ_BLINK_ALARME_INTRUSION;
    int half_period_gaz = FREQ_CYCLE_ALARME / (2 * FREQ_BLINK_ALARME_GAZ);

    // alarme intrusion en continu, alarme gaz eteinte
    if (blink_counter_alarm <= half_period_intrusion) {
      frequence_buzzer(FREQ_BUZZER_ALARME_INTRUSION);
      led_alarme_gaz.state = false;

      led_alarme_intrusion.state = true;
      buzzer = true;
    // alarme gaz clignote, alarme intrusion eteinte
    } else {
      frequence_buzzer(FREQ_BUZZER_ALARME_GAZ);
      led_alarme_intrusion.state = false;

      if ((blink_counter_alarm - half_period_intrusion) % half_period_gaz == 0) {
        led_alarme_gaz.state = !led_alarme_gaz.state;
        buzzer = led_alarme_gaz.state;
      }
    }

    // fin periode alarme intrusion
    if (blink_counter_alarm >= full_period_intrusion) {
      blink_counter_alarm = 0;
      led_alarme_gaz.state = false;
    }
  }
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
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(TIMER_DEBOUNCE);
  HAL_TIM_Base_Start_IT(TIMER_CAPTEURS);
  HAL_TIM_PWM_Start(TIMER_BUZZER, CANAL_BUZZER);
  HAL_TIM_PWM_Start(TIMER_SERVO, CANAL_SERVO);
  HAL_ADC_Start_DMA(ADC_CAPTEURS, (uint32_t*)adc_valeurs, NBR_CAPTEURS_ADC);

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
    if (mode == HOME) 
    {
      if      (bouton_home_appuye) 
      {
        bouton_home_appuye = false;
        mode = AWAY;
        fenetre = false;
        printf("Je sors de la maison\r\n");
      }
      else if (pluie_detectee)
      {
        fenetre = false;
    	  bouton_fenetre_appuye = false;
      } 
      else if (bouton_fenetre_appuye) 
      {
        bouton_fenetre_appuye = false;
	  		if      (fenetre == OUVERTE) 
        {
          fenetre = FERMEE;
	  			printf("Je ferme la fenetre\r\n");
	  		} 
        else if (fenetre == FERMEE) 
        {
          fenetre = OUVERTE;
          printf("J'ouvre la fenetre\r\n");
        }
	  	}
      alarme_intrusion = false;
    }

    if ( mode == AWAY) 
    {
    	bouton_fenetre_appuye = false;
      if      (bouton_home_appuye) 
      {
        bouton_home_appuye = false;
        mode = HOME;
        printf("Je rentre à la maison\r\n");
      } 
      else if (presence_detectee) 
      {
        alarme_intrusion = true;
        presence_detectee = false;
        printf("Intrusion detectee !\r\n");
      }
    }


    // en continu
    if (timer_debounce_flag) {
    	timer_debounce_flag = false;
      debounce_filter();
      update_cycle_alarme();
    }
    if (timer_capteurs_flag) {
      timer_capteurs_flag = false;
      led_capteurs.state = !led_capteurs.state;
    }
    if (fin_mesure_ADC_flag){
      fin_mesure_ADC_flag = false;
      evaluer_pluie();
      evaluer_gaz();
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
