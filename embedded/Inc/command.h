/**
  ******************************************************************************
  * File Name          : 
  * Description        : 
  ******************************************************************************
  */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __COMMAND_H
#define __COMMAND_H
#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os2.h"
#include "flash_if.h"
#include "rtc.h"
	 
/* define ------------------------------------------------------------*/

/* constants --------------------------------------------------------*/


/* macro ------------------------------------------------------------*/

	
/* types ------------------------------------------------------------*/

/* functions prototypes ---------------------------------------------*/
void process_command_task(void);
void ClearCmd(void);  
void CheckUpdate(void);

/* variables ---------------------------------------------------------*/
extern osMessageQueueId_t cmdRxQueueHandle;
extern osMessageQueueId_t cmdTxQueueHandle;
extern osTimerId_t cmdReceiveTimeoutHandle;
extern uint32_t temperature_adc;

#ifdef __cplusplus
}
#endif
#endif


/*****************************END OF FILE****/
