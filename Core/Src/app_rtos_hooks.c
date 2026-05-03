#include "main.h"

#include "FreeRTOS.h"
#include "task.h"
#include "tim.h"
#include "display_service.h"

__weak void configureTimerForRunTimeStats(void)
{
  HAL_TIM_Base_Start(&htim2);
}

__weak unsigned long getRunTimeCounterValue(void)
{
  return __HAL_TIM_GET_COUNTER(&htim2);
}

void vApplicationStackOverflowHook(xTaskHandle xTask, char *pcTaskName)
{
  (void)xTask;

  __disable_irq();
  DisplayService_Clear();
  DisplayService_ShowText(0, 0, "Error!");
  DisplayService_ShowText(0, 8, "Stack Overflow!");
  if (pcTaskName != NULL)
  {
    DisplayService_ShowText(0, 16, pcTaskName);
  }
  for (;;)
  {
  }
}

void vApplicationMallocFailedHook(void)
{
  __disable_irq();
  DisplayService_Clear();
  DisplayService_ShowText(0, 0, "Error!");
  DisplayService_ShowText(0, 8, "Out of memory!");
  for (;;)
  {
  }
}
