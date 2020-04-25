/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CTRL_RX_Pin GPIO_PIN_2
#define CTRL_RX_GPIO_Port GPIOE
#define CTRL_TX_Pin GPIO_PIN_3
#define CTRL_TX_GPIO_Port GPIOE
#define BITMAP_CS_Pin GPIO_PIN_4
#define BITMAP_CS_GPIO_Port GPIOE
#define BITMAP_CS_EXTI_IRQn EXTI4_IRQn
#define BITMAP_MISO_Pin GPIO_PIN_5
#define BITMAP_MISO_GPIO_Port GPIOE
#define LCD4_MOSI_Pin GPIO_PIN_6
#define LCD4_MOSI_GPIO_Port GPIOE
#define LCD2_MOSI_Pin GPIO_PIN_3
#define LCD2_MOSI_GPIO_Port GPIOC
#define BRIGHTNESS_IN_1_Pin GPIO_PIN_0
#define BRIGHTNESS_IN_1_GPIO_Port GPIOA
#define BRIGHTNESS_IN_2_Pin GPIO_PIN_1
#define BRIGHTNESS_IN_2_GPIO_Port GPIOA
#define CONTRAST_1_Pin GPIO_PIN_4
#define CONTRAST_1_GPIO_Port GPIOA
#define CONTRAST_2_Pin GPIO_PIN_5
#define CONTRAST_2_GPIO_Port GPIOA
#define LCD1_MOSI_Pin GPIO_PIN_7
#define LCD1_MOSI_GPIO_Port GPIOA
#define LCD1_LATCH_Pin GPIO_PIN_4
#define LCD1_LATCH_GPIO_Port GPIOC
#define BITMAP_SCK_Pin GPIO_PIN_0
#define BITMAP_SCK_GPIO_Port GPIOB
#define AUX_IN_1_Pin GPIO_PIN_7
#define AUX_IN_1_GPIO_Port GPIOE
#define AUX_IN_1_EXTI_IRQn EXTI9_5_IRQn
#define DOOR_A_Pin GPIO_PIN_8
#define DOOR_A_GPIO_Port GPIOE
#define LCD_FRAME_Pin GPIO_PIN_9
#define LCD_FRAME_GPIO_Port GPIOE
#define DOOR_B_Pin GPIO_PIN_10
#define DOOR_B_GPIO_Port GPIOE
#define LCD4_LATCH_Pin GPIO_PIN_11
#define LCD4_LATCH_GPIO_Port GPIOE
#define LCD4_SCK_Pin GPIO_PIN_12
#define LCD4_SCK_GPIO_Port GPIOE
#define BITMAP_MOSI_Pin GPIO_PIN_14
#define BITMAP_MOSI_GPIO_Port GPIOE
#define LCD2_LATCH_Pin GPIO_PIN_15
#define LCD2_LATCH_GPIO_Port GPIOE
#define LCD2_SCK_Pin GPIO_PIN_10
#define LCD2_SCK_GPIO_Port GPIOB
#define LCD3_SCK_Pin GPIO_PIN_12
#define LCD3_SCK_GPIO_Port GPIOB
#define LCD3_LATCH_Pin GPIO_PIN_13
#define LCD3_LATCH_GPIO_Port GPIOB
#define HEATER_1_Pin GPIO_PIN_8
#define HEATER_1_GPIO_Port GPIOD
#define HEATER_2_Pin GPIO_PIN_9
#define HEATER_2_GPIO_Port GPIOD
#define FAN_1_Pin GPIO_PIN_10
#define FAN_1_GPIO_Port GPIOD
#define FAN_2_Pin GPIO_PIN_11
#define FAN_2_GPIO_Port GPIOD
#define FAN_3_Pin GPIO_PIN_12
#define FAN_3_GPIO_Port GPIOD
#define FAN_4_Pin GPIO_PIN_13
#define FAN_4_GPIO_Port GPIOD
#define LIGHT_Pin GPIO_PIN_14
#define LIGHT_GPIO_Port GPIOD
#define AUX_OUT_1_Pin GPIO_PIN_15
#define AUX_OUT_1_GPIO_Port GPIOD
#define TEMP_1_ONEWIRE_Pin GPIO_PIN_6
#define TEMP_1_ONEWIRE_GPIO_Port GPIOC
#define TEMP_2_ONEWIRE_Pin GPIO_PIN_7
#define TEMP_2_ONEWIRE_GPIO_Port GPIOC
#define BUTTON_1_Pin GPIO_PIN_8
#define BUTTON_1_GPIO_Port GPIOC
#define BUTTON_2_Pin GPIO_PIN_9
#define BUTTON_2_GPIO_Port GPIOC
#define LCD3_MOSI_Pin GPIO_PIN_12
#define LCD3_MOSI_GPIO_Port GPIOC
#define STATUS_LED_1_Pin GPIO_PIN_5
#define STATUS_LED_1_GPIO_Port GPIOD
#define STATUS_LED_2_Pin GPIO_PIN_6
#define STATUS_LED_2_GPIO_Port GPIOD
#define LCD1_SCK_Pin GPIO_PIN_3
#define LCD1_SCK_GPIO_Port GPIOB
#define CTRL_AUX2_OUT_Pin GPIO_PIN_8
#define CTRL_AUX2_OUT_GPIO_Port GPIOB
#define CTRL_AUX1_OUT_Pin GPIO_PIN_9
#define CTRL_AUX1_OUT_GPIO_Port GPIOB
#define CTRL_AUX2_IN_Pin GPIO_PIN_0
#define CTRL_AUX2_IN_GPIO_Port GPIOE
#define CTRL_AUX1_IN_Pin GPIO_PIN_1
#define CTRL_AUX1_IN_GPIO_Port GPIOE
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
