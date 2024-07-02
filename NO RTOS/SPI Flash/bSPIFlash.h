/*
 * bSPIFlash.h
 *
 *  Created on: Jun 20, 2024
 *      Author: ben-linux
 */

#ifndef BSPIFLASH_BSPIFLASH_H_
#define BSPIFLASH_BSPIFLASH_H_

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"

/* Public typedef ------------------------------------------------------------*/
typedef struct
{
	uint32_t jedecID;
	uint16_t sectorSize;
	uint16_t pageSize;
	uint32_t flashSize;
}BFLASH_Info_td;
typedef enum
{
	BFLASH_ERROK = 0,
	BFLASH_ERRINUSE,						//Already in use by another process
	BFLASH_ERRBUSY,							//Busy processing request
	BFLASH_ERRTIMEOUT,						//Busy processing request
	BFLASH_ERRNOTSUPPORTED,					//Operation or feature not supported

}BFLASH_ERR;
typedef struct BFLASH_Access_td
{
	uint32_t address;
	uint8_t *data;
	uint32_t size;
	uint8_t complete:1;
	BFLASH_ERR result:7;
	void (*completeCallback)(struct BFLASH_Access_td *access, BFLASH_ERR result);
}BFLASH_Access_td;

/* Public define -------------------------------------------------------------*/
/* Public macro --------------------------------------------------------------*/
/* Public variables ----------------------------------------------------------*/
/* Public function prototypes ------------------------------------------------*/
void BFLASH_fastTick (void);
void BFLASH_tick (void);

BFLASH_ERR BFLASH_ConfigureFlash (uint32_t jedecID);
BFLASH_ERR BFLASH_GetID (BFLASH_Access_td *user);
BFLASH_ERR BFLASH_Read (BFLASH_Access_td *user);
BFLASH_ERR BFLASH_Write(BFLASH_Access_td *user);
BFLASH_ERR BFLASH_EraseFlash(BFLASH_Access_td *user);
BFLASH_ERR BFLASH_EraseSector(BFLASH_Access_td *user);

//SPI Control Routines
void BFLASH_TransmitCompleteHandler(void);
void BFLASH_TransmitReceiveCompleteHandler(void);

#endif /* BSPIFLASH_BSPIFLASH_H_ */
