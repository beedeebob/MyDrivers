/*
 * bFile.h
 *
 *  Created on: Jul 4, 2024
 *      Author: ben-linux
 */

#ifndef INC_BFILEINDEX_H_
#define INC_BFILEINDEX_H_

/* Includes ------------------------------------------------------------------*/
#include "bFileCore.h"
#include "stdbool.h"

/* Public typedef ------------------------------------------------------------*/
/* Public define -------------------------------------------------------------*/
/* Public macro --------------------------------------------------------------*/
/* Public variables ----------------------------------------------------------*/
/* Public function prototypes ------------------------------------------------*/
void BFILE_tickFast(void);
bool BFILE_IsIndexingComplete(void);

BFILE_FILELIST_td* BFILE_CreateIndexedFile(uint32_t length);
uint32_t BFILE_GetUniqueId(void);
BFILE_IDXSEG_td* BFILE_CreateSegment(uint32_t length);

#endif /* INC_BFILEINDEX_H_ */
