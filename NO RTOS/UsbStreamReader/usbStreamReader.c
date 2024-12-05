/**
  ******************************************************************************
  * @file     	usbStreamReader.c
  * @author		beede
  * @version	1V0
  * @date		Jul 20, 2024
  * @brief
  */
/*
 * INFORMATION
 *
 * HEADER
 * 		BYTES				DESCRIPTION
 * 		0-3					Length of the data to transmit/receive (little endian)
 * 		4-5					CRC of the data
 *
 * DATA
 * 		BYTES				DESCRIPTION
 * 		0-3					Offset of this data within the file
 * 		4-5					Length of this data
 *
 * BUFFER
 * 	Assumption 1: chunks will be in order within the buffer
 *	Assumption 2: chunks will be contiguous
 *
 *	RECEIVER
 *		o Slave to the stream
 *		o While the stream is open, send packet every 1000ms
 *		o On stream read/count request, adjust the streamOffset and load data buffers
 *		o On close send EOT to the server
 */

/* Includes ------------------------------------------------------------------*/
#include <usbStreamReader.h>
#include "string.h"
#include "usbPacketIDs.h"
#include "usbHandler.h"
#include "stdbool.h"

/* Private define ------------------------------------------------------------*/
#define USR_KEEPALIVETIME					500			//500ms
#define USR_DATARECTIMEOUT					100			//100ms
#define USR_USBTIMEOUT						1100		//1100ms

//STATES
enum USR_STATEs
{
	USR_STATE_IDLE = 0,
	USR_STATE_READ,
	USR_STATE_READREQUESTDATA,
	USR_STATE_READAWAITDATA,
	USR_STATE_READCOMPLETE
};

/* Private typedef -----------------------------------------------------------*/
//FLAGS
enum USR_FLAGs
{
	USR_FLAG_STARTED = 0x01,
	USR_FLAG_STRMOPENED = 0x02,
	USR_FLAG_USBTIMEDOUT = 0x04,
	USR_FLAG_STRMCLOSED = 0x08,
	USR_FLAG_CANCELLED = 0x10,
	USR_FLAG_REMOVED = 0x20,
	USR_FLAG_STREAMACCESSDENIED = (USR_FLAG_USBTIMEDOUT | USR_FLAG_STRMCLOSED | USR_FLAG_CANCELLED | USR_FLAG_REMOVED)
};

/* Private variables ---------------------------------------------------------*/
static USR_StreamReader_td *usrBaseStream;
static uint8_t usrID;

/* Private function prototypes -----------------------------------------------*/
static void USR_tickStreams(USR_StreamReader_td *stream);
static void USR_RequestUSBData(USR_StreamReader_td *stream);
static void USR_StackStream(USR_StreamReader_td *stream);
static void USR_DeStackStream(USR_StreamReader_td *stream);
static uint8_t USR_GetStreamID();

static BSTREAM_Enum USR_ReceiverStreamOpen(struct BSTREAM_Reader_td *stream);
static BSTREAM_Enum USR_StreamCount(BSTREAM_Reader_td *stream, uint32_t offset, uint32_t *count);
static BSTREAM_Enum USR_StreamRead(struct BSTREAM_Reader_td *stream, uint32_t offset, uint8_t *data, uint32_t length, uint32_t *actualLength);
static BSTREAM_Enum USR_ReceiverStreamClose(struct BSTREAM_Reader_td *stream);
static void USR_GetLowAndHiContiguousOffset(uint32_t *offsetsLo, uint32_t *offsetsHi, uint8_t offsetPairCount, uint32_t *offsetLo, uint32_t *offsetHi);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief	Tick controller
  * @param	None
  * @retval	None
  */
static uint32_t tickTimer;
void USR_millisecondTick(void)
{
	tickTimer++;
	if(usrBaseStream == NULL)
		return;

	USR_StreamReader_td *stream = usrBaseStream;
	while(stream != NULL)
	{
		USR_StreamReader_td *next = stream->next;
		USR_tickStreams(stream);
		stream = next;
	}
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Start a data transfer
  * @param	stream: pointer to the stream interface for received data
  * @param	length: amount of data to transfer
  * @param	crc: CRC checksum of the data
  * @retval	BSTREAM_Enum
  */
static uint32_t testCRC;
static uint32_t testCRCOffset;
BSTREAM_Enum USR_Start(USR_StreamReader_td *stream, uint32_t length, uint32_t crc)
{
	if((stream->flags & USR_FLAG_STARTED) && !(stream->flags & USR_FLAG_REMOVED))
		return BSTREAM_BUSY;

	//Stack stream for processing
	stream->streamID = USR_GetStreamID();
	USR_StackStream(stream);

	//Initialize stream interface
	stream->stream.length = length;
	stream->stream.crc = crc;
	stream->stream.open = USR_ReceiverStreamOpen;
	stream->stream.count = USR_StreamCount;
	stream->stream.readData = USR_StreamRead;
	stream->stream.close = USR_ReceiverStreamClose;

	//Reset buffers
	BCHAIN_CHAIN_CLEAR(&stream->availableBuffers);
	for(uint8_t i = 0; i < USR_BUFFERCOUNT; i++)
	{
		BCHAIN_BUFFER_CLEAR(&stream->buffers[i]);
		stream->buffers[i].offset = 0;
		BCHAIN_ChainAddTail(&stream->availableBuffers, &stream->buffers[i]);
	}
	BCHAIN_CHAIN_CLEAR(&stream->streamBuffers);

	//Runtime Variables
	stream->streamOffset = 0;
	stream->keepAliveTmr = 0xffff;
	stream->usbTimeoutTmr = 0;
	stream->flags = USR_FLAG_STARTED;
	testCRC = 0;
	testCRCOffset = 0;

	stream->reqTimeoutTmr = 0xffff;
	USR_RequestUSBData(stream);
	return BSTREAM_OK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Cancel a data transfer
  * @param	stream: pointer to the stream interface for received data
  * @retval	None
  */
void USR_Cancel(USR_StreamReader_td *stream)
{
	stream->flags |= USR_FLAG_CANCELLED;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Tick controller for streams
  * @param	stream: pointer to the stream to tick
  * @retval	None
  */
static void USR_tickStreams(USR_StreamReader_td *stream)
{
	uint8_t data[8];

	//Monitor for request timeout
	if(stream->reqTimeoutTmr < 0xffff)
		stream->reqTimeoutTmr++;
	if(stream->reqTimeoutTmr >= USR_DATARECTIMEOUT)
		USR_RequestUSBData(stream);

	//Monitor for USB timeout
	if(stream->usbTimeoutTmr < 0xffff)
		stream->usbTimeoutTmr++;
	if(stream->usbTimeoutTmr >= USR_USBTIMEOUT)
		stream->flags |= USR_FLAG_USBTIMEDOUT;

	//Monitor for stream end
	if(stream->flags & (USR_FLAG_STRMCLOSED | USR_FLAG_CANCELLED | USR_FLAG_USBTIMEDOUT))
	{
		//Request data
		data[0] = pktUSRClose;
		data[1] = stream->streamID;
		if(USBHND_sendPacket(data, 2) == true)
		{
			USR_DeStackStream(stream);
			stream->flags |= USR_FLAG_REMOVED;
			return;
		}
	}

	//Keep Comms Alive
	if(stream->keepAliveTmr < 0xffff)
		stream->keepAliveTmr++;
	if(stream->keepAliveTmr >= USR_KEEPALIVETIME)
	{
		data[0] = pktUSRAlive;
		data[1] = stream->streamID;
		if(USBHND_sendPacket(data, 2) == true)
			stream->keepAliveTmr = 0;
	}
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Find stream by unique ID
  * @param	streamID: id unique to the desired stream
  * @retval	Pointer to the stream or NULL if not found
  */
static uint32_t events[4096];
static uint16_t eventsIdx = 0;
static void USR_RequestUSBData(USR_StreamReader_td *stream)
{
	//Get offset of next data required for stream
	uint32_t offset = stream->streamOffset + BCHAIN_GetChainDataCount(&stream->streamBuffers, stream->streamOffset);

	//Restructure available buffers for streamOffset
	BCHAIN_Chain_td outofRangeBuffers;
	BCHAIN_GetChainBuffersApplicableToOffset(&stream->availableBuffers, offset, &outofRangeBuffers);	//Remove buffers in available not required for stream
	offset += BCHAIN_GetChainDataCount(&stream->availableBuffers, offset);	//Update the available offset
	if(BCHAIN_CHAIN_HEAD(&stream->availableBuffers) != NULL)
		BCHAIN_ResetChain(&outofRangeBuffers, 0);									//Tag buffers on the end
	else
		BCHAIN_ResetChain(&outofRangeBuffers, offset);							//Align to offset
	BCHAIN_ChainAddChainTail(&stream->availableBuffers, &outofRangeBuffers);

	//Get amount of data to request
	uint32_t chainSize = BCHAIN_GetChainSize(&stream->availableBuffers);
	uint32_t usedSpace = 0;
	if(BCHAIN_CHAIN_HEAD(&stream->availableBuffers) != NULL)
		usedSpace = BCHAIN_GetChainDataCount(&stream->availableBuffers, BCHAIN_CHAIN_HEAD(&stream->availableBuffers)->offset);
	uint32_t availableSpace = (chainSize - usedSpace);
	if(availableSpace > (stream->stream.length - offset))
		availableSpace = (stream->stream.length - offset);
	if(availableSpace == 0)
		return;

	//Request data
	uint8_t data[8];
	data[0] = pktUSRDataRequest;
	data[1] = stream->streamID;
	data[2] = (uint8_t)(offset);
	data[3] = (uint8_t)(offset >> 8);
	data[4] = (uint8_t)(offset >> 16);
	data[5] = (uint8_t)(offset >> 24);
	data[6] = (uint8_t)(availableSpace);
	data[7] = (uint8_t)(availableSpace >> 8);
	if(USBHND_sendPacket(data, 8) == true)
	{
		stream->requestedOffset = offset;
		stream->requestedLength = availableSpace;
		stream->reqTimeoutTmr = 0;
	}
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Stack a stream
  * @param	stream: pointer to the stream to tick
  * @retval	None
  */
static void USR_StackStream(USR_StreamReader_td *stream)
{
	USR_StreamReader_td *srch = usrBaseStream;
	while((srch != NULL) && (srch != stream))
		srch = srch->next;
	if(srch != NULL)	//Already on stack
		return;

	stream->next = usrBaseStream;
	usrBaseStream = stream;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Remove a stream from the stack
  * @param	stream: pointer to the stream to tick
  * @retval	None
  */
static void USR_DeStackStream(USR_StreamReader_td *stream)
{
	USR_StreamReader_td *srch = usrBaseStream;
	USR_StreamReader_td *prev = NULL;
	while((srch != NULL) && (srch != stream))
	{
		prev = srch;
		srch = srch->next;
	}
	if(srch == NULL)	//Not on stack
		return;

	if(prev == NULL)
		usrBaseStream = srch->next;
	else
		prev->next = srch->next;
	srch->next = NULL;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Find unique stream ID
  * @param	None
  * @retval	None
  */
static uint8_t USR_GetStreamID()
{
	while(1)
	{
		//Is in use
		USR_StreamReader_td *srch = usrBaseStream;
		while((srch != NULL) && (srch->streamID != usrID))
			srch = srch->next;
		if(srch == NULL)	//ID unique
			return usrID++;

		usrID++;
	}
	return 0xff;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Find stream by unique ID
  * @param	streamID: id unique to the desired stream
  * @retval	Pointer to the stream or NULL if not found
  */
USR_StreamReader_td* USR_GetStreamByID(uint8_t id)
{
	USR_StreamReader_td *srch = usrBaseStream;
	while((srch != NULL) && (srch->streamID != id))
		srch = srch->next;
	return srch;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Handler for data received
  * @param	offset: offset from which to write the data
  * @param	data: pointer to the array from which to read
  * @param	length: amount of data received
  * @retval	None
  */
void USR_DataReceivedHandler(USR_StreamReader_td *stream, uint32_t offset, uint8_t *data, uint16_t length)
{
	if(BCHAIN_ISCHAINEMPTY(&stream->availableBuffers))
		return;

#warning BEN: Debugging information
	if((testCRCOffset >= offset) && (testCRCOffset < (offset + length)))
	{
		uint32_t lclOS = (testCRCOffset - offset);
		uint32_t lclen = length - lclOS;
		testCRC = crc32_calculateData(testCRC, data, lclOS, lclen);
		testCRCOffset += lclen;
	}

	//Populate specified chunks
	BCHAIN_WriteChainData(&stream->availableBuffers, offset, data, length);

	//Get filled buffers
	BCHAIN_Chain_td filledBuffers;
	if((stream->receivedOffset + stream->receivedLength) == (stream->requestedOffset + stream->requestedLength))
		BCHAIN_GetLoadedChainBuffers(&stream->availableBuffers, &filledBuffers, BCHAIN_FLAG_ACCEPTPARTIALBUFFERS);
	else
		BCHAIN_GetLoadedChainBuffers(&stream->availableBuffers, &filledBuffers, 0);

	//Move to stream buffer
	if(BCHAIN_CHAIN_HEAD(&filledBuffers) != NULL)
		BCHAIN_ChainAddChainTail(&stream->streamBuffers, &filledBuffers);

	//Update
	stream->receivedOffset = offset;
	stream->receivedLength = length;
	stream->usbTimeoutTmr = 0;

	//Check if new data request required
	if((stream->receivedOffset + stream->receivedLength) == (stream->requestedOffset + stream->requestedLength))
	{
		stream->reqTimeoutTmr = USR_DATARECTIMEOUT;	//Prepare for retry if the call below fails
		USR_RequestUSBData(stream);
	}
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Handler for alive message
  * @param	stream: pointer to the stream
  * @retval	None
  */
void USR_AliveHandler(USR_StreamReader_td *stream)
{
	stream->usbTimeoutTmr = 0;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Open stream
  * @param	stream: pointer to the stream
  * @retval	BSTREAM_Enum
  */
static BSTREAM_Enum USR_ReceiverStreamOpen(struct BSTREAM_Reader_td *stream)
{
	USR_StreamReader_td *usrStream = (USR_StreamReader_td*)stream;
	if(usrStream->flags & USR_FLAG_STREAMACCESSDENIED)
		return BSTREAM_CLOSED;

	usrStream->flags |= USR_FLAG_STRMOPENED;
	return BSTREAM_OK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Count of data available from offset
  * @param	stream: pointer to the stream
  * @param[out]	count: amount of available data
  * @retval	BSTREAM_Enum
  */
static BSTREAM_Enum USR_StreamCount(BSTREAM_Reader_td *stream, uint32_t offset, uint32_t *count)
{
	USR_StreamReader_td *usrStream = (USR_StreamReader_td*)stream;
	if(usrStream->flags & USR_FLAG_STREAMACCESSDENIED)
		return BSTREAM_CLOSED;

	usrStream->streamOffset = offset;			//Used to know what to request next

	//Release unrequired buffers
	BCHAIN_Chain_td usedBuffers;
	BCHAIN_GetChainBuffersApplicableToOffset(&usrStream->streamBuffers, usrStream->streamOffset, &usedBuffers);
	BCHAIN_ResetChain(&usedBuffers, 0);
	BCHAIN_ChainAddChainTail(&usrStream->availableBuffers, &usedBuffers);

	//Check if new USB request required
	uint32_t offsetsLo[3];
	uint32_t offsetsHi[3];
	offsetsLo[0] = BCHAIN_CHAIN_HEAD(&usrStream->streamBuffers) != NULL ? BCHAIN_CHAIN_HEAD(&usrStream->streamBuffers)->offset : 0xffffffff;
	offsetsHi[0] = offsetsLo[0] + BCHAIN_GetChainDataCount(&usrStream->streamBuffers, offsetsLo[0]);
	offsetsLo[1] = BCHAIN_CHAIN_HEAD(&usrStream->availableBuffers) != NULL ? BCHAIN_CHAIN_HEAD(&usrStream->availableBuffers)->offset : 0xffffffff;
	offsetsHi[1] = offsetsLo[1] + BCHAIN_GetChainDataCount(&usrStream->availableBuffers, offsetsLo[1]);
	offsetsLo[2] = usrStream->requestedOffset;
	offsetsHi[2] = usrStream->requestedOffset + usrStream->requestedLength;
	uint32_t offsetLo;
	uint32_t offsetHi;
	USR_GetLowAndHiContiguousOffset(offsetsLo, offsetsHi, 3, &offsetLo, &offsetHi);


	if((usrStream->streamOffset < offsetLo) || (usrStream->streamOffset >= offsetHi))
	{
		usrStream->reqTimeoutTmr = USR_DATARECTIMEOUT;	//Force request again soon if request below fails
		USR_RequestUSBData(usrStream);
	}

	//Get available data
	uint32_t available = BCHAIN_GetChainDataCount(&usrStream->streamBuffers, offset);
	*count = available;

	return BSTREAM_OK;
}

/*----------------------------------------------------------------------------*/
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
static BSTREAM_Enum USR_StreamRead(struct BSTREAM_Reader_td *stream, uint32_t offset, uint8_t *data, uint32_t length, uint32_t *actualLength)
{
	USR_StreamReader_td *usrStream = (USR_StreamReader_td*)stream;
	if(usrStream->flags & USR_FLAG_STREAMACCESSDENIED)
		return BSTREAM_CLOSED;

	usrStream->streamOffset = offset;			//Used to know what to request next

	//Release unrequired buffers
	BCHAIN_Chain_td usedBuffers;
	BCHAIN_GetChainBuffersApplicableToOffset(&usrStream->streamBuffers, usrStream->streamOffset, &usedBuffers);
	BCHAIN_ResetChain(&usedBuffers, 0);
	BCHAIN_ChainAddChainTail(&usrStream->availableBuffers, &usedBuffers);

	//Check if new USB request required
	uint32_t offsetsLo[3];
	uint32_t offsetsHi[3];
	offsetsLo[0] = BCHAIN_CHAIN_HEAD(&usrStream->streamBuffers) != NULL ? BCHAIN_CHAIN_HEAD(&usrStream->streamBuffers)->offset : 0xffffffff;
	offsetsHi[0] = offsetsLo[0] + BCHAIN_GetChainDataCount(&usrStream->streamBuffers, offsetsLo[0]);
	offsetsLo[1] = BCHAIN_CHAIN_HEAD(&usrStream->availableBuffers) != NULL ? BCHAIN_CHAIN_HEAD(&usrStream->availableBuffers)->offset : 0xffffffff;
	offsetsHi[1] = offsetsLo[1] + BCHAIN_GetChainDataCount(&usrStream->availableBuffers, offsetsLo[1]);
	offsetsLo[2] = usrStream->requestedOffset;
	offsetsHi[2] = usrStream->requestedOffset + usrStream->requestedLength;
	uint32_t offsetLo;
	uint32_t offsetHi;
	USR_GetLowAndHiContiguousOffset(offsetsLo, offsetsHi, 3, &offsetLo, &offsetHi);


	if((usrStream->streamOffset < offsetLo) || (usrStream->streamOffset >= offsetHi))
	{
		usrStream->reqTimeoutTmr = USR_DATARECTIMEOUT;	//Force request again soon if request below fails
		USR_RequestUSBData(usrStream);
	}

	//Check for enough data
	uint32_t available = BCHAIN_GetChainDataCount(&usrStream->streamBuffers, offset);
	if((available < length) && (actualLength == NULL))
		return BSTREAM_NOTENOUGHDATA;

	//Read data
	if(available > length)
		available = length;
	BCHAIN_ReadChainData(&usrStream->streamBuffers, offset, data, available);
	if(actualLength != NULL)
		*actualLength = available;
	return BSTREAM_OK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	close the stream
  * @param	stream: pointer to the stream
  * @retval	BSTREAM_Enum
  */
static BSTREAM_Enum USR_ReceiverStreamClose(struct BSTREAM_Reader_td *stream)
{
	USR_StreamReader_td *usrStream = (USR_StreamReader_td*)stream;
	if(usrStream->flags & USR_FLAG_STREAMACCESSDENIED)
		return BSTREAM_CLOSED;

	usrStream->flags |= USR_FLAG_STRMCLOSED;
	return BSTREAM_OK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Get the high and low offsets of continuous data from multiple data sources
  * @param	offsetsLo: pointer to the low offsets
  * @param	offsetsHi: pointer to the high offsets
  * @param	offsetPairCount: number of hi/lo data pairs are available
  * @param[out]	offsetLo: pointer to the returned low offset
  * @param[out]	offsetHi: pointer to the returned high offset
  * @retval	None
  */
static void USR_GetLowAndHiContiguousOffset(uint32_t *offsetsLo, uint32_t *offsetsHi, uint8_t offsetPairCount, uint32_t *offsetLo, uint32_t *offsetHi)
{
	//Bubble sort by offsetsLo
	for(uint8_t pass = 0; pass < (offsetPairCount - 1); pass++)
	{
		for(uint8_t i = 0; i < (offsetPairCount - 1 - pass); i++)
		{
			if(offsetsLo[i] > offsetsLo[i + 1])
			{
				uint32_t val = offsetsLo[i];
				offsetsLo[i] = offsetsLo[i + 1];
				offsetsLo[i + 1] = val;

				val = offsetsHi[i];
				offsetsHi[i] = offsetsHi[i + 1];
				offsetsHi[i + 1] = val;
			}
		}
	}


	uint32_t lo = offsetsLo[0];
	uint32_t hi = offsetsHi[0];
	for(uint8_t i = 1; i < offsetPairCount; i++)
	{
		if((offsetsLo[i] <= hi) && (offsetsHi[i] > hi))
			hi = offsetsHi[i];
	}

	*offsetLo = lo;
	*offsetHi = hi;
}
