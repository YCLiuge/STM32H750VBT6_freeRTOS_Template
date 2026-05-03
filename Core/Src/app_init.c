#include "app_init.h"

#include "usb_device.h"
#include "display_service.h"

void AppInit_Task(void *argument)
{
  (void)argument;

  MX_USB_DEVICE_Init();
  AppAdc_Init();
  AppSysMon_UpdateSnapshot();

  if (LCDMutexHandle != NULL)
  {
    if (osMutexAcquire(LCDMutexHandle, 10U) == osOK)
    {
      DisplayService_ShowText(0, 8, "RTOS Ready");
      osMutexRelease(LCDMutexHandle);
    }
  }

  vTaskDelete(NULL);
}
