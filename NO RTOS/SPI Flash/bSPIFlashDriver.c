/*
 * bSPIFlashDriver.c
 *
 *  Created on: Jun 20, 2024
 *      Author: ben-linux
 */

/* Includes ------------------------------------------------------------------*/
#include "bSPIFlash.h"
#include "main.h"
#include "utils.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void BFLASHDRIV_GetICCompleteCallback(BFLASH_Access_td *access, BFLASH_ERR result);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief 	Chip select control callback
  * @param 	pinState: the desired state of the chip select pin on the flash
  * @retval None
  */
void BFLASHDRIV_init(void)
{
	LL_SPI_Enable(SPI1);
	LL_SPI_EnableDMAReq_TX(SPI1);

	static BFLASH_Access_td getIDAccess;
	static uint8_t id[3];
	getIDAccess.data = id;
	getIDAccess.completeCallback = BFLASHDRIV_GetICCompleteCallback;
	BFLASH_GetID(&getIDAccess);
}
/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Callback handler for ID received
  * @param 	result:
  * @retval None
  */
static void BFLASHDRIV_GetICCompleteCallback(BFLASH_Access_td *access, BFLASH_ERR result)
{
	if(result == BFLASH_ERROK)
	{
		uint32_t jedecID = BYTESTOUINT24BIGENDIAN(access->data, 0);
		if(BFLASH_ConfigureFlash(jedecID) != BFLASH_ERROK)
			Error_Handler();
	}
	else
	{
		BFLASH_GetID(access);
	}
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Chip select control callback
  * @param 	pinState: the desired state of the chip select pin on the flash
  * @retval None
  */
void BFLASH_ChipSelectCallback(uint8_t pinState)
{
	HAL_GPIO_WritePin(GPIO_FLASH_NCS_GPIO_Port, GPIO_FLASH_NCS_Pin, pinState);
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	SPI transmit routine
  * @param 	data: Pointer to the data to transmit
  * @param 	length: Amount of data to transmit
  * @retval BFLASH_ERR
  */
BFLASH_ERR BFLASH_TransmitCallback(uint8_t *data, uint32_t length)
{
	SPI1->CR1 |= SPI_CR1_BIDIOE;
	SPI1->CR1 |= SPI_CR1_BIDIMODE;
	LL_SPI_DisableDMAReq_RX(SPI1);

	//SPI TX
	LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, length);
	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_2, (uint32_t)data);
	LL_DMA_ClearFlag_TC1(DMA1);
	LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_2);
	LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);

	return BFLASH_ERROK;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	SPI transmit routine
  * @param 	data: Pointer to the buffer into which to read
  * @param 	length: Amount of data to transmit
  * @retval BFLASH_ERR
  */
BFLASH_ERR BFLASH_TransmitReceiveCallback(uint8_t * txData, uint8_t *rxData, uint32_t length)
{
	if(LL_SPI_IsActiveFlag_BSY(SPI1))
		return BFLASH_ERRINUSE;

	SPI1->CR1 &= ~SPI_CR1_BIDIMODE;
	SPI1->CR1 &= ~SPI_CR1_BIDIOE;
	LL_SPI_EnableDMAReq_RX(SPI1);

	//SPI RX
	LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, length);
	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t)rxData);
	LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
	LL_DMA_ClearFlag_TC1(DMA1);
	LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);

	//SPI TX
	LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, length);
	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_2, (uint32_t)txData);
	LL_DMA_ClearFlag_TC1(DMA1);
	LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_2);
	LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);

	return BFLASH_ERROK;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Get the SPI status
  * @param 	None
  * @retval OK if ready
  */
BFLASH_ERR BFLASH_GetSPIStatus(void)
{
	if(LL_SPI_IsActiveFlag_BSY(SPI1))
		return BFLASH_ERRBUSY;
	return BFLASH_ERROK;
}
