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
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdbool.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define HOME true
#define AWAY false

#define FREQ_TIM5 20
#define FREQ_BLINK_ALARM_GAZ 4
#define FREQ_BLINK_ALARM_INTRUSION 2
#define SEUIL_GAZ 3000

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile bool mode = HOME;
volatile bool buzzer = false;
volatile bool falling_edge_detected = false;
volatile bool presence_detectee = false;
volatile bool alarme_gaz = false;
volatile bool alarme_intrusion = false;
volatile bool bouton_appuye = false;
volatile int blink_counter_alarm_gaz = 0;
volatile int blink_counter_alarm_intrusion = 0;
volatile int gaz_value = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM5_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim5);
  HAL_TIM_Base_Start_IT(&htim2);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_ADC_Start(&hadc1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	// Machine à état avec HOME et AWAY
	if (mode == HOME) {
		if(bouton_appuye) {
			bouton_appuye = false;
			mode = AWAY;
		}
	}
	if ( mode == AWAY) {
		if(bouton_appuye) {
			bouton_appuye = false;
			mode = HOME;
			alarme_intrusion = false;
			buzzer = false;
			HAL_GPIO_WritePin(YELLOW_GPIO_Port, YELLOW_Pin, 0);
		}
		if (presence_detectee) {
			alarme_intrusion = true;
			presence_detectee = false;
		}
	}

	if (buzzer) {
		HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
	} else {
		HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_2);
	}
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

void debounce_filter() {
	if (falling_edge_detected && !HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin)) {
		bouton_appuye = true;
		falling_edge_detected = false;
	}
}

void update_cycle_alarme_intrusion() {
	if (alarme_intrusion == true) {
		blink_counter_alarm_intrusion ++;
		if (blink_counter_alarm_intrusion == FREQ_TIM5/(2*FREQ_BLINK_ALARM_INTRUSION)) {
			HAL_GPIO_TogglePin(YELLOW_GPIO_Port, YELLOW_Pin);
			buzzer = !buzzer;
			blink_counter_alarm_intrusion = 0;
		}
	}
}

void update_cycle_alarme_gaz() {
	if (alarme_gaz == true) {
		blink_counter_alarm_gaz ++;
		if (blink_counter_alarm_gaz == FREQ_TIM5/(2*FREQ_BLINK_ALARM_GAZ)) {
			HAL_GPIO_TogglePin(RGB_ROUGE_GPIO_Port, RGB_ROUGE_Pin);
			buzzer = !buzzer;
			blink_counter_alarm_gaz = 0;
		}
	}
}

void update_mesure_gaz() {
	gaz_value = HAL_ADC_GetValue(&hadc1);
	if (gaz_value >= SEUIL_GAZ) {
		alarme_gaz = true;
	} else {
		alarme_gaz = false;
	}
}
// INterruption des GPIO
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	// Bouton avec Debounce
	if (GPIO_Pin == BTN_Pin)
	{
		if (!falling_edge_detected) {
			falling_edge_detected = true;
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
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	// TIM 5 -> PER = 50ms pour le debounce et compter des periodes de clignotement du buzzer et de la LED d'alarme
	if (htim->Instance == TIM5)
	{
		debounce_filter();
		update_cycle_alarme_gaz();
		update_cycle_alarme_intrusion();

	}
	// TIM 2 -> PER = 4s pour l'échantillonnage des capteurs
	if (htim->Instance == TIM2)
	{
		HAL_GPIO_TogglePin(BLUE_GPIO_Port, BLUE_Pin);
		update_mesure_gaz();
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
