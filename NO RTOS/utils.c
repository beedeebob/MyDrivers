/*
 * utils.c
 *
 *  Created on: Jun 24, 2024
 *      Author: ben-linux
 */


/* Includes ------------------------------------------------------------------*/
#include "utils.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* CRC-32C (iSCSI) polynomial in reversed bit order. */
#define POLY 0x82f63b78

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/


/**
  * @brief	Calculate CRC value on a queue
  * @param	crc: the current crc value
  * @param	buf: pointer to the buffer array to encode
  * @param	len: number of bytes to encode
  * @retval	CRC32 value
  */
uint32_t crc32_calculateQueue(uint32_t crc, QUEUE_Typedef *queue, uint32_t offset, uint32_t len)
{
    int k;

    crc = ~crc;
    while (len--) {
        crc ^= queue->pBuff[QUEUE_PTRLOOP(queue, offset++)];
        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    }
    return ~crc;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Calculate CRC value on a data array
  * @param	crc: the current crc value
  * @param	buf: pointer to the buffer array to encode
  * @param	len: number of bytes to encode
  * @retval	CRC32 value
  */
uint32_t crc32_calculateData(uint32_t crc, uint8_t *data, uint32_t offset, uint32_t len)
{
    int k;

    crc = ~crc;
    while (len--) {
        crc ^= data[offset++];
        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    }
    return ~crc;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Calculate CRC16 value on a queue. Starting value of 0xffff
  * @param	crc: the starting crc value
  * @param	queue: pointer to the queue
  * @param	offset: offset within the queue from which to start
  * @param	length: number of bytes on which to calaculate
  * @retval	CRC16 value
  */
uint16_t crc16_ccitt_calculateQueue(uint16_t crc, QUEUE_Typedef *queue, uint32_t offset, uint32_t length)
{
	for (uint32_t i = 0; i < length; i++)
		crc = crc16_ccitt_accumulate(crc, queue->pBuff[QUEUE_PTRLOOP(queue, offset + i)]);

	return crc;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Calculate CRC16 value on a queue. Starting value of 0xffff
  * @param	crc: the starting crc value
  * @param	queue: pointer to the queue
  * @param	offset: offset within the queue from which to start
  * @param	length: number of bytes on which to calaculate
  * @retval	CRC16 value
  */
uint16_t crc16_ccitt_calculateData(uint16_t crc, uint8_t *data, uint32_t offset, uint32_t length)
{
	for (uint32_t i = 0; i < length; i++)
		crc = crc16_ccitt_accumulate(crc, data[offset + i]);

	return crc;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Calculte CRC16 value on a queue
  * @param	crc: the starting crc value
  * @param	value: value to accumulate with the provided crc
  * @retval	CRC16 value
  */
uint16_t crc16_ccitt_accumulate(uint16_t crc, uint8_t value)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if ((((crc & 0x8000) >> 8) ^ (value & 0x80)) > 0)
        	crc = (uint16_t)((crc << 1) ^ 0x1021);
        else
        	crc = (uint16_t)(crc << 1);

        value <<= 1;
    }
    return crc;
}
