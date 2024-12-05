/**
  ******************************************************************************
  * @file     	bme280.c
  * @author		beede
  * @version	1V0
  * @date		Oct 14, 2023
  * @brief
  */

/* Includes ------------------------------------------------------------------*/
#include <bme280.h>
#include "string.h"

/* Private define ------------------------------------------------------------*/
//MACROS
#define TOU16(ARRAY, INDEX)				(ARRAY[INDEX] + (ARRAY[INDEX + 1] << 8))

//REGISTERS
#define BME_REG_CALIB00					0x88	//BME_REG_calib00 - BME_REG_calib25 -> 0x88 - 0xA1
#define BME_REG_CALIB25					0xA1	//
#define BME_REG_ID						0xD0	//
#define BME_REG_RESET						0xE0	//
#define BME_REG_RESET_RST				0xB6
#define BME_REG_CALIB26					0xE1	//BME_REG_calib00 - BME_REG_calib25 -> 0x88 - 0xA1
#define BME_REG_CALIB41					0xF0	//
#define BME_REG_CTRLHUM					0xF2	//
#define BME_REG_CTRLHUM_OSRSH_POS		0x00
#define BME_REG_CTRLHUM_OSRSH_BITS		0x07
#define BME_REG_CTRLHUM_OSRSH_MASK		(BME_REG_CTRLHUM_OSRSH_BITS << BME_REG_CTRLHUM_OSRSH_POS)
#define BME_REG_STATUS					0xF3
#define BME_REG_STATUS_IMUPDATE_POS		0x00
#define BME_REG_STATUS_IMUPDATE_BITS	0x01
#define BME_REG_STATUS_IMUPDATE_MASK	(BME_REG_STATUS_IMUPDATE_BITS << BME_REG_STATUS_IMUPDATE_POS)
#define BME_REG_STATUS_MEASURING_POS	0x03
#define BME_REG_STATUS_MEASURING_BITS	0x01
#define BME_REG_STATUS_MEASURING_MASK	(BME_REG_STATUS_MEASURING_BITS << BME_REG_STATUS_MEASURING_POS)
#define BME_REG_CTRLMEAS					0xF4
#define BME_REG_CTRLMEAS_MODE_POS		0x00
#define BME_REG_CTRLMEAS_MODE_BITS		0x03
#define BME_REG_CTRLMEAS_MODE_MASK		(BME_REG_CTRLMEAS_MODE_BITS << BME_REG_CTRLMEAS_MODE_POS)
#define BME_REG_CTRLMEAS_OSRSP_POS		0x02
#define BME_REG_CTRLMEAS_OSRSP_BITS		0x07
#define BME_REG_CTRLMEAS_OSRSP_MASK		(BME_REG_CTRLMEAS_OSRSP_BITS << BME_REG_CTRLMEAS_OSRSP_POS)
#define BME_REG_CTRLMEAS_OSRST_POS		0x05
#define BME_REG_CTRLMEAS_OSRST_BITS		0x07
#define BME_REG_CTRLMEAS_OSRST_MASK		(BME_REG_CTRLMEAS_OSRST_BITS << BME_REG_CTRLMEAS_OSRST_POS)
#define BME_REG_CONFIG					0xF5
#define BME_REG_CONFIG_SPI3WEN_POS		0x00
#define BME_REG_CONFIG_SPI3WEN_BITS		0x01
#define BME_REG_CONFIG_SPI3WEN_MASK		(BME_REG_CONFIG_SPI3WEN_BITS << BME_REG_CONFIG_SPI3WEN_POS)
#define BME_REG_CONFIG_FILTER_POS		0x02
#define BME_REG_CONFIG_FILTER_BITS		0x07
#define BME_REG_CONFIG_FILTER_MASK		(BME_REG_CONFIG_FILTER_BITS << BME_REG_CONFIG_FILTER_POS)
#define BME_REG_CONFIG_TSB_POS			0x05
#define BME_REG_CONFIG_TSB_BITS			0x07
#define BME_REG_CONFIG_TSB_MASK			(BME_REG_CONFIG_TSB_BITS << BME_REG_CONFIG_TSB_POS)
#define BME_REG_PRESS_MSB				0xF7
#define BME_REG_PRESS_LSB				0xF8
#define BME_REG_PRESS_XLSB				0xF9
#define BME_REG_TEMP_MSB					0xFA
#define BME_REG_TEMP_LSB					0xFB
#define BME_REG_TEMP_XLSB				0xFC
#define BME_REG_HUM_MSB					0xFD
#define BME_REG_HUM_LSB					0xFE

//FLAGS
enum BME_FLAGS
{
	BME_FLAGRST = 0x01,
	BME_FLAGGETID = 0x02,
	BME_FLAGCMPLT = 0x04,
	BME_FLAGCALIB1 = 0x08,
	BME_FLAGCALIB2 = 0x10,
};


/* Private typedef -----------------------------------------------------------*/
typedef struct
{
	uint16_t dig_t1;	/*! Calibration coefficients for the temperature sensor */
	int16_t dig_t2;
	int16_t dig_t3;

	uint16_t dig_p1;	/*! Calibration coefficients for the pressure sensor */
	int16_t dig_p2;
	int16_t dig_p3;
	int16_t dig_p4;
	int16_t dig_p5;
	int16_t dig_p6;
	int16_t dig_p7;
	int16_t dig_p8;
	int16_t dig_p9;
	uint8_t dig_h1;
	int16_t dig_h2;
	uint8_t dig_h3;
	int16_t dig_h4;
	int16_t dig_h5;
	int8_t dig_h6;

	int32_t t_fine;		/*! Variable to store the intermediate temperature coefficient */
}BME_Calib_TypeDef;

//STATES
typedef enum
{
	//Start up
	BME_STATE_IDLE = 0,
	BME_STATE_START,
	BME_STATE_AWAITID,
	BME_STATE_AWAITRST,
	BME_STATE_AWAITCALIB1,
	BME_STATE_AWAITCALIB2,
	BME_STATE_AWAITWRITECTRLHUM,
	BME_STATE_AWAITWRITECTRLMEAS,
	BME_STATE_AWAITWRITECONFIG,

	//Read Sensors
	BME_STATE_READ,
	BME_STATE_AWAITSENSORS,

	//Read Forced Sensors
	BME_STATE_FORCEDREAD,
	BME_STATE_AWAITFORCEDREADCOMPLETE,
	BME_STATE_AWAITFORCEDMEASURECOMPLETE,
	BME_STATE_AWAITFORCEDSENSORS,
}BME_STATE;

/* Private variables ---------------------------------------------------------*/
static BME_Access_td *user;
static uint8_t tmr;
static uint8_t state = BME_STATE_IDLE;
static uint8_t flags;
static uint8_t deviceID;
static uint8_t data[27];
static BME_InitTypeDef setup;
static BME_Calib_TypeDef calibration;

/* Private function prototypes -----------------------------------------------*/
static void BME_startupControl(void);
static void BME_readSensorsControl(void);
static void BME_forcedReadOfSensorControl(void);

static int32_t compensate_temperature(int32_t rawTemperature);
static uint32_t compensate_pressure(int32_t rawPressure);
static uint32_t compensate_humidity(int32_t rawHumidity);

BME_ERR BME_transmitReceive(uint8_t *pTx, uint8_t *pRx, uint8_t length);
BME_ERR BME_transmitReceiveHandler(uint8_t *pTx, uint8_t *pRx, uint8_t length);
void BME_chipSelectGPIOHandler(uint8_t gpioState);

/* Private functions ---------------------------------------------------------*/


/**
  * @brief	Initialization of the setup struct
  * @param	pInitStruct: pointer to the setup struct to
  * @retval	None
  */
void BME_initSetupStruct(BME_InitTypeDef *pInitStruct)
{
	//Initialize device
	memset(pInitStruct, 0, sizeof(BME_InitTypeDef));
	pInitStruct->humidityOversampling = BME_REG_CTRLHUM_OSRS_OVRSMPX1;
	pInitStruct->iirFilter = BME_REG_CONFIG_FILTER_OFF;
	pInitStruct->mode = BME_REG_CTRLMEAS_MODE_NORMAL;	//Normal
	pInitStruct->pressureOversampling = BME_REG_CTRLMEAS_OSRSP_OVRSMPX1;
	pInitStruct->spi3Wire = 0;
	pInitStruct->standbyInactiveDuration = BME_REG_CONFIG_TSB_0_5;
	pInitStruct->temperatureOversampling = BME_REG_CTRLMEAS_OSRST_OVRSMPX1;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Millisecond tick routine
  * @param	None
  * @retval	None
  */
void BME_milli(void)
{
	if(user == NULL)
		return;

	if(tmr)
		tmr--;

	BME_startupControl();
	BME_readSensorsControl();
	BME_forcedReadOfSensorControl();
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Request the initialization of the BME device
  * @param	pAccess: pointer to the access control
  * @param	pInitStruct: pointer to startup settings
  * @retval	BME_ERR
  */
BME_ERR BME_StartUp(BME_Access_td *pAccess, BME_InitTypeDef *pInitStruct)
{
	if((user != NULL) && (user != pAccess))
		return BME_ERRINUSE;
	else if (user != NULL)
		return BME_ERRBUSY;
	user = pAccess;

	user->complete = false;
	memcpy(&setup, pInitStruct, sizeof(BME_InitTypeDef));
	state = BME_STATE_START;

	return BME_ERROK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Control starting the device
  * @param	pBMP: pointer to the BME struct
  * @retval	None
  */
static void BME_startupControl(void)
{
	switch(state)
	{
	case BME_STATE_START:
		data[0] = BME_REG_ID | 0x80;
		data[1] = 0;
		if(BME_transmitReceive(data, data, 2) == BME_ERROK)
		{
			flags &= ~BME_FLAGCMPLT;
			state = BME_STATE_AWAITID;
		}
		break;
	case BME_STATE_AWAITID:
		if(flags & BME_FLAGCMPLT)
		{
			deviceID = data[1];

			data[0] = (BME_REG_RESET & ~0x80);
			data[1] = BME_REG_RESET_RST;
			if(BME_transmitReceive(data, data, 2) == BME_ERROK)
			{
				flags &= ~BME_FLAGCMPLT;
				state = BME_STATE_AWAITRST;
				tmr = 10;		//Reset time
			}
		}
		break;
	case BME_STATE_AWAITRST:
		if((flags & BME_FLAGCMPLT) && (tmr == 0))
		{
			data[0] = (BME_REG_CALIB00 | 0x80);
			if(BME_transmitReceive(data, data, (BME_REG_CALIB25 - BME_REG_CALIB00 + 2)) == BME_ERROK)
			{
				flags &= ~BME_FLAGCMPLT;
				state = BME_STATE_AWAITCALIB1;
			}
		}
		break;
	case BME_STATE_AWAITCALIB1:
		if(flags & BME_FLAGCMPLT)
		{
			calibration.dig_t1 = TOU16(data, 1);
		    calibration.dig_t2 = (int16_t)TOU16(data, 3);
		    calibration.dig_t3 = (int16_t)TOU16(data, 5);
		    calibration.dig_p1 = TOU16(data, 7);
		    calibration.dig_p2 = (int16_t)TOU16(data, 9);
		    calibration.dig_p3 = (int16_t)TOU16(data, 11);
		    calibration.dig_p4 = (int16_t)TOU16(data, 13);
		    calibration.dig_p5 = (int16_t)TOU16(data, 15);
		    calibration.dig_p6 = (int16_t)TOU16(data, 17);
		    calibration.dig_p7 = (int16_t)TOU16(data, 19);
		    calibration.dig_p8 = (int16_t)TOU16(data, 21);
		    calibration.dig_p9 = (int16_t)TOU16(data, 23);
		    calibration.dig_h1 = (uint8_t)data[25];


			data[0] = (BME_REG_CALIB26 | 0x80);
			if(BME_transmitReceive(data, data, (BME_REG_CALIB41 - BME_REG_CALIB26 + 2)) == BME_ERROK)
			{
				flags &= ~BME_FLAGCMPLT;
				state = BME_STATE_AWAITCALIB2;
			}
		}
		break;
	case BME_STATE_AWAITCALIB2:
		if(flags & BME_FLAGCMPLT)
		{
		    calibration.dig_h2 = (int16_t)TOU16(data, 1);
		    calibration.dig_h3 = (uint8_t)data[3];
		    calibration.dig_h4 = (int16_t)((data[4] << 4) | (data[5] & 0x0f));
		    calibration.dig_h5 = (int16_t)((data[6] << 4) | (data[5] >> 4));
		    calibration.dig_h6 = (int16_t)TOU16(data, 7);


			data[0] = (BME_REG_CTRLHUM & ~0x80);
			data[1] = setup.humidityOversampling << BME_REG_CTRLHUM_OSRSH_POS;
			if(BME_transmitReceive(data, data, 2) == BME_ERROK)
			{
				flags &= ~BME_FLAGCMPLT;
				state = BME_STATE_AWAITWRITECTRLHUM;
			}
		}
		break;
	case BME_STATE_AWAITWRITECTRLHUM:
		if(flags & BME_FLAGCMPLT)
		{
			data[0] = (BME_REG_CTRLMEAS & ~0x80);
			data[1] = (setup.mode << BME_REG_CTRLMEAS_MODE_POS) & BME_REG_CTRLMEAS_MODE_MASK;
			data[1] |= (setup.pressureOversampling << BME_REG_CTRLMEAS_OSRSP_POS) & BME_REG_CTRLMEAS_OSRSP_MASK;
			data[1] |= (setup.temperatureOversampling << BME_REG_CTRLMEAS_OSRST_POS) & BME_REG_CTRLMEAS_OSRST_MASK;
			if(BME_transmitReceive(data, data, 2) == BME_ERROK)
			{
				flags &= ~BME_FLAGCMPLT;
				state = BME_STATE_AWAITWRITECTRLMEAS;
			}
		}
		break;
	case BME_STATE_AWAITWRITECTRLMEAS:
		if(flags & BME_FLAGCMPLT)
		{
			data[0] = (BME_REG_CONFIG & ~0x80);
			data[1] = (setup.spi3Wire << BME_REG_CONFIG_SPI3WEN_POS) & BME_REG_CONFIG_SPI3WEN_MASK;
			data[1] |= (setup.iirFilter << BME_REG_CONFIG_FILTER_POS) & BME_REG_CONFIG_FILTER_MASK;
			data[1] |= (setup.standbyInactiveDuration << BME_REG_CONFIG_TSB_POS) & BME_REG_CONFIG_TSB_MASK;
			if(BME_transmitReceive(data, data, 2) == BME_ERROK)
			{
				flags &= ~BME_FLAGCMPLT;
				state = BME_STATE_AWAITWRITECONFIG;
			}
		}
		break;
	case BME_STATE_AWAITWRITECONFIG:
		if(flags & BME_FLAGCMPLT)
		{
			user->result = BME_ERROK;
			user->complete = true;

			BME_Access_td *access = user;
			user = NULL;
			if(access->completeCallback != NULL)
				access->completeCallback(access, access->result);
			state = BME_STATE_IDLE;
		}
		break;
	}
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Request sensor readings when in normal mode of operation (BME280 remains awake)
  * @param	pAccess: pointer to the access control
  * @param	BME_SENSORS: select which sensor readings are required
  * @retval	BME_ERR
  */
BME_ERR BME_readSensors(BME_Access_td *pAccess)
{
	if((user != NULL) && (user != pAccess))
		return BME_ERRINUSE;
	else if (user != NULL)
		return BME_ERRBUSY;
	user = pAccess;

	user->complete = false;
	state = BME_STATE_READ;

	return BME_ERROK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Control starting the device
  * @param	pBMP: pointer to the BME struct
  * @retval	None
  */
static void BME_readSensorsControl(void)
{
	switch(state)
	{
	case BME_STATE_READ:
		data[0] = (BME_REG_PRESS_MSB | 0x80);
		if(BME_transmitReceive(data, data, (BME_REG_HUM_MSB - BME_REG_PRESS_MSB + 2)) == BME_ERROK)
		{
			flags &= ~BME_FLAGCMPLT;
			state = BME_STATE_AWAITSENSORS;
		}
		break;
	case BME_STATE_AWAITSENSORS:
		if(flags & BME_FLAGCMPLT)
		{
			int32_t rawPressure = (data[1] << 12) + (data[2] << 4) + (data[3] >> 4);
			int32_t rawTemperature = (data[4] << 12) + (data[5] << 4) + (data[6] >> 4);
			int32_t rawHumidity = (data[7] << 8) + data[8];

			user->temperature = compensate_temperature(rawTemperature);
			user->pressure = compensate_pressure(rawPressure);
			user->humidity = compensate_humidity(rawHumidity);
			user->result = BME_ERROK;
			user->complete = true;

			BME_Access_td *access = user;
			user = NULL;
			if(access->completeCallback != NULL)
				access->completeCallback(access, access->result);
			state = BME_STATE_IDLE;
		}
		break;
	}
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Request sensor readings when in forced mode (BME280 wakes, takes measurements, and sleeps
  * @param	pAccess: pointer to the access control
  * @param	BME_SENSORS: select which sensor readings are required
  * @retval	BME_ERR
  */
BME_ERR BME_forcedReadOfSensors(BME_Access_td *pAccess)
{
	if((user != NULL) && (user != pAccess))
		return BME_ERRINUSE;
	else if (user != NULL)
		return BME_ERRBUSY;
	user = pAccess;

	user->complete = false;
	state = BME_STATE_FORCEDREAD;

	return BME_ERROK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Control starting the device
  * @param	pBMP: pointer to the BME struct
  * @retval	None
  */
static void BME_forcedReadOfSensorControl(void)
{
	switch(state)
	{
	case BME_STATE_FORCEDREAD:

		data[0] = (BME_REG_CTRLMEAS & ~0x80);
		data[1] = (BME_REG_CTRLMEAS_MODE_FORCED << BME_REG_CTRLMEAS_MODE_POS) & BME_REG_CTRLMEAS_MODE_MASK;
		data[1] |= (setup.pressureOversampling << BME_REG_CTRLMEAS_OSRSP_POS) & BME_REG_CTRLMEAS_OSRSP_MASK;
		data[1] |= (setup.temperatureOversampling << BME_REG_CTRLMEAS_OSRST_POS) & BME_REG_CTRLMEAS_OSRST_MASK;
		if(BME_transmitReceive(data, data, 2) == BME_ERROK)
		{
			flags &= ~BME_FLAGCMPLT;
			state = BME_STATE_AWAITFORCEDREADCOMPLETE;
		}
		break;
	case BME_STATE_AWAITFORCEDREADCOMPLETE:
		if(flags & BME_FLAGCMPLT)
		{
			data[0] = (BME_REG_STATUS | 0x80);
			if(BME_transmitReceive(data, data, 2) == BME_ERROK)
			{
				flags &= ~BME_FLAGCMPLT;
				state = BME_STATE_AWAITFORCEDMEASURECOMPLETE;
			}
		}
		break;
	case BME_STATE_AWAITFORCEDMEASURECOMPLETE:
		if(flags & BME_FLAGCMPLT)
		{
			if(data[1] & 0x08)
			{
				state = BME_STATE_AWAITFORCEDREADCOMPLETE;
				break;
			}

			data[0] = (BME_REG_PRESS_MSB | 0x80);
			if(BME_transmitReceive(data, data, (BME_REG_HUM_MSB - BME_REG_PRESS_MSB + 2)) == BME_ERROK)
			{
				flags &= ~BME_FLAGCMPLT;
				state = BME_STATE_AWAITFORCEDSENSORS;
			}
		}
		break;
	case BME_STATE_AWAITFORCEDSENSORS:
		if(flags & BME_FLAGCMPLT)
		{
			int32_t rawPressure = (data[1] << 12) + (data[2] << 4) + (data[3] >> 4);
			int32_t rawTemperature = (data[4] << 12) + (data[5] << 4) + (data[6] >> 4);
			int32_t rawHumidity = (data[7] << 8) + data[8];

			user->temperature = compensate_temperature(rawTemperature);
			user->pressure = compensate_pressure(rawPressure);
			user->humidity = compensate_humidity(rawHumidity);
			user->result = BME_ERROK;
			user->complete = true;

			BME_Access_td *access = user;
			user = NULL;
			if(access->completeCallback != NULL)
				access->completeCallback(access, access->result);
			state = BME_STATE_IDLE;
		}
		break;
	}
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Compensate temperature
  * @param	pBMP: pointer to the BME struct
  * @retval	None
  */
static int32_t compensate_temperature(int32_t rawTemperature)
{
	int32_t var1, var2, t;
	var1 = ((((rawTemperature >> 3) - ((int32_t)calibration.dig_t1 << 1))) * ((int32_t)calibration.dig_t2)) >> 11;
	var2 = (((((rawTemperature >> 4) - ((int32_t)calibration.dig_t1)) * ((rawTemperature >> 4) - ((int32_t)calibration.dig_t1))) >> 12) * ((int32_t)calibration.dig_t3)) >> 14;
	calibration.t_fine = var1 + var2;
	t = (calibration.t_fine * 5 + 128) >> 8;
	return t;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Compensate pressure
  * @param	pBMP: pointer to the BME struct
  * @retval	None
  */
static uint32_t compensate_pressure(int32_t rawPressure)
{
	int64_t var1, var2, p;
	var1 = ((int64_t)calibration.t_fine) - 128000;
	var2 = var1 * var1 * (int64_t)calibration.dig_p6;
	var2 = var2 + ((var1 * (int64_t)calibration.dig_p5) << 17);
	var2 = var2 + (((int64_t)calibration.dig_p4) << 35);
	var1 = ((var1 * var1 * (int64_t)calibration.dig_p3) >> 8) + ((var1 * (int64_t)calibration.dig_p2) << 12);
	var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calibration.dig_p1) >> 33;
	if(var1 == 0)
		return 0;

	p = 1048576 - rawPressure;
	p = (((p << 31) - var2) * 3125) / var1;
	var1 = (((int64_t) calibration.dig_p9) * (p >> 13) * (p >> 13)) >> 25;
	var2 = (((int64_t)calibration.dig_p8) * p) >> 19;
	p = ((p + var1 + var2) >> 8) + (((int64_t)calibration.dig_p7) << 4);
	return (uint32_t)p;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Compensate humidity
  * @param	pBMP: pointer to the BME struct
  * @retval	None
  */
static uint32_t compensate_humidity(int32_t rawHumidity)
{
	int32_t v_x1_u32r;
	v_x1_u32r = (calibration.t_fine - ((int32_t)76800));
	v_x1_u32r = (((((rawHumidity << 14) - (((int32_t)calibration.dig_h4) << 20) - (((int32_t)calibration.dig_h5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)calibration.dig_h6)) >> 10) * (((v_x1_u32r * ((int32_t)calibration.dig_h3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)calibration.dig_h2) + 8192) >> 14));
	v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)calibration.dig_h1)) >> 4));
	v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
	v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
	return (uint32_t)(v_x1_u32r >> 12);
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Transmit over SPI
  * @param	pTx: pointer to transmit array
  * @param	pRx: pointer to receive array
  * @param	length: amount of data to transmit
  * @retval	None
  */
BME_ERR BME_transmitReceive(uint8_t *pTx, uint8_t *pRx, uint8_t length)
{
	BME_chipSelectGPIOHandler(0);
	return BME_transmitReceiveHandler(pTx, pRx, length);
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Transmit over SPI
  * @param	pTx: pointer to transmit array
  * @param	pRx: pointer to receive array
  * @param	length: amount of data to transmit
  * @retval	None
  */
__attribute ((weak)) BME_ERR BME_transmitReceiveHandler(uint8_t *pTx, uint8_t *pRx, uint8_t length)
{
	return BME_ERROK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Set chip select GPIO
  * @param	gpioState: gpio state required
  * @retval	None
  */
__attribute ((weak)) void BME_chipSelectGPIOHandler(uint8_t gpioState)
{

}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Handler for the SPI transmit/receive complete
  * @param	None
  * @retval	None
  */
void BME_transmitReceiveCompleteCallback(void)
{
	BME_chipSelectGPIOHandler(1);
	flags |= BME_FLAGCMPLT;
}
