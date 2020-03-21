/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
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
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>

#include "aditech.h"
#include "fia.h"
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

/* USER CODE BEGIN PV */
uint8_t lcdData1[LCD_BUF_SIZE];
uint8_t lcdData2[LCD_BUF_SIZE];
uint8_t lcdData3[LCD_BUF_SIZE];
uint8_t lcdData4[LCD_BUF_SIZE];
uint8_t currentRAM[NUM_LCD_ROWS];
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
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_DAC_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_SPI3_Init();
  MX_SPI4_Init();
  MX_SPI5_Init();
  MX_TIM1_Init();
  MX_UART10_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
    FIA_Init();

    HAL_GPIO_WritePin(GPIOD, LIGHT_Pin, 1);
    HAL_GPIO_WritePin(GPIOD, HEATER_1_Pin, 1);
    HAL_GPIO_WritePin(GPIOD, HEATER_2_Pin, 1);
    HAL_GPIO_WritePin(GPIOD, FAN_1_Pin, 1);
    HAL_GPIO_WritePin(GPIOD, FAN_2_Pin, 1);
    HAL_GPIO_WritePin(GPIOD, FAN_3_Pin, 1);
    HAL_GPIO_WritePin(GPIOD, FAN_4_Pin, 1);

    FIA_setBacklightBrightness(SIDE_A, 0);
    FIA_setBacklightBrightness(SIDE_B, 0);

    FIA_setLCDContrast(SIDE_A, 4095);
    FIA_setLCDContrast(SIDE_B, 4095);

    memset(bitmapBufferSideA, 0x0F, BITMAP_BUF_SIZE);
    memset(bitmapBufferSideB, 0x0F, BITMAP_BUF_SIZE);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
        if (!bitmapReceiveActive) {
            HAL_SPI_Receive_DMA(&BITMAP_DATA_SPI, bitmapBufferSideA, BITMAP_BUF_SIZE);
            FIA_setStatusLED(2, 1);
        }

        if(updateBacklightBrightnessFlag) {
          FIA_setBacklightBrightness(SIDE_A, FIA_getEnvBrightness(SIDE_A));
          FIA_setBacklightBrightness(SIDE_B, FIA_getEnvBrightness(SIDE_B));
          updateBacklightBrightnessFlag = 0;
        }

        if (!isTransmitActive(ROW_1)) {
            convertBitmap(lcdData1, bitmapBufferSideA, NUM_HALF_PANELS, currentRAM[ROW_1]);
            transmitBitmap(lcdData1, NUM_HALF_PANELS, ROW_1);
            if (currentRAM[ROW_1] == RAM1) {
                currentRAM[ROW_1] = RAM2;
            } else {
                currentRAM[ROW_1] = RAM1;
            }
        }
        if (!isTransmitActive(ROW_2)) {
            convertBitmap(lcdData2, &bitmapBufferSideA[BITMAP_BUF_HALF_SIZE], NUM_HALF_PANELS, currentRAM[ROW_2]);
            transmitBitmap(lcdData2, NUM_HALF_PANELS, ROW_2);
            if (currentRAM[ROW_2] == RAM1) {
                currentRAM[ROW_2] = RAM2;
            } else {
                currentRAM[ROW_2] = RAM1;
            }
        }
        if (!isTransmitActive(ROW_3)) {
            convertBitmap(lcdData3, bitmapBufferSideA, NUM_HALF_PANELS, currentRAM[ROW_3]);
            transmitBitmap(lcdData3, NUM_HALF_PANELS, ROW_3);
            if (currentRAM[ROW_3] == RAM1) {
                currentRAM[ROW_3] = RAM2;
            } else {
                currentRAM[ROW_3] = RAM1;
            }
        }
        if (!isTransmitActive(ROW_4)) {
            convertBitmap(lcdData4, &bitmapBufferSideA[BITMAP_BUF_HALF_SIZE], NUM_HALF_PANELS, currentRAM[ROW_4]);
            transmitBitmap(lcdData4, NUM_HALF_PANELS, ROW_4);
            if (currentRAM[ROW_4] == RAM1) {
                currentRAM[ROW_4] = RAM2;
            } else {
                currentRAM[ROW_4] = RAM1;
            }
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
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 10;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
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

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */

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
       tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
