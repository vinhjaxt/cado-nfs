#ifndef UTILS_SIEVE_H
#define UTILS_SIEVE_H 

#include <stdint.h>
#include "cado.h"
#include "utils.h"
#include "int64_vector.h"
#include "double_vector.h"
#include "mat_int64.h"
#include "array.h"
#include "ideal.h"

/*
 * List of int64_vector.
 */
typedef struct
{
  int64_vector_t * v;
  unsigned int length;
}  s_list_int64_vector_t;

typedef s_list_int64_vector_t list_int64_vector_t[1];
typedef s_list_int64_vector_t * list_int64_vector_ptr;
typedef const s_list_int64_vector_t * list_int64_vector_srcptr;

void list_int64_vector_init(list_int64_vector_ptr SV);

void list_int64_vector_add_int64_vector(list_int64_vector_ptr SV, int64_vector_srcptr v);

void list_int64_vector_clear(list_int64_vector_ptr SV);

void list_int64_vector_fprintf(FILE * file, list_int64_vector_srcptr SV);

/*
 * Extract all the vector (in column) of the matrix and put them in a list.
 *
 * list: the list of vector.
 * matrix: the matrix.
 */
void list_int64_vector_extract_mat_int64(list_int64_vector_ptr list,
    mat_int64_srcptr matrix);

/*
 * List of double vector.
 */

typedef struct
{
  double_vector_t * v;
  unsigned int length;
}  s_list_double_vector_t;

typedef s_list_double_vector_t list_double_vector_t[1];
typedef s_list_double_vector_t * list_double_vector_ptr;
typedef const s_list_double_vector_t * list_double_vector_srcptr;

void list_double_vector_init(list_double_vector_ptr SV);

void list_double_vector_add_double_vector(list_double_vector_ptr SV, double_vector_srcptr v);

void list_double_vector_clear(list_double_vector_ptr SV);

void list_double_vector_fprintf(FILE * file, list_double_vector_srcptr SV);

/*
 * Extract all the vector (in column) of the matrix and put them in a list.
 *
 * list: the list of vector.
 * matrix: the matrix.
 */
void list_double_vector_extract_mat_int64(list_double_vector_ptr list,
    mat_int64_srcptr matrix);

/*
 * Utils.
 */

/*
 * Compute the acute Gauss reduction of two vector (v0_root and v1_root) if they
 *  are in the same plane. Return the determinant of U if U is not set as NULL,
 *  0 otherwise.
 *
 * v0: the first output vector.
 * v1: the second output vector.
 * U: unimodular matrix to go from the two first coordinate of v0_root and
 *  v1_root to the two first coordinate of v0 and v1. (v0_root, v1_root) * U =
 *  (v0, v1).
 * v0_root: the first input vector.
 * v1_root: the second input vector.
 */
int gauss_reduction(int64_vector_ptr v0, int64_vector_ptr v1, mat_int64_ptr U,
    int64_vector_srcptr v0_root, int64_vector_srcptr v1_root);

/*
 * Do the same thing as gauss_reduction, but set all of the other two first
 * coordinates of v0 and v1 to zero.
 */
int gauss_reduction_zero(int64_vector_ptr v0, int64_vector_ptr v1,
    mat_int64_ptr U, int64_vector_srcptr v0_root, int64_vector_srcptr v1_root);

/*
 * Use the Franke-Kleinjung to modify the two first coordinates of v0_root and
 *  v1_root, if they respect the conditions. See sieve/las-plattice.h
 *  (03/31/2015). Return 1 if the reduction is done, 0 otherwise.
 *
 * v0: the output first vector.
 * v1: the output second vector.
 * v0_root: the initial first vector.
 * v1_root: the initial second vector
 * I: minimal gap between the two x coordinate of v0 and v1. Usually, I = 2*H0.
 */
int reduce_qlattice(int64_vector_ptr v0, int64_vector_ptr v1,
    int64_vector_srcptr v0_root, int64_vector_srcptr v1_root, int64_t I);

/*
 * Same thing as reduce_qlattice above, but set the other two first coordinates
 *  to zero for v0 and v1.
 */
int reduce_qlattice_zero(int64_vector_ptr v0, int64_vector_ptr v1,
                         int64_vector_srcptr v0_root,
                         int64_vector_srcptr v1_root, int64_t I);

/*
 * The SV4 (for instance, is just SV3 but I keep the name for instance) to find
 *  some short vector in the lattice defined by (v0_root, v1_root, v2)  with a
 *  z coordinate equal to 1. Very dependent on the form of the vector: v0 = (a,
 *  b, 0, …, 0), v1 = (c, d, 0, …, 0) and v2 = (e, 0, 1, 0, …, 0).
 *
 * SV: the list of short vector close to v2.
 * v0_root, v1_root, v2: three vectors that gives a basis of a lattice.
 */
void SV4(list_int64_vector_ptr SV, int64_vector_srcptr v0_root,
         int64_vector_srcptr v1_root, int64_vector_srcptr v2);

/*
 * Same thing as SV4, but the basis vector is in a Mqr matrix (vector in
 * columns).
 */
void SV4_Mqr(list_int64_vector_ptr SV, mat_int64_srcptr Mqr);

/*
 * Enumerate with the Franke-Kleinjung algorithm for x increasing. v->c[0]
 *  is in [A, A + I[.
 *
 * v: v_old + l v0 + m v1, (l, m) in [0, 1]^2. v->c[0] > v_old->c[0].
 * v_old: the starting point vector.
 * v0: a vector given by reduce_qlattice, v0->c[0] < 0.
 * v1: a vector given by reduce_qlattice, v1->c[0] > 0.
 * A: how the interval is centered.
 * I: the width of the interval.
 */
unsigned int enum_pos_with_FK(int64_vector_ptr v, int64_vector_srcptr v_old,
    int64_vector_srcptr v0, int64_vector_srcptr v1, int64_t A, int64_t I);

/*
 * Enumerate with the same technique as in Franke-Kleinjung algorithm but for x
 *  decreasing. v->c[0] is in ]-A - I, -A].
 *
 * v: v_old + l v0 + m v1, (l, m) in [0, 1]^2. v->c[0] < v_old->c[0].
 * v_old: the starting point vector.
 * v0: a vector given by reduce_qlattice, v0->c[0] < 0.
 * v1: a vector given by reduce_qlattice, v1->c[0] > 0.
 * A: how the interval is centered.
 * I: the width of the interval.
 */
unsigned int enum_neg_with_FK(int64_vector_ptr v, int64_vector_srcptr v_old,
    int64_vector_srcptr v0, int64_vector_srcptr v1, int64_t A, int64_t I);

/*
 * Add an FK vector (e0 or e1) if v is outside of the sieving region defined by
 * H to have the x coordinate of v in [-H0, H0[.
 *
 * v: current vector.
 * e0: a vector given by the Franke-Kleinjung algorithm.
 * e1: a vector given by the Franke-Kleinjung algorithm.
 * H: sieving bound.
 */
void add_FK_vector(int64_vector_ptr v, int64_vector_srcptr e0,
    int64_vector_srcptr e1, sieving_bound_srcptr H);

unsigned int find_min_x(list_int64_vector_srcptr SV, sieving_bound_srcptr H,
    unsigned char * stamp, unsigned char val_stamp);

void add_FK_vector(int64_vector_ptr v, int64_vector_srcptr e0,
    int64_vector_srcptr e1, sieving_bound_srcptr H);


void coordinate_FK_vector(uint64_t * coord_v0, uint64_t * coord_v1,
    int64_vector_srcptr v0, int64_vector_srcptr v1, sieving_bound_srcptr H,
    uint64_t number_element);

void plane_sieve_next_plane(int64_vector_ptr vs, list_int64_vector_srcptr SV,
    int64_vector_srcptr e0, int64_vector_srcptr e1, sieving_bound_srcptr H);
#endif // UTILS_SIEVE_H
