# stm32IAPDemo

stm32f103 + freeRTOS + uart command + IAP

This IAP demo put bootloader and application into one project.

## Flash Distribution

| Description   | Start Address| Size         |
| :-------------| :----------: | -----------: |
| Bootloader    | 0x08000000   | 0x14000      |
| Applicaton    | 0x08014000   | 0x14000      |
| Update Package| 0x08028000   | 0x14000      |
| Update Info   | 0x0803C000   | 0x800        |

## Boot Sequence

1. System start from address 0x08000000 to load bootloader
2. Bootloader load info from 'Update Info' region to check whether there was an update exist
3. If there was an updata package and package size is less then max package size and the CRC is correct, bootloader move the 'Update Package' region content to the 'Application' region and clear the Update Flag
4. Check the 'Application' region CRC, if CRC is correct, jump to the Application address and run, else stay on bootloader, the bootloader serves as an factory version

## Update Sequence

1. Host send a 'Init Update' command to device to notify the device prepare for update
2. Host send update content to device by packages. The first package's first 64 bytes contains the totoal firmware bytes count, CRC, and other validate information, we use this info to check whether the updata file matchs this device, and whether the totoal firmware is correctly received. When a FLASH_PAGE_SIZE firmware received, we write the content to the 'Update Package' region
3. If all the firmware was received and the CRC is correct, we set a Update flag to the 'Update Info' region, and restart this device to load bootloader and update firmware

## 1.How to build bootloader

uncomment '#define BOOTLOADER_APPLICATION' on main.h

```C
/* USER CODE BEGIN Private defines */

#define BOOTLOADER_APPLICATION

/* USER CODE END Private defines */
```

This will enable ```CheckUpdate``` Function on startup located at main.c

 ```C
/* USER CODE BEGIN 2 */
#ifdef BOOTLOADER_APPLICATION
CheckUpdate();
#endif
LL_USART_EnableIT_RXNE(USART1);
/* USER CODE END 2 */

 ```

 Set bootloader rom address to 0x8000000

![alt text](https://github.com/zachary-chi/stm32IAPDemo/blob/master/test/set%20bootloader%20rom%20address.png?raw=true)

## 2.How to build application

Comment '#define BOOTLOADER_APPLICATION' on main.h

```C
/* USER CODE BEGIN Private defines */

//#define BOOTLOADER_APPLICATION

/* USER CODE END Private defines */
```

This will disable ```CheckUpdate``` Function on startup located at main.c, and set set VTOR register to the application address

```C
/* USER CODE BEGIN 1 */
#ifndef BOOTLOADER_APPLICATION
 SCB->VTOR = APPLICATION_ADDRESS;
#endif
/* USER CODE END 1 */
```

 Set application rom address to 0x8014000

![alt text](https://github.com/zachary-chi/stm32IAPDemo/blob/master/test/set%20bootloader%20rom%20address.png?raw=true)

## 3.Test

We build a tool to simply our test, this tool was wrriten on C# language

### 1.First

we build the bootloader and download it to the flash, we use the get info command to see what msg income

```BASH
D:\stm32IAPDemo\test>stm32IAPDemo.exe -c COM2 -i
get info
msg from bootloader
press any key to exit...
```

### 2.Second

we use the update command to download the firmware APP1.bin to the flash

```BASH
D:\stm32IAPDemo\test>stm32IAPDemo.exe -c COM2 -u APP1.bin
update firmware...
update firmware done
press any key to exit...
```

 and excute the get info command again

```BASH
D:\stm32IAPDemo\test>stm32IAPDemo.exe -c COM2 -i
get info
msg from application1
press any key to exit...
```

### 3.Third

we use the update command to download the firmware APP2.bin to the flash

```BASH
D:\stm32IAPDemo\test>stm32IAPDemo.exe -c COM2 -u APP2.bin
update firmware...
update firmware done
press any key to exit...
```

 and excute the get info command again

```BASH
D:\stm32IAPDemo\test>stm32IAPDemo.exe -c COM2 -i
get info
msg from application2
press any key to exit...
```

Enjoy!
