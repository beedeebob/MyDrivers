/**
  ******************************************************************************
  * @file     	bme280.h
  * @author		beede
  * @version	1V0
  * @date		Oct 22, 2023
  * @brief
  */


#ifndef BMP280_H_
#define BMP280_H_

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"
#include "stdbool.h"

/* Exported defines ----------------------------------------------------------*/
#define BME_REG_CTRLHUM_OSRS_SKIPPED		0b000
#define BME_REG_CTRLHUM_OSRS_OVRSMPX1		0b001
#define BME_REG_CTRLHUM_OSRS_OVRSMPX2		0b010
#define BME_REG_CTRLHUM_OSRS_OVRSMPX4		0b011
#define BME_REG_CTRLHUM_OSRS_OVRSMPX8		0b100
#define BME_REG_CTRLHUM_OSRS_OVRSMPX16		0b101

#define BME_REG_STATUS_IMUPDATE				0x01	//Set when NVM data being transferred to image registers
#define BME_REG_STATUS_MEASURING				0x01	//Set when conversion is running and cleared when results are available

#define BME_REG_CTRLMEAS_MODE_SLEEP			0b00
#define BME_REG_CTRLMEAS_MODE_FORCED		0b01
#define BME_REG_CTRLMEAS_MODE_NORMAL		0b11

#define BME_REG_CTRLMEAS_OSRSP_SKIPPED		0b000
#define BME_REG_CTRLMEAS_OSRSP_OVRSMPX1		0b001
#define BME_REG_CTRLMEAS_OSRSP_OVRSMPX2		0b010
#define BME_REG_CTRLMEAS_OSRSP_OVRSMPX4		0b011
#define BME_REG_CTRLMEAS_OSRSP_OVRSMPX8		0b100
#define BME_REG_CTRLMEAS_OSRSP_OVRSMPX16	0b101

#define BME_REG_CTRLMEAS_OSRST_SKIPPED		0b000
#define BME_REG_CTRLMEAS_OSRST_OVRSMPX1		0b001
#define BME_REG_CTRLMEAS_OSRST_OVRSMPX2		0b010
#define BME_REG_CTRLMEAS_OSRST_OVRSMPX4		0b011
#define BME_REG_CTRLMEAS_OSRST_OVRSMPX8		0b100
#define BME_REG_CTRLMEAS_OSRST_OVRSMPX16	0b101

#define BME_REG_CONFIG_SPI3WEN_EN			0x01	//ENABLE the SPI 3 wire interface

#define BME_REG_CONFIG_FILTER_OFF			0b000	//IIR filter time constant
#define BME_REG_CONFIG_FILTER_2				0b001
#define BME_REG_CONFIG_FILTER_4				0b010
#define BME_REG_CONFIG_FILTER_8				0b011
#define BME_REG_CONFIG_FILTER_16				0b100

#define BME_REG_CONFIG_TSB_0_5				0b000	//Standby inactive duration in milliseconds in normal mode
#define BME_REG_CONFIG_TSB_62_5				0b001
#define BME_REG_CONFIG_TSB_125				0b010
#define BME_REG_CONFIG_TSB_250				0b011
#define BME_REG_CONFIG_TSB_500				0b100
#define BME_REG_CONFIG_TSB_1000				0b101
#define BME_REG_CONFIG_TSB_10				0b110
#define BME_REG_CONFIG_TSB_20				0b111

/* Exported types ------------------------------------------------------------*/

//ERRORS
typedef enum
{
	BME_ERROK = 0,
	BME_ERRINUSE,
	BME_ERRBUSY,
	BME_ERRPERIPH,
	BME_ERRFULL,
}BME_ERR;

//TYPES
typedef struct BME_Access_td
{
	uint32_t pressure;		//pressure/256 ->Pa
	int32_t temperature;	//resolution of 0.01C
	uint32_t humidity;		//humidity/1024 ->%RH

	void (*completeCallback)(struct BME_Access_td *pAccess, BME_ERR result);
	bool complete:1;
	BME_ERR result:7;
}BME_Access_td;
typedef struct
{
	uint8_t mode:2;
	uint8_t pressureOversampling:3;
	uint8_t temperatureOversampling:3;
	uint8_t humidityOversampling:3;
	uint8_t spi3Wire:1;
	uint8_t iirFilter:3;
	uint8_t standbyInactiveDuration:3;
}BME_InitTypeDef;

/* Exported variables --------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void BME_milli(void);
void BME_transmitReceiveCompleteCallback(void);

void BME_initSetupStruct(BME_InitTypeDef *pInitStruct);
BME_ERR BME_StartUp(BME_Access_td *pAccess, BME_InitTypeDef *pInitStruct);
BME_ERR BME_readSensors(BME_Access_td *pAccess);
BME_ERR BME_forcedReadOfSensors(BME_Access_td *pAccess);

#endif /* BMP280_H_ */
