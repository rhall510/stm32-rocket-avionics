#ifndef MISC_H_
#define MISC_H_

#include "stm32g4xx.h"

inline uint32_t GetMicros() {
	return TIM2->CNT;
}


#endif /* MISC_H_ */
