/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app.h"
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

/* USER CODE END Variables */
/* Definitions for Init_Task */
osThreadId_t Init_TaskHandle;
const osThreadAttr_t Init_Task_attributes = {
  .name = "Init_Task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for SysMon_Task */
osThreadId_t SysMon_TaskHandle;
const osThreadAttr_t SysMon_Task_attributes = {
  .name = "SysMon_Task",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for ADC1_Task */
osThreadId_t ADC1_TaskHandle;
const osThreadAttr_t ADC1_Task_attributes = {
  .name = "ADC1_Task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for ADC2_Task */
osThreadId_t ADC2_TaskHandle;
const osThreadAttr_t ADC2_Task_attributes = {
  .name = "ADC2_Task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for ADC3_Task */
osThreadId_t ADC3_TaskHandle;
const osThreadAttr_t ADC3_Task_attributes = {
  .name = "ADC3_Task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for Console_Task */
osThreadId_t Console_TaskHandle;
const osThreadAttr_t Console_Task_attributes = {
  .name = "Console_Task",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for ADCData_Queue */
osMessageQueueId_t ADCData_QueueHandle;
const osMessageQueueAttr_t ADCData_Queue_attributes = {
  .name = "ADCData_Queue"
};
/* Definitions for UsbTxMutex */
osMutexId_t UsbTxMutexHandle;
const osMutexAttr_t UsbTxMutex_attributes = {
  .name = "UsbTxMutex"
};
/* Definitions for LCDMutex */
osMutexId_t LCDMutexHandle;
const osMutexAttr_t LCDMutex_attributes = {
  .name = "LCDMutex"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void Init_Handler(void *argument);
void SysMon_Handler(void *argument);
void ADC1_Handler(void *argument);
void ADC2_Handler(void *argument);
void ADC3_Handler(void *argument);
void Console_Handler(void *argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void configureTimerForRunTimeStats(void);
unsigned long getRunTimeCounterValue(void);
void vApplicationStackOverflowHook(xTaskHandle xTask, char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/* USER CODE BEGIN 4 */
__weak void vApplicationStackOverflowHook(xTaskHandle xTask, char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
__weak void vApplicationMallocFailedHook(void)
{
   /* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created. It is also called by various parts of the
   demo application. If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
}
/* USER CODE END 5 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  AppConsole_Init();
  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of UsbTxMutex */
  UsbTxMutexHandle = osMutexNew(&UsbTxMutex_attributes);

  /* creation of LCDMutex */
  LCDMutexHandle = osMutexNew(&LCDMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of ADCData_Queue */
  ADCData_QueueHandle = osMessageQueueNew (16, sizeof(uint16_t), &ADCData_Queue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of Init_Task */
  Init_TaskHandle = osThreadNew(Init_Handler, NULL, &Init_Task_attributes);

  /* creation of SysMon_Task */
  SysMon_TaskHandle = osThreadNew(SysMon_Handler, NULL, &SysMon_Task_attributes);

  /* creation of ADC1_Task */
  ADC1_TaskHandle = osThreadNew(ADC1_Handler, NULL, &ADC1_Task_attributes);

  /* creation of ADC2_Task */
  ADC2_TaskHandle = osThreadNew(ADC2_Handler, NULL, &ADC2_Task_attributes);

  /* creation of ADC3_Task */
  ADC3_TaskHandle = osThreadNew(ADC3_Handler, NULL, &ADC3_Task_attributes);

  /* creation of Console_Task */
  Console_TaskHandle = osThreadNew(Console_Handler, NULL, &Console_Task_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_Init_Handler */
/**
  * @brief  Function implementing the Init_Task thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_Init_Handler */
void Init_Handler(void *argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN Init_Handler */
  AppInit_Task(argument);
  /* USER CODE END Init_Handler */
}

/* USER CODE BEGIN Header_SysMon_Handler */
/**
* @brief Function implementing the SysMon_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_SysMon_Handler */
void SysMon_Handler(void *argument)
{
  /* USER CODE BEGIN SysMon_Handler */
  AppSysMon_Task(argument);
  /* USER CODE END SysMon_Handler */
}

/* USER CODE BEGIN Header_ADC1_Handler */
/**
* @brief Function implementing the ADC1_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_ADC1_Handler */
void ADC1_Handler(void *argument)
{
  /* USER CODE BEGIN ADC1_Handler */
  AppAdc_Task1(argument);
  /* USER CODE END ADC1_Handler */
}

/* USER CODE BEGIN Header_ADC2_Handler */
/**
* @brief Function implementing the ADC2_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_ADC2_Handler */
void ADC2_Handler(void *argument)
{
  /* USER CODE BEGIN ADC2_Handler */
  AppAdc_Task2(argument);
  /* USER CODE END ADC2_Handler */
}

/* USER CODE BEGIN Header_ADC3_Handler */
/**
* @brief Function implementing the ADC3_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_ADC3_Handler */
void ADC3_Handler(void *argument)
{
  /* USER CODE BEGIN ADC3_Handler */
  AppAdc_Task3(argument);
  /* USER CODE END ADC3_Handler */
}

/* USER CODE BEGIN Header_Console_Handler */
/**
* @brief Function implementing the Console_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Console_Handler */
void Console_Handler(void *argument)
{
  /* USER CODE BEGIN Console_Handler */
  AppConsole_Task(argument);
  /* USER CODE END Console_Handler */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  AppAdc_DmaConvCpltCallback(hadc);
}
/* USER CODE END Application */

