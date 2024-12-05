/**
  ******************************************************************************
  * @file     	bBufferChaining.c
  * @author		beede
  * @version	1V0
  * @date		Jul 22, 2024
  * @brief
  */


/* Includes ------------------------------------------------------------------*/
#include "bBufferChaining.h"
#include "string.h"

/* Private define ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/


/**
  * @brief	Get the size of a buffer chain
  * @param	chain: pointer to the chain of buffers
  * @retval	Size of the buffer chaing
  */
uint32_t BCHAIN_GetChainSize(BCHAIN_Chain_td *chain)
{
	assert(chain);
	uint32_t size = 0;
	BCHAIN_Buffer_td *buffer = chain->buffer;
	while(buffer != NULL)
	{
		size += sizeof(buffer->data);
		buffer = buffer->next;
	}
	return size;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Get the amount data in a buffer chain
  * @param	chain: pointer to the buffer chain
  * @param	offset: offset from which to check
  * @retval	count
  */
uint32_t BCHAIN_GetChainDataCount(BCHAIN_Chain_td *chain, uint32_t offset)
{
	uint32_t count = 0;
	BCHAIN_Buffer_td *buff = chain->buffer;
	while(buff != NULL)
	{
		if((offset >= buff->offset) && (offset < (buff->offset + buff->length)))
			count += (buff->length - (offset - buff->offset));
		else if((count > 0) && (offset < (buff->offset + buff->length)))
			count += buff->length;
		buff = buff->next;
	}
	return count;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Add a buffer to the tail of a chain. If the buffer length is 0 the
  * 		buffer offset will be contiguous with the previous buffer offset in
  * 		the chain
  * @param	chain: pointer to the buffer chain
  * @param	buffer: pointer to the buffer to add to the chain
  * @retval	None
  */
void BCHAIN_ChainAddTail(BCHAIN_Chain_td *chain, BCHAIN_Buffer_td *buffer)
{
	BCHAIN_Buffer_td *endBuff = BCHAIN_GetChainTail(chain);
	if(endBuff == NULL)
	{
		buffer->next = NULL;
		chain->buffer = buffer;
		return;
	}

	//Align empty buffer
	if(buffer->length == 0)
	{
		buffer->offset = endBuff->offset + sizeof(endBuff->data);
		buffer->next = NULL;
		endBuff->next = buffer;
		return;
	}

	//Add full buffer
	assert(( buffer->offset == (endBuff->offset + sizeof(endBuff->data))) || ( buffer->offset == (endBuff->offset + endBuff->length)));
	buffer->next = NULL;
	endBuff->next = buffer;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Remove a buffer from a chain
  * @param	chain: pointer to the buffer chain
  * @param	buffer: pointer to the buffer to add to the chain
  * @retval	None
  */
void BCHAIN_ChainRemoveHead(BCHAIN_Chain_td *chain)
{
	BCHAIN_Buffer_td *buff = BCHAIN_CHAIN_HEAD(chain);
	assert(buff != NULL);
	chain->buffer = buff->next;
	buff->next = NULL;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Add a chain to the tail of a chain. If the buffer length is 0 the
  * 		buffer offset will be contiguous with the previous buffer offset in
  * 		the chain
  * @param	chain: pointer to the buffer chain
  * @param	addChain: pointer to the chain of buffers to add to the end
  * @retval	None
  */
void BCHAIN_ChainAddChainTail(BCHAIN_Chain_td *chain, BCHAIN_Chain_td *addChain)
{
	BCHAIN_Buffer_td *buff = BCHAIN_CHAIN_HEAD(addChain);
	while(buff != NULL)
	{
		BCHAIN_Buffer_td *next = buff->next;
		BCHAIN_ChainRemoveHead(addChain);
		BCHAIN_ChainAddTail(chain, buff);
		buff = next;
	}
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Get the tail buffer in a chain
  * @param	chain: pointer to the buffer chain
  * @retval	Buffer at the end of the chain
  */
BCHAIN_Buffer_td* BCHAIN_GetChainTail(BCHAIN_Chain_td *chain)
{
	BCHAIN_Buffer_td *tail = chain->buffer;
	while((tail != NULL) && (tail->next != NULL))
		tail = tail->next;
	return tail;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Remove all contiguous buffers below or unreachable by offset
  * @param	chain: pointer to the buffer chain
  * @param	offset: offset from which to check
  * @param	removedChain: pointer to the chain of buffers removed
  * @retval	None
  */
void BCHAIN_GetChainBuffersApplicableToOffset(BCHAIN_Chain_td *chain, uint32_t offset, BCHAIN_Chain_td *removedChain)
{
	removedChain->buffer = NULL;

	BCHAIN_Buffer_td *buffer = BCHAIN_CHAIN_HEAD(chain);
	while(buffer != NULL)
	{
		BCHAIN_Buffer_td *nextChunk = buffer->next;
		if((offset >= buffer->offset) && (offset < (buffer->offset + buffer->length)))
			return;

		BCHAIN_ChainRemoveHead(chain);
		BCHAIN_ChainAddTail(removedChain, buffer);

		buffer = nextChunk;
	}
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Get all contiguous buffers loaded with data
  * @param	chain: pointer to the buffer chain
  * @param	removedChain: pointer to the chain of buffers removed
  * @param	flags: @ref BCHAIN_FLAGS
  * @retval	None
  */
void BCHAIN_GetLoadedChainBuffers(BCHAIN_Chain_td *chain, BCHAIN_Chain_td *removedChain, uint8_t flags)
{
	removedChain->buffer = NULL;

	BCHAIN_Buffer_td *buffer = BCHAIN_CHAIN_HEAD(chain);
	uint32_t offset = 0;
	while(buffer != NULL)
	{
		BCHAIN_Buffer_td *nextChunk = buffer->next;
		if(buffer->length == 0)
			return;

		offset += buffer->length;
		if((buffer->length != sizeof(buffer->data)) && (flags & BCHAIN_FLAG_ACCEPTPARTIALBUFFERS))
			return;

		BCHAIN_ChainRemoveHead(chain);
		BCHAIN_ChainAddTail(removedChain, buffer);

		buffer = nextChunk;
	}
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Read chain data from offset
  * @param	chain: pointer to the buffer chain
  * @param	offset: offset from which to read
  * @param	data: pointer to the buffer into which to read
  * @param	length: amount of data to read
  * @retval	BCHAIN_StatusEnum
  */
BCHAIN_StatusEnum BCHAIN_ReadChainData(BCHAIN_Chain_td *chain, uint32_t offset, uint8_t *data, uint32_t length)
{
	uint32_t localOffset = 0;
	BCHAIN_Buffer_td *buffer = BCHAIN_CHAIN_HEAD(chain);
	while((buffer != NULL) && (localOffset < length))
	{
		if(((offset + localOffset) >= buffer->offset) && ((offset + localOffset) < (buffer->offset + buffer->length)))
		{
			uint32_t chunkOffset = (offset + localOffset) - buffer->offset;
			uint32_t chunkLength = (buffer->length - chunkOffset);
			if(chunkLength > (length - localOffset))
				chunkLength = (length - localOffset);
			memcpy(&data[localOffset], &buffer->data[chunkOffset], chunkLength);
			localOffset+= chunkLength;
		}
		buffer = buffer->next;
	}
	return BCHAIN_OK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	write chain data from offset
  * @param	chain: pointer to the buffer chain
  * @param	offset: offset from which to write
  * @param	data: pointer to the buffer from which to write
  * @param	length: amount of data to write
  * @retval	BCHAIN_StatusEnum
  */
BCHAIN_StatusEnum BCHAIN_WriteChainData(BCHAIN_Chain_td *chain, uint32_t offset, uint8_t *data, uint32_t length)
{
	uint32_t localOffset = 0;
	BCHAIN_Buffer_td *buffer = BCHAIN_CHAIN_HEAD(chain);
	while((buffer != NULL) && (localOffset < length))
	{
		if(((offset + localOffset) >= buffer->offset) && ((offset + localOffset) < (buffer->offset + sizeof(buffer->data))))
		{
			uint32_t chunkOffset = (offset + localOffset) - buffer->offset;
			uint32_t chunkLength = (sizeof(buffer->data) - chunkOffset);
			if(chunkLength > (length - localOffset))
				chunkLength = (length - localOffset);
			memcpy(&buffer->data[chunkOffset], &data[localOffset], chunkLength);
			buffer->length += chunkLength;
			localOffset+= chunkLength;
		}
		buffer = buffer->next;
	}
	return BCHAIN_OK;
}

/*----------------------------------------------------------------------------*/
/**
  * @brief	Reset the chain of buffers to 0 length with contiguous offsets
  * @param	chain: pointer to the buffer chain
  * @param	offset: offset from which to write
  * @retval	void
  */
void BCHAIN_ResetChain(BCHAIN_Chain_td *chain, uint32_t offset)
{
	BCHAIN_Buffer_td *buffer = BCHAIN_CHAIN_HEAD(chain);
	while(buffer != NULL)
	{
		buffer->offset = offset;
		offset += (sizeof(buffer->data));
		buffer->length = 0;
		buffer = buffer->next;
	}
}
