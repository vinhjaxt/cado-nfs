#ifndef _EC_ARITH_EDWARDS_H_
#define _EC_ARITH_EDWARDS_H_

#include "ec_arith_common.h"

#ifdef ECM_COUNT_OPS
static unsigned int _count_edwards_add, _count_edwards_dbl, _count_edwards_tpl;
static int _count_edwards_extraM;
#define EDWARDS_COUNT_OPS_M _count_edwards_add*8 + _count_edwards_dbl*7 \
                            + _count_edwards_tpl*12 + _count_edwards_extraM
#define EDWARDS_COUNT_OPS_RESET() do { _count_edwards_extraM = 0;           \
      _count_edwards_add = _count_edwards_dbl = _count_edwards_tpl = 0;    \
    } while (0)
#endif

/* #define SAFE_TWISTED_EDWARDS_TO_MONTGOMERY */

/* Compute d = -(A-2)/(A+2). A and d can be the same variable. */
static inline void
edwards_d_from_montgomery_A (residue_t d, const residue_t A, const modulus_t m)
{
  residue_t (t);
  mod_init_noset0 (t, m);

  mod_add_ul (t, A, 2UL, m);
  mod_inv (d, t, m);
  mod_sub_ul (t, t, 4UL, m); /* t = A-2 */
  mod_mul (d, d, t, m);
  mod_neg (d, d, m);

  mod_clear (t, m);
}

static inline void
edwards_neg (ec_point_t Q, const ec_point_t P, const modulus_t m)
{
  mod_neg (Q->x, P->x, m);
  mod_set (Q->y, P->y, m);
  mod_set (Q->z, P->z, m);
  mod_neg (Q->t, P->t, m);
}

/* edwards_addsub (R:output_flag, P:edwards_ext, Q:edwards_ext, sub,output_flag)
 *     R <- P+Q      if sub == 0
 *     R <- P-Q      if sub != 0
 *     output_flag can be edwards_proj, edwards_ext or montgomery
 * R can be the same variable as P or Q.
 * All coordinates of the output point R can be modified (because they may be
 * used as temporary variables).
 * Cost:
 *    7M + 8add + 2*2         if output_flag == TWISTED_EDWARDS_proj
 *    8M + 8add + 2*2         if output_flag == TWISTED_EDWARDS_ext
 *    4M + 10add + 2*2        if output_flag == TWISTED_EDWARDS_ext
 * Notations in the comments come from:
 *    https://hyperelliptic.org/EFD/g1p/auto-twisted-extended-1.html#addition-add-2008-hwcd-4
 * Source: Hisil–Wong–Carter–Dawson, 2008, section 3.2 of
 *    http://eprint.iacr.org/2008/522
 */
static inline void
edwards_addsub (ec_point_t R, const ec_point_t P, const ec_point_t Q, int sub,
                const modulus_t m, const ec_point_coord_type_t output_type)
{
  ASSERT_EXPENSIVE (output_flag == TWISTED_EDWARDS_ext ||
                    output_flag == TWISTED_EDWARDS_proj ||
                    output_flag == MONTGOMERY_xz);

#ifdef ECM_COUNT_OPS
  _count_edwards_add++;
  if (output_type == TWISTED_EDWARDS_ext)
    _count_edwards_extraM++;
  else if (output_type == MONTGOMERY_xz)
    _count_edwards_extraM -= 4;
#endif

  residue_t u0, u1, u2;

  mod_init_noset0 (u0, m);
  mod_init_noset0 (u1, m);
  mod_init_noset0 (u2, m);

  if (sub)
  {
    mod_add (u1, Q->y, Q->x, m);    /* u1 <-      (Y2+X2) */
    mod_sub (u0, Q->y, Q->x, m);    /* u0 <-      (Y2-X2) */
  }
  else
  {
    mod_add (u0, Q->y, Q->x, m);    /* u0 <-      (Y2+X2) */
    mod_sub (u1, Q->y, Q->x, m);    /* u1 <-      (Y2-X2) */
  }

  mod_sub (u2, P->y, P->x, m);      /* u2 <-      (Y1-X1) */
  mod_mul (u0, u0, u2, m);          /* u0 <- A := (Y1-X1)*(Y2+/-X2) */
  mod_add (u2, P->y, P->x, m);      /* u2 <-      (X1+Y1) */
  mod_mul (u1, u1, u2, m);          /* u1 <- B := (Y1+X1)*(Y2-/+X2) */
  mod_sub (R->x, u1, u0, m);        /* Rx <- F := B-A */
  mod_add (R->y, u1, u0, m);        /* Ry <- G := B+A */

  mod_add (u1, P->z, P->z, m);      /* u1 <-      2*Z1 */
  mod_mul (u1, u1, Q->t, m);        /* u1 <- C := 2*Z1*T2 */
  mod_add (u2, Q->z, Q->z, m);      /* u2 <-      2*Z2 */
  mod_mul (u2, u2, P->t, m);        /* u2 <- D := 2*T1*Z2 */
  if (sub)
  {
    mod_add (u0, u2, u1, m);        /* u0 <- H := D+C */
    mod_sub (u1, u2, u1, m);        /* u1 <- E := D-C */
  }
  else
  {
    mod_sub (u0, u2, u1, m);        /* u0 <- H := D-C */
    mod_add (u1, u2, u1, m);        /* u1 <- E := D+C */
  }


  if (output_type == TWISTED_EDWARDS_ext || output_type == TWISTED_EDWARDS_proj)
  {
    mod_mul (R->z, R->x, R->y, m);  /* Rz <- Z3 := F*G */
    mod_mul (R->x, R->x, u1, m);    /* Rx <- X3 := E*F */
    mod_mul (R->y, R->y, u0, m);    /* Ry <- Y3 := G*H */
    if (output_type == TWISTED_EDWARDS_ext)
      mod_mul (R->t, u0, u1, m);    /* Rt <- T3 := E*H */
  }
  else /* if (output_type == MONTGOMERY_xz) */
  {
#ifdef SAFE_TWISTED_EDWARDS_TO_MONTGOMERY
    mod_sub (R->z, R->x, u0, m);    /* Rz <-       F-H */
    mod_mul (R->z, R->z, R->y, m);  /* Rz <-       G*(F-H) */
    mod_add (R->x, R->x, u0, m);    /* Rx <-       F+H */
    mod_mul (R->x, R->x, R->y, m);  /* Rx <-       G*(F+H) */
#else
    /* CAUTION! */
    /* This may produce unstable results */
    /* But seems to "work" for our purpose */
    /* TODO: COMMENTS */
    /* Edwards proj -> Montgomery proj
     * (X:Y:Z) -> (X(Z+Y) : Z(Z+Y) : X(Z-Y))
     *                        [ work for (X:Y:Z) != point of order 2 (0:-1:1) ]
     *  drop Y
     *         -> (X(Z+Y) :: X(Z-Y))
     *                        [ work for (X:Y:Z) != point of order 2 (0:-1:1) ]
     *  divides by X
     *         -> (Z+Y :: (Z-Y))
     *                        [ work for (X:Y:Z) != point at infinity (0:1:1) ]
     *        -> (GF+GH :: GF-GH)
     * divides by G           [ G = 0 => Z = Y = 0 = > (X:Y:Z) = (1:0:0) ]
     *        -> (F+H :: F-H)
     */
    mod_sub (R->z, R->x, u0, m);    /* Rz <-       F-H */
    mod_add (R->x, R->x, u0, m);    /* Rx <-       F+H */
#endif
  }

  mod_clear (u0, m);
  mod_clear (u1, m);
  mod_clear (u2, m);
}

static inline void
edwards_add (ec_point_t R, const ec_point_t P, const ec_point_t Q,
             const modulus_t m, const ec_point_coord_type_t output_type)
{
  edwards_addsub (R, P, Q, 0, m, output_type);
}

static inline void
edwards_sub (ec_point_t R, const ec_point_t P, const ec_point_t Q,
             const modulus_t m, const ec_point_coord_type_t output_type)
{
  edwards_addsub (R, P, Q, 1, m, output_type);
}

/* edwards_dbl (R:output_flag, P:edwards_proj, output_flag)
 *     R <- 2*P
 *     output_flag can be edwards_proj or edwards_ext
 * R can be the same variable as P.
 * All coordinates of the output point R can be modified (because they may be
 * used as temporary variables).
 * Cost:
 *    3M + 4S + 5add + 1*2         if output_flag == TWISTED_EDWARDS_proj
 *    4M + 4S + 5add + 1*2         if output_flag == TWISTED_EDWARDS_ext
 * Notations in the comments come from:
 *    https://hyperelliptic.org/EFD/g1p/auto-twisted-projective.html#doubling-dbl-2008-bbjlp
 * Source: Bernstein–Birkner–Joye–Lange–Peters, 2008
 *    http://eprint.iacr.org/2008/013.
 */
static inline void
edwards_dbl (ec_point_t R, const ec_point_t P,
             const modulus_t m, const ec_point_coord_type_t output_type)
{
  ASSERT_EXPENSIVE (output_flag == TWISTED_EDWARDS_ext ||
                    output_flag == TWISTED_EDWARDS_proj);

#ifdef ECM_COUNT_OPS
  _count_edwards_dbl++;
  if (output_type == TWISTED_EDWARDS_ext)
    _count_edwards_extraM++;
#endif

  residue_t u0, u1;

  mod_init_noset0 (u0, m);
  mod_init_noset0 (u1, m);

  mod_sqr (u0, P->x, m);            /* u0 <-  C := X1^2 */
  mod_sqr (u1, P->y, m);            /* u1 <-  D := Y1^2 */
  mod_add (R->x, P->x, P->y, m);    /* Rx <-       X1+Y1 */
  mod_sqr (R->x, R->x, m);          /* Rx <-  B := (X1+Y1)^2 */
  mod_add (R->y, u0, u1, m);        /* Ry <-       C+D */
  mod_sub (u0, u0, u1, m);          /* u0 <- -F := C-D */
  mod_sub (R->x, R->y, R->x, m);    /* Rx <-    := C+D-B */

  mod_sqr (u1, P->z, m);            /* u1 <-  H := Z1^2 */
  mod_add (u1, u1, u1, m);          /* u1 <-    := 2*H  */
  mod_add (u1, u0, u1, m);          /* u1 <- -J := -F + 2*H */

  if (output_type == TWISTED_EDWARDS_ext)
    mod_mul(R->t, R->x, R->y, m);   /* Rt <- T3 := (B-C-D)*(-C-D) */
  mod_mul (R->x, R->x, u1, m);      /* Rx <- X3 := (B-C-D) * J */
  mod_mul (R->y, R->y, u0, m);      /* Ry <- Y3 := F*(-C-D) */
  mod_mul (R->z, u0, u1, m);        /* Rz <- Z3 := F * J */

  mod_clear (u0, m);
  mod_clear (u1, m);
}


/* edwards_tpl (R:output_flag, P:edwards_proj, output_flag)
 *     R <- 3*P
 *     output_flag can be edwards_proj or edwards_ext
 * R can be the same variable as P.
 * All coordinates of the output point R can be modified (because they may be
 * used as temporary variables).
 * Cost:
 *     9M + 3S + 7add + 2*2 + 1*-1      if output_flag == TWISTED_EDWARDS_proj
 *    11M + 3S + 7add + 2*2 + 1*-1      if output_flag == TWISTED_EDWARDS_ext
 * Notations in the comments come from:
 *    https://hyperelliptic.org/EFD/g1p/auto-twisted-extended-1.html#tripling-tpl-2015-c
 * Source: 2015 Chuengsatiansup.
 */
static inline void
edwards_tpl (ec_point_t R, const ec_point_t P,
             const modulus_t m, const ec_point_coord_type_t output_type)
{
  ASSERT_EXPENSIVE (output_flag == TWISTED_EDWARDS_ext ||
                    output_flag == TWISTED_EDWARDS_proj);

#ifdef ECM_COUNT_OPS
  _count_edwards_tpl++;
  if (output_type == TWISTED_EDWARDS_ext)
    _count_edwards_extraM += 2;
#endif

  residue_t u0, u1, u2, u3;

  mod_init_noset0 (u0, m);
  mod_init_noset0 (u1, m);
  mod_init_noset0 (u2, m);
  mod_init_noset0 (u3, m);

  mod_sqr (u0, P->x, m);            /* u0 <-     := X1^2 */
  mod_neg (u0, u0, m);              /* u0 <- aXX := -X1^2 */
  mod_sqr (u1, P->y, m);            /* u1 <-  YY := Y1^2 */
  mod_add (u2, u1, u0, m);          /* u2 <-  Ap := YY+aXX */
  mod_sub (u3, u1, u0, m);          /* u3 <-        YY-aXX */
  mod_sqr (R->t, P->z, m);          /* Rt <-        Z1^2 */
  mod_add (R->t, R->t, R->t, m);    /* Rt <-        2*Z1^2 */
  mod_sub (R->t, R->t, u2, m);      /* Rt <-        2*Z1^2-Ap */
  mod_add (R->t, R->t, R->t, m);    /* Rt <-   B := 2*(2*Z1^2-Ap) */
  mod_mul (u2, u2, u3, m);          /* u2 <-  AA := Ap*(YY-aXX) */
  mod_mul (u0, u0, R->t, m);        /* u0 <-  xB := aXX*B */
  mod_mul (u1, u1, R->t, m);        /* u1 <-  yB := YY*B */
  mod_sub (R->t, u2, u1, m);        /* Rt <-   F := AA-yB */
  mod_add (u1, u1, u2, m);          /* u1 <-        yB+AA */
  mod_add (u3, u2, u0, m);          /* u3 <-   G := AA+xB */
  mod_sub (u2, u0, u2, m);          /* u2 <-        xB-AA */
  mod_mul (u1, P->x, u1, m);        /* u1 <-  xE := X1*(yB+AA) */
  mod_mul (u2, P->y, u2, m);        /* u2 <-  yH := Y1*(xB-AA) */
  mod_mul (u0, P->z, R->t, m);      /* u0 <-  zF := Z1*F */

  if (output_type == TWISTED_EDWARDS_proj)
  {
    mod_mul (R->x, u1, R->t, m);    /* Rx <-  X3 := xE*F */
    mod_mul (R->y, u2, u3, m);      /* Ry <-  Y3 := yH*G */
    mod_mul (R->z, u0, u3, m);      /* Rz <-  Z3 := zF*G */
  }
  else /* if (output_type == TWISTED_EDWARDS_ext) */
  {
    mod_mul (u3, P->z, u3, m);      /* u3 <-  zG := Z1*G */
    mod_mul (R->x, u1, u0, m);      /* Rx <-  X3 := xE*zF */
    mod_mul (R->y, u2, u3, m);      /* Ry <-  Y3 := yH*zG */
    mod_mul (R->z, u0, u3, m);      /* Rz <-  Z3 := zF*zG */
    mod_mul (R->t, u1, u2, m);      /* Rt <- T3 := xE*yH */
  }

  mod_clear (u0, m);
  mod_clear (u1, m);
  mod_clear (u2, m);
  mod_clear (u3, m);
}


/* ------------------------------------------------------------------------- */

/* Computes R = [e]P (mod m)  */
MAYBE_UNUSED
static inline void
edwards_smul_ui (ec_point_t R, const ec_point_t P, const unsigned long e,
                 const modulus_t m)
{
  unsigned long j;
  long k;
  ec_point_t T, Pe;

  if (e == 0UL)
    {
      mod_set0 (R->x, m);
      mod_set1 (R->y, m);
      mod_set1 (R->z, m);
      return;
    }

  if (e == 1UL)
    {
      ec_point_set (R, P, m);
      return;
    }

  if (e == 2UL)
    {
      edwards_dbl (R, P, m, TWISTED_EDWARDS_ext);
      return;
    }

  if (e == 4UL)
    {
      edwards_dbl (R, P, m, TWISTED_EDWARDS_ext);
      edwards_dbl (R, R, m, TWISTED_EDWARDS_ext);
      return;
    }

  ec_point_init (T, m);
  ec_point_init (Pe, m);
  ec_point_set (Pe, P, m);

  /* basic double-and-add */

  mod_set0 (T->x, m);
  mod_set0 (T->t, m);
  mod_set1 (T->y, m);
  mod_set1 (T->z, m);

  k = CHAR_BIT * sizeof(e) - 1;
  j = (1UL << k);

  while(k-- >= 0)
    {
      edwards_dbl (T, T, m, TWISTED_EDWARDS_ext);
      if (j & e)
        edwards_add (T, T, Pe, m, TWISTED_EDWARDS_ext);
      j >>= 1;
    }

  ec_point_set (R, T, m);

  ec_point_clear (T, m);
  ec_point_clear (Pe, m);
}

#endif /* _EC_ARITH_EDWARDS_H_ */