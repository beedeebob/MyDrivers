/*
 * bSPIFlash.c
 *
 *  Created on: Jun 20, 2024
 *      Author: ben-linux
 */

/* Includes ------------------------------------------------------------------*/
#include "bSPIFlash.h"
#include "stddef.h"
#include "utils.h"
#include "string.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

//INSTRUCTIONS
#define bFLASH_READJEDECID								0x9F
#define bFLASH_READ										0x03
#define bFLASH_WRITEENABLE								0x06
#define bFLASH_PAGEPROGRAM								0x02
#define bFLASH_READSTATUS								0x05
#define 	bFLASH_READSTATUS_BSY						0x01
#define 	bFLASH_READSTATUS_WEL						0x02
#define bFLASH_ERASEFLASH								0x60
#define bFLASH_ERASESECTOR								0x20

//STATES
enum BFLASH_STATEs
{
	BFLASH_STATE_IDLE = 0,
	BFLASH_STATE_GETID,
	BFLASH_STATE_AWAITID,
	BFLASH_STATE_CMPLTID,
	BFLASH_STATE_READ,
	BFLASH_STATE_AWAITREADADD,
	BFLASH_STATE_READDATA,
	BFLASH_STATE_AWAITREADDATA,
	BFLASH_STATE_READCMPLT,
	BFLASH_STATE_WRITE,
	BFLASH_STATE_AWAITWRITEENABLE,
	BFLASH_STATE_WRITEADD,
	BFLASH_STATE_AWAITWRITEADD,
	BFLASH_STATE_WRITEDATA,
	BFLASH_STATE_AWAITWRITEDATA,
	BFLASH_STATE_WRITEREADSTATUS,
	BFLASH_STATE_AWAITWRITEREADSTATUS,
	BFLASH_STATE_AWAITWRITECMPLT,
	BFLASH_STATE_WRITECMPLT,
	BFLASH_STATE_READSTAT,
	BFLASH_STATE_AWAITREADSTAT,
	BFLASH_STATE_READSTATCMPLT,
	BFLASH_STATE_ERASEFLASH,
	BFLASH_STATE_AWAITERASEFLASHWRITEENABLE,
	BFLASH_STATE_ERASEFLASHINS,
	BFLASH_STATE_AWAITERASEFLASHINS,
	BFLASH_STATE_ERASEFLASHREADSTATUS,
	BFLASH_STATE_AWAITERASEFLASHREADSTATUS,
	BFLASH_STATE_AWAITERASEFLASHCMPLT,
	BFLASH_STATE_ERASESECTOR,
	BFLASH_STATE_AWAITERASESECTORWRITEENABLE,
	BFLASH_STATE_ERASESECTORINS,
	BFLASH_STATE_AWAITERASESECTORINS,
	BFLASH_STATE_ERASESECTORREADSTATUS,
	BFLASH_STATE_AWAITERASESECTORREADSTATUS,
	BFLASH_STATE_AWAITERASESECTORCMPLT,
};

//FLAGS
enum BFLASH_FLAGs
{
	BFLASH_FLAG_TXCOMPLETE = 0x01,
	BFLASH_FLAG_HAVEID = 0x02,
	BFLASH_FLAG_TXRXCOMPLETE = 0x04,
};

/* Private macro -------------------------------------------------------------*/
#define PAGEOFFSET(ADD, SIZE)				(ADD & (SIZE - 1))
#define PAGESPACE(ADD, SIZE)				(SIZE - PAGEOFFSET(ADD, SIZE))
#define SECTOROFFSET(ADD, SIZE)				(ADD & (SIZE - 1))
#define SECTORSPACE(ADD, SIZE)				(SIZE - SECTOROFFSET(ADD, SIZE))

/* Private variables ---------------------------------------------------------*/
static BFLASH_Info_td flashInfo;
static uint8_t buffer[20];
static BFLASH_Access_td *currentUser;

static uint8_t state;
static uint8_t flags;
static uint8_t spiTmr;
static uint32_t offset;

/* Private function prototypes -----------------------------------------------*/
void BFLASH_ChipSelectCallback(uint8_t pinState);
BFLASH_ERR BFLASH_TransmitCallback(uint8_t *data, uint32_t length);
BFLASH_ERR BFLASH_TransmitReceiveCallback(uint8_t * txData, uint8_t *rxData, uint32_t length);
BFLASH_ERR BFLASH_GetSPIStatus(void);

static void BFLASH_ManageGetID (void);
static void BFLASH_ManageRead (void);
static void BFLASH_ManageWrite (void);
static void BFLASH_ManageEraseFlash(void);
static void BFLASH_ManageEraseSector(void);

/* Private functions ---------------------------------------------------------*/
/**
  * @brief 	Tick interface at a 100us/10us for faster SPI
  * @param 	None
  * @retval None
  */
void BFLASH_fastTick (void)
{
	if(currentUser == NULL)
		return;

	BFLASH_ManageGetID();
	BFLASH_ManageRead();
	BFLASH_ManageWrite();
	BFLASH_ManageEraseFlash();
	BFLASH_ManageEraseSector();
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Tick interface at standard 1 millisecond
  * @param 	None
  * @retval None
  */
void BFLASH_tick (void)
{
	if(spiTmr > 0)
		spiTmr--;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Configure the flash using the JEDEC ID
  * @param 	jedecID: JEDEC ID of the flash
  * @retval None
  */
BFLASH_ERR BFLASH_ConfigureFlash (uint32_t jedecID)
{
	flashInfo.jedecID = jedecID;
	switch(jedecID)
	{
	//Winbond
	case 0xef4017:
		flashInfo.flashSize = 0x800000;
		flashInfo.pageSize = 0x100;
		flashInfo.sectorSize = 0x1000;
		return BFLASH_ERROK;
	}
	return BFLASH_ERRNOTSUPPORTED;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Request the ID of the spi flash. The access struct will be marked as
  * 		complete when complete, and the completeCallback will be called.
  * @param 	user: pointer to the user requesting the information
  * @retval BFLASH_ERR
  */
BFLASH_ERR BFLASH_GetID (BFLASH_Access_td *user)
{
	if((currentUser != NULL) && (currentUser != user))
		return BFLASH_ERRINUSE;
	if(currentUser == user)
		return BFLASH_ERRBUSY;
	currentUser = user;

	user->complete = 0;

	state = BFLASH_STATE_GETID;
	BFLASH_ManageGetID();

	return BFLASH_ERROK;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Manage the process of retrieving the ID
  * @param 	None
  * @retval None
  */
static void BFLASH_ManageGetID (void)
{
	//GET JEDEC ID
	/*
	 * On call
	 * 	o CS LOW
	 * 	o SPI START
	 * On RXTX complete
	 * 	o CS HI
	 *  o Release
	 */

	switch(state)
	{
	case BFLASH_STATE_GETID:
		BFLASH_ChipSelectCallback(0);

		buffer[0] = bFLASH_READJEDECID;
		buffer[1] = 0x00;
		buffer[2] = 0x00;
		buffer[3] = 0x00;

		flags &= ~BFLASH_FLAG_TXRXCOMPLETE;
		if(BFLASH_TransmitReceiveCallback(buffer, buffer, 4) == BFLASH_ERROK)
		{
			state = BFLASH_STATE_AWAITID;
			spiTmr = 10;
		}
		break;

	case BFLASH_STATE_AWAITID:
		if(flags & BFLASH_FLAG_TXRXCOMPLETE)
		{
			currentUser->result =  BFLASH_ERROK;
			state = BFLASH_STATE_CMPLTID;
		}
		else if (spiTmr == 0)
		{
			currentUser->result =  BFLASH_ERRTIMEOUT;
			state = BFLASH_STATE_GETID;
		}
		else
			break;

	case BFLASH_STATE_CMPLTID:
		BFLASH_ChipSelectCallback(1);

		memcpy(currentUser->data, &buffer[1], 3);
		currentUser->complete = 1;
		BFLASH_Access_td *user = currentUser;
		currentUser = NULL;
		state = BFLASH_STATE_IDLE;

		if(user->completeCallback != NULL)
			user->completeCallback(user, user->result);
		break;
	}
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Read data from the flash.
  * @param 	user: pointer to the user requesting the information
  * @retval BFLASH_ERR
  */
BFLASH_ERR BFLASH_Read (BFLASH_Access_td *user)
{
	if((currentUser != NULL) && (currentUser != user))
		return BFLASH_ERRINUSE;
	if(currentUser == user)
		return BFLASH_ERRBUSY;
	currentUser = user;

	user->complete = 0;

	state = BFLASH_STATE_READ;
	BFLASH_ManageRead();

	return BFLASH_ERROK;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Manage the process of reading data from the flash
  * @param 	None
  * @retval None
  */
static void BFLASH_ManageRead (void)
{
	//Read
	/*
	 * On call
	 * 	o CS LOW
	 * 	o SPI ADDR START
	 * On TX complete
	 * 	o SPI DATA START
	 * On RXTX complete
	 *  o CS HI
	 *  o Release
	 */

	switch(state)
	{
	case BFLASH_STATE_READ:

		BFLASH_ChipSelectCallback(0);

		buffer[0] = bFLASH_READ;
		buffer[1] = (uint8_t)(currentUser->address >> 16);
		buffer[2] = (uint8_t)(currentUser->address >> 8);
		buffer[3] = (uint8_t)(currentUser->address);

		flags &= ~BFLASH_FLAG_TXRXCOMPLETE;
		if(BFLASH_TransmitReceiveCallback(buffer, buffer, 4) == BFLASH_ERROK)
		{
			state = BFLASH_STATE_AWAITREADADD;
			spiTmr = 10;
		}
		break;

	case BFLASH_STATE_AWAITREADADD:
		if(flags & BFLASH_FLAG_TXRXCOMPLETE)
		{
			state = BFLASH_STATE_READDATA;
		}
		else if (spiTmr == 0)
		{
			BFLASH_ChipSelectCallback(1);

			state = BFLASH_STATE_IDLE;

			currentUser->result = BFLASH_ERRTIMEOUT;
			currentUser->complete = 1;
			BFLASH_Access_td *user = currentUser;
			currentUser = NULL;

			if(user->completeCallback != NULL)
				user->completeCallback(user, user->result);
			break;
		}
		else
			break;

	case BFLASH_STATE_READDATA:
		flags &= ~BFLASH_FLAG_TXRXCOMPLETE;
		if(BFLASH_TransmitReceiveCallback(currentUser->data, currentUser->data, currentUser->size) == BFLASH_ERROK)
		{
			state = BFLASH_STATE_AWAITREADDATA;
			spiTmr = 10;
		}
		break;

	case BFLASH_STATE_AWAITREADDATA:
		if(flags & BFLASH_FLAG_TXRXCOMPLETE)
		{
			currentUser->result = BFLASH_ERROK;
			state = BFLASH_STATE_READCMPLT;
		}
		else if (spiTmr == 0)
		{
			currentUser->result = BFLASH_ERRTIMEOUT;
			state = BFLASH_STATE_READCMPLT;
		}
		else
			break;

	case BFLASH_STATE_READCMPLT:
		BFLASH_ChipSelectCallback(1);

		state = BFLASH_STATE_IDLE;

		currentUser->complete = 1;
		BFLASH_Access_td *user = currentUser;
		currentUser = NULL;

		if(user->completeCallback != NULL)
			user->completeCallback(user, user->result);
		break;
	}
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Write data to the flash. When complete, the access struct complete
  * 		will be set, and the access struct callback will be executed
  * @param 	user: pointer to the user requesting the information
  * @retval BFLASH_ERR
  */
BFLASH_ERR BFLASH_Write(BFLASH_Access_td *user)
{
	if((currentUser != NULL) && (currentUser != user))
		return BFLASH_ERRINUSE;
	if(currentUser == user)
		return BFLASH_ERRBUSY;
	currentUser = user;

	user->complete = 0;
	offset = 0;

	state = BFLASH_STATE_WRITE;
	BFLASH_ManageWrite();

	return BFLASH_ERROK;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Manage the process of reading data from the flash
  * @param 	None
  * @retval None
  */
static void BFLASH_ManageWrite(void)
{
	//Write
	/*
	 * On call
	 * 	o CS LOW
	 * 	o SPI WRITE ENABLE
	 * On RXTX complete
	 *  o CS HI
	 * On flash thread
	 *  o CS LO
	 * 	o SPI ADDR START
	 * On RXTX complete
	 * 	o SPI DATA START
	 * On RXTX complete
	 *  o CS HI
	 * On flash thread
	 *  o CS LO
	 *  o SPI READ STATUS
	 * on RXTX complete
	 *  o CS HI
	 *  o Check result
	 *  o Repeat check/Write again/Release
	 */
	switch(state)
	{
	case BFLASH_STATE_WRITE:
		BFLASH_ChipSelectCallback(0);

		buffer[0] = bFLASH_WRITEENABLE;

		flags &= ~BFLASH_FLAG_TXRXCOMPLETE;
		if(BFLASH_TransmitReceiveCallback(buffer, buffer, 1) == BFLASH_ERROK)
		{
			state = BFLASH_STATE_AWAITWRITEENABLE;
			spiTmr = 10;
		}
		break;

	case BFLASH_STATE_AWAITWRITEENABLE:
		if(flags & BFLASH_FLAG_TXRXCOMPLETE)
		{
			BFLASH_ChipSelectCallback(1);

			state = BFLASH_STATE_WRITEADD;
		}
		else if (spiTmr == 0)
		{
			BFLASH_ChipSelectCallback(1);

			state = BFLASH_STATE_IDLE;

			currentUser->result = BFLASH_ERRTIMEOUT;
			currentUser->complete = 1;
			BFLASH_Access_td *user = currentUser;
			currentUser = NULL;

			if(user->completeCallback != NULL)
				user->completeCallback(user, user->result);
		}
		break;

	case BFLASH_STATE_WRITEADD:
		BFLASH_ChipSelectCallback(0);

		buffer[0] = bFLASH_PAGEPROGRAM;
		buffer[1] = (uint8_t)((currentUser->address + offset) >> 16);
		buffer[2] = (uint8_t)((currentUser->address + offset) >> 8);
		buffer[3] = (uint8_t)((currentUser->address + offset));

		flags &= ~BFLASH_FLAG_TXCOMPLETE;
		if(BFLASH_TransmitCallback(buffer, 4) == BFLASH_ERROK)
		{
			state = BFLASH_STATE_AWAITWRITEADD;
			spiTmr = 10;
		}
		break;

	case BFLASH_STATE_AWAITWRITEADD:
		if(flags & BFLASH_FLAG_TXCOMPLETE)
		{
			state = BFLASH_STATE_WRITEDATA;
		}
		else if (spiTmr == 0)
		{
			BFLASH_ChipSelectCallback(1);

			state = BFLASH_STATE_IDLE;

			currentUser->result = BFLASH_ERRTIMEOUT;
			currentUser->complete = 1;
			BFLASH_Access_td *user = currentUser;
			currentUser = NULL;

			if(user->completeCallback != NULL)
				user->completeCallback(user, user->result);
			break;
		}
		else
			break;

	case BFLASH_STATE_WRITEDATA:
	{
		uint32_t length = currentUser->size - offset;
		if(length > (PAGESPACE((currentUser->address + offset), flashInfo.pageSize)))
			length = (PAGESPACE((currentUser->address + offset), flashInfo.pageSize));

		flags &= ~BFLASH_FLAG_TXCOMPLETE;
		if(BFLASH_TransmitCallback(&currentUser->data[offset], length) == BFLASH_ERROK)
		{
			offset += length;
			state = BFLASH_STATE_AWAITWRITEDATA;
			spiTmr = 10;
		}
		break;
	}

	case BFLASH_STATE_AWAITWRITEDATA:
		if(BFLASH_GetSPIStatus() == BFLASH_ERROK)
		{
			BFLASH_ChipSelectCallback(1);
			state = BFLASH_STATE_WRITEREADSTATUS;
		}
		else if (spiTmr == 0)
		{
			BFLASH_ChipSelectCallback(1);

			state = BFLASH_STATE_IDLE;

			currentUser->result = BFLASH_ERRTIMEOUT;
			currentUser->complete = 1;
			BFLASH_Access_td *user = currentUser;
			currentUser = NULL;

			if(user->completeCallback != NULL)
				user->completeCallback(user, user->result);
			break;
		}
		break;

	case BFLASH_STATE_WRITEREADSTATUS:

		BFLASH_ChipSelectCallback(0);

		buffer[0] = bFLASH_READSTATUS;

		flags &= ~BFLASH_FLAG_TXRXCOMPLETE;
		if(BFLASH_TransmitReceiveCallback(buffer, buffer, 2) == BFLASH_ERROK)
		{
			state = BFLASH_STATE_AWAITWRITEREADSTATUS;
			spiTmr = 10;
		}
		break;

	case BFLASH_STATE_AWAITWRITEREADSTATUS:
		if(flags & BFLASH_FLAG_TXRXCOMPLETE)
		{
			state = BFLASH_STATE_AWAITWRITECMPLT;
		}
		else if (spiTmr == 0)
		{
			BFLASH_ChipSelectCallback(1);

			state = BFLASH_STATE_IDLE;

			currentUser->result = BFLASH_ERRTIMEOUT;
			currentUser->complete = 1;
			BFLASH_Access_td *user = currentUser;
			currentUser = NULL;

			if(user->completeCallback != NULL)
				user->completeCallback(user, user->result);
			break;
		}
		else
			break;

	case BFLASH_STATE_AWAITWRITECMPLT:
		BFLASH_ChipSelectCallback(1);

		if(buffer[1] & (bFLASH_READSTATUS_BSY | bFLASH_READSTATUS_WEL))
		{
			state = BFLASH_STATE_WRITEREADSTATUS;
			break;
		}
		else if (offset < currentUser->size)
		{
			state = BFLASH_STATE_WRITE;
			break;
		}
		else
		{
			state = BFLASH_STATE_WRITECMPLT;
		}

	case BFLASH_STATE_WRITECMPLT:

		state = BFLASH_STATE_IDLE;

		currentUser->result = BFLASH_ERROK;
		currentUser->complete = 1;
		BFLASH_Access_td *user = currentUser;
		currentUser = NULL;

		if(user->completeCallback != NULL)
			user->completeCallback(user, user->result);
		break;
	}
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Erase the entire flash. When complete, the access struct complete
  * 		will be set, and the access struct callback will be executed
  * @param 	user: pointer to the user requesting the information
  * @retval BFLASH_ERR
  */
BFLASH_ERR BFLASH_EraseFlash(BFLASH_Access_td *user)
{
	if((currentUser != NULL) && (currentUser != user))
		return BFLASH_ERRINUSE;
	if(currentUser == user)
		return BFLASH_ERRBUSY;
	currentUser = user;

	user->complete = 0;
	offset = 0;

	state = BFLASH_STATE_ERASEFLASH;
	BFLASH_ManageEraseFlash();

	return BFLASH_ERROK;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Manage the process of reading data from the flash
  * @param 	None
  * @retval None
  */
static void BFLASH_ManageEraseFlash(void)
{
	//Erase
	/*
	 * On call
	 * 	o CS LOW
	 * 	o SPI WRITE ENABLE
	 * On RXTX complete
	 *  o CS HI
	 * On flash thread
	 *  o CS LO
	 * 	o SPI ERASE START
	 * On RXTX complete
	 *  o CS HI
	 * On flash thread
	 *  o CS LO
	 *  o SPI READ STATUS
	 * on RXTX complete
	 *  o CS HI
	 *  o Check result
	 *  o Repeat check/Release
	 */
	switch(state)
	{
	case BFLASH_STATE_ERASEFLASH:
		BFLASH_ChipSelectCallback(0);

		buffer[0] = bFLASH_WRITEENABLE;

		flags &= ~BFLASH_FLAG_TXRXCOMPLETE;
		if(BFLASH_TransmitReceiveCallback(buffer, buffer, 1) == BFLASH_ERROK)
		{
			state = BFLASH_STATE_AWAITERASEFLASHWRITEENABLE;
			spiTmr = 10;
		}
		break;

	case BFLASH_STATE_AWAITERASEFLASHWRITEENABLE:
		if(flags & BFLASH_FLAG_TXRXCOMPLETE)
		{
			BFLASH_ChipSelectCallback(1);

			state = BFLASH_STATE_ERASEFLASHINS;
		}
		else if (spiTmr == 0)
		{
			BFLASH_ChipSelectCallback(1);

			state = BFLASH_STATE_IDLE;

			currentUser->result = BFLASH_ERRTIMEOUT;
			currentUser->complete = 1;
			BFLASH_Access_td *user = currentUser;
			currentUser = NULL;

			if(user->completeCallback != NULL)
				user->completeCallback(user, user->result);
		}
		break;

	case BFLASH_STATE_ERASEFLASHINS:
		BFLASH_ChipSelectCallback(0);

		buffer[0] = bFLASH_ERASEFLASH;

		flags &= ~BFLASH_FLAG_TXRXCOMPLETE;
		if(BFLASH_TransmitReceiveCallback(buffer, buffer, 1) == BFLASH_ERROK)
		{
			state = BFLASH_STATE_AWAITERASEFLASHINS;
			spiTmr = 10;
		}
		break;

	case BFLASH_STATE_AWAITERASEFLASHINS:
		if(flags & BFLASH_FLAG_TXRXCOMPLETE)
		{
			BFLASH_ChipSelectCallback(1);
			state = BFLASH_STATE_ERASEFLASHREADSTATUS;
		}
		else if (spiTmr == 0)
		{
			BFLASH_ChipSelectCallback(1);

			state = BFLASH_STATE_IDLE;

			currentUser->result = BFLASH_ERRTIMEOUT;
			currentUser->complete = 1;
			BFLASH_Access_td *user = currentUser;
			currentUser = NULL;

			if(user->completeCallback != NULL)
				user->completeCallback(user, user->result);
		}
		break;

	case BFLASH_STATE_ERASEFLASHREADSTATUS:

		BFLASH_ChipSelectCallback(0);

		buffer[0] = bFLASH_READSTATUS;

		flags &= ~BFLASH_FLAG_TXRXCOMPLETE;
		if(BFLASH_TransmitReceiveCallback(buffer, buffer, 2) == BFLASH_ERROK)
		{
			state = BFLASH_STATE_AWAITERASEFLASHREADSTATUS;
			spiTmr = 10;
		}
		break;

	case BFLASH_STATE_AWAITERASEFLASHREADSTATUS:
		if(flags & BFLASH_FLAG_TXRXCOMPLETE)
		{
			state = BFLASH_STATE_AWAITERASEFLASHCMPLT;
		}
		else if (spiTmr == 0)
		{
			BFLASH_ChipSelectCallback(1);

			state = BFLASH_STATE_IDLE;

			currentUser->result = BFLASH_ERRTIMEOUT;
			currentUser->complete = 1;
			BFLASH_Access_td *user = currentUser;
			currentUser = NULL;

			if(user->completeCallback != NULL)
				user->completeCallback(user, user->result);
			break;
		}
		else
			break;

	case BFLASH_STATE_AWAITERASEFLASHCMPLT:
		BFLASH_ChipSelectCallback(1);

		if(buffer[1] & (bFLASH_READSTATUS_BSY | bFLASH_READSTATUS_WEL))
		{
			state = BFLASH_STATE_ERASEFLASHREADSTATUS;
			break;
		}
		else
		{
			state = BFLASH_STATE_IDLE;

			currentUser->result = BFLASH_ERROK;
			currentUser->complete = 1;
			BFLASH_Access_td *user = currentUser;
			currentUser = NULL;

			if(user->completeCallback != NULL)
				user->completeCallback(user, user->result);
		}
	}
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Erase a flash sector.
  * 		Specify the address of the sector from which to erase
  * 		When complete, the access struct complete
  * 		will be set, and the access struct callback will be executed
  * @param 	user: pointer to the user requesting the information
  * @retval BFLASH_ERR
  */
BFLASH_ERR BFLASH_EraseSector(BFLASH_Access_td *user)
{
	if((currentUser != NULL) && (currentUser != user))
		return BFLASH_ERRINUSE;
	if(currentUser == user)
		return BFLASH_ERRBUSY;
	currentUser = user;

	user->complete = 0;

	state = BFLASH_STATE_ERASESECTOR;
	BFLASH_ManageEraseSector();

	return BFLASH_ERROK;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Manage the process of erasing a sector from flash
  * @param 	None
  * @retval None
  */
static void BFLASH_ManageEraseSector(void)
{
	//Erase
	/*
	 * On call
	 * 	o CS LOW
	 * 	o SPI WRITE ENABLE
	 * On RXTX complete
	 *  o CS HI
	 * On flash thread
	 *  o CS LO
	 * 	o SPI ERASE START
	 * On RXTX complete
	 *  o CS HI
	 * On flash thread
	 *  o CS LO
	 *  o SPI READ STATUS
	 * on RXTX complete
	 *  o CS HI
	 *  o Check result
	 *  o Repeat check/Release
	 */
	switch(state)
	{
	case BFLASH_STATE_ERASESECTOR:
		BFLASH_ChipSelectCallback(0);

		buffer[0] = bFLASH_WRITEENABLE;

		flags &= ~BFLASH_FLAG_TXRXCOMPLETE;
		if(BFLASH_TransmitReceiveCallback(buffer, buffer, 1) == BFLASH_ERROK)
		{
			state = BFLASH_STATE_AWAITERASESECTORWRITEENABLE;
			spiTmr = 10;
		}
		break;

	case BFLASH_STATE_AWAITERASESECTORWRITEENABLE:
		if(flags & BFLASH_FLAG_TXRXCOMPLETE)
		{
			BFLASH_ChipSelectCallback(1);

			state = BFLASH_STATE_ERASESECTORINS;
		}
		else if (spiTmr == 0)
		{
			BFLASH_ChipSelectCallback(1);

			state = BFLASH_STATE_IDLE;

			currentUser->result = BFLASH_ERRTIMEOUT;
			currentUser->complete = 1;
			BFLASH_Access_td *user = currentUser;
			currentUser = NULL;

			if(user->completeCallback != NULL)
				user->completeCallback(user, user->result);
		}
		break;

	case BFLASH_STATE_ERASESECTORINS:
		BFLASH_ChipSelectCallback(0);

		buffer[0] = bFLASH_ERASESECTOR;
		buffer[1] = (uint8_t)((currentUser->address) >> 16);
		buffer[2] = (uint8_t)((currentUser->address) >> 8);
		buffer[3] = (uint8_t)((currentUser->address));


		flags &= ~BFLASH_FLAG_TXRXCOMPLETE;
		if(BFLASH_TransmitReceiveCallback(buffer, buffer, 4) == BFLASH_ERROK)
		{
			state = BFLASH_STATE_AWAITERASESECTORINS;
			spiTmr = 10;
		}
		break;

	case BFLASH_STATE_AWAITERASESECTORINS:
		if(flags & BFLASH_FLAG_TXRXCOMPLETE)
		{
			BFLASH_ChipSelectCallback(1);
			state = BFLASH_STATE_ERASESECTORREADSTATUS;
		}
		else if (spiTmr == 0)
		{
			BFLASH_ChipSelectCallback(1);

			state = BFLASH_STATE_IDLE;

			currentUser->result = BFLASH_ERRTIMEOUT;
			currentUser->complete = 1;
			BFLASH_Access_td *user = currentUser;
			currentUser = NULL;

			if(user->completeCallback != NULL)
				user->completeCallback(user, user->result);
		}
		break;

	case BFLASH_STATE_ERASESECTORREADSTATUS:

		BFLASH_ChipSelectCallback(0);

		buffer[0] = bFLASH_READSTATUS;

		flags &= ~BFLASH_FLAG_TXRXCOMPLETE;
		if(BFLASH_TransmitReceiveCallback(buffer, buffer, 2) == BFLASH_ERROK)
		{
			state = BFLASH_STATE_AWAITERASESECTORREADSTATUS;
			spiTmr = 10;
		}
		break;

	case BFLASH_STATE_AWAITERASESECTORREADSTATUS:
		if(flags & BFLASH_FLAG_TXRXCOMPLETE)
		{
			state = BFLASH_STATE_AWAITERASESECTORCMPLT;
		}
		else if (spiTmr == 0)
		{
			BFLASH_ChipSelectCallback(1);

			state = BFLASH_STATE_IDLE;

			currentUser->result = BFLASH_ERRTIMEOUT;
			currentUser->complete = 1;
			BFLASH_Access_td *user = currentUser;
			currentUser = NULL;

			if(user->completeCallback != NULL)
				user->completeCallback(user, user->result);
			break;
		}
		else
			break;

	case BFLASH_STATE_AWAITERASESECTORCMPLT:
		BFLASH_ChipSelectCallback(1);

		if(buffer[1] & (bFLASH_READSTATUS_BSY | bFLASH_READSTATUS_WEL))
		{
			state = BFLASH_STATE_ERASESECTORREADSTATUS;
		}
		else
		{
			state = BFLASH_STATE_IDLE;

			currentUser->result = BFLASH_ERROK;
			currentUser->complete = 1;
			BFLASH_Access_td *user = currentUser;
			currentUser = NULL;

			if(user->completeCallback != NULL)
				user->completeCallback(user, user->result);
		}
		break;
	}
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Chip select control callback
  * @param 	pinState: the desired state of the chip select pin on the flash
  * @retval None
  */
__attribute ((weak)) void BFLASH_ChipSelectCallback(uint8_t pinState)
{

}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	SPI transmit routine
  * @param 	data: Pointer to the data to transmit
  * @param 	length: Amount of data to transmit
  * @retval BFLASH_ERR
  */
__attribute ((weak)) BFLASH_ERR BFLASH_TransmitCallback(uint8_t *data, uint32_t length)
{
	return BFLASH_ERROK;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	SPI transmit routine
  * @param 	data: Pointer to the buffer into which to read
  * @param 	length: Amount of data to transmit
  * @retval BFLASH_ERR
  */
__attribute ((weak)) BFLASH_ERR BFLASH_TransmitReceiveCallback(uint8_t * txData, uint8_t *rxData, uint32_t length)
{
	return BFLASH_ERROK;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Get the SPI status
  * @param 	None
  * @retval OK if ready
  */
__attribute ((weak)) BFLASH_ERR BFLASH_GetSPIStatus(void)
{
	return BFLASH_ERROK;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Notify the system that the SPI transmit has completed
  * @param 	None
  * @retval None
  */
void BFLASH_TransmitCompleteHandler(void)
{
	flags |= BFLASH_FLAG_TXCOMPLETE;

//	if(currentUser == NULL)
//		return;
//
//	BFLASH_ManageGetID();
//	BFLASH_ManageRead();
//	BFLASH_ManageWrite();
//	BFLASH_ManageEraseFlash();
//	BFLASH_ManageEraseSector();
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Notify the system that the SPI transmit has completed
  * @param 	None
  * @retval None
  */
void BFLASH_TransmitReceiveCompleteHandler(void)
{
	flags |= BFLASH_FLAG_TXRXCOMPLETE;

//	if(currentUser == NULL)
//		return;
//
//	BFLASH_ManageGetID();
//	BFLASH_ManageRead();
//	BFLASH_ManageWrite();
//	BFLASH_ManageEraseFlash();
//	BFLASH_ManageEraseSector();
}

