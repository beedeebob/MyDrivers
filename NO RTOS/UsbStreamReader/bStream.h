/**
  ******************************************************************************
  * @file     	bStream.h
  * @author		beede
  * @version	1V0
  * @date		Jul 20, 2024
  * @brief
  */


#ifndef INC_BSTREAM_H_
#define INC_BSTREAM_H_

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"

/* Exported defines ----------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
typedef enum
{
	BSTREAM_OK = 0,
	BSTREAM_NOTENOUGHSPACE = 1,
	BSTREAM_NOTENOUGHDATA = 2,
	BSTREAM_BUSY = 3,
	BSTREAM_CLOSED = 4,
}BSTREAM_Enum;

typedef struct BSTREAM_Reader_td
{
	uint32_t length;			//Amount of data
	uint32_t crc;				//CRC verification of the data

	/**
	  * @brief	Open stream
	  * @param	stream: pointer to the stream
	  * @retval	BSTREAM_Enum
	  */
	BSTREAM_Enum (*open)(struct BSTREAM_Reader_td *stream);

	/**
	  * @brief	Count of data available from offset
	  * @param	stream: pointer to the stream
	  * @param	offset: offset from which to count the available data
	  * @param[out]	count: amount of available data
	  * @retval	BSTREAM_Enum
	  */
	BSTREAM_Enum (*count)(struct BSTREAM_Reader_td *stream, uint32_t offset, uint32_t *count);

	/**
	  * @brief	Read data from stream
	  * @param	stream: pointer to the stream
	  * @param	offset: offset from which to read data
	  * @param	data: pointer to the array into which to read the data
	  * @param	length: amount of data to read from the array
	  * @param	actualLength: actual amount of data read. If null data will only
	  * 		be read if "length" bytes are available
	  * @retval	BSTREAM_Enum
	  */
	BSTREAM_Enum (*readData)(struct BSTREAM_Reader_td *stream, uint32_t offset, uint8_t *data, uint32_t length, uint32_t *actualLength);

	/**
	  * @brief	close the stream
	  * @param	stream: pointer to the stream
	  * @retval	BSTREAM_Enum
	  */
	BSTREAM_Enum (*close)(struct BSTREAM_Reader_td *stream);
} BSTREAM_Reader_td;

/* Exported variables --------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

#endif /* INC_BSTREAM_H_ */
