/*
 * bFile.c
 *
 *  Created on: Jul 3, 2024
 *      Author: ben-linux
 */

/* Includes ------------------------------------------------------------------*/
#include "bFile.h"
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

//FILE HEADER FLAGS
enum BFILE_HEADERFLAGS
{
	BFILE_HEADERFLAG_VALID = 0x01,
	BFILE_HEADERFLAG_DELETED = 0x02,
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
typedef struct BFIL_IDXSEG_td
{
	uint32_t uniqueID;
	uint32_t crc;
	uint32_t address;
	struct BFIL_IDXSEG_td *nextSegment;
	uint16_t segmentNo;
	uint16_t length;
	uint8_t version;
	uint8_t flags;
}BFILE_IDXSEG_td;
typedef struct
{
	BFILE_IDXSEG_td *firstSegment;
	uint32_t length;
	uint32_t uniqueID;
}BFILE_FILE_td;
typedef struct BFILE_FILELIST_td
{
	struct BFILE_FILELIST_td *next;
	BFILE_FILE_td file;
}BFILE_FILELIST_td;


/* Private variables ---------------------------------------------------------*/
static uint8_t flags;
static BFILE_FILELIST_td* fileListStart;
static BFILE_FILELIST_td* fileListEnd;
static uint8_t bfileBuffer[30];

/* Private function prototypes -----------------------------------------------*/
static void BFILE_Indexer(void);
static uint8_t BFILE_ParseHeader(uint8_t *data, BFILE_IDXSEG_td *segment);
void Error_Handler(void);

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

		BFILE_IDXSEG_td tmpSeg;
		if(!BFILE_ParseHeader(bfileBuffer, &tmpSeg))
		{
			address = BLOCKNEXT(address, BFLASH_GetInfo()->sectorSize);
			indexer = BFIL_INDEXERSTATE_READHEADER;
			break;
		}

		if(!(tmpSeg.flags & BFILE_HEADERFLAG_VALID))
		{
			address = BLOCKNEXT(address, BFLASH_GetInfo()->sectorSize);
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
		BFILE_FILELIST_td *fileLink = fileListStart;
		while((fileLink != NULL) && (fileLink->file.uniqueID != segment->uniqueID ))
			fileLink = fileLink->next;
		if(fileLink == NULL)
		{
			if(fileListStart == NULL)
				fileListStart = fileLink;

			fileLink = (BFILE_FILELIST_td*)malloc(sizeof(BFILE_FILELIST_td));
			if(fileLink == NULL)
				Error_Handler();
			memset(fileLink, 0, sizeof(BFILE_FILELIST_td));

			if(fileListEnd != NULL)
				fileListEnd->next = fileLink;
			fileListEnd = fileLink;
		}

		//Add segment
		fileLink->file.uniqueID = segment->uniqueID;
		BFILE_IDXSEG_td *pSeg = fileLink->file.firstSegment;
		BFILE_IDXSEG_td *pSegPrev = NULL;
		while((pSeg != NULL) && (pSeg->segmentNo > segment->segmentNo))
		{
			pSegPrev = pSeg;
			pSeg = pSeg->nextSegment;
		}
		if(pSegPrev == NULL)
		{
			segment->nextSegment = fileLink->file.firstSegment;
			fileLink->file.firstSegment = segment;
		}
		else
		{
			segment->nextSegment = pSegPrev->nextSegment;
			pSegPrev->nextSegment = segment;
		}

		//Move On
		address += tmpSeg.length;
		indexer = BFIL_INDEXERSTATE_READHEADER;
		break;
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
