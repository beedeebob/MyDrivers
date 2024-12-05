/**
  ******************************************************************************
  * @file     	bBufferChaining.h
  * @author		beede
  * @version	1V0
  * @date		Jul 22, 2024
  * @brief
  */


#ifndef INC_BBUFFERCHAINING_H_
#define INC_BBUFFERCHAINING_H_

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"
#include "stddef.h"

/* Exported defines ----------------------------------------------------------*/
#define BCHAIN_SIZE								256

//CHAIN MACROS
#define BCHAIN_CHAIN_CLEAR(CHAIN)				((CHAIN)->buffer = NULL)
#define BCHAIN_CHAIN_HEAD(CHAIN)					(CHAIN)->buffer
#define BCHAIN_ISCHAINEMPTY(CHAIN)				((CHAIN)->buffer == NULL)

//BUFFER MACROS
#define BCHAIN_BUFFER_CLEAR(BUFF)				(BUFF)->length = 0;\
													(BUFF)->next = NULL

//FLAGS
enum BCHAIN_FLAGS
{
	BCHAIN_FLAG_ACCEPTPARTIALBUFFERS = 0x01
};


/* Exported types ------------------------------------------------------------*/
typedef enum
{
	BCHAIN_OK = 0,
	BCHAIN_NOTENOUGHDATA,

}BCHAIN_StatusEnum;
typedef struct BCHAIN_Buffer_td
{
	uint32_t offset;			//If buffers are a part of a much larger file
	uint8_t data[BCHAIN_SIZE];
	uint32_t length;
	struct BCHAIN_Buffer_td *next;
}BCHAIN_Buffer_td;
typedef struct
{
	BCHAIN_Buffer_td *buffer;
}BCHAIN_Chain_td;

/* Exported variables --------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
uint32_t BCHAIN_GetChainSize(BCHAIN_Chain_td *chain);
uint32_t BCHAIN_GetChainDataCount(BCHAIN_Chain_td *chain, uint32_t offset);
void BCHAIN_ChainAddTail(BCHAIN_Chain_td *chain, BCHAIN_Buffer_td *buffer);
void BCHAIN_ChainRemoveHead(BCHAIN_Chain_td *chain);
void BCHAIN_ChainAddChainTail(BCHAIN_Chain_td *chain, BCHAIN_Chain_td *addChain);
BCHAIN_Buffer_td* BCHAIN_GetChainTail(BCHAIN_Chain_td *chain);
void BCHAIN_GetChainBuffersApplicableToOffset(BCHAIN_Chain_td *chain, uint32_t offset, BCHAIN_Chain_td *removedChain);
void BCHAIN_GetLoadedChainBuffers(BCHAIN_Chain_td *chain, BCHAIN_Chain_td *removedChain, uint8_t flags);
BCHAIN_StatusEnum BCHAIN_ReadChainData(BCHAIN_Chain_td *chain, uint32_t offset, uint8_t *data, uint32_t length);
BCHAIN_StatusEnum BCHAIN_WriteChainData(BCHAIN_Chain_td *chain, uint32_t offset, uint8_t *data, uint32_t length);
void BCHAIN_ResetChain(BCHAIN_Chain_td *chain, uint32_t offset);

#endif /* INC_BBUFFERCHAINING_H_ */
