/**
  ******************************************************************************
  * @file     	queue.h
  * @author		beede
  * @version	1V0
  * @date		Nov 25, 2023
  * @brief
  */


#ifndef QUEUE_H_
#define QUEUE_H_

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"

/* Exported defines ----------------------------------------------------------*/
#define QUEUE_COUNT(Q)					(((Q)->in - (Q)->out) & ((Q)->size  - 1))
#define QUEUE_SPACE(Q)					((Q)->size - 1 - QUEUE_COUNT(Q))
#define QUEUE_PTRLOOP(Q, PTR)			((PTR) & (Q->size - 1))
#define QUEUE_TOU32(Q, OFFSET)			((Q)->pBuff[QUEUE_PTRLOOP((Q), OFFSET)] + ((Q)->pBuff[QUEUE_PTRLOOP((Q), OFFSET + 1)] << 8) + ((Q)->pBuff[QUEUE_PTRLOOP((Q), OFFSET + 2)] << 16)  + ((Q)->pBuff[QUEUE_PTRLOOP((Q), OFFSET + 3)] << 24) )
#define QUEUE_TOU24(Q, OFFSET)			((Q)->pBuff[QUEUE_PTRLOOP((Q), OFFSET)] + ((Q)->pBuff[QUEUE_PTRLOOP((Q), OFFSET + 1)] << 8) + ((Q)->pBuff[QUEUE_PTRLOOP((Q), OFFSET + 2)] << 16))
#define QUEUE_TOU16(Q, OFFSET)			((Q)->pBuff[QUEUE_PTRLOOP((Q), OFFSET)] + ((Q)->pBuff[QUEUE_PTRLOOP((Q), OFFSET + 1)] << 8))

/* Exported types ------------------------------------------------------------*/
typedef struct
{
	uint8_t *pBuff;
	uint32_t size;		//Must be a power of 2
	uint32_t in;
	uint32_t out;
}QUEUE_Typedef;

typedef enum
{
	QUEUE_OK = 0,
	QUEUE_NOSPACE = -1,
	QUEUE_PARAM = -2,
	QUEUE_NOTENOUGHDATA = -3
}QUEUE_STATUS;

/* Exported variables --------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void QUEUE_Initialize(QUEUE_Typedef *queue, uint8_t *pBuff, uint32_t size);
QUEUE_STATUS QUEUE_Add(QUEUE_Typedef *queue, uint8_t data);
QUEUE_STATUS QUEUE_AddArray(QUEUE_Typedef *queue, uint8_t *data, uint32_t length);
QUEUE_STATUS QUEUE_AddQueue(QUEUE_Typedef *queue, QUEUE_Typedef *data, uint32_t length);
uint8_t QUEUE_ElementAt(QUEUE_Typedef *queue, uint32_t index);
uint8_t QUEUE_ReadOutByte(QUEUE_Typedef *queue);
QUEUE_STATUS QUEUE_ReadOutArray(QUEUE_Typedef *queue, uint8_t *data, uint32_t length);
QUEUE_STATUS QUEUE_ReadOutQueue(QUEUE_Typedef *queue, QUEUE_Typedef *data, uint32_t length);
QUEUE_STATUS QUEUE_ReadToArray(QUEUE_Typedef *queue, uint32_t offset, uint8_t *data, uint32_t length);
QUEUE_STATUS QUEUE_Remove(QUEUE_Typedef *queue, uint32_t count);

#endif /* BUFFER_H_ */
