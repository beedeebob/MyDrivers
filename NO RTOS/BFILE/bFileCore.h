/*
 * File name:	bFileCore.h
 * Created on: 	Jul 6, 2024
 * Author: 		ben-linux
 */

#ifndef INC_BFILECORE_H_
#define INC_BFILECORE_H_

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"

/* Public define -------------------------------------------------------------*/
#define BFILE_VERSION									1

//FILE HEADER FLAGS
enum BFILE_HEADERFLAGS
{
	BFILE_HEADERFLAG_VALID = 0x01,
	BFILE_HEADERFLAG_DELETED = 0x02,
};

/* Public typedef ------------------------------------------------------------*/
typedef enum
{
	BFILE_ERROK = 0,
	BFILE_ERRNOTINDEXED,
	BFILE_ERRNOTENOUGHSPACE,
	BFILE_ERRBUSY,
}BFILE_StatusEnum;

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
	uint8_t byteAfterSegment;		//The value after the last segment byte to determine if there is data there
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

/* Public macro --------------------------------------------------------------*/
#define BFILE_SEGMENTHEADERLENGTH						(1 /*STX*/ + 4 /*Unique ID*/ + 2 /*Sequence No*/ + 1 /*Version*/ + 2 /*Length*/ + 1 /*Flags*/ + 4 /*Segment crc*/ + 4 /*Header CRC*/)
#define BFILE_SEGMENTLENGTH(DATALENGTH)					(DATALENGTH + BFILE_SEGMENTHEADERLENGTH)
#define BFILE_SEGMENTDATALENGTH(SEGMENTLENGTH)			(SEGMENTLENGTH - BFILE_SEGMENTHEADERLENGTH)

/* Public variables ----------------------------------------------------------*/
extern BFILE_FILELIST_td* BFILE_FileListStart;
extern BFILE_FILELIST_td* BFILE_FileListEnd;

/* Public function prototypes ------------------------------------------------*/

#endif /* INC_BFILECORE_H_ */
