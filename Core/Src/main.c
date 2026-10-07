/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include <string.h>
#include "si5351.h"
#include "fonts.h"
#include "SSD1306.h"


#include "adc.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
 ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c3;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
extern TIM_HandleTypeDef htim3;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM3_Init(void);
static void MX_I2C3_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */
//factor de correcion de medicion
uint16_t correction_dbm=3.2;
uint16_t corection_antenna_dbm=0; //Correccion por perdidas de antena
// Pines BCD → decodificador 7-seg
#define D0_PIN   GPIO_PIN_3  // BIT0 → PB3
#define D1_PIN   GPIO_PIN_4  // BIT1 → PB4
#define D2_PIN   GPIO_PIN_5  // BIT2 → PB5
#define D3_PIN   GPIO_PIN_6  // BIT3 → PB6
#define D_PORT   GPIOB

// Pines de latch para cada dígito
#define LATCH_HIGH_DIGIT_PIN   GPIO_PIN_2  // decenas → PB2
#define LATCH_LOW_DIGIT_PIN    GPIO_PIN_1  // unidades → PB1
#define LATCH_PORT             GPIOB


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Variables globales
volatile uint16_t data_to_send = 0b111111;  // Dato de 6 bits; // Emipieza con 33db de atenuacion
volatile uint16_t aux = 0;
volatile uint8_t bit_index = 5;
volatile uint8_t sending = 0;
uint32_t  last_up_ms    = 0;
uint32_t  last_down_ms  = 0;
uint16_t samples=200;
uint16_t RFIN=0;
uint16_t voltaje_mV=0;
#define   DEBOUNCE_MS   200U
#define MAX_ATT 63
#define MIN_ATT 1

static void init(void);
static void UART_TransmitString(const char* str);
static void I2C_Scan(void);
static void loop(void);

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
  MX_TIM3_Init();
  MX_I2C3_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  OLED_init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim3);
  HAL_Delay(10);  // pequeña espera por seguridad

  void UART_TransmitString(const char* str) {
        HAL_UART_Transmit(&huart2, (uint8_t*)str, strlen(str), HAL_MAX_DELAY);
    }

    void I2C_Scan() {
    	UART_TransmitString("Scanning I2C bus...\r\n");
        HAL_StatusTypeDef res;
        for(uint16_t i = 0; i < 128; i++) {
            res = HAL_I2C_IsDeviceReady(&hi2c3, i << 1, 1, 10);
            if(res == HAL_OK) {
                char msg[64];
                snprintf(msg, sizeof(msg), "0x%02X", i);
    			UART_TransmitString(msg);
            } else {
    			UART_TransmitString(".");
            }
        }

    	UART_TransmitString("\r\n");
    }

    void init3() {
        UART_TransmitString("Calling I2C_Scan()...\r\n");
    	I2C_Scan();

        UART_TransmitString("Initializing Si5351...\r\n");
    	const int32_t correction = 11300;
    	si5351_Init(correction);
    	si5351_SetupCLK0(53000000, SI5351_DRIVE_STRENGTH_2MA);
    	si5351_EnableOutputs(1 << 0);

        UART_TransmitString("Ready!\r\n");
    }



  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    init3();
    char buffer[20];

   // --- Encabezado Display ---  //
    OLED_print_text(28, 0, "Potencia", &font1);
    OLED_print_text(35, 2, "Medida:", &font1);

    while (1)
  {
    	RFIN = Get_ADC_Average(samples);
    	voltaje_mV = (RFIN * 3300UL) / 4095;
      	float potin = (((voltaje_mV - 2319) / 0.02503f)/1000)+data_to_send/2-correction_dbm+corection_antenna_dbm;//el factor de correccion de antena es para eliminar la posible desadaptacion con el filtro pasa banda de la entrada (Pequeña)
        sprintf(buffer, "%.1f dBm", potin);
      	OLED_print_text(20, 4, (uint8_t*)buffer, &font1);
      	float att_db = data_to_send * 0.5f;   // cada paso = 0.5 dB la idea es mostrar el atenuador en el display
      	sprintf(buffer, "Att: %.1f dB", att_db);
      	OLED_print_text(5, 6, (uint8_t*)buffer, &font1);
    	Check_Atte();
    	CheckButtonsAndUpdate();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 80;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV16;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV16;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.ClockSpeed = 400000;
  hi2c3.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 100;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 100-1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PC0 PC1 PC2 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PB1 PB2 PB3 PB4
                           PB5 PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
void Check_Atte(void)
{
	if (voltaje_mV > 1800)
		{
		data_to_send=63; //Pongo maxima atenuacion por proteccion
		HAL_TIM_Base_Start_IT(&htim3);  // activa TIM3 si hay cambio
		UpdateDisplays();
		}

	if (voltaje_mV >1300 && voltaje_mV <1800)
		{
			add_att();
		}
	if (voltaje_mV > 0 && voltaje_mV <1000)
	{
		dec_att();

	}

}
void add_att(void)
{
	if(data_to_send==60)
		data_to_send=63;

	if(data_to_send<55)
		data_to_send=data_to_send+10; //incremento 5db

	HAL_TIM_Base_Start_IT(&htim3);  // activa TIM3 si hay cambio
	UpdateDisplays();


}
void dec_att(void)
{
	if(data_to_send<=5)
			data_to_send=0;

		if(data_to_send>5)
			data_to_send=data_to_send-10; //decremento 5db

		HAL_TIM_Base_Start_IT(&htim3);  // activa TIM3 si hay cambio
		UpdateDisplays();

}
void CheckButtonsAndUpdate(void)
{
    uint32_t now = HAL_GetTick();

    // Verifica botón PB8 (subir)
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8) == GPIO_PIN_RESET)
    {
        if ((now - last_up_ms) > DEBOUNCE_MS && data_to_send < MAX_ATT)
        {
            data_to_send++;
            last_up_ms = now;
            HAL_TIM_Base_Start_IT(&htim3);  // activa TIM3 si hay cambio
        }
    }

    // Verifica botón PB9 (bajar)
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9) == GPIO_PIN_RESET)
    {
        if ((now - last_down_ms) > DEBOUNCE_MS && data_to_send > MIN_ATT)
        {
            data_to_send--;
            last_down_ms = now;
            HAL_TIM_Base_Start_IT(&htim3);  // activa TIM3 si hay cambio
        }
    }
}
void SendBit_Handler(void)
	{
		if (data_to_send != aux)
		{

	    if (sending == 0)
	    {
	        // Inicio del envío: poner LE en bajo
	        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET); // LE = 0

	    }

	    if (bit_index >= 0)
	    {
	        // Escribir el bit actual
	        uint8_t bit = (data_to_send >> bit_index) & 0x01;
	        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, bit ? GPIO_PIN_SET : GPIO_PIN_RESET); // DATA

	        // Generar pulso de CLOCK
	        // breve retardo por software
	            for (volatile int i = 0; i < 10; i++);
	        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);   // CLOCK = 1
	        // breve retardo por software
	            for (volatile int i = 0; i < 10; i++);
	        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET); // CLOCK = 0

	        bit_index--;
	    }

	    // Si terminó el último bit, generar LE
	            if (bit_index == 255)
	            {
	                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_SET); // LE = 1
	                sending = 1;
	                bit_index = 5; // MSB primero
	                UpdateDisplays(data_to_send);
	                aux=data_to_send;
	                HAL_TIM_Base_Stop_IT(&htim3);

	            }
		}

	    }

static void send_nibble_and_latch(uint8_t bcd, uint16_t latch_pin)
{
    // 1) Coloca los 4 bits BCD en PB3–PB6
    HAL_GPIO_WritePin(D_PORT, D0_PIN, (bcd & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(D_PORT, D1_PIN, (bcd & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(D_PORT, D2_PIN, (bcd & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(D_PORT, D3_PIN, (bcd & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // 2) Pulso de latch (SET→small delay→RESET)
    HAL_GPIO_WritePin(LATCH_PORT, latch_pin, GPIO_PIN_RESET);
    // breve retardo por software
    for (volatile int i = 0; i < 50; i++);

    HAL_GPIO_WritePin(LATCH_PORT, latch_pin, GPIO_PIN_SET);
}

/**
 * @brief   Actualiza los dos displays para mostrar data_to_send*0.5.
 *          data_to_send va de 1 (0.5) hasta 62 (31.0).
 * @param   data_to_send  Valor de 6 bits (1…62)
 */
void UpdateDisplays(void)
{
    uint16_t scaled;
    uint8_t  high, low;

    // 1) Escala a media unidad: data_to_send * 0.5 = (data_to_send * 5) / 10
    scaled = data_to_send>>1;  // rango 5…310

    // 2) Obtén dígitos decimal
    high = (scaled / 10);  // decenas
    low  = (scaled % 10);       // unidades

    // 3) Envía y latch al dígito “unidades�? (PB1)
    send_nibble_and_latch(low, LATCH_LOW_DIGIT_PIN);

    // 4) Envía y latch al dígito “decenas�? (PB2)
    send_nibble_and_latch(high, LATCH_HIGH_DIGIT_PIN);
}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* Prevent unused argument(s) compilation warning */
	{
	    if (htim->Instance == TIM3) // Ejemplo: usando TIM3
	    {
	         SendBit_Handler(); // Se llama cada vez que ocurra la interrupción
	    }
	}

	}
  /* NOTE : This function should not be modified, when the callback is needed,
            the HAL_TIM_PeriodElapsedCallback could be implemented in the user file
   */


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

#ifdef  USE_FULL_ASSERT
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
