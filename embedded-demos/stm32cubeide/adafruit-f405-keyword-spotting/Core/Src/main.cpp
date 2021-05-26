/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdarg.h>

#include "../../ei-keyword-spotting/edge-impulse-sdk/classifier/ei_run_classifier.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/** Audio buffers, pointers and selectors */
typedef struct
{
  int16_t *buffers[2];
  uint8_t buf_select;
  volatile uint8_t buf_ready;
  volatile uint32_t buf_count;
  uint32_t n_samples;
} inference_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define I2S_BUF_LEN 6400  // 8x desired size to downsample and throw out 1 ch
#define I2S_BUF_SKIP 8    // (2x L/R ch) * (2x sample rate)
#define ELEMENTS_PER_WORD 2	// Number buffer elements per 32-bit word

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2S_HandleTypeDef hi2s2;
DMA_HandleTypeDef hdma_spi2_rx;

TIM_HandleTypeDef htim14;

UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */

// Settings
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal

// Globals
uint16_t i2s_buf[I2S_BUF_LEN + 2];
static inference_t inference;
static volatile bool record_ready = false;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2S2_Init(void);
static void MX_TIM14_Init(void);
static void MX_USART6_UART_Init(void);
/* USER CODE BEGIN PFP */

static int get_audio_signal_data(size_t offset, size_t length, float *out_ptr);
static void audio_buffer_inference_callback(uint32_t n_bytes, uint32_t offset);
bool ei_microphone_inference_record(void);
bool ei_microphone_inference_end(void);
void ei_printf(const char *format, ...);
void vprint(const char *fmt, va_list argp);

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
  HAL_StatusTypeDef hal_res;
  int print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);
  uint32_t timestamp = 0;

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
  MX_I2S2_Init();
  MX_TIM14_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */

  // Say some stuff
  ei_printf("Inferencing settings:\r\n");
  ei_printf("\tInterval: %.2f ms.\r\n", (float)EI_CLASSIFIER_INTERVAL_MS);
  ei_printf("\tFrame size: %d\r\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  ei_printf("\tSample length: %d ms.\r\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
  ei_printf("\tNo. of classes: %d\r\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));

  // Create double buffer
  inference.buffers[0] = (int16_t *)malloc(EI_CLASSIFIER_SLICE_SIZE * sizeof(int16_t));
  if(inference.buffers[0] == NULL)
  {
    ei_printf("ERROR: Could not create buffer 1. Likely ran out of heap memory.\r\n");
  }
  inference.buffers[1] = (int16_t *)malloc(EI_CLASSIFIER_SLICE_SIZE * sizeof(int16_t));
  if(inference.buffers[1] == NULL)
  {
    ei_printf("ERROR: Could not create buffer 2. Likely ran out of heap memory.\r\n");
  }

  // Set inference parameters
  inference.buf_select = 0;
  inference.buf_count  = 0;
  inference.n_samples  = EI_CLASSIFIER_SLICE_SIZE;
  inference.buf_ready  = 0;

  // Add guard to I2S buffer
  i2s_buf[I2S_BUF_LEN] = 0xbeef;
  i2s_buf[I2S_BUF_LEN + 1] = 0xbeef;

  // Start receiving I2S audio data
  hal_res =  HAL_I2S_Receive_DMA(&hi2s2, i2s_buf, (I2S_BUF_LEN / ELEMENTS_PER_WORD));
  if (hal_res != HAL_OK)
  {
    ei_printf("ERROR: Could not initialize I2S microphone.\r\n");
  }

  // Start doing inference
  record_ready = true;

  if (record_ready)
  {
    ei_printf("Listening!\r\n");
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    //ei_printf("Waiting...");

    // Wait until buffer is full
    bool m = ei_microphone_inference_record();
    if (!m)
    {
      ei_printf("ERROR: Audio buffer overrun\r\n");
      break;
    }

    //ei_printf("Inferencing...");

    // Do classification (i.e. the inference part)
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
    signal.get_data = &get_audio_signal_data;
    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK)
    {
	ei_printf("ERROR: Failed to run classifier (%d)\r\n", r);
	break;
    }

    //ei_printf("Done!\r\n");

    // Print output predictions (once every 4 predictions)
    if(++print_results >= (EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW >> 1))
    {
      // Comment this section out if you don't want to see the raw scores
      ei_printf("Predictions (DSP: %d ms, NN: %d ms)\r\n", result.timing.dsp, result.timing.classification);
      for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
      {
	  ei_printf("    %s: %.5f\r\n", result.classification[ix].label, result.classification[ix].value);
      }
      print_results = 0;
    }

    // ***EXAMPLES***

    // Your code goes here
    // Note: see model_metadata.h for labels and indices

    // Example: print if "yes" is above 0.5 threshold
    if (result.classification[3].value > 0.5)
    {
      ei_printf("YES!\r\n");
    }

    // Example: turn on LED if "no" is above 0.5 threshold
    if (result.classification[0].value > 0.5)
    {
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
    }
    else
    {
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);
    }

    // ***END OF EXAMPLES***
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }

  ei_microphone_inference_end();

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
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

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
  RCC_OscInitStruct.PLL.PLLM = 6;
  RCC_OscInitStruct.PLL.PLLN = 168;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2S;
  PeriphClkInitStruct.PLLI2S.PLLI2SN = 50;
  PeriphClkInitStruct.PLLI2S.PLLI2SR = 2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2S2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S2_Init(void)
{

  /* USER CODE BEGIN I2S2_Init 0 */

  /* USER CODE END I2S2_Init 0 */

  /* USER CODE BEGIN I2S2_Init 1 */

  /* USER CODE END I2S2_Init 1 */
  hi2s2.Instance = SPI2;
  hi2s2.Init.Mode = I2S_MODE_MASTER_RX;
  hi2s2.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s2.Init.DataFormat = I2S_DATAFORMAT_24B;
  hi2s2.Init.MCLKOutput = I2S_MCLKOUTPUT_DISABLE;
  hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_32K;
  hi2s2.Init.CPOL = I2S_CPOL_LOW;
  hi2s2.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s2.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
  if (HAL_I2S_Init(&hi2s2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S2_Init 2 */

  /* USER CODE END I2S2_Init 2 */

}

/**
  * @brief TIM14 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM14_Init(void)
{

  /* USER CODE BEGIN TIM14_Init 0 */

  /* USER CODE END TIM14_Init 0 */

  /* USER CODE BEGIN TIM14_Init 1 */

  /* USER CODE END TIM14_Init 1 */
  htim14.Instance = TIM14;
  htim14.Init.Prescaler = 8400 - 1;
  htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim14.Init.Period = 65535;
  htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim14) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM14_Init 2 */

  /* USER CODE END TIM14_Init 2 */

}

/**
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/**
 * @brief      Wait for a full buffer
 *
 * @return     In case of an buffer overrun return false
 */
bool ei_microphone_inference_record(void)
{
  bool ret = true;

  // Check to see if the buffer has overrun
  if (inference.buf_ready == 1) {
      ret = false;
  }

  // %%%TODO: make this non-blocking
  while (inference.buf_ready == 0)
  {
    continue;
  }

  inference.buf_ready = 0;

  return ret;
}

/**
 * @brief      Stop audio sampling, release sampling buffers
 *
 * @return     false on error
 */
bool ei_microphone_inference_end(void)
{
  // Stop I2S
  HAL_I2S_DMAStop(&hi2s2);

  // Free up double buffer
  record_ready = false;
  free(inference.buffers[0]);
  free(inference.buffers[1]);

  return true;
}

/**
 * @brief      Copy sample data in selected buf and signal ready when buffer is full
 *
 * @param[in]  n_bytes  Number of bytes to copy
 * @param[in]  offset   offset in sampleBuffer
 */
static void audio_buffer_inference_callback(uint32_t n_samples, uint32_t offset)
{
  // Copy samples from I2S buffer to inference buffer. Convert 24-bit, 32kHz
  // samples to 16-bit, 16kHz
  for (uint32_t i = 0; i < (n_samples / I2S_BUF_SKIP); i++) {
    inference.buffers[inference.buf_select][inference.buf_count++] =
        (int16_t)(i2s_buf[offset + (I2S_BUF_SKIP * i)]);

    if (inference.buf_count >= inference.n_samples) {
      inference.buf_select ^= 1;
      inference.buf_count = 0;
      inference.buf_ready = 1;
    }
  }
}

/**
 * Get raw audio signal data
 */
static int get_audio_signal_data(size_t offset, size_t length, float *out_ptr)
{
  numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);

  return 0;
}

/**
 * Low-level print function that uses UART to print status messages.
 */
void vprint(const char *fmt, va_list argp)
{
  char string[200];
  if(0 < vsprintf(string, fmt, argp)) // build string
  {
      HAL_UART_Transmit(&huart6, (uint8_t*)string, strlen(string), 0xffffff);
  }
}

/**
 * Wrapper for vprint. Use this like you would printf to print messages to the serial console.
 */
void ei_printf(const char *format, ...)
{
  va_list myargs;
  va_start(myargs, format);
  vprint(format, myargs);
  va_end(myargs);
}

/**
 * Called when the first half of the receive buffer is full
 */
void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
  if (record_ready == true)
  {
    audio_buffer_inference_callback(I2S_BUF_LEN / 2, 0);
  }
}

/**
 * Called when the second half of the receive buffer is full
 */
void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
{
  if (record_ready == true)
  {
    audio_buffer_inference_callback(I2S_BUF_LEN / 2, I2S_BUF_LEN / 2);
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
