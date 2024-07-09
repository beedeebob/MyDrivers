/*
 * File name:	bFileWrite.c
 * Created on: 	Jul 6, 2024
 * Author: 		ben-linux
 */

/* Includes ------------------------------------------------------------------*/
#include <bFileIndex.h>
#include "bFileWrite.h"
#include "bFileCore.h"
#include "bStream.h"
#include "utils.h"
#include "bSPIFlash.h"
#include "stddef.h"

/* Private define ------------------------------------------------------------*/
#define BFILE_WRITERCOUNT					4
#define BFILE_WRITEBUFFERSIZE				256

enum BFILE_WRITESTATES
{
	BFILE_WRITESTATE_IDLE = 0,
	BFILE_WRITESTATE_ERASESEGMENT,
	BFILE_WRITESTATE_AWAITERASESEGMENT,
	BFILE_WRITESTATE_WRITESEGMENTHEADER,
	BFILE_WRITESTATE_AWAITWRITESEGMENTHEADER,
	BFILE_WRITESTATE_WRITESEGMENTDATA,
	BFILE_WRITESTATE_AWAITWRITESEGMENTDATA,
	BFILE_WRITESTATE_FINISHSEGMENTHEADER,
	BFILE_WRITESTATE_AWAITFINISHSEGMENTHEADER,
};

/* Private macro -------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
typedef struct BFILE_WRITELIST_td
{
	struct BFILE_WRITELIST_td *next;
	BSTREAM_td *stream;
	uint32_t offset;
	uint32_t crc;
	uint8_t state;
	uint8_t buffer[BFILE_WRITEBUFFERSIZE];
	BFLASH_Access_td flash;
	BFILE_FILE_td *file;
}BFILE_WRITELIST_td;

/* Private variables ---------------------------------------------------------*/
static BFILE_WRITELIST_td writers[BFILE_WRITERCOUNT];
static BFILE_WRITELIST_td *writerList;

/* Private function prototypes -----------------------------------------------*/
static void BFILE_WriteFileManager(BFILE_WRITELIST_td *writer);
static BFILE_WRITELIST_td* BFILE_ActivateAndReturnWriter(void);
static void BFILE_DeactivateWriter(BFILE_WRITELIST_td *writer);
static BFILE_IDXSEG_td* BFILE_GetOffsetSegment(BFILE_FILE_td *file, uint32_t offset);
static uint32_t BFILE_GetSegmentDataOffset(BFILE_FILE_td *file, uint32_t offset);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief 	Start process of writing a file to the file system
  * @param 	file: file to write
  * @param 	stream: pointer to the initialized stream from which to get the data
  * @retval BFILE_StatusEnum
  */
BFILE_StatusEnum BFILE_WriteFile(BSTREAM_td *stream)
{
	if(!BFILE_IsIndexingComplete())
		return BFILE_ERRNOTINDEXED;

	//Create file
	BFILE_FILELIST_td *fileLink = BFILE_CreateIndexedFile(stream->length);
	if(fileLink == NULL)
		return BFILE_ERRNOTENOUGHSPACE;
	BFILE_FILE_td* newFile = &fileLink->file;

	//Get file write controller
	BFILE_WRITELIST_td *writer = BFILE_ActivateAndReturnWriter();
	if(writer == NULL)
		return BFILE_ERRBUSY;

	writer->file = newFile;
	writer->stream = stream;
	writer->state = BFILE_WRITESTATE_IDLE;
	BFILE_WriteFileManager(writer);
	return BFILE_ERROK;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Manage the write processes
  * @param 	None
  * @retval None
  */
void BFILE_WriteFileListManager(void)
{
	if(writerList == NULL)
		return;

	BFILE_WRITELIST_td *writer = writerList;
	while(writer != NULL)
	{
		BFILE_WriteFileManager(writer);
		writer = writer->next;
	}
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Manage the individual write processes
  * @param	writer: pointer to the writer
  * @retval	None
  */
static void BFILE_WriteFileManager(BFILE_WRITELIST_td *writer)
{
	//Handle stream cancelled
	if(writer->stream->flags & BSTREAM_FLAG_CANCEL)
	{
		writer->stream->flags |= BSTREAM_FLAG_COMPLETE;
		BFILE_DeactivateWriter(writer);
		return;
	}

	BFILE_IDXSEG_td* segment;

	switch(writer->state)
	{
	case BFILE_WRITESTATE_IDLE:
		writer->offset = 0;

	case BFILE_WRITESTATE_ERASESEGMENT:
		if(writer->offset >= writer->stream->length)
		{
			writer->stream->flags |= BSTREAM_FLAG_COMPLETE;
			BFILE_DeactivateWriter(writer);
			break;
		}

		segment = BFILE_GetOffsetSegment(writer->file, writer->offset);
		if(BLOCKSTART(segment->address, BFLASH_GetInfo()->sectorSize) != segment->address)	//Not the first thing on the sector
		{
			writer->state = BFILE_WRITESTATE_WRITESEGMENTHEADER;
			break;
		}

		writer->flash.address = segment->address;
		if(BFLASH_EraseSector(&writer->flash) == BFLASH_ERROK)
		{
			writer->state = BFILE_WRITESTATE_WRITESEGMENTHEADER;
		}
		break;

	case BFILE_WRITESTATE_AWAITERASESEGMENT:
		if(writer->flash.complete)
		{
			if(writer->flash.result == BFLASH_ERROK)
				writer->state = BFILE_WRITESTATE_WRITESEGMENTHEADER;
			else
			{
				writer->state = BFILE_WRITESTATE_ERASESEGMENT;
				break;
			}
		}
		else
			break;

	case BFILE_WRITESTATE_WRITESEGMENTHEADER:

		writer->crc = 0;
		segment = BFILE_GetOffsetSegment(writer->file, writer->offset);

		writer->buffer[0] = 0xA5;	//STX
		writer->buffer[1] = (uint8_t)(segment->uniqueID);
		writer->buffer[2] = (uint8_t)(segment->uniqueID >> 8);
		writer->buffer[3] = (uint8_t)(segment->uniqueID >> 16);
		writer->buffer[4] = (uint8_t)(segment->uniqueID >> 24);
		writer->buffer[5] = (uint8_t)(segment->segmentNo);
		writer->buffer[6] = (uint8_t)(segment->segmentNo >> 8);
		writer->buffer[7] = (uint8_t)(segment->version);
		writer->buffer[8] = (uint8_t)(segment->length);
		writer->buffer[9] = (uint8_t)(segment->length >> 8);
		writer->buffer[10] = 0xff;	//Flags
		writer->buffer[11] = 0xff;	//Segment CRC
		writer->buffer[12] = 0xff;	//Segment CRC
		writer->buffer[13] = 0xff;	//Segment CRC
		writer->buffer[14] = 0xff;	//Segment CRC
		writer->buffer[15] = 0xff;	//Header CRC
		writer->buffer[16] = 0xff;	//Header CRC
		writer->buffer[17] = 0xff;	//Header CRC
		writer->buffer[18] = 0xff;	//Header CRC

		writer->flash.address = segment->address;
		writer->flash.data = writer->buffer;
		writer->flash.size = BFILE_SEGMENTHEADERLENGTH;
		if(BFLASH_Write(&writer->flash) == BFLASH_ERROK)
			writer->state = BFILE_WRITESTATE_AWAITWRITESEGMENTHEADER;
		break;

	case BFILE_WRITESTATE_AWAITWRITESEGMENTHEADER:
		if(writer->flash.complete)
		{
			if(writer->flash.result == BFLASH_ERROK)
				writer->state = BFILE_WRITESTATE_WRITESEGMENTDATA;
			else
			{
				writer->state = BFILE_WRITESTATE_ERASESEGMENT;
				break;
			}
		}
		else
			break;
		break;

	case BFILE_WRITESTATE_WRITESEGMENTDATA:
		segment = BFILE_GetOffsetSegment(writer->file, writer->offset);
		uint32_t segDataOffset = BFILE_GetSegmentDataOffset(writer->file, writer->offset);
		uint32_t length = BFILE_SEGMENTDATALENGTH(segment->length) - segDataOffset;
		if(length == 0)	//Segment complete
		{
			writer->state = BFILE_WRITESTATE_FINISHSEGMENTHEADER;
			break;
		}

		if(length > BFILE_WRITEBUFFERSIZE)
			length = BFILE_WRITEBUFFERSIZE;

		if(writer->stream->count(writer->stream, writer->offset) < length)
			break;
		writer->stream->read(writer->stream, writer->offset, writer->buffer, length);
		writer->crc = crc32_accumulate(writer->crc, writer->buffer, 0, length);

		writer->flash.address = segment->address + BFILE_SEGMENTHEADERLENGTH + segDataOffset;
		writer->flash.data = writer->buffer;
		writer->flash.size = length;
		if(BFLASH_Write(&writer->flash) == BFLASH_ERROK)
			writer->state = BFILE_WRITESTATE_AWAITWRITESEGMENTHEADER;
		break;

	case BFILE_WRITESTATE_AWAITWRITESEGMENTDATA:
		if(writer->flash.complete)
		{
			 if(writer->flash.result == BFLASH_ERROK)
			 {
				 writer->offset += writer->flash.size;
				 writer->state = BFILE_WRITESTATE_WRITESEGMENTDATA;
			 }
			 else
			 {
				 writer->state = BFILE_WRITESTATE_WRITESEGMENTDATA;
			 }
		}
		break;

	case BFILE_WRITESTATE_FINISHSEGMENTHEADER:

		segment = BFILE_GetOffsetSegment(writer->file, writer->offset);

		writer->buffer[0] = 0xA5;	//STX
		writer->buffer[1] = (uint8_t)(segment->uniqueID);
		writer->buffer[2] = (uint8_t)(segment->uniqueID >> 8);
		writer->buffer[3] = (uint8_t)(segment->uniqueID >> 16);
		writer->buffer[4] = (uint8_t)(segment->uniqueID >> 24);
		writer->buffer[5] = (uint8_t)(segment->segmentNo);
		writer->buffer[6] = (uint8_t)(segment->segmentNo >> 8);
		writer->buffer[7] = (uint8_t)(segment->version);
		writer->buffer[8] = (uint8_t)(segment->length);
		writer->buffer[9] = (uint8_t)(segment->length >> 8);
		writer->buffer[10] = 0xff;	//Flags
		writer->buffer[11] = (uint8_t)(writer->crc);
		writer->buffer[12] = (uint8_t)(writer->crc >> 8);
		writer->buffer[13] = (uint8_t)(writer->crc >> 16);
		writer->buffer[14] = (uint8_t)(writer->crc >> 24);

		writer->crc =  crc32_accumulate(0, writer->buffer, 0, BFILE_SEGMENTHEADERLENGTH - 4);
		writer->buffer[15] = (uint8_t)(writer->crc);
		writer->buffer[16] = (uint8_t)(writer->crc >> 8);
		writer->buffer[17] = (uint8_t)(writer->crc >> 16);
		writer->buffer[18] = (uint8_t)(writer->crc >> 24);
		writer->buffer[10] = ~(BFILE_HEADERFLAG_VALID);	//Flags

		writer->flash.address = segment->address;
		writer->flash.data = writer->buffer;
		writer->flash.size = BFILE_SEGMENTHEADERLENGTH;
		if(BFLASH_Write(&writer->flash) == BFLASH_ERROK)
			writer->state = BFILE_WRITESTATE_AWAITFINISHSEGMENTHEADER;
		break;

	case BFILE_WRITESTATE_AWAITFINISHSEGMENTHEADER:
		if(writer->flash.complete)
		{
			if(writer->flash.result == BFLASH_ERROK)
				writer->state = BFILE_WRITESTATE_ERASESEGMENT;
			else
				writer->state = BFILE_WRITESTATE_FINISHSEGMENTHEADER;
		}
		break;
	}
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Activate an unused writer and return it
  * @param	None
  * @retval	writer: pointer to the writer
  */
static BFILE_WRITELIST_td* BFILE_ActivateAndReturnWriter(void)
{
	uint8_t i;
	for(i = 0; i < BFILE_WRITERCOUNT; i++)
	{
		BFILE_WRITELIST_td *writer = writerList;
		while(writer != NULL)
		{
			if(writer == &writers[i])
				break;
		}
		if(writer == NULL)
			break;
	}
	if(i >= BFILE_WRITERCOUNT)
		return NULL;

	writers[i].next = writerList;
	writerList = &writers[i];
	return writerList;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Relegate a writer back to dusty back rooms
  * @param	writer: pointer to the writer
  * @retval	None
  */
static void BFILE_DeactivateWriter(BFILE_WRITELIST_td *writer)
{
	BFILE_WRITELIST_td *srch = writerList;
	BFILE_WRITELIST_td *prev = NULL;
	while(srch != NULL)
	{
		if(srch == writer)
			break;
		prev = srch;
		srch = srch->next;
	}

	if(prev == NULL)
		writerList = writer->next;
	else
		prev->next = writer->next;
	writer->next = NULL;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Get the file segment from the file offset
  * @param 	file: pointer to the file in which to search
  * @param 	offset: byte offset within the file
  * @retval Reference to the segment
  */
static BFILE_IDXSEG_td* BFILE_GetOffsetSegment(BFILE_FILE_td *file, uint32_t offset)
{
	BFILE_IDXSEG_td *srch = file->firstSegment;
	uint32_t localOffset = 0;
	while(srch != NULL)
	{
		localOffset += BFILE_SEGMENTDATALENGTH(srch->length);
		if(localOffset > offset)
			return srch;
		srch = srch->nextSegment;
	}
	return NULL;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Get the segment data offset from the file offset
  * @param 	file: pointer to the file in which to search
  * @param 	offset: byte offset within the file
  * @retval Offset of the data in the segment
  */
static uint32_t BFILE_GetSegmentDataOffset(BFILE_FILE_td *file, uint32_t offset)
{
	BFILE_IDXSEG_td *srch = file->firstSegment;
	uint32_t localOffset = 0;
	while(srch != NULL)
	{
		if((localOffset = BFILE_SEGMENTDATALENGTH(srch->length)) > offset)
			return offset - localOffset;
		localOffset += BFILE_SEGMENTDATALENGTH(srch->length);
		srch = srch->nextSegment;
	}
	return 0xffffffff;
}


