/*
 * check.h
 *
 *  Created on: 2023��10��26��
 *      Author: liuyaohui
 */

#ifndef SRC_CHECK_H_
#define SRC_CHECK_H_

#include <stdint.h>

typedef enum {
	right=0,
	error=1,
}CheckResult;

CheckResult CheckOutAll(uint32_t *write,uint32_t *read,uint32_t num);
CheckResult CheckOut_HeadTail(uint32_t *frist,uint32_t *last,uint32_t blocksize);
#endif /* SRC_CHECK_H_ */
