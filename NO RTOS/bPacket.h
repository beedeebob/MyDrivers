/**
  ******************************************************************************
  * @file     	bPacket.c
  * @author		beede
  * @version	1V0
  * @date		Feb 26, 2024
  * @brief
  */

#ifndef BEN_PACKET_H
#define BEN_PACKET_H

/* Includes ------------------------------------------------------------------*/
#include "bQueue.h"

/* Exported defines ----------------------------------------------------------*/
#define BPKT_HEADEROVERHEAD       	(1 /*STX*/ + 1 /*Frame*/ + 2/*Length*/ + 2/*CRC16*/)
#define BPKT_DATAOVERHEAD           	(4/*CRC32*/)
#define BPKT_PACKETOVERHEAD          (BPKT_HEADEROVERHEAD + BPKT_DATAOVERHEAD)
#define BPKT_PACKETSIZE(N)      		(BPKT_PACKETOVERHEAD + N)
#define BPKT_MAXDATALENGTH      		400

/* Exported types ------------------------------------------------------------*/
typedef struct
{
    uint8_t data[BPKT_MAXDATALENGTH];
    uint16_t length;
    uint8_t frame;
}BPKT_Packet_TD;

typedef enum
{
	BPKT_OK = 0,
    BPKT_NOTENOUGHDATA = -1,
    BPKT_STX = -2,
    BPKT_LENGTH = -3,
    BPKT_HCRC = -4,
    BPKT_ETX = -5,
    BPKT_DCRC = -6,
    BPKT_NOTENOUGHSPACE = -7,
    BPKT_EXCEEDSMAXSIZE = -8
}BPKT_STATUS_ENUM;

/* Exported variables --------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
BPKT_STATUS_ENUM PKT_Decode(QUEUE_Typedef *queue, BPKT_Packet_TD *packet);
BPKT_STATUS_ENUM PKT_Encode(uint8_t *data, uint16_t length, QUEUE_Typedef *queue);

#endif /* BEN_PACKET_H */
