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

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// BTN_FEN   | D2  | PA_10 | INPUT PULLUP
// BUZZER    | D4  | PB_5  | TIMER 3 - CHANNEL 2
// JAUNE     | D5  | PB_4  | OUTPUT PULLDOWN
// BLEUE     | D6  | PB_10 | OUTPUT PULLDOWN
// BOUTON    | D7  | PB_3  | INPUT  PULLDOWN
// SERVO     | D8  | PA_9  | TIMER 1 - CHANNEL 2
// PIR       | D9  | PC_7  | INPUT  PULLDOWN
// RGB_BLEU  | D10 | PB_6  | OUTPUT PULLDOWN
// RGB_VERT  | D11 | PA_7  | OUTPUT PULLDOWN
// RGB_ROUGE | D12 | PA_6  | OUTPUT PULLDOWN
// GAZ       | A2  | PA_4  | ADC 1 - CHANNEL 4

#define TIMER_SERVO (&htim1)
#define TIMER_CAPTEURS (&htim2)
#define TIMER_BUZZER (&htim3)
#define TIMER_DEBOUNCE (&htim5)
#define SERVO_CHANNEL TIM_CHANNEL_2
#define BUZZER_CHANNEL TIM_CHANNEL_2

#define ADC_CAPTEURS (&hadc1)
#define NBR_CAPTEURS_ADC 2

#define TIMER_DEBOUNCE_FREQ 20
#define TIMER_SERVO_FREQ 2500000
#define TIMER_CAPTEURS_FREQ 0.5
#define TIMER_BUZZER_FREQ 440

#define FREQ_BLINK_ALARM_GAZ 4
#define FREQ_BLINK_ALARM_INTRUSION 2
#define SEUIL_GAZ 1700
#define SEUIL_PLUIE 3000

#define FREQ_PWM_SERVO 50
#define SERVO_0_DEG_MS 0.5
#define SERVO_180_DEG_MS 2.5

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
volatile bool mode = HOME;
volatile bool buzzer = false;
volatile bool falling_edge_home_detected = false;
volatile bool falling_edge_fen_detected = false;
volatile bool presence_detectee = false;
volatile bool alarme_gaz = false;
volatile bool alarme_intrusion = false;
volatile bool bouton_home_appuye = false;
volatile bool bouton_fen_appuye = false;
volatile bool fenetre = false;
volatile bool mesure_capteurs_prete = false;
volatile int blink_counter_alarm_gaz = 0;
volatile int blink_counter_alarm_intrusion = 0;
volatile int gaz_valeur = 0;
volatile int pluie_valeur = 0;
volatile uint16_t adc_valeurs[2];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void angle_servo(int angle){
	int angle_to_tics = (((SERVO_0_DEG_MS/1000.0) + (angle / 180.0) * (SERVO_180_DEG_MS - SERVO_0_DEG_MS)/1000.0)) * TIMER_SERVO_FREQ;
	__HAL_TIM_SET_COMPARE(TIMER_SERVO, SERVO_CHANNEL ,angle_to_tics);
}

void commande_fenetre(){
	if (fenetre == OUVERTE){
		angle_servo(120);
		printf("120 degres !\r\n");
	}
	if (fenetre == FERMEE){
		angle_servo(0);
		printf("0 degres !\r\n");

	}
}

void commande_buzzer() {
	if (buzzer) {
	  HAL_TIM_PWM_Start(TIMER_BUZZER, BUZZER_CHANNEL);
	} else {
	  HAL_TIM_PWM_Stop(TIMER_BUZZER, BUZZER_CHANNEL);
	}
}

void evaluer_gaz() {
	if (gaz_valeur >= SEUIL_GAZ) {
    printf("Gaz detecte !\r\n");
		alarme_gaz = true;
	} else {
		alarme_gaz = false;
	}
    printf("                                      VALEURS CAPTEURS: gaz = %d\r\n", gaz_valeur);
}

void evaluer_pluie() {
	if (pluie_valeur >= SEUIL_PLUIE) {
		printf("Pluie detecte !\r\n");
		fenetre = OUVERTE;
	} else {
		fenetre = FERMEE;
	}
	printf("                                      VALEURS CAPTEURS: pluie = %d\r\n", pluie_valeur);
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
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(TIMER_DEBOUNCE);
  HAL_TIM_Base_Start_IT(TIMER_CAPTEURS);
  HAL_TIM_PWM_Start(TIMER_BUZZER, BUZZER_CHANNEL);
  HAL_TIM_PWM_Start(TIMER_SERVO, SERVO_CHANNEL);
  HAL_ADC_Start_DMA(ADC_CAPTEURS, (uint32_t*)adc_valeurs, NBR_CAPTEURS_ADC);

  printf("\r\n--- Demarrage Systeme STM32 ---\r\n\n");
  printf("Seuil gaz configure: %d\r\n", SEUIL_GAZ);
  printf("Frequence clignotement alarme gaz: %d Hz\r\n", FREQ_BLINK_ALARM_GAZ);
  printf("Frequence clignotement alarme intrusion: %d Hz\r\n", FREQ_BLINK_ALARM_INTRUSION);
  printf("Periode echantillonnage capteurs: %d s\r\n", (int)(1/TIMER_CAPTEURS_FREQ));
  printf("Periode debounce bouton: %d ms\r\n", (int)(1000/TIMER_DEBOUNCE_FREQ));
  printf("\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	// Mesures capteurs
	if (mesure_capteurs_prete){
		mesure_capteurs_prete = false;
		evaluer_pluie();
		evaluer_gaz();
	}

    // Machine à état avec HOME et AWAY
    if (mode == HOME) {
      if(bouton_home_appuye) {
        bouton_home_appuye = false;
        mode = AWAY;
        printf("Je sors de la maison\r\n");
      }
    }
    if ( mode == AWAY) {
      if(bouton_home_appuye) {
        bouton_home_appuye = false;
        mode = HOME;
        alarme_intrusion = false;
        buzzer = false;
        HAL_GPIO_WritePin(YELLOW_GPIO_Port, YELLOW_Pin, 0);
        printf("Je rentre à la maison\r\n");
      } else if (presence_detectee) {
        alarme_intrusion = true;
        presence_detectee = false;
        printf("Intrusion detectee !\r\n");
      }
	}
	if(bouton_fen_appuye) {
		bouton_fen_appuye = false;
		fenetre=!fenetre;
		commande_fenetre();

	}

    commande_buzzer();
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

// affecter angel servo



void debounce_filter() {
	if (falling_edge_home_detected && !HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin)) {
		bouton_home_appuye = true;
		falling_edge_home_detected = false;
	}

	if (falling_edge_fen_detected && !HAL_GPIO_ReadPin(BTN_FEN_GPIO_Port, BTN_FEN_Pin)) {
		bouton_fen_appuye = true;
		falling_edge_fen_detected = false;
	}

}

void update_cycle_alarme_intrusion() {
	if (alarme_intrusion == true) {
		blink_counter_alarm_intrusion ++;
		if (blink_counter_alarm_intrusion == TIMER_DEBOUNCE_FREQ/(2*FREQ_BLINK_ALARM_INTRUSION)) {
			HAL_GPIO_TogglePin(YELLOW_GPIO_Port, YELLOW_Pin);
			buzzer = !buzzer;
			blink_counter_alarm_intrusion = 0;
		}
	}
}

void update_cycle_alarme_gaz() {
	if (alarme_gaz == true) {
		blink_counter_alarm_gaz ++;
		if (blink_counter_alarm_gaz == TIMER_DEBOUNCE_FREQ/(2*FREQ_BLINK_ALARM_GAZ)) {
			HAL_GPIO_TogglePin(RGB_ROUGE_GPIO_Port, RGB_ROUGE_Pin);
			buzzer = !buzzer;
			blink_counter_alarm_gaz = 0;
		}
	}
}

// INterruption des GPIO
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	// Bouton avec Debounce
	if (GPIO_Pin == BTN_Pin)
	{
		if (!falling_edge_home_detected) {
			falling_edge_home_detected = true;
			__HAL_TIM_SET_COUNTER(&htim5, 0);
		}
	}
	// PIR mouvement détecté
	if (GPIO_Pin == PIR_Pin)
	{
		if (mode == AWAY){
			presence_detectee = true;
		}
	}
	if (GPIO_Pin == BTN_FEN_Pin){
		if (!falling_edge_fen_detected) {
			falling_edge_fen_detected = true;
			__HAL_TIM_SET_COUNTER(&htim5, 0);
		}
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim == TIMER_DEBOUNCE)
  {
    debounce_filter();
    update_cycle_alarme_gaz();
    update_cycle_alarme_intrusion();
    
  }
  if (htim == TIMER_CAPTEURS)
  {
    HAL_GPIO_TogglePin(BLUE_GPIO_Port, BLUE_Pin);
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc == ADC_CAPTEURS)
    {
        gaz_valeur   = adc_valeurs[0];
        pluie_valeur = adc_valeurs[1];
        mesure_capteurs_prete = true;
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
