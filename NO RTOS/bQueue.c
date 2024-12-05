/**
  ******************************************************************************
  * @file     	queue.c
  * @author		beede
  * @version	1V0
  * @date		Nov 25, 2023
  * @brief
  */


/* Includes ------------------------------------------------------------------*/
#include "bQueue.h"

/* Private define ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief	Initialize the queue system
  * @param	queue: pointer to the queue struct to initialize
  * @param	pBuff: pointer to the buffer array to use
  * @param	size: size of the buffer. Must be a power of 2
  * @retval	None
  */
void QUEUE_Initialize(QUEUE_Typedef *queue, uint8_t *pBuff, uint32_t size)
{
	queue->pBuff = pBuff;
	queue->size = size;
	queue->in = 0;
	queue->out = 0;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Add a byte to the queue
  * @param	queue: pointer to the queue struct
  * @param	data: byte to add
  * @retval QUEUE_STATUS
  */
QUEUE_STATUS QUEUE_Add(QUEUE_Typedef *queue, uint8_t data)
{
	if(QUEUE_SPACE(queue) == 0)
		return QUEUE_NOSPACE;

	queue->pBuff[queue->in] = data;
	queue->in = QUEUE_PTRLOOP(queue, queue->in + 1);

	return QUEUE_OK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Add data from an array to the queue
  * @param	queue: pointer to the queue struct
  * @param	data: pointer to data array to add
  * @param	length: amount of data to add
  * @retval QUEUE_STATUS
  */
QUEUE_STATUS QUEUE_AddArray(QUEUE_Typedef *queue, uint8_t *data, uint32_t length)
{
	if(QUEUE_SPACE(queue) < length)
		return QUEUE_NOSPACE;

	for(uint32_t i = 0; i < length; i++)
		queue->pBuff[QUEUE_PTRLOOP(queue, queue->in + i)] = data[i];
	queue->in = QUEUE_PTRLOOP(queue, queue->in + length);

	return QUEUE_OK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Add data from an array to the queue
  * @param	queue: pointer to the queue struct
  * @param	data: pointer to data array to add
  * @param	length: amount of data to add
  * @retval QUEUE_STATUS
  */
QUEUE_STATUS QUEUE_AddQueue(QUEUE_Typedef *queue, QUEUE_Typedef *data, uint32_t length)
{
	if(QUEUE_SPACE(queue) < length)
		return QUEUE_NOSPACE;
	if(QUEUE_COUNT(data) < length)
		return QUEUE_NOTENOUGHDATA;

	for(uint32_t i = 0; i < length; i++)
		queue->pBuff[QUEUE_PTRLOOP(queue, queue->in + i)] = data->pBuff[QUEUE_PTRLOOP(data, data->out + i)];

	queue->in = QUEUE_PTRLOOP(queue, queue->in + length);
	data->out = QUEUE_PTRLOOP(data, data->out + length);

	return QUEUE_OK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Get the byte at the out pointer referenced index location
  * @param	queue: pointer to the queue struct
  * @param	index: index to read with reference to the out pointer
  * @retval value or 0xff if the index exceeds the data
  */
uint8_t QUEUE_ElementAt(QUEUE_Typedef *queue, uint32_t index)
{
	if(index >= QUEUE_COUNT(queue))
		return 0xff;

	return queue->pBuff[QUEUE_PTRLOOP(queue, (index + queue->out))];
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Read the next byte from the queue and remove it
  * @param	queue: pointer to the queue struct
  * @retval value of 0xff if there is not data
  */
uint8_t QUEUE_ReadOutByte(QUEUE_Typedef *queue)
{
	if(QUEUE_COUNT(queue) == 0)
		return 0xff;

	uint8_t byte = queue->pBuff[queue->out];
	queue->out = QUEUE_PTRLOOP(queue, (queue->out + 1));
	return byte;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Read and remove the a number of bytes from the queue to an array
  * @param	queue: pointer to the queue from which to read
  * @param	data: pointer to the data into which to read
  * @param	length: amount of data to read out
  * @retval QUEUE_STATUS
  */
QUEUE_STATUS QUEUE_ReadOutArray(QUEUE_Typedef *queue, uint8_t *data, uint32_t length)
{
	if(QUEUE_COUNT(queue) < length)
		return QUEUE_NOTENOUGHDATA;

	for(uint32_t i = 0; i < length; i++)
		data[i] = queue->pBuff[QUEUE_PTRLOOP(queue, (queue->out + i))];
	queue->out = QUEUE_PTRLOOP(queue, (queue->out + length));

	return QUEUE_OK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Read and remove the a number of bytes from the queue to another queue
  * @param	queue: pointer to the queue from which to read
  * @param	data: pointer to the queue into which to read
  * @param	length: amount of data to read out
  * @retval QUEUE_STATUS
  */
QUEUE_STATUS QUEUE_ReadOutQueue(QUEUE_Typedef *queue, QUEUE_Typedef *data, uint32_t length)
{
	if(QUEUE_COUNT(queue) < length)
		return QUEUE_NOTENOUGHDATA;
	if(QUEUE_SPACE(data) < length)
		return QUEUE_NOSPACE;

	for(uint32_t i = 0; i < length; i++)
		data->pBuff[QUEUE_PTRLOOP(data, (data->in + i))] = queue->pBuff[QUEUE_PTRLOOP(queue, (queue->out + i))];
	queue->out = QUEUE_PTRLOOP(queue, (queue->out + length));
	data->in = QUEUE_PTRLOOP(data, (data->in + length));

	return QUEUE_OK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Read a number of bytes from the queue to an array
  * @param	queue: pointer to the queue from which to read
  * @param	data: pointer to the data into which to read
  * @param	length: amount of data to read out
  * @retval QUEUE_STATUS
  */
QUEUE_STATUS QUEUE_ReadToArray(QUEUE_Typedef *queue, uint32_t offset, uint8_t *data, uint32_t length)
{
	for(uint32_t i = 0; i < length; i++)
		data[i] = queue->pBuff[QUEUE_PTRLOOP(queue, (queue->out + offset + i))];

	return QUEUE_OK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Remove bytes off the front of the queue
  * @param	count: number of bytes to remove
  * @retval value
  */
QUEUE_STATUS QUEUE_Remove(QUEUE_Typedef *queue, uint32_t count)
{
	if(QUEUE_COUNT(queue) < count)
		return QUEUE_PARAM;

	queue->out = QUEUE_PTRLOOP(queue, (queue->out + count));
	return QUEUE_OK;
}
