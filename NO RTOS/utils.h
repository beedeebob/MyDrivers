/*
 * utils.h
 *
 *  Created on: Jun 24, 2024
 *      Author: ben-linux
 */

#ifndef INC_UTILS_H_
#define INC_UTILS_H_

/* Includes ------------------------------------------------------------------*/
/* Public typedef ------------------------------------------------------------*/
/* Public define -------------------------------------------------------------*/
/* Public macro --------------------------------------------------------------*/
#define BYTESTOUINT16(ARR, IDX)						((ARR)[IDX] + ((ARR)[IDX + 1] << 8))
#define BYTESTOUINT24(ARR, IDX)						((ARR)[IDX] + ((ARR)[IDX + 1] << 8) + ((ARR)[IDX + 2] << 16))
#define BYTESTOUINT32(ARR, IDX)						((ARR)[IDX] + ((ARR)[IDX + 1] << 8) + ((ARR)[IDX + 2] << 16) + ((ARR)[IDX + 3] << 24))
#define BYTESTOUINT16BIGENDIAN(ARR, IDX)			((ARR)[IDX + 1] + ((ARR)[IDX] << 8))
#define BYTESTOUINT24BIGENDIAN(ARR, IDX)			((ARR)[IDX + 2] + ((ARR)[IDX + 1] << 8) + ((ARR)[IDX] << 16))
#define BYTESTOUINT32BIGENDIAN(ARR, IDX)			((ARR)[IDX + 3] + ((ARR)[IDX + 2] << 8) + ((ARR)[IDX + 1] << 16) + ((ARR)[IDX] << 24))

/* Public variables ----------------------------------------------------------*/
/* Public function prototypes ------------------------------------------------*/


#endif /* INC_UTILS_H_ */
