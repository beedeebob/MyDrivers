/**
  ******************************************************************************
  * @file     	usbStreamReader.h
  * @author		beede
  * @version	1V0
  * @date		Jul 20, 2024
  * @brief
  */


#ifndef INC_USR_H_
#define INC_USR_H_

/* Includes ------------------------------------------------------------------*/
#include "bStream.h"
#include "bBufferChaining.h"

/* Exported defines ----------------------------------------------------------*/
#define USR_BUFFERCOUNT					4

/* Exported types ------------------------------------------------------------*/
typedef struct USR_StreamReader_td
{
	BSTREAM_Reader_td stream;			//Stream interface
	uint8_t streamID;					//ID unique from other USB stream readers for comms identification

	//RUNTIME VARIABLES
	BCHAIN_Buffer_td buffers[USR_BUFFERCOUNT];					//Physical buffers to use
	BCHAIN_Chain_td availableBuffers;	//Chain of avaiable buffers
	BCHAIN_Chain_td streamBuffers;		//Chain of stream client buffers

	uint32_t streamOffset;				//Offset required by stream client

	uint32_t requestedOffset;
	uint16_t requestedLength;
	uint32_t receivedOffset;
	uint16_t receivedLength;

	uint16_t keepAliveTmr;
	uint16_t reqTimeoutTmr;
	uint16_t usbTimeoutTmr;

	uint8_t flags;				//@ref USR_FLAGs

	struct USR_StreamReader_td *next;
}USR_StreamReader_td;

/* Exported variables --------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void USR_millisecondTick(void);
BSTREAM_Enum USR_Start(USR_StreamReader_td *stream, uint32_t length, uint32_t crc);
void USR_Cancel(USR_StreamReader_td *stream);
void USR_DataReceivedHandler(USR_StreamReader_td *stream, uint32_t offset, uint8_t *data, uint16_t length);
void USR_AliveHandler(USR_StreamReader_td *stream);
USR_StreamReader_td* USR_GetStreamByID(uint8_t id);

#endif /* INC_USR_H_ */
