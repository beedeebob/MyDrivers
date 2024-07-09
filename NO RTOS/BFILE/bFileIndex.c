/*
 * bFile.c
 *
 *  Created on: Jul 3, 2024
 *      Author: ben-linux
 */

/* Includes ------------------------------------------------------------------*/
#include <bFileIndex.h>
#include "bFileCore.h"
#include "bSPIFlash.h"
#include "utils.h"
#include "string.h"
#include "stdlib.h"

/*
 * INFO
 *
 * o File data stored as SEGMENT chunks on the flash
 *
 * 			NAME			SIZE		DESCRIPTION
 * 		o 	STX				1 byte		0xA5 (Required to verify blankness of anything written after a segment)
 * 		o	Unique ID 		4 bytes		Unique ID of a file allowing segments to be correctly associated with each other
 * 		o	Order No		2 bytes		Order number of the segment within the file starting from 0
 * 		o	Version			1 byte		Version of the system for future proofing
 * 		o 	Length			2 bytes		Length of the segment from the STX to the end of the data
 * 		o 	Flags			1 byte		Flags to indicate the state of the segment
 * 										0x01	- Write valid
 * 										0x02	- Segment deleted
 * 		o	Data CRC32		4 bytes		Segment data CRC32 value for verification
 * 		o	Header CRC32	4 bytes		CRC32 value for the header with all flags cleared to 0xff
 *
 * o File identifying information stored as tags at the beginning of the file
 *
 * 		o 	Tag start		1 byte		0x01
 * 			(Repeating sequence for all tags)
 * 			o	Tag name		L1 bytes	NULL terminating string representing the name of the tag
 * 			o	Tag value		V1 bytes	NULL terminating string representing the value of the tag
 * 		o 	Tag	end			1 byte		0x02
 */

/* Private define ------------------------------------------------------------*/
//FLAGS
enum BFILE_FLAGS
{
	BFILE_FLAG_INDEXED = 0x01
};

enum BFILE_INDEXERSTATES
{
	BFIL_INDEXERSTATE_IDLE = 0,
	BFIL_INDEXERSTATE_AWAITFLASH,
	BFIL_INDEXERSTATE_READHEADER,
	BFIL_INDEXERSTATE_PARSEHEADER,
	BFIL_INDEXERSTATE_COMPLETE
};

/* Private macro -------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
BFILE_FILELIST_td* BFILE_FileListStart;
BFILE_FILELIST_td* BFILE_FileListEnd;
static uint8_t flags;
static uint8_t bfileBuffer[30];

/* Private function prototypes -----------------------------------------------*/
static void BFILE_Indexer(void);
static uint8_t BFILE_ParseHeader(uint8_t *data, BFILE_IDXSEG_td *segment);

extern void BFILE_WriteFileListManager(void);
void BFILE_FileAddSegment(BFILE_FILE_td *file, BFILE_IDXSEG_td *segment);
void BFILE_AddFileToList(BFILE_FILELIST_td* fileLink);
BFILE_IDXSEG_td* BFILE_FindSegmentBeforeAddress(uint32_t address);
BFILE_IDXSEG_td* BFILE_FindSegmentAfterAddress(uint32_t address);

__attribute ((weak)) void Error_Handler(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief 	Index manager
  * @param 	None
  * @retval None
  */
void BFILE_tickFast(void)
{
	if(!(flags & BFILE_FLAG_INDEXED))
		BFILE_Indexer();
	BFILE_WriteFileListManager();
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 	Index manager
  * @param 	None
  * @retval None
  */
static void BFILE_Indexer(void)
{
	static uint8_t indexer = BFIL_INDEXERSTATE_IDLE;
	static uint32_t address;
	static BFLASH_Access_td flash;

	switch(indexer)
	{
	case BFIL_INDEXERSTATE_IDLE:
		address = 0;
		indexer = BFIL_INDEXERSTATE_AWAITFLASH;

	case BFIL_INDEXERSTATE_AWAITFLASH:
		if(!BFLASH_GetInfo()->isReady)
			break;
		indexer = BFIL_INDEXERSTATE_READHEADER;

	case BFIL_INDEXERSTATE_READHEADER:
		if(address >= BFLASH_GetInfo()->flashSize)
		{
			indexer = BFIL_INDEXERSTATE_COMPLETE;
			break;
		}

		flash.address = address;
		flash.data = bfileBuffer;
		flash.size = 30;
		if(BFLASH_Read(&flash) != BFLASH_ERROK)
			break;
		indexer = BFIL_INDEXERSTATE_PARSEHEADER;
		break;

	case BFIL_INDEXERSTATE_PARSEHEADER:
		if(!flash.complete)
			break;
		if(flash.result != BFLASH_ERROK)
		{
			indexer = BFIL_INDEXERSTATE_READHEADER;
			break;
		}

		uint16_t sectorSize = BFLASH_GetInfo()->sectorSize;
		//Set "byteAfterSegment" of segment before
		if(BLOCKSTART(address, sectorSize) != address)
		{
			BFILE_IDXSEG_td *seg = BFILE_FindSegmentBeforeAddress(address);
			if((seg != NULL) && ((seg->address + seg->length) == address))
				seg->byteAfterSegment = bfileBuffer[0];
		}

		BFILE_IDXSEG_td tmpSeg;
		if(!BFILE_ParseHeader(bfileBuffer, &tmpSeg))
		{
			address = BLOCKNEXT(address, sectorSize);
			indexer = BFIL_INDEXERSTATE_READHEADER;
			break;
		}

		if(!(tmpSeg.flags & BFILE_HEADERFLAG_VALID))
		{
			address = BLOCKNEXT(address, sectorSize);
			indexer = BFIL_INDEXERSTATE_READHEADER;
			break;
		}

		if(tmpSeg.flags & BFILE_HEADERFLAG_DELETED)
		{
			address += tmpSeg.length;
			indexer = BFIL_INDEXERSTATE_READHEADER;
			break;
		}

		//Create new Segment
		BFILE_IDXSEG_td *segment = (BFILE_IDXSEG_td*)malloc(sizeof(BFILE_IDXSEG_td));
		memcpy(segment, &tmpSeg, sizeof(BFILE_IDXSEG_td));
		segment->address = address;
		segment->nextSegment = NULL;

		//Find/Create file
		BFILE_FILELIST_td *fileLink = BFILE_FileListStart;
		while((fileLink != NULL) && (fileLink->file.uniqueID != segment->uniqueID ))
			fileLink = fileLink->next;
		if(fileLink == NULL)
		{
			fileLink = (BFILE_FILELIST_td*)malloc(sizeof(BFILE_FILELIST_td));
			assert(fileLink);
			memset(fileLink, 0, sizeof(BFILE_FILELIST_td));

			BFILE_AddFileToList(fileLink);
		}

		//Add segment
		fileLink->file.uniqueID = segment->uniqueID;
		BFILE_FileAddSegment(&fileLink->file, segment);

		//Move On
		address += tmpSeg.length;
		indexer = BFIL_INDEXERSTATE_READHEADER;
		break;
	}
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Add a segment into a file in order of sequence
  * @param 	file: pointer to the file in which to add the segment
  * @param	segment: pointer to the segment added to the file
  * @retval None
  */
void BFILE_FileAddSegment(BFILE_FILE_td *file, BFILE_IDXSEG_td *segment)
{
	BFILE_IDXSEG_td *pSeg = file->firstSegment;
	BFILE_IDXSEG_td *pSegPrev = NULL;
	while((pSeg != NULL) && (pSeg->segmentNo > segment->segmentNo))
	{
		pSegPrev = pSeg;
		pSeg = pSeg->nextSegment;
	}
	if(pSegPrev == NULL)
	{
		segment->nextSegment = file->firstSegment;
		file->firstSegment = segment;
	}
	else
	{
		segment->nextSegment = pSegPrev->nextSegment;
		pSegPrev->nextSegment = segment;
	}
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief 			Parse data for valid header
  * @param 			data: pointer to the pre-loaded data from which to parse
  * @param[out] 	segment: pointer to the returned segment information
  * @retval 		0 - Not found; 1 - found
  */
static uint8_t BFILE_ParseHeader(uint8_t *data, BFILE_IDXSEG_td *segment)
{
	 /* o Segment Data
	 * 		o STX - 1 byte ( required to verify blankness of anything written after a segment)
	 * 		o Unique ID - 4 bytes
	 * 		o Segment number - 2 byte
	 * 		o Version - 1 byte
	 * 		o Segment length - 2 bytes (from STX to end of segment data)
	 * 		o Flags - 1 byte
	 * 			o 0x01 - VALID
	 * 			o 0x02 - DELETED
	 * 	 	o Segment Checksum - 4 bytes
	 * 	 	o Header Checksum - 4 bytes
	 */

	if(data[0] != 0xA5)			//STX
		return 0;

	segment->flags = ~data[10];
	data[10] = 0xff;							//CRC calculation originally done on 0xff
	uint32_t hdrCRCSaved = BYTESTOUINT32(data, 15);
	uint32_t hdrCRCCalculated = crc32_accumulate(0, data, 0, 15);
	if(hdrCRCCalculated != hdrCRCSaved)
		return 0;
	data[10] = ~segment->flags;

	segment->uniqueID = BYTESTOUINT32(data, 1);
	segment->segmentNo = BYTESTOUINT16(data, 5);
	segment->version = data[7];
	segment->length = BYTESTOUINT16(data, 8);
	segment->crc = BYTESTOUINT32(data, 11);
	return 1;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Get whether the file indexing is complete
  * @param 	None
  * @retval 0 - Not complete; else complete
  */
bool BFILE_IsIndexingComplete(void)
{
	return (flags & BFILE_FLAG_INDEXED) ? true : false;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Create a file and segments and add it to the index system
  * @param 	length: amount of space required for the new file
  * @retval Pointer to the new
  */
BFILE_FILELIST_td* BFILE_CreateIndexedFile(uint32_t length)
{
	//Allocate file
	BFILE_FILELIST_td *fileLink = (BFILE_FILELIST_td*)malloc(sizeof(BFILE_FILELIST_td));
	if(fileLink == NULL)
		return NULL;
	memset(fileLink, 0, sizeof(BFILE_FILELIST_td));
	fileLink->file.length = length;
	fileLink->file.uniqueID = BFILE_GetUniqueId();

	//Create chain of segments
	uint32_t remainingLength = length;
	uint8_t segmentNo = 0;
	while(remainingLength < length)
	{
		BFILE_IDXSEG_td *newSeg = BFILE_CreateSegment(remainingLength);
		newSeg->uniqueID = fileLink->file.uniqueID;
		newSeg->segmentNo = segmentNo;
		BFILE_FileAddSegment(&fileLink->file, newSeg);
	}

	//Link file to list
	BFILE_AddFileToList(fileLink);
	return fileLink;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Find a unique ID for the new file
  * @param 	None
  * @retval Unique ID
  */
uint32_t BFILE_GetUniqueId(void)
{
	uint32_t uniqueID = 0;
	while(true)
	{
		BFILE_FILELIST_td *fileLink = BFILE_FileListStart;
		while((fileLink != NULL) && (fileLink->file.uniqueID != uniqueID))
			fileLink = fileLink->next;
		if(fileLink == NULL)
			return uniqueID;
		uniqueID++;
	}
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Create a file and segments and add it to the index system
  * @param 	length: amount of space required for the new file
  * @retval Pointer to the new
  */
BFILE_IDXSEG_td* BFILE_CreateSegment(uint32_t length)
{
	uint32_t address = 0;		//TODO: Need better method to reduce wear
	uint16_t space = 0;
	uint16_t sectorsize = BFLASH_GetInfo()->sectorSize;
	uint32_t flashsize = BFLASH_GetInfo()->flashSize;
	while(true)
	{
		if(address >= flashsize)
			return NULL;

		BFILE_IDXSEG_td *segmentBefore = BFILE_FindSegmentBeforeAddress(address);

		//Check if in segment
		if((address >= segmentBefore->address) && (address < (segmentBefore->address + segmentBefore->length)))
			address = segmentBefore->address + segmentBefore->length;

		//Check if next segment in my sector (can't write because we can't erase with other segment in sector)
		BFILE_IDXSEG_td *segmentAfter = BFILE_FindSegmentAfterAddress(address);
		if(BLOCKSTART(address, sectorsize) == BLOCKSTART(segmentAfter->address, sectorsize))
			address = segmentAfter->address + segmentAfter->length;

		//Get Gap
		space = segmentAfter->address - address;
		if(segmentAfter->address > BLOCKNEXT(address, sectorsize))	//Limit to end of current sector
			space = BLOCKNEXT(address, sectorsize) - address;

		//Check for gap large enough for at least 50 data bytes
		if(space < BFILE_SEGMENTLENGTH(50))
			address += space;

		//On sector start
		if(address == BLOCKSTART(address, sectorsize))
			break;

		//Space empty
		if(segmentBefore->byteAfterSegment == 0xff)
			break;

		address = BLOCKNEXT(address, sectorsize);
	}

	//Limit the space to length
	if(BFILE_SEGMENTLENGTH(length) < space)
		space = BFILE_SEGMENTLENGTH(length);

	//Create segment
	BFILE_IDXSEG_td *segment = (BFILE_IDXSEG_td*)malloc(sizeof(BFILE_IDXSEG_td));
	assert(segment);
	memset(segment, 0, sizeof(BFILE_IDXSEG_td));
	segment->address = address;
	segment->length = space;
	segment->byteAfterSegment = 0xff;
	segment->version = BFILE_VERSION;
	return segment;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Find segment which is right before or over address
  * @param 	address: Address from which to search
  * @retval Pointer to the segment
  */
BFILE_IDXSEG_td* BFILE_FindSegmentBeforeAddress(uint32_t address)
{
	BFILE_IDXSEG_td *closest = NULL;
	BFILE_FILELIST_td *fileLink = BFILE_FileListStart;
	while(fileLink != NULL)
	{
		BFILE_IDXSEG_td *segment = fileLink->file.firstSegment;
		while(segment != NULL)
		{
			if(segment->address <= address)
			{
				if((closest == NULL) || (segment->address > closest->address))
					closest = segment;
			}

			segment = segment->nextSegment;
		}
		fileLink = fileLink->next;
	}
	return closest;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Find segment which is right after address
  * @param 	address: Address from which to search
  * @retval Pointer to the segment
  */
BFILE_IDXSEG_td* BFILE_FindSegmentAfterAddress(uint32_t address)
{
	BFILE_IDXSEG_td *closest = NULL;
	BFILE_FILELIST_td *fileLink = BFILE_FileListStart;
	while(fileLink != NULL)
	{
		BFILE_IDXSEG_td *segment = fileLink->file.firstSegment;
		while(segment != NULL)
		{
			if(segment->address > address)
			{
				if((closest == NULL) || (segment->address < closest->address))
					closest = segment;
			}

			segment = segment->nextSegment;
		}
		fileLink = fileLink->next;
	}
	return closest;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief	Add a fileLink to the file linked list
  * @param 	fileLink: pointer to the file link to add to the list
  * @retval None
  */
void BFILE_AddFileToList(BFILE_FILELIST_td* fileLink)
{
	fileLink = NULL;

	if(BFILE_FileListStart == NULL)
		BFILE_FileListStart = fileLink;

	if(BFILE_FileListEnd != NULL)
		BFILE_FileListEnd->next = fileLink;
	BFILE_FileListEnd = fileLink;
}

/* ---------------------------------------------------------------------------*/
/**
  * @brief  This function is executed in case of error occurrence.
  * @param 	None
  * @retval None
  */
__attribute ((weak)) void Error_Handler(void)
{
  while (1)
  {
  }
}
