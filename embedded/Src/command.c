/**
  ******************************************************************************
  * File Name          : 
  * Description        : 
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "command.h"

/* define ------------------------------------------------------------*/
#define CMD_OK                       0x00
#define CMD_Excute_Error             0x01
#define CMD_Checksum_Error           0x02
#define CMD_Param_Out_Of_Range_Error 0x03
#define CMD_Unknown_Command_Error    0x04

#define  SET_TIME                    0x01
#define  GET_TIME                    0x02
#define  GET_Info                    0x03
#define  INIT_UPDATA_FIRMWARE        0X04
#define  GET_UPDATA_DATA             0x05
#define  RUN_FIRMWARE                0X06

#define CMD_BUFF_SIZE               (256+64) // depend on max cmd size
	 
#define HW_VER                      "DEMO_V1.00"	 

#define UPDATE_KEY                   0x5A

#define FRAME_HEADER                 0xA5

/* constants --------------------------------------------------------*/

/* macro ------------------------------------------------------------*/

/* types ------------------------------------------------------------*/
typedef  void (*pFunction)(void);

enum CmdParseState
{
	WaitingForCmdHeader,
	WaitingForCmdID,
	WaitingForCmdLength,
	WaitingForCmdPayload,
	WaitingForCmdChecksum
};

typedef struct {
	uint16_t cmdId;
	uint16_t cmdLength;
	uint16_t cmdChecksum;
	enum CmdParseState cmdState;
	uint8_t cmdBuff[CMD_BUFF_SIZE];
	uint16_t cmdBuffIndex;
	uint8_t *cmdPayload;
	uint16_t cmdPayloadLength;
} CommandStuct;

typedef struct {
	uint8_t enableUpdate; // 1 : enable , other : disable
	uint16_t nextPackageNumber;
	uint8_t data[FLASH_PAGE_SIZE]; // buff size must equal to flash page size
	uint32_t totalFirmwareBytes;
	uint32_t receivedFirmwareBytes;
	uint8_t firmwareChecksum;
} FirmwareUpdateStuct;

/* functions prototypes ---------------------------------------------*/
uint8_t CalculateChecksum(uint8_t *buff,uint16_t length);
void SendCmd(uint16_t cmd,uint8_t *buff,  uint16_t len, uint8_t status);
void SendCmdExecuteErrorRespose(void);
void SendCmdOkRespose(void);
void SendCmdOkWithDataRespose(uint8_t *buff,  uint16_t len);
void SendCmdParamErrorRespose(void);
void process_command_id(void);
void process_get_time(void);
void process_set_time(void);
void process_init_update_firmware(void);
void process_get_update_data(void);
void process_run_firmware(void);
void process_get_info(void);
void JumpToApp(void);
uint8_t CalculateFlashFirmwareChecksum(uint32_t addr);

/* variables ---------------------------------------------------------*/
CommandStuct cmd;
FirmwareUpdateStuct firmwareUpdate;
pFunction JumpToApplication;
uint32_t JumpAddress;

/* user code ---------------------------------------------------------*/
void process_command_id(void)
{
	switch(cmd.cmdId)
	{
		case GET_TIME:
			process_get_time();
			break;
		case SET_TIME:
			process_set_time();
			break;
		case INIT_UPDATA_FIRMWARE:
			process_init_update_firmware();
			break;
		case GET_UPDATA_DATA:
			process_get_update_data();
			break;
		case RUN_FIRMWARE:
			process_run_firmware();
			break;
		case GET_Info:
			process_get_info();
			break;
		default:
			// unknown command
			SendCmd(cmd.cmdId,0,0,CMD_Unknown_Command_Error);
			break;
	}
}

void process_get_time(void)
{
	RTC_DateTypeDef data = {0};
  RTC_TimeTypeDef time= {0};
	uint8_t buff[6];
	
  if (HAL_RTC_GetDate(&hrtc, &data, RTC_FORMAT_BIN) != HAL_OK)
	{
		SendCmdExecuteErrorRespose();
		return;
	}
  if (HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK)
	{
		SendCmdExecuteErrorRespose();
		return;
	}	
	buff[0] = data.Year;
	buff[1] = data.Month;
	buff[2] = data.Date;
	buff[3] = time.Hours;
	buff[4] = time.Minutes;
	buff[5] = time.Seconds;
	SendCmdOkWithDataRespose(buff,6);
}

void process_set_time(void)
{
	RTC_DateTypeDef data = {0};
  RTC_TimeTypeDef time= {0};
	if (cmd.cmdPayloadLength == 6)
	{
		data.Year = cmd.cmdPayload[0];
		data.Month = cmd.cmdPayload[1];
		data.Date = cmd.cmdPayload[2];
		time.Hours = cmd.cmdPayload[3];
		time.Minutes = cmd.cmdPayload[4];
		time.Seconds = cmd.cmdPayload[5];
		if (HAL_RTC_SetDate(&hrtc, &data, RTC_FORMAT_BIN) != HAL_OK)
		{
			SendCmdExecuteErrorRespose();
			return;
		}		
		if (HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK)
		{
			SendCmdExecuteErrorRespose();
			return;
		}
		SendCmdOkRespose();
	}
	else		
	{
		SendCmdParamErrorRespose();
	}	
}

void process_init_update_firmware(void)
{
	if ((cmd.cmdPayloadLength == 1) && (cmd.cmdPayload[0] == 0x01))
	{
		firmwareUpdate.enableUpdate = 1;
		firmwareUpdate.nextPackageNumber = 1;
		firmwareUpdate.totalFirmwareBytes = 0;
		firmwareUpdate.receivedFirmwareBytes = 0;
		SendCmdOkRespose();
	}
	else
	{
		firmwareUpdate.enableUpdate = 0;
		SendCmdParamErrorRespose();
	}
}

void process_get_update_data(void)
{	
	uint16_t i;
	uint16_t packageNum;
	uint8_t  *pkgFirmwareData;
	uint16_t pkgFirmwareSize; // this pakage firmware size in byte
	uint32_t flashDestination;
	uint8_t  dataNeedToWriteToFlash = 0;
	uint16_t dataWriteToFlashBytes = 0;
	uint32_t lastReceivedFirmwareBytes;
	
	packageNum = (uint16_t)(cmd.cmdPayload[0]<<8) + cmd.cmdPayload[1];
	
	if (firmwareUpdate.enableUpdate == 1)
	{
		if (packageNum != firmwareUpdate.nextPackageNumber)
		{ // update package must be sequency increasement
			SendCmdParamErrorRespose();
			return;
		}
		if ((firmwareUpdate.totalFirmwareBytes > 0) 
			&& (firmwareUpdate.receivedFirmwareBytes == firmwareUpdate.totalFirmwareBytes))
		{ // all firmware content has been received
			SendCmdParamErrorRespose();
			return;
		}
		if (packageNum == 1)
		{ // first package must contains the validate information(64 bytes)
			if (cmd.cmdPayloadLength < (64+2))
			{
				SendCmdParamErrorRespose();
				return;
			}
			firmwareUpdate.totalFirmwareBytes = (uint32_t)(cmd.cmdPayload[2]<<24) 
				+ (uint32_t)(cmd.cmdPayload[3]<<16) 
				+ (uint32_t)(cmd.cmdPayload[4]<<8) 
				+ cmd.cmdPayload[5];
			firmwareUpdate.firmwareChecksum = cmd.cmdPayload[6];
			if (firmwareUpdate.totalFirmwareBytes > MAX_FIRMWARE_BYTES)
			{
				SendCmdParamErrorRespose();
				return;
			}	
			FLASH_If_Init();			
		}
		pkgFirmwareData = (packageNum == 1) ? &cmd.cmdPayload[64 + 2] : &cmd.cmdPayload[2];
		pkgFirmwareSize = (packageNum == 1) ? (cmd.cmdPayloadLength - 64 - 2) : (cmd.cmdPayloadLength - 2);
		lastReceivedFirmwareBytes = firmwareUpdate.receivedFirmwareBytes;
		for (i = 0;i < pkgFirmwareSize;i++)
		{
			firmwareUpdate.data[firmwareUpdate.receivedFirmwareBytes % FLASH_PAGE_SIZE] = pkgFirmwareData[i];
			firmwareUpdate.receivedFirmwareBytes++;
			
			if (firmwareUpdate.receivedFirmwareBytes < firmwareUpdate.totalFirmwareBytes)
			{
				if ((firmwareUpdate.receivedFirmwareBytes % FLASH_PAGE_SIZE) == 0)
				{ 
					dataNeedToWriteToFlash = 1;
					dataWriteToFlashBytes = FLASH_PAGE_SIZE;					
				}
				else
				{
					dataNeedToWriteToFlash = 0;
					dataWriteToFlashBytes = 0;
				}
			}
			else
			{ // last package
				dataNeedToWriteToFlash = 1;
				dataWriteToFlashBytes = firmwareUpdate.receivedFirmwareBytes % FLASH_PAGE_SIZE;
				if (dataWriteToFlashBytes == 0)
				{
					dataWriteToFlashBytes = FLASH_PAGE_SIZE;
				}
			}
			if ((dataNeedToWriteToFlash == 1) && (dataWriteToFlashBytes > 0))
			{ // one page size of data received, write to the flash
				flashDestination = UPDATE_PACKAGE_ADDRESS 
					+ firmwareUpdate.receivedFirmwareBytes 
					- dataWriteToFlashBytes;
				if (FLASH_If_ErasePage(flashDestination) != FLASHIF_OK)
				{ // flash erase error
					SendCmdExecuteErrorRespose();	
					firmwareUpdate.receivedFirmwareBytes = lastReceivedFirmwareBytes;						
					return;
				}
				if (FLASH_If_Write(firmwareUpdate.data,flashDestination,dataWriteToFlashBytes) != FLASHIF_OK)
				{ // flash write error
					SendCmdExecuteErrorRespose();				
					firmwareUpdate.receivedFirmwareBytes = lastReceivedFirmwareBytes;		
					return;
				}
			}
			if (firmwareUpdate.receivedFirmwareBytes == firmwareUpdate.totalFirmwareBytes)
			{
				break;
			}
		}
		firmwareUpdate.nextPackageNumber++; // increase next package number
		SendCmdOkRespose();
	}
	else
	{
		SendCmdParamErrorRespose();
	}
}

void process_run_firmware(void)
{				
	if ((firmwareUpdate.enableUpdate == 1) 
		&& (firmwareUpdate.totalFirmwareBytes > 0)
		&& (firmwareUpdate.receivedFirmwareBytes == firmwareUpdate.totalFirmwareBytes))
	{
		if (CalculateFlashFirmwareChecksum(UPDATE_PACKAGE_ADDRESS) != firmwareUpdate.firmwareChecksum)
		{
			SendCmdParamErrorRespose();	
			return;
		}
		firmwareUpdate.data[0] = UPDATE_KEY;
		firmwareUpdate.data[1] = (firmwareUpdate.totalFirmwareBytes >> 24) & 0xFF;
		firmwareUpdate.data[2] = (firmwareUpdate.totalFirmwareBytes >> 16) & 0xFF;
		firmwareUpdate.data[3] = (firmwareUpdate.totalFirmwareBytes >> 8) & 0xFF;
		firmwareUpdate.data[4] = firmwareUpdate.totalFirmwareBytes & 0xFF;
		firmwareUpdate.data[5] = firmwareUpdate.firmwareChecksum;
		if (FLASH_If_ErasePage(UPDATE_INFO_ADDRESS) != FLASHIF_OK)
		{ // flash erase error
			SendCmdExecuteErrorRespose();						
			return;
		}
		if (FLASH_If_Write(firmwareUpdate.data,UPDATE_INFO_ADDRESS,FLASH_PAGE_SIZE) != FLASHIF_OK)
		{ // flash write error
			SendCmdExecuteErrorRespose();				
			return;
		}
		SendCmdOkRespose();
		osDelay(100);
		HAL_NVIC_SystemReset();
	}
	else
	{
		SendCmdParamErrorRespose();
	}
}

void process_get_info(void)
{	
	#ifdef BOOTLOADER_APPLICATION
	uint8_t msg[] = "msg from bootloader";
	SendCmdOkWithDataRespose(msg,sizeof(msg)/sizeof(char));
	#else
	uint8_t msg[] = "msg from application1"; // APP1 msg
	//uint8_t msg[] = "msg from application2"; // APP2 msg
	SendCmdOkWithDataRespose(msg,sizeof(msg)/sizeof(char));
	#endif
}

void JumpToApp(void)
{
	/* Test if user code is programmed starting from address "APPLICATION_ADDRESS" */
		if (((*(__IO uint32_t*)APPLICATION_ADDRESS) & 0x2FFE0000 ) == 0x20000000)
		{
			/* Jump to user application */
			JumpAddress = *(__IO uint32_t*) (APPLICATION_ADDRESS + 4);
			JumpToApplication = (pFunction) JumpAddress;
			/* Initialize user application's Stack Pointer */
		  __set_CONTROL(0);         
			__set_MSP(*(__IO uint32_t*) APPLICATION_ADDRESS);
			JumpToApplication();
		}
}

uint8_t CalculateFlashFirmwareChecksum(uint32_t addr)
{
	uint32_t i;	
	uint8_t checksum = 0;
	uint8_t dat;
	for (i = 0;i < firmwareUpdate.totalFirmwareBytes;i++)
	{
		FLASH_If_Read(&dat,addr + i,1);
		checksum = checksum^dat;
	}
	return checksum+1;
}

void CheckUpdate(void)
{
	uint32_t i;
	uint32_t updata_page_size;
	FLASH_If_Read(firmwareUpdate.data,UPDATE_INFO_ADDRESS,FLASH_PAGE_SIZE);
	firmwareUpdate.totalFirmwareBytes = (uint32_t)(firmwareUpdate.data[1]<<24) 
		+ (uint32_t)(firmwareUpdate.data[2]<<16) 
		+ (uint32_t)(firmwareUpdate.data[3]<<8) 
		+ firmwareUpdate.data[4];
	firmwareUpdate.firmwareChecksum = firmwareUpdate.data[5];
	if ((firmwareUpdate.totalFirmwareBytes > 0) 
		&& (firmwareUpdate.totalFirmwareBytes <= MAX_FIRMWARE_BYTES))
	{
		if ((firmwareUpdate.data[0] == UPDATE_KEY)
			&& (CalculateFlashFirmwareChecksum(UPDATE_PACKAGE_ADDRESS) == firmwareUpdate.firmwareChecksum))
		{ // update needed	
			if (firmwareUpdate.totalFirmwareBytes % FLASH_PAGE_SIZE == 0)
			{
				updata_page_size = firmwareUpdate.totalFirmwareBytes / FLASH_PAGE_SIZE;
			}
			else
			{
				updata_page_size = (firmwareUpdate.totalFirmwareBytes / FLASH_PAGE_SIZE) + 1;
			}
			FLASH_If_Init();
			for (i = 0;i < updata_page_size;i++)
			{
				FLASH_If_Read(firmwareUpdate.data,UPDATE_PACKAGE_ADDRESS + i * FLASH_PAGE_SIZE,FLASH_PAGE_SIZE);
				if (FLASH_If_ErasePage(APPLICATION_ADDRESS + i * FLASH_PAGE_SIZE) != FLASHIF_OK)
				{ // error : erase error
					break;
				}
				if (FLASH_If_Write(firmwareUpdate.data,APPLICATION_ADDRESS + i * FLASH_PAGE_SIZE,FLASH_PAGE_SIZE) != FLASHIF_OK)
				{ // error : write error
					break;
				}
				FeedWDG();
			}
			if (CalculateFlashFirmwareChecksum(APPLICATION_ADDRESS) == firmwareUpdate.firmwareChecksum)
			{ // clear update flag
				FLASH_If_Read(firmwareUpdate.data,UPDATE_INFO_ADDRESS,FLASH_PAGE_SIZE);
				firmwareUpdate.data[0] = 0x00;
				if (FLASH_If_ErasePage(UPDATE_INFO_ADDRESS) != FLASHIF_OK)
				{ // error : erase error
				}
				if (FLASH_If_Write(firmwareUpdate.data,UPDATE_INFO_ADDRESS,FLASH_PAGE_SIZE) != FLASHIF_OK)
				{ // error : write error
				}
			}
		}
		if (CalculateFlashFirmwareChecksum(APPLICATION_ADDRESS) == firmwareUpdate.firmwareChecksum)
		{
			HAL_Delay(1);
			JumpToApp();
		}
	}
}

void process_command_task(void)
{
	uint8_t data;
	osStatus_t status;
	ClearCmd();
  for(;;)
  {
		do
		{
			status = osMessageQueueGet(cmdRxQueueHandle, &data, NULL, 0U);
			if (status == osOK)
			{
				if (((cmd.cmdState == WaitingForCmdHeader) && (FRAME_HEADER == data)) 
					|| (cmd.cmdState != WaitingForCmdHeader))
				{ // add data to cmd buff
					if (cmd.cmdBuffIndex < CMD_BUFF_SIZE)
					{
						cmd.cmdBuff[cmd.cmdBuffIndex++] = data;
					}
					else
					{ // error : buff full
						SendCmdParamErrorRespose();
						ClearCmd();	
						break;	
					}
				}
				switch(cmd.cmdState)
				{
					case WaitingForCmdHeader:
						if(FRAME_HEADER == data)
						{
							cmd.cmdState = WaitingForCmdLength;
							if (osTimerStart(cmdReceiveTimeoutHandle, 900U) != osOK)
							{ // error : Timer could not be started
							}
						}
						break;
					case WaitingForCmdLength:
						cmd.cmdLength = data;
						cmd.cmdState = WaitingForCmdID;
						cmd.cmdPayloadLength = cmd.cmdLength - 4;
						break;	
					case WaitingForCmdID:		
						if (cmd.cmdBuffIndex >= 4)
						{							
							for (int i = 0;i < 2;i++)
							{
								cmd.cmdId = cmd.cmdId<<8;
								cmd.cmdId += cmd.cmdBuff[2+i];
							}	
							if (cmd.cmdPayloadLength > 0)
								cmd.cmdState = WaitingForCmdPayload;
							else
								cmd.cmdState = WaitingForCmdChecksum;
						}
						break;	
					case WaitingForCmdPayload:
						if (cmd.cmdBuffIndex >= cmd.cmdLength)
						{
							cmd.cmdState = WaitingForCmdChecksum;
						}
						break;	
					case WaitingForCmdChecksum:
						cmd.cmdChecksum = data;
						osDelay(1);
						if (osTimerStop(cmdReceiveTimeoutHandle) != osOK)
						{ // Timer could not be stopped
						}
						if(CalculateChecksum(&cmd.cmdBuff[1],cmd.cmdBuffIndex-2) == cmd.cmdChecksum)
						{
							process_command_id();
						}
						else							
						{ // checksum error
							SendCmd(cmd.cmdId,0,0,CMD_Checksum_Error);							
						}
						ClearCmd();
						break;
					default:
						break;						
				}
			}
		}while(status == osOK);
    osDelay(1);
  }
}

void SendCmdExecuteErrorRespose(void)
{
	SendCmd(cmd.cmdId,0,0,CMD_Excute_Error);
}

void SendCmdOkRespose(void)
{
	SendCmd(cmd.cmdId,0,0,CMD_OK);
}

void SendCmdOkWithDataRespose(uint8_t *buff,  uint16_t len)
{
	SendCmd(cmd.cmdId,buff,len,CMD_OK);
}

void SendCmdParamErrorRespose(void)
{
	SendCmd(cmd.cmdId,0,0,CMD_Param_Out_Of_Range_Error);
}

void ClearCmd(void)
{
	cmd.cmdId = 0;
	cmd.cmdLength = 0;
	cmd.cmdChecksum = 0;
	cmd.cmdState = WaitingForCmdHeader;
	cmd.cmdBuffIndex = 0;
	cmd.cmdPayload = &cmd.cmdBuff[4];
	cmd.cmdPayloadLength = 0;
}	

uint8_t CalculateChecksum(uint8_t *buff,uint16_t length)
{   
	uint16_t  i;
	uint8_t  tmp;
	tmp = 0;
	for(i=0;i<length;i++)
	{
		tmp = tmp^*buff;
		buff++;
	}
	tmp = tmp+1;
	return tmp; 
}

void SendCmd(uint16_t cmd,uint8_t *buff,  uint16_t len, uint8_t status)
{
	uint8_t data;
	uint8_t checksum = 0;
	data = FRAME_HEADER;
	osMessageQueuePut(cmdTxQueueHandle,&data,NULL,0U);
	data = len + 5;
	checksum = checksum^data;
	osMessageQueuePut(cmdTxQueueHandle,&data,NULL,0U);
	data = (cmd >> 8) & 0xff;
	checksum = checksum^data;
	osMessageQueuePut(cmdTxQueueHandle,&data,NULL,0U);
	data = cmd & 0xff;
	checksum = checksum^data;
	osMessageQueuePut(cmdTxQueueHandle,&data,NULL,0U);
	if (len > 0)
	{
		for (int i = 0;i < len;i++)
		{
			checksum = checksum^buff[i];
			osMessageQueuePut(cmdTxQueueHandle,&buff[i],NULL,0U);
		}
	}
	data = status;
	checksum = checksum^data;
	osMessageQueuePut(cmdTxQueueHandle,&data,NULL,0U);
	checksum = checksum + 1;
	osMessageQueuePut(cmdTxQueueHandle,&checksum,NULL,0U);
	LL_USART_EnableIT_TXE(USART1);
}

/***************************END OF FILE****/
