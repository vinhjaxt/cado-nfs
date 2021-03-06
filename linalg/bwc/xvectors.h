#ifndef XVECTORS_H_
#define XVECTORS_H_

#include <stdint.h>
#include "parallelizing_info.h"
#include "balancing.h"

#ifdef __cplusplus
extern "C" {
#endif

void setup_x_random(uint32_t * xs,
        unsigned int m, unsigned int nx, unsigned int nr,
        parallelizing_info_ptr pi);
void load_x(uint32_t * xs, unsigned int m, unsigned int nx,
        parallelizing_info_ptr pi, balancing_ptr bal);
void save_x(uint32_t * xs, unsigned int m, unsigned int nx,
        parallelizing_info_ptr pi, balancing_ptr bal);

#ifdef __cplusplus
}
#endif

#endif	/* XVECTORS_H_ */
