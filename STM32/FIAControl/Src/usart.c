/**
  ******************************************************************************
  * File Name          : USART.c
  * Description        : This file provides code for the configuration
  *                      of the USART instances.
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

/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

UART_HandleTypeDef huart10;

/* UART10 init function */
void MX_UART10_Init(void)
{

  huart10.Instance = UART10;
  huart10.Init.BaudRate = 115200;
  huart10.Init.WordLength = UART_WORDLENGTH_8B;
  huart10.Init.StopBits = UART_STOPBITS_1;
  huart10.Init.Parity = UART_PARITY_NONE;
  huart10.Init.Mode = UART_MODE_TX_RX;
  huart10.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart10.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart10) != HAL_OK)
  {
    Error_Handler();
  }

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==UART10)
  {
  /* USER CODE BEGIN UART10_MspInit 0 */

  /* USER CODE END UART10_MspInit 0 */
    /* UART10 clock enable */
    __HAL_RCC_UART10_CLK_ENABLE();
  
    __HAL_RCC_GPIOE_CLK_ENABLE();
    /**UART10 GPIO Configuration    
    PE2     ------> UART10_RX
    PE3     ------> UART10_TX 
    */
    GPIO_InitStruct.Pin = CTRL_RX_Pin|CTRL_TX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_UART10;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* USER CODE BEGIN UART10_MspInit 1 */

  /* USER CODE END UART10_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==UART10)
  {
  /* USER CODE BEGIN UART10_MspDeInit 0 */

  /* USER CODE END UART10_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_UART10_CLK_DISABLE();
  
    /**UART10 GPIO Configuration    
    PE2     ------> UART10_RX
    PE3     ------> UART10_TX 
    */
    HAL_GPIO_DeInit(GPIOE, CTRL_RX_Pin|CTRL_TX_Pin);

  /* USER CODE BEGIN UART10_MspDeInit 1 */

  /* USER CODE END UART10_MspDeInit 1 */
  }
} 

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
