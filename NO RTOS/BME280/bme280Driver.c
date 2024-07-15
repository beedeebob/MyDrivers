/**
  ******************************************************************************
  * @file     	bme280Driver.c
  * @author		beede
  * @version	1V0
  * @date		Jul 14, 2024
  * @brief
  */


/* Includes ------------------------------------------------------------------*/
#include "bme280.h"
#include "main.h"

/* Private define ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief	Transmit over SPI
  * @param	pTx: pointer to transmit array
  * @param	pRx: pointer to receive array
  * @param	length: amount of data to transmit
  * @retval	None
  */
BME_ERR BME_transmitReceiveHandler(uint8_t *pTx, uint8_t *pRx, uint8_t length)
{
	//RX DMA
	LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_4);
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_4, length);
	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_4, (uint32_t)pRx);
	LL_DMA_ClearFlag_TC4(DMA1);
	LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_4);
	LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_4);

	/* SPI2_TX Init */
	LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_5);
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_5, length);
	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_5, (uint32_t)pTx);
	LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_5);
	return BME_ERROK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Set chip select GPIO
  * @param	gpioState: gpio state required
  * @retval	None
  */
void BME_chipSelectGPIOHandler(uint8_t gpioState)
{
	HAL_GPIO_WritePin(GPIO_BME280_NCS_GPIO_Port, GPIO_BME280_NCS_Pin, gpioState);
}
