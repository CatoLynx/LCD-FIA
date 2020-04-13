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
#include "gpio.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>

#include "aditech.h"
#include "ds18b20.h"
#include "fia.h"
#include "uart_protocol.h"
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
// LCD Data buffers per LCD bus
uint8_t lcdData1[LCD_BUF_SIZE];
uint8_t lcdData2[LCD_BUF_SIZE];
uint8_t lcdData3[LCD_BUF_SIZE];
uint8_t lcdData4[LCD_BUF_SIZE];

// Status variables for selected RAM per LCD bus
uint8_t currentRAM[NUM_LCD_BUSES];
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
int main(void) {
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
    MX_TIM4_Init();
    /* USER CODE BEGIN 2 */
    FIA_Init();
    UART_StartRxRingBuffer();

    FIA_SetLCDContrast(SIDE_A, 2200);
    FIA_SetLCDContrast(SIDE_B, 2200);

    FIA_SetBacklightBrightness(SIDE_A, 0);
    FIA_SetBacklightBrightness(SIDE_B, 0);

    FIA_SetHeaters(0);

    FIA_SetBacklight(1);
    HAL_Delay(500);
    FIA_SetBacklightBallastFans(1);
    HAL_Delay(500);
    FIA_SetCirculationFans(1);
    HAL_Delay(500);
    FIA_SetCirculationFans(2);
    HAL_Delay(500);
    FIA_SetHeatExchangerFan(1);

    memset(bitmapBufferSideA, 0xFF, BITMAP_BUF_SIZE);
    memset(bitmapBufferSideB, 0xFF, BITMAP_BUF_SIZE);

    FIA_Side_t oldDoorStatus = FIA_GetDoors();
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        UART_HandleProtocol();

        if (!bitmapReceiveActive) {
            HAL_SPI_Receive_DMA(&BITMAP_DATA_SPI, bitmapBufferSideA, BITMAP_BUF_SIZE);
            FIA_SetStatusLED(2, 1);
        }

        if (updateLCDContrastFlag) {
            FIA_UpdateLCDContrast();
            updateLCDContrastFlag = 0;
        }

        FIA_Side_t doorStatus = FIA_GetDoors();
        if (updateBacklightBrightnessFlag || (doorStatus != oldDoorStatus)) {
            // Auto-adjust brightness or set to minimum if door is open
            FIA_SetBacklightBrightness(
                SIDE_A,
                (doorStatus & SIDE_A) ? 0 : FIA_CalculateBacklightBrightness(SIDE_A, FIA_GetEnvBrightness(SIDE_A)));
            FIA_SetBacklightBrightness(
                SIDE_B,
                (doorStatus & SIDE_B) ? 0 : FIA_CalculateBacklightBrightness(SIDE_B, FIA_GetEnvBrightness(SIDE_B)));
            updateBacklightBrightnessFlag = 0;
            oldDoorStatus = doorStatus;
        }

        if (!LCD_IsTransmitActive(BUS_1)) {
            LCD_ConvertBitmap(lcdData1, bitmapBufferSideA, 0, currentRAM[BUS_1]);
            LCD_TransmitBitmap(lcdData1, NUM_HALF_PANELS, BUS_1);
            if (currentRAM[BUS_1] == RAM1) {
                currentRAM[BUS_1] = RAM2;
            } else {
                currentRAM[BUS_1] = RAM1;
            }
        }
        if (!LCD_IsTransmitActive(BUS_2)) {
            LCD_ConvertBitmap(lcdData2, bitmapBufferSideA, 1, currentRAM[BUS_2]);
            LCD_TransmitBitmap(lcdData2, NUM_HALF_PANELS, BUS_2);
            if (currentRAM[BUS_2] == RAM1) {
                currentRAM[BUS_2] = RAM2;
            } else {
                currentRAM[BUS_2] = RAM1;
            }
        }
        if (!LCD_IsTransmitActive(BUS_3)) {
            LCD_ConvertBitmap(lcdData3, bitmapBufferSideA, 0, currentRAM[BUS_3]);
            LCD_TransmitBitmap(lcdData3, NUM_HALF_PANELS, BUS_3);
            if (currentRAM[BUS_3] == RAM1) {
                currentRAM[BUS_3] = RAM2;
            } else {
                currentRAM[BUS_3] = RAM1;
            }
        }
        if (!LCD_IsTransmitActive(BUS_4)) {
            LCD_ConvertBitmap(lcdData4, bitmapBufferSideA, 1, currentRAM[BUS_4]);
            LCD_TransmitBitmap(lcdData4, NUM_HALF_PANELS, BUS_4);
            if (currentRAM[BUS_4] == RAM1) {
                currentRAM[BUS_4] = RAM2;
            } else {
                currentRAM[BUS_4] = RAM1;
            }
        }
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
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
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }
    /** Initializes the CPU, AHB and APB busses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */

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
void assert_failed(uint8_t* file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
