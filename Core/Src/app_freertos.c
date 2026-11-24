/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS applicative file
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
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "vl6180x.h"
#include "amg88xx.h"
#include <stdio.h>
#include <string.h>
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
/* USER CODE BEGIN Variables */
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart1;
/* USER CODE END Variables */
/* Definitions for tofTask */
osThreadId_t tofTaskHandle;
const osThreadAttr_t tofTask_attributes = {
  .name = "tofTask",
  .priority = (osPriority_t) osPriorityAboveNormal,
  .stack_size = 512 * 4
};
/* Definitions for thermalTask */
osThreadId_t thermalTaskHandle;
const osThreadAttr_t thermalTask_attributes = {
  .name = "thermalTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 512 * 4
};
/* Definitions for heartbeatTask */
osThreadId_t heartbeatTaskHandle;
const osThreadAttr_t heartbeatTask_attributes = {
  .name = "heartbeatTask",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 128 * 4
};
/* Definitions for i2c1Mutex */
osMutexId_t i2c1MutexHandle;
const osMutexAttr_t i2c1Mutex_attributes = {
  .name = "i2c1Mutex"
};
/* Definitions for telemetryMutex */
osMutexId_t telemetryMutexHandle;
const osMutexAttr_t telemetryMutex_attributes = {
  .name = "telemetryMutex"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void UART_Print(const char *message);
static HAL_StatusTypeDef I2C1_Reinitialize(void);
/* USER CODE END FunctionPrototypes */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* creation of i2c1Mutex */
  i2c1MutexHandle = osMutexNew(&i2c1Mutex_attributes);

  /* creation of telemetryMutex */
  telemetryMutexHandle = osMutexNew(&telemetryMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  if (i2c1MutexHandle == NULL || telemetryMutexHandle == NULL) {
    Error_Handler();
  }
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
  /* creation of tofTask */
  tofTaskHandle = osThreadNew(StartTofTask, NULL, &tofTask_attributes);

  /* creation of thermalTask */
  thermalTaskHandle = osThreadNew(StartThermalTask, NULL, &thermalTask_attributes);

  /* creation of heartbeatTask */
  heartbeatTaskHandle = osThreadNew(StartHeartbeatTask, NULL, &heartbeatTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  if (tofTaskHandle == NULL || thermalTaskHandle == NULL || heartbeatTaskHandle == NULL) {
    Error_Handler();
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}
/* USER CODE BEGIN Header_StartTofTask */
/**
* @brief Function implementing the tofTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTofTask */
void StartTofTask(void *argument)
{
  /* USER CODE BEGIN tofTask */
  (void)argument;

  VL6180X_t sensors[3] = {
    {&hi2c1, VL6180X_DEFAULT_ADDRESS},
    {&hi2c1, VL6180X_DEFAULT_ADDRESS},
    {&hi2c1, VL6180X_DEFAULT_ADDRESS},
  };

  GPIO_TypeDef * const ports[3] = {
    VL_FRONT_GPIO_Port, VL_LEFT_GPIO_Port, VL_RIGHT_GPIO_Port
  };

  const uint16_t pins[3] = {
    VL_FRONT_Pin, VL_LEFT_Pin, VL_RIGHT_Pin
  };

  const uint8_t addresses[3] = {0x30U, 0x31U, 0x32U};
  const char * const name[3] = {"Centro", "Izquierdo", "Derecho"};

  char message[128];

  // Repetir el arranque completo si falla alguno de los tres VL.
  for (;;) {
    for (uint32_t i = 0; i < 3; ++i) {
      HAL_GPIO_WritePin(ports[i], pins[i], GPIO_PIN_RESET);
      sensors[i].address_7bit = VL6180X_DEFAULT_ADDRESS;
    }
    osDelay(10);

    if (I2C1_Reinitialize() != HAL_OK) {
      UART_Print("I2C1: no se pudo reinicializar; reintentando\r\n");
      osDelay(1000U);
      continue;
    }

    uint8_t initialized = 1U;

    for (uint32_t i = 0; i < 3U; ++i) {
      HAL_GPIO_WritePin(ports[i], pins[i], GPIO_PIN_SET);
      osDelay(10U);

      HAL_StatusTypeDef status = VL6180X_SetAddress(&sensors[i], addresses[i]);

      if (status == HAL_OK) {
        status = VL6180X_Init(&sensors[i]);
      }

      if (status != HAL_OK) {
        snprintf(message, sizeof(message),
          "VL %s: fallo, reintentando\r\n", name[i]);
        UART_Print(message);
        initialized = 0;
        break;
      }
    }

    if (initialized != 0) {
      UART_Print("VL: los tres sensores inicializados\r\n");
      break;
    }

    for (uint32_t i = 0; i < 3U; ++i) {
      HAL_GPIO_WritePin(ports[i], pins[i], GPIO_PIN_RESET);
    }
    osDelay(1000);
  }

  for(;;){
    for(uint32_t i = 0; i < 3; ++i){
      VL6180X_Range_t result;
      HAL_StatusTypeDef status = VL6180X_ReadRange(&sensors[i], &result);

      if (status == HAL_OK && result.range_status == 0U) {
        snprintf(message, sizeof(message),
          "VL %s: %u mm\r\n",
          name[i], (unsigned int)result.distance_mm);
        UART_Print(message);

      } else if (status != HAL_OK) {
        snprintf(message, sizeof(message),
            "VL %s: fallo HAL=%u ERR=0x%08lX\r\n",
            name[i], (unsigned int)status,
            (unsigned long)HAL_I2C_GetError(&hi2c1));
        UART_Print(message);
      }
    }

    osDelay(200);
  }
  /* USER CODE END tofTask */
}

/* USER CODE BEGIN Header_StartThermalTask */
/**
* @brief Function implementing the thermalTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartThermalTask */
void StartThermalTask(void *argument)
{
  /* USER CODE BEGIN thermalTask */
  (void)argument;

  const AMG88XX_t thermal = {
    &hi2c1,
    i2c1MutexHandle,
    AMG88XX_DEFAULT_ADDRESS
  };

  int32_t temperatures[AMG88XX_PIXEL_COUNT];
  static char message[768];

  while (AMG88XX_Init(&thermal) != HAL_OK) {
    UART_Print("AMG8831: no se pudo iniciar\r\n");
    osDelay(1000U);
  }

  for (;;) {
    if (AMG88XX_ReadPixels(&thermal, temperatures) == HAL_OK) {
      size_t used = (size_t)snprintf(
          message, sizeof(message), "AMG8831 - grados C:\r\n");

      for (uint32_t row = 0; row < 8; ++row) {
        for (uint32_t col = 0; col < 8; ++col) {
          int32_t value = temperatures[row * 8 + col];
          int32_t magnitude = value < 0 ? -value : value;

          used += (size_t)snprintf(
            message + used, sizeof(message) - used,
            "%s%ld.%02ld ", value < 0 ? "-" : "",
            (long)(magnitude / 100), (long)(magnitude % 100));
        }

        used += (size_t)snprintf( message + used, sizeof(message) - used, "\r\n");
      }

      UART_Print(message);
    }

    osDelay(1000U);
  }
  /* USER CODE END thermalTask */
}

/* USER CODE BEGIN Header_StartHeartbeatTask */
/**
* @brief Function implementing the heartbeatTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartHeartbeatTask */
void StartHeartbeatTask(void *argument)
{
  /* USER CODE BEGIN heartbeatTask */
  (void)argument;

  for (;;) {
    osDelay(500U);
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
  }
  /* USER CODE END heartbeatTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/* Las dos tareas imprimen por USART1 usando el mismo mutex. */
static void UART_Print(const char *message)
{
  if (osMutexAcquire(telemetryMutexHandle, osWaitForever) != osOK) {
    return;
  }

  HAL_UART_Transmit(&huart1, (const uint8_t *)message,
                    (uint16_t)strlen(message), 100U);
  osMutexRelease(telemetryMutexHandle);
}

/* Conserva el temporizado generado en main.c y protege todo el reinicio. */
static HAL_StatusTypeDef I2C1_Reinitialize(void)
{
  if (osMutexAcquire(i2c1MutexHandle, osWaitForever) != osOK) {
    return HAL_BUSY;
  }

  HAL_StatusTypeDef status = HAL_I2C_DeInit(&hi2c1);

  if (status == HAL_OK) {
    __HAL_RCC_I2C1_FORCE_RESET();
    __HAL_RCC_I2C1_RELEASE_RESET();
    status = HAL_I2C_Init(&hi2c1);
  }
  if (status == HAL_OK) {
    status = HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE);
  }
  if (status == HAL_OK) {
    status = HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0U);
  }

  osMutexRelease(i2c1MutexHandle);
  return status;
}
/* USER CODE END Application */

