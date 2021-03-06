/* This file defines some functions that work more or less the same 
   with mod_ul.h and modredc_ul.h. I.e. mod_div3() and mod_gcd() work
   unchanged with plain and Montgomery representation (so we can work on 
   the stored residue directly, whatever its representation is); 
   mod_jacobi() converts to plain "unsigned long" first, the others use 
   only mod_*() inline functions.
   Speed-critical functions need to be rewritten in assembly for REDC,
   but this is a start.
*/

int
mod_div3 (residue_t r, const residue_t a, const modulus_t m)
{
  const unsigned long a3 = a[0] % 3UL;
  unsigned long ml, m3;
  residue_t t;

  ASSERT_EXPENSIVE (a[0] < mod_getmod_ul (m));

  ml = mod_getmod_ul (m);
  m3 = ml % 3UL;
  if (m3 == 0)
    return 0;

  mod_init_noset0 (t, m);
  
  mod_set (t, a, m);
  if (a3 != 0UL)
    {
      if (a3 + m3 == 3UL) /* Hence a3 == 1, m3 == 2 or a3 == 2, m3 == 1 */
	t[0] = t[0] + ml;
      else /* a3 == 1, m3 == 1 or a3 == 2, m3 == 2 */
	t[0] = t[0] + 2UL * ml;
    }

  /* Now t[0] == a+k*m (mod 2^w) so that a+k*m is divisible by 3.
     (a+k*m)/3 < 2^w, so doing a division (mod 2^w) produces the 
     correct result. */
  
#if LONG_BIT == 32
    t[0] *= 0xaaaaaaabUL; /* 1/3 (mod 2^32) */
#elif LONG_BIT == 64 
    t[0] *= 0xaaaaaaaaaaaaaaabUL; /* 1/3 (mod 2^64) */
#else
#error LONG_BIT is neither 32 nor 64
#endif
  
#ifdef WANT_ASSERT_EXPENSIVE
  mod_sub (r, a, t, m);
  mod_sub (r, r, t, m);
  mod_sub (r, r, t, m);
  ASSERT_EXPENSIVE (mod_is0 (r, m));
#endif

  mod_set (r, t, m);
  mod_clear (t, m);

  return 1;
}


int
mod_div5 (residue_t r, const residue_t a, const modulus_t m)
{
  unsigned long ml, m5, k;
  residue_t t;
  const unsigned long a5 = a[0] % 5UL;
  const unsigned long inv5[5] = {0,4,2,3,1}; /* inv5[i] = -1/i (mod 5) */

  ASSERT_EXPENSIVE (a[0] < mod_getmod_ul (m));

  ml = mod_getmod_ul (m);
  m5 = ml % 5UL;
  if (m5 == 0)
    return 0;
      
  mod_init_noset0 (t, m);
  mod_set (t, a, m);
  if (a5 != 0UL)
    {
      /* We want a+km == 0 (mod 5), so k = -a*m^{-1} (mod 5) */
      k = (a5 * inv5[m5]) % 5UL;
      ASSERT_EXPENSIVE ((k*m5 + a5) % 5UL == 0UL);
      t[0] = a[0] + k * ml;
    }

  /* Now t[0] == a+k*m (mod 2^w) so that a+k*m is divisible by 5.
     (a+k*m)/5 < 2^w, so doing a division (mod 2^w) produces the 
     correct result. */
  
#if LONG_BIT == 32
    t[0] *= 0xcccccccdUL; /* 1/5 (mod 2^32) */
#elif LONG_BIT == 64 
    t[0] *= 0xcccccccccccccccdUL; /* 1/5 (mod 2^64) */
#else
#error LONG_BIT is neither 32 nor 64
#endif
  
#ifdef WANT_ASSERT_EXPENSIVE
  ASSERT_EXPENSIVE (t[0] < mod_getmod_ul (m));
  mod_sub (r, a, t, m);
  mod_sub (r, r, t, m);
  mod_sub (r, r, t, m);
  mod_sub (r, r, t, m);
  mod_sub (r, r, t, m);
  ASSERT_EXPENSIVE (mod_is0 (r, m));
#endif

  mod_set (r, t, m);
  mod_clear (t, m);
  
  return 1;
}


int
mod_div7 (residue_t r, const residue_t a, const modulus_t m)
{
  unsigned long ml, m7, k;
  residue_t t;
  const unsigned long a7 = a[0] % 7UL;
  const unsigned long inv7[7] = {0,6,3,2,5,4,1}; /* inv7[i] = -1/i (mod 7) */

  ASSERT_EXPENSIVE (a[0] < mod_getmod_ul (m));

  ml = mod_getmod_ul (m);
  m7 = ml % 7UL;
  if (m7 == 0)
    return 0;
  
  mod_init_noset0 (t, m);
  mod_set (t, a, m);
  if (a7 != 0UL)
    {
      /* We want a+km == 0 (mod 7), so k = -a*m^{-1} (mod 7) */
      k = (a7 * inv7[m7]) % 7UL;
      ASSERT_EXPENSIVE ((k*m7 + a7) % 7UL == 0UL);
      t[0] = a[0] + k * ml;
    }

  /* Now t[0] == a+k*m (mod 2^w) so that a+k*m is divisible by 7.
     (a+k*m)/7 < 2^w, so doing a division (mod 2^w) produces the 
     correct result. */
  
#if LONG_BIT == 32
    t[0] *= 0xb6db6db7UL; /* 1/7 (mod 2^32) */
#elif LONG_BIT == 64 
    t[0] *= 0x6db6db6db6db6db7UL; /* 1/7 (mod 2^64) */
#else
#error LONG_BIT is neither 32 nor 64
#endif
  
#ifdef WANT_ASSERT_EXPENSIVE
  ASSERT_EXPENSIVE (t[0] < mod_getmod_ul (m));
  mod_sub (r, a, t, m);
  mod_sub (r, r, t, m);
  mod_sub (r, r, t, m);
  mod_sub (r, r, t, m);
  mod_sub (r, r, t, m);
  mod_sub (r, r, t, m);
  mod_sub (r, r, t, m);
  ASSERT_EXPENSIVE (mod_is0 (r, m));
#endif

  mod_set (r, t, m);
  mod_clear (t, m);
  
  return 1;
}


int
mod_div11 (residue_t r, const residue_t a, const modulus_t m)
{
  unsigned long ml, m11, k;
  residue_t t;
  const unsigned long a11 = a[0] % 11UL;
  /* inv11[i] = -1/i (mod 11) */
  const unsigned long inv11[11] = {0, 10, 5, 7, 8, 2, 9, 3, 4, 6, 1}; 

  ASSERT_EXPENSIVE (a[0] < mod_getmod_ul (m));

  ml = mod_getmod_ul (m);
  m11 = ml % 11UL;
  if (m11 == 0)
    return 0;
  
  mod_init_noset0 (t, m);
  mod_set (t, a, m);
  if (a11 != 0UL)
    {
      /* We want a+km == 0 (mod 11), so k = -a*m^{-1} (mod 11) */
      k = (a11 * inv11[m11]) % 11UL;
      ASSERT_EXPENSIVE ((k*m11 + a11) % 11UL == 0UL);
      t[0] = a[0] + k * ml;
    }

  /* Now t[0] == a+k*m (mod 2^w) so that a+k*m is divisible by 11.
     (a+k*m)/11 < 2^w, so doing a division (mod 2^w) produces the 
     correct result. */
  
#if LONG_BIT == 32
    t[0] *= 0xba2e8ba3UL; /* 1/11 (mod 2^32) */
#elif LONG_BIT == 64 
    t[0] *= 0x2e8ba2e8ba2e8ba3UL; /* 1/11 (mod 2^64) */
#else
#error LONG_BIT is neither 32 nor 64
#endif
  
  mod_set (r, t, m);
  mod_clear (t, m);

  return 1;
}


int
mod_div13 (residue_t r, const residue_t a, const modulus_t m)
{
  unsigned long ml, m13, k;
  residue_t t;
  const unsigned long a13 = a[0] % 13UL;
  /* inv13[i] = -1/i (mod 13) */
  const unsigned long inv13[13] = {0, 12, 6, 4, 3, 5, 2, 11, 8, 10, 9, 7, 1}; 

  ASSERT_EXPENSIVE (a[0] < mod_getmod_ul (m));

  ml = mod_getmod_ul (m);
  m13 = ml % 13UL;
  if (m13 == 0)
    return 0;
  
  mod_init_noset0 (t, m);
  mod_set (t, a, m);
  if (a13 != 0UL)
    {
      /* We want a+km == 0 (mod 13), so k = -a*m^{-1} (mod 13) */
      k = (a13 * inv13[m13]) % 13UL;
      ASSERT_EXPENSIVE ((k*m13 + a13) % 13UL == 0UL);
      t[0] = a[0] + k * ml;
    }

  /* Now t[0] == a+k*m (mod 2^w) so that a+k*m is divisible by 13.
     (a+k*m)/13 < 2^w, so doing a division (mod 2^w) produces the 
     correct result. */
  
#if LONG_BIT == 32
    t[0] *= 0xc4ec4ec5UL; /* 1/13 (mod 2^32) */
#elif LONG_BIT == 64 
    t[0] *= 0x4ec4ec4ec4ec4ec5UL; /* 1/13 (mod 2^64) */
#else
#error LONG_BIT is neither 32 nor 64
#endif
  
  mod_set (r, t, m);
  mod_clear (t, m);

  return 1;
}


void 
mod_gcd (modint_t g, const residue_t r, const modulus_t m)
{
  unsigned long a, b, t;

  a = r[0]; /* This works the same for "a" in plain or Montgomery 
               representation */
  b = mod_getmod_ul (m);
  /* ASSERT (a < b); Should we require this? */
  ASSERT (b > 0UL);
  
  if (a >= b)
    a %= b;

  while (a > 0UL)
    {
      /* Here 0 < a < b */
      t = b % a;
      b = a;
      a = t;
    }

  g[0] = b;
}


/* Compute r = b^e. Here, e is an unsigned long */
void
mod_pow_ul (residue_t r, const residue_t b, const unsigned long e, 
            const modulus_t m)
{
  unsigned long mask;
  residue_t t;
#ifndef NDEBUG
  unsigned long e1 = e;
#endif
  if (e == 0UL)
    {
      mod_set1 (r, m);
      return;
    }

  /* Assume t = 1 here for the invariant.
     t^mask * b^e is invariant, and is the result we want */

  /* Find highest set bit in e. */
  mask = (1UL << (LONG_BIT - 1)) >> ularith_clz (e);

  /* Exponentiate */

  mod_init (t, m);
  mod_set (t, b, m);       /* (t*b)^mask * b^(e-mask) = t^mask * b^e */
#ifndef NDEBUG
  e1 -= mask;
#endif

  while (mask > 1UL)
    {
      mod_sqr (t, t, m);
      mask >>= 1;            /* (t^2)^(mask/2) * b^e = t^mask * b^e */
      if (e & mask)
        {
	  mod_mul (t, t, b, m);
#ifndef NDEBUG
          e1 -= mask;
#endif
        }
    }
  ASSERT (e1 == 0UL && mask == 1UL);
  /* Now e = 0, mask = 1, and t^mask * b^0 = t^mask is the result we want */
  mod_set (r, t, m);
  mod_clear (t, m);
}


/* Compute r = 2^e. Here, e is an unsigned long */
void
mod_2pow_ul (residue_t r, const unsigned long e, const modulus_t m)
{
  unsigned long mask;
  residue_t t, u;

  if (e == 0UL)
    {
      mod_set1 (r, m);
      return;
    }

  mask = (1UL << (LONG_BIT - 1)) >> ularith_clz (e);

  mod_init_noset0 (t, m);
  mod_init_noset0 (u, m);
  mod_set1 (t, m);
  mod_add (t, t, t, m);
  mask >>= 1;

  while (mask > 0UL)
    {
      mod_sqr (t, t, m);
      mod_add (u, t, t, m);
      if (e & mask)
        mod_set (t, u, m);
      mask >>= 1;
    }
  mod_set (r, t, m);
  mod_clear (t, m); 
  mod_clear (u, m); 
}


/* Compute r = 2^e. Here, e is an unsigned long */
static void
mod_3pow_ul (residue_t r, const unsigned long e, const modulus_t m)
{
  unsigned long mask;
  residue_t t, u;

  if (e == 0UL)
    {
      mod_set1 (r, m);
      return;
    }

  mask = (1UL << (LONG_BIT - 1)) >> ularith_clz (e);

  mod_init_noset0 (t, m);
  mod_init_noset0 (u, m);
  mod_set1 (u, m);
  mod_add (t, u, u, m);
  mod_add (t, t, u, m);
  mask >>= 1;

  while (mask > 0UL)
    {
      mod_sqr (t, t, m);
      mod_add (u, t, t, m);
      mod_add (u, u, t, m);
      if (e & mask)
        mod_set (t, u, m);
      mask >>= 1;
    }
  mod_set (r, t, m);
  mod_clear (t, m); 
  mod_clear (u, m); 
}


/* Compute r = b^e. Here e is a multiple precision integer 
   sum_{i=0}^{e_nrwords} e[i] * (machine word base)^i */
void
mod_pow_mp (residue_t r, const residue_t b, const unsigned long *e, 
            const int e_nrwords, const modulus_t m)
{
  unsigned long mask;
  residue_t t;
  int i = e_nrwords - 1;

  if (e_nrwords == 0 || e[i] == 0UL)
    {
      mod_set1 (r, m);
      return;
    }

  mask = (1UL << (LONG_BIT - 1)) >> ularith_clz (e[i]);

  mod_init (t, m);
  mod_set (t, b, m);
  mask >>= 1;

  for ( ; i >= 0; i--)
    {
      while (mask > 0UL)
        {
          mod_sqr (t, t, m);
          if (e[i] & mask)
            mod_mul (t, t, b, m);
          mask >>= 1;
        }
      mask = ~0UL - (~0UL >> 1);
    }
  mod_set (r, t, m);
  mod_clear (t, m);
}


/* Computes 2^e (mod m), where e is a multiple precision integer.
   Requires e != 0. The value of 2 in Montgomery representation 
   (i.e. 2*2^w (mod m) must be passed. */

void
mod_2pow_mp (residue_t r, const unsigned long *e, const int e_nrwords, 
             const modulus_t m)
{
  residue_t t, u;
  unsigned long mask;
  int i = e_nrwords - 1;

  ASSERT (e_nrwords != 0 && e[i] != 0);

  mask = (1UL << (LONG_BIT - 1)) >> ularith_clz (e[i]);

  mod_init_noset0 (t, m);
  mod_init_noset0 (u, m);
  mod_set1 (t, m);
  mod_add (t, t, t , m);
  mask >>= 1;

  for ( ; i >= 0; i--)
    {
      while (mask > 0UL)
        {
          mod_sqr (t, t, m);
          mod_add (u, t, t, m);
          if (e[i] & mask)
            mod_set (t, u, m);
          mask >>= 1;
        }
      mask = ~0UL - (~0UL >> 1);
    }
    
  mod_set (r, t, m);
  mod_clear (t, m);
  mod_clear (u, m);
}


/* Compute r = V_e(b), where V_e(x) is the Chebyshev polynomial defined by
   V_e(x + 1/x) = x^e + 1/x^e. Here e is an unsigned long. */

void
mod_V_ul (residue_t r, const residue_t b, const unsigned long e, 
	  const modulus_t m)
{
  unsigned long mask;
  residue_t t, t1, two;

  if (e == 0UL)
    {
      mod_set1 (r, m);
      mod_add (r, r, r, m);
      return;
    }

  /* Find highest set bit in e. */
  mask = (1UL << (LONG_BIT - 1)) >> ularith_clz (e);

  /* Exponentiate */

  mod_init_noset0 (t, m);
  mod_init_noset0 (t1, m);
  mod_init_noset0 (two, m);
  mod_set1 (two, m);
  mod_add (two, two, two, m);
  mod_set (t, b, m);         /* t = b = V_1 (b) */
  mod_sqr (t1, b, m);
  mod_sub (t1, t1, two, m);  /* t1 = b^2 - 2 = V_2 (b) */
  mask >>= 1;

  /* Here t = V_j (b) and t1 = V_{j+1} (b) for j = 1 */

  while (mask > 0UL)
    {
      if (e & mask)
        {
          /* j -> 2*j+1. Compute V_{2j+1} and V_{2j+2} */
          mod_mul (t, t, t1, m);
          mod_sub (t, t, b, m); /* V_j * V_{j+1} - V_1 = V_{2j+1} */
          mod_sqr (t1, t1, m);
          mod_sub (t1, t1, two, m); /* (V_{j+1})^2 - 2 = V_{2j+2} */
        }
      else
        {
          /* j -> 2*j. Compute V_{2j} and V_{2j+1} */
          mod_mul (t1, t1, t, m);
          mod_sub (t1, t1, b, m); /* V_j * V_{j+1} - V_1 = V_{2j+1}*/
          mod_sqr (t, t, m);
          mod_sub (t, t, two, m);
        }
      mask >>= 1;
    }

  mod_set (r, t, m);
  mod_clear (t, m);
  mod_clear (t1, m);
  mod_clear (two, m);
}


/* Compute r = V_e(b), where V_e(x) is the Chebyshev polynomial defined by
   V_e(x + 1/x) = x^e + 1/x^e. Here e is a multiple precision integer 
   sum_{i=0}^{e_nrwords} e[i] * (machine word base)^i */

void
mod_V_mp (residue_t r, const residue_t b, const unsigned long *e, 
          const int e_nrwords, const modulus_t m)
{
  unsigned long mask;
  int i = e_nrwords - 1;
  residue_t t, t1, two;

  if (e_nrwords == 0 || e[i] == 0UL)
    {
      mod_set1 (r, m);
      mod_add (r, r, r, m);
      return;
    }

  mask = (1UL << (LONG_BIT - 1)) >> ularith_clz (e[i]);

  mod_init_noset0 (t, m);
  mod_init_noset0 (t1, m);
  mod_init_noset0 (two, m);
  mod_set1 (two, m);
  mod_add (two, two, two, m);
  mod_set (t, b, m);
  mod_sqr (t1, b, m);
  mod_sub (t1, t1, two, m);
  mask >>= 1;

  for ( ; i >= 0; i--)
    {
      while (mask > 0UL)
        {
          if (e[i] & mask)
	    {
	      mod_mul (t, t, t1, m);
	      mod_sub (t, t, b, m);
	      mod_sqr (t1, t1, m);
	      mod_sub (t1, t1, two, m);
	    }
	  else
	    {
	      mod_mul (t1, t1, t, m);
	      mod_sub (t1, t1, b, m);
	      mod_sqr (t, t, m);
	      mod_sub (t, t, two, m);
	    }
          mask >>= 1;
        }
      mask = ~0UL - (~0UL >> 1);
    }
  mod_set (r, t, m);
  mod_clear (two, m);
  mod_clear (t, m);
  mod_clear (t1, m);
}


/* Returns 1 if r1 == 1 (mod m) or if r1 == -1 (mod m) or if
   one of r1^(2^1), r1^(2^2), ..., r1^(2^(po2-1)) == -1 (mod m),
   zero otherwise. Requires -1 (mod m) in minusone. */

static inline int
find_minus1 (residue_t r1, const residue_t minusone, const int po2, 
             const modulus_t m)
{
  int i;

  if (mod_is1 (r1, m) || mod_equal (r1, minusone, m))
    return 1;

  for (i = 1 ; i < po2; i++)
    {
      mod_sqr (r1, r1, m);
      if (mod_equal (r1, minusone, m))
        break;
    }

  return (i < po2) ? 1 : 0;
}


/* Returns 1 if m is a strong probable prime wrt base b, 0 otherwise. */
int
mod_sprp (const residue_t b, const modulus_t m)
{
  residue_t r1, minusone;
  int i = 0, po2 = 1;
  unsigned long mm1;

  mm1 = mod_getmod_ul (m);

  if (mm1 <= 3UL)
    return (mm1 >= 2UL);

  if (mm1 % 2UL == 0UL)
    return 0;

  /* Set mm1 to the odd part of m-1 */
  mm1 = (mm1 - 1) >> 1;
  while (mm1 % 2UL == 0UL)
    {
      po2++;
      mm1 >>= 1;
    }
  /* Hence, m-1 = mm1 * 2^po2 */

  mod_init_noset0 (r1, m);
  mod_init_noset0 (minusone, m);
  mod_set1 (minusone, m);
  mod_neg (minusone, minusone, m);

  /* Exponentiate */
  mod_pow_ul (r1, b, mm1, m);

  /* Now r1 == b^mm1 (mod m) */
#if defined(PARI)
  printf ("(Mod(%lu,%lu) ^ %lu) == %lu /* PARI */\n", 
	  mod_get_ul (b, m), mod_getmod_ul (m), mm1, mod_get_ul (r1, m));
#endif
  
  i = find_minus1 (r1, minusone, po2, m);
  
  mod_clear (r1, m);
  mod_clear (minusone, m);
  return i;
}


/* Returns 1 if m is a strong probable prime wrt base 2, 0 otherwise. */
int
mod_sprp2 (const modulus_t m)
{
  residue_t r, minusone;
  int i = 0, po2 = 1;
  unsigned long mm1;

  mm1 = mod_getmod_ul (m);

  if (mm1 <= 3UL)
    return (mm1 >= 2UL);

  if (mm1 % 2UL == 0UL)
    return 0;

  /* If m == 1,7 (mod 8), then 2 is a quadratic residue, and we must find
     -1 with one less squaring. This does not reduce the number of
     pseudo-primes because strong pseudo-primes are also Euler pseudo-primes, 
     but makes identifying composites a little faster on avarage. */
  if (mm1 % 8 == 1 || mm1 % 8 == 7)
    po2--;

  /* Set mm1 to the odd part of m-1 */
  mm1 = (mm1 - 1) >> 1;
  while (mm1 % 2UL == 0UL)
    {
      po2++;
      mm1 >>= 1;
    }
  /* Hence, m-1 = mm1 * 2^po2 */

  mod_init_noset0 (r, m);
  mod_init_noset0 (minusone, m);
  mod_set1 (minusone, m);
  mod_neg (minusone, minusone, m);

  /* Exponentiate */
  mod_2pow_ul (r, mm1, m);

  /* Now r == b^mm1 (mod m) */
#if defined(PARI)
  printf ("(Mod(2,%lu) ^ %lu) == %lu /* PARI */\n", 
	  mod_getmod_ul (m), mm1, mod_get_ul (r, m));
#endif
  
  i = find_minus1 (r, minusone, po2, m);

  mod_clear (r, m);
  mod_clear (minusone, m);
  return i;
}


int
mod_isprime (const modulus_t m)
{
  residue_t b, minusone, r1;
  const unsigned long n = mod_getmod_ul (m);
  unsigned long mm1;
  int r = 0, po2;
  
  if (n == 1UL)
    return 0;

  if (n % 2UL == 0UL)
    {
      r = (n == 2UL);
#if defined(PARI)
      printf ("isprime(%lu) == %d /* PARI */\n", n, r);
#endif
      return r;
    }

  /* Set mm1 to the odd part of m-1 */
  mm1 = n - 1UL;
  po2 = ularith_ctz (mm1);
  mm1 >>= po2;
  
  mod_init_noset0 (b, m);
  mod_init_noset0 (minusone, m);
  mod_init_noset0 (r1, m);
  mod_set1 (minusone, m);
  mod_neg (minusone, minusone, m);

  /* Do base 2 SPRP test */
  mod_2pow_ul (r1, mm1, m);   /* r = 2^mm1 mod m */
  /* If n is prime and 1 or 7 (mod 8), then 2 is a square (mod n)
     and one less squaring must suffice. This does not strengthen the
     test but saves one squaring for composite input */
  if (n % 8 == 7)
    { 
      if (!mod_is1 (r1, m))
        goto end;
    }
  else if (!find_minus1 (r1, minusone, po2 - ((n % 8 == 1) ? 1 : 0), m))
    goto end; /* Not prime */

  if (n < 2047UL)
    {
      r = 1;
      goto end;
    }

  /* Base 3 is poor at identifying composites == 1 (mod 3), but good at
     identifying composites == 2 (mod 3). Thus we use it only for 2 (mod 3) */
  if (n % 3UL == 1UL)
    {
      mod_set1 (b, m);
      mod_add (b, b, b, m);
      mod_add (b, b, b, m);
      mod_add (b, b, b, m);
      mod_add (b, b, minusone, m);  /* b = 7 */
      mod_pow_ul (r1, b, mm1, m);   /* r = 7^mm1 mod m */
      if (!find_minus1 (r1, minusone, po2, m))
	goto end; /* Not prime */

      if (n < 2269093UL)
        {
	  r = (n != 314821UL);
	  goto end;
        }
      
      /* b is still 7 here */
      mod_add (b, b, b, m); /* 14 */
      mod_sub (b, b, minusone, m); /* 15 */
      mod_add (b, b, b, m); /* 30 */
      mod_add (b, b, b, m); /* 60 */
      mod_sub (b, b, minusone, m); /* 61 */
      mod_pow_ul (r1, b, mm1, m);   /* r = 61^mm1 mod m */
      if (!find_minus1 (r1, minusone, po2, m))
	goto end; /* Not prime */

#if (ULONG_MAX > 4294967295UL)
      if (n != 4759123141UL && n != 8411807377UL && n < 11207066041UL)
        {
	  r = 1;
	  goto end;
        }
      
      mod_set1 (b, m);
      mod_add (b, b, b, m);
      mod_add (b, b, b, m);
      mod_sub (b, b, minusone, m);    /* b = 5 */
      mod_pow_ul (r1, b, mm1, m);   /* r = 5^mm1 mod m */
      if (!find_minus1 (r1, minusone, po2, m))
	goto end; /* Not prime */
	  
          /* These are the base 5,7,61 SPSP < 10^13 and n == 1 (mod 3) */
	  r = (n != 30926647201UL && n != 45821738881UL && 
	       n != 74359744201UL && n != 90528271681UL && 
	       n != 110330267041UL && n != 373303331521UL && 
	       n != 440478111067UL && n != 1436309367751UL && 
	       n != 1437328758421UL && n != 1858903385041UL && 
	       n != 4897239482521UL && n != 5026103290981UL && 
	       n != 5219055617887UL && n != 5660137043641UL && 
	       n != 6385803726241UL);
#else
	      r = 1;
#endif
    }
  else
    {
      /* Case n % 3 == 0, 2 */
      
      mod_3pow_ul (r1, mm1, m);   /* r = 3^mm1 mod m */
      if (!find_minus1 (r1, minusone, po2, m))
	goto end; /* Not prime */
      
      if (n < 102690677UL && n != 5173601UL && n != 16070429UL && 
          n != 54029741UL)
        {
	  r = 1;
	  goto end;
	}
      
      mod_set1 (b, m);
      mod_add (b, b, b, m);
      mod_add (b, b, b, m);
      mod_sub (b, b, minusone, m);    /* b = 5 */
      mod_pow_ul (r1, b, mm1, m);   /* r = 5^mm1 mod m */
      if (!find_minus1 (r1, minusone, po2, m))
	goto end; /* Not prime */

#if (ULONG_MAX > 4294967295UL)
      /* These are the base 3,5 SPSP < 10^13 with n == 2 (mod 3) */
      r = (n != 244970876021UL && n != 405439595861UL && 
	   n != 1566655993781UL && n != 3857382025841UL &&
	   n != 4074652846961UL && n != 5783688565841UL);
#else
      r = 1;
#endif
    }
 
 end:
#if defined(PARI)
  printf ("isprime(%lu) == %d /* PARI */\n", n, r);
#endif
  mod_clear (b, m);
  mod_clear (minusone, m);
  mod_clear (r1, m);
  return r;
}

int
mod_jacobi (const residue_t a_par, const modulus_t m_par)
{
  unsigned long a, m, s;
  int t = 1;

  /* We probably could stay in Montgomery representation here,
     a_par = a * 2^w, w even, so 2^w is a square and won't change
     the quadratic character */
  a = mod_get_ul (a_par, m_par);
  m = mod_getmod_ul (m_par);
  ASSERT (a < m);
  
  while (a != 0UL)
  {
    while (a % 2UL == 0UL)
    {
      a /= 2UL;
      if (m % 8UL == 3UL || m % 8UL == 5UL)
        t = -t;
    }
    s = a; a = m; m = s; /* swap */
    if (a % 4UL == 3UL && m % 4UL == 3UL)
      t = -t;
    a %= m;
  }
  if (m != 1UL)
    t = 0;
  
#ifdef MODTRACE
  printf ("kronecker(%lu, %lu) == %d\n", 
          mod_get_ul (a_par, m_par), mod_getmod_ul (m_par), t);
#endif
  return t;
}
