#ifndef MATMUL_THREADED_H_
#define MATMUL_THREADED_H_

#include "matmul.h"

#ifdef __cplusplus
extern "C" {
#endif

struct matmul_threaded_data_s;

extern struct matmul_threaded_data_s * matmul_threaded_init(abobj_ptr, param_list pl, int);
extern void matmul_threaded_build_cache(struct matmul_threaded_data_s *, uint32_t *);
extern int matmul_threaded_reload_cache(struct matmul_threaded_data_s *);
extern void matmul_threaded_save_cache(struct matmul_threaded_data_s *);
extern void matmul_threaded_mul(struct matmul_threaded_data_s *, abt *, abt const *, int);
extern void matmul_threaded_report(struct matmul_threaded_data_s *, double);
extern void matmul_threaded_clear(struct matmul_threaded_data_s * mm);
extern void matmul_threaded_aux(struct matmul_threaded_data_s * mm, int op, ...);
extern void matmul_threaded_auxv(struct matmul_threaded_data_s * mm, int op, va_list ap);

#ifdef __cplusplus
}
#endif

#endif	/* MATMUL_THREADED_H_ */
