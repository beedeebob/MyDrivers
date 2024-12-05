/**
  ******************************************************************************
  * @file     	bPacket.c
  * @author		beede
  * @version	1V0
  * @date		Feb 26, 2024
  * @brief
  */


/* Information ---------------------------------------------------------------*/
/*
PACKET
o STX           1 byte          0x02
o Length        2 bytes
o CRC           4 bytes
o Data
o CRC           4 bytes
o ETX                           0x03
*/

/* Includes ------------------------------------------------------------------*/
#include "bPacket.h"
#include "bQueue.h"
#include "stdint.h"
#include "utils.h"

/* Private define ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/


/**
  * @brief	Parse for packet
  * @param	queue: Queue from which to remove the packet
  * @param[out]	packet: pointer to the returned packet when valid
  * @retval	BPKT_STATUS_ENUM
  */
BPKT_STATUS_ENUM PKT_Decode(QUEUE_Typedef *queue, BPKT_Packet_TD *packet)
{
    if(QUEUE_COUNT(queue) < BPKT_PACKETSIZE(1))
        return BPKT_NOTENOUGHDATA;
    
    if(QUEUE_ElementAt(queue, 0) != 0x02)
        return BPKT_STX;

    uint16_t calccrc = crc16_ccitt_calculateQueue(0xffff, queue, queue->out, 4);
    uint16_t lclcrc = QUEUE_TOU16(queue, queue->out + 4);
    if(lclcrc != calccrc)
        return BPKT_HCRC;

    uint16_t length = QUEUE_TOU16(queue, queue->out + 2);
    if((length > BPKT_MAXDATALENGTH) || (length == 0))
        return BPKT_LENGTH;
    
    if(QUEUE_COUNT(queue) < BPKT_PACKETSIZE(length))
        return BPKT_NOTENOUGHDATA;

    uint32_t crc32 = crc32_calculateQueue(0, queue, queue->out, length + 6);
    uint32_t lclcrc32 = QUEUE_TOU32(queue, queue->out + BPKT_PACKETSIZE(length) - 4);
    if(lclcrc32 != crc32)
        return BPKT_DCRC;

    //All good now
    packet->frame = QUEUE_ElementAt(queue, 1);
    QUEUE_ReadToArray(queue, 6, packet->data, length);
    packet->length = length;
    return BPKT_OK;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Parse for packet
  * @param	queue: Queue from which to remove the packet
  * @param[out]	packet: pointer to the returned packet when valid
  * @retval	BPKT_STATUS_ENUM
  */
BPKT_STATUS_ENUM PKT_Encode(uint8_t *data, uint16_t length, QUEUE_Typedef *queue)
{
    if(QUEUE_SPACE(queue) < BPKT_PACKETSIZE(length))
        return BPKT_NOTENOUGHSPACE;
    if(length > BPKT_MAXDATALENGTH)
    	return BPKT_EXCEEDSMAXSIZE;
    
    uint32_t strt = queue->in;
    QUEUE_Add(queue, 0x02);
    static uint8_t frame;
    QUEUE_Add(queue, frame++);
    QUEUE_Add(queue, (uint8_t)length);
    QUEUE_Add(queue, (uint8_t)(length >> 8));

    uint16_t calccrc = crc16_ccitt_calculateQueue(0xffff, queue, strt, 4);
    QUEUE_Add(queue, (uint8_t)calccrc);
    QUEUE_Add(queue, (uint8_t)(calccrc >> 8));

    QUEUE_AddArray(queue, data, length);

    uint32_t calccrc32 = crc32_calculateQueue(0, queue, strt, length + 6);
    QUEUE_Add(queue, (uint8_t)calccrc32);
    QUEUE_Add(queue, (uint8_t)(calccrc32 >> 8));
    QUEUE_Add(queue, (uint8_t)(calccrc32 >> 16));
    QUEUE_Add(queue, (uint8_t)(calccrc32 >> 24));
    return BPKT_OK;
}
