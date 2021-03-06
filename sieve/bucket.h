/*
 * Some bucket stuff.
 */

#include <stdlib.h>   // for malloc and friends
#include <stdint.h>

//#define SAFE_BUCKETS

/* We store ((p-1)/2) % 2^16 in the bucket updates and reconstruct
   p from that. In rare cases a wrap-around is missed and p does not 
   divide the norm; in such a case we try p + P_WRAP to see if that
   divides */
#define BUCKET_P_WRAP (1U<<17)

/*
 * This bucket module provides a way to store elements (that are called
 * updates), while partially sorting them, according to some criterion (to
 * be defined externally): the incoming data is stored into several
 * buckets. The user says for each data to which bucket it belongs. This
 * module is supposed to perform this storage in a cache-friendly way and
 * so on...
 */

/*
 * Main available commands are 
 *   push_bucket_update(i, x)  :   add x to bucket number i
 *   get_next_bucket_update(i) :   iterator to read contents of bucket number i
 *
 * See the MAIN FUNCTIONS section below for the complete interface, with
 * prototypes of exported functions.
 */

/********  Data structure for the contents of buckets **************/

/* In principle, the typedef for the bucket_update can be changed without
 * affecting the rest of the code. 
 */

/*
 * For the moment, we store the bucket updates and a 16-bit field
 * that can contain, for instance, the low bits of p.
 */

typedef struct {
    uint16_t x;
    uint16_t p;
} bucket_update_t;


/* After a bucket is purged, it is usually not possible to reconstruct
 * the primes from their lower 16 bits any more, so purging does the 
 * reconstruction and stores the remaining updates with the complete primes 
 */

typedef struct {
    uint32_t p;
    uint16_t x;
} bucket_prime_t;


/*
 * will be used as a sentinel
 */
static const bucket_update_t LAST_UPDATE = {0,0};

/******** Bucket array typedef **************/

/*
 * A bucket is just an array of bucket_update_t.
 * We are going to manipulate several of them simultaneously when
 * sieving. In order to reduce the cache pressure, we are going to
 * introduce another structure that handles an array of bucket
 * efficiently.
 *
 * When doing a (long) sequence of pushes, only the bucket_write array is
 * used: pointer to it should be in a register, and the set of pointer
 * will probably fit in one page-size (please align!), so that the pressure
 * on the TLB is minimal. The other parts should not really be used
 * during this memory intensive part.
 * (except if one want to check for overflow, which can probably be
 * avoided by having large enough bucket_size).
 */

typedef struct {
    bucket_update_t ** bucket_start;    // Contains pointers to beginning of
                                        // buckets.
    bucket_update_t ** bucket_write;    // Contains pointers to first empty
                                        // location in each bucket.
    bucket_update_t ** bucket_read;     // Contains pointers to first unread
                                        // location in each bucket.
    unsigned char   * logp_val;         // Each time logp changes, the
                                        // new value is added here (including
                                        // the initial logp value used) 
    bucket_update_t ** logp_idx;        // For each logp value there are
                                        // n_bucket pointers, each pointer
                                        // tells where in the corresponding 
                                        // bucket that logp values starts 
                                        // being used
    int bucket_size;                    // The allocated size of one bucket.
    int n_bucket;                       // Number of buckets.
    int nr_logp;
    unsigned char last_logp;
} bucket_array_t;

/* What does the logp entry do?
   In order to avoid having to store the logp value inside the 
   bucket_update_t, we use the fact that logp changes very rarely,
   and only store pointers to the locations inside the buckets where
   logp changes.
   Assuming that very small primes are not bucket sieved, and that
   factor base entries are sorted in order of increasing logp, 
   \footnote{with maybe a few exceptions where maintainging strict order 
     is too awkward, but note that 
     log(largest-bucket-sieved-p) - log(smallest-bucket-sieved-p)
     is really a rather small value, and the number of exceptions 
     should not be very much larger}
   we store a pointer for *every* bucket when logp changes, even though 
   not necessarily every bucket receives an update with that logp. We do 
   this because we'd like to have a single last_logp value to compare to, 
   rather than n_bucket different ones.
   
   Thus there is an array logp_val with the logp values, starting with 
   the first logp value to use, plus one entry whenever the logp value 
   changes. For each entry in logp_val, logp_idx has a pointer to an 
   array of n_bucket pointers, each pointer giving the address inside
   the corresponding bucket from where on this new logp values should be 
   used.
   I.e., logp_val[j] should be used for the updates in bucket[i] whose 
   addresses are >= logp_idx[j*n_bucket+i], but less than 
     logp_idx[(j+1)*n_bucket+i] if j+1 < nr_logp, bucket_write otherwise.
*/


/* Similar, but stores info containing the complete prime instead of
   only the low 16 bits, and has only one bucket */

typedef struct {
  bucket_prime_t *start;
  bucket_prime_t *read;
  bucket_prime_t *write;
  int size;
} bucket_primes_t;



/*
 * Notes for future improvements:
 *
 * 1) Double buckets.
 * If we can not afford enough buckets (limited by TLB) and we don't want
 * to make the bucket_region larger than L1, we can have a double bucket
 * system: A first level of buckets, that correspond to very large
 * bucket_regions, and a second level of bucket regions fitting in L1.
 * We collect all updates in the buckets of the first level, then we
 * process one first level bucket at a time, copying its updates to the
 * corresponding second level buckets and processing each second level
 * bucket by applying its updates to the L1 bucket_regions.
 *
 * 2) Buffered buckets.
 * One can have a buffer that contains one cache-line for each bucket.
 * Once a cache-line is full of updates, it can be moved in a
 * write-combined (non-temporal) way to the memory location of the
 * bucket. Details are left to the reader ;-)
 *
 */


/******** MAIN FUNCTIONS **************/


/* Returns an allocated array of <n_bucket> buckets each having size
 * <bucket_size>. Must be freed with clear_bucket_array().
 * It also put pointers in position ready for read/writes.
 */
static inline bucket_array_t
init_bucket_array(const int n_bucket, const int bucket_size);

static inline void
clear_bucket_array(bucket_array_t BA);

/* Main writing function: appends update to bucket number i.
 * If SAFE_BUCKETS is not #defined, then there is no checking that there is
 * enough room for the update. This could lead to a segfault, with the
 * current implementation!
 */
static inline void
push_bucket_update(bucket_array_t BA, const int i, 
                   const bucket_update_t update);

/* Main reading iterator: returns the first unread update in bucket i.
 * If SAFE_BUCKETS is not #defined, there is no checking that you are reading
 * at most as much as what you wrote. If keeping the count while pushing
 * is impossible, the following functions can help:
 * - get the count after the last push using nb_of_updates()
 * - put a sentinel after your bucket using push_sentinel() and check if
 * the value returned by get_next_bucket_update() is LAST_UPDATE.
 * - call the is_end() functions that tells you if you can apply
 *   get_next_bucket_update().
 */
static inline bucket_update_t
get_next_bucket_update(bucket_array_t BA, const int i);
static inline int
nb_of_updates(const bucket_array_t BA, const int i);
static inline void
push_sentinel(bucket_array_t BA, const int i);
static inline int
is_end(const bucket_array_t BA, const int i);

/* If you need to read twice a bucket, you can rewind it: */
static inline void
rewind_bucket(bucket_array_t BA, const int i);

/* If you want to read the most recently read update again, rewind by 1: */
static inline void
rewind_bucket_by_1 (bucket_array_t BA, const int i);

/* If you want to access updates in a non-sequential way: */
static inline bucket_update_t
get_kth_bucket_update(const bucket_array_t BA, const int i, const int k);

/* Call this to mark the current write position in the buckets the start
   of this logp value. */
static inline void
bucket_new_logp(bucket_array_t *BA, const unsigned char logp);


/* Functions for handling entries with x and complete prime p */

static inline bucket_primes_t
init_bucket_primes (const int size);

static inline void
clear_bucket_primes (bucket_primes_t *BP);

static inline void
push_bucket_prime (bucket_primes_t *BP, const bucket_prime_t prime);

static inline bucket_prime_t
get_next_bucket_prime (bucket_primes_t *BP);


/******** Bucket array implementation **************/

#include "utils/misc.h"

static inline bucket_array_t
init_bucket_array(const int n_bucket, const int bucket_size)
{
    bucket_array_t BA;
    int i;
    BA.bucket_size = bucket_size;
    BA.n_bucket = n_bucket;

    BA.bucket_start = (bucket_update_t **)malloc_pagealigned(n_bucket*sizeof(bucket_update_t *));
    BA.bucket_write = (bucket_update_t **)malloc_check(n_bucket*sizeof(bucket_update_t *));
    BA.bucket_read  = (bucket_update_t **)malloc_check(n_bucket*sizeof(bucket_update_t *));

    for (i = 0; i < n_bucket; ++i) {
        // TODO: shall we ensure here that those pointer do not differ by
        // a large power of 2, to take into account the associativity of
        // L1 cache ?
        // TODO: would it be better to have a single big malloc for all
        // the bucket_start[i] ?
        BA.bucket_start[i] = (bucket_update_t *)malloc_check(bucket_size*sizeof(bucket_update_t));
        BA.bucket_write[i] = BA.bucket_start[i];
        BA.bucket_read[i] = BA.bucket_start[i];
    }
    BA.logp_val = NULL;
    BA.logp_idx = NULL;
    BA.nr_logp = 0;
    BA.last_logp = 0;
    return BA;
}

static inline void
clear_bucket_array(bucket_array_t BA)
{
    int i;
    for (i = 0; i < BA.n_bucket; ++i)
      free(BA.bucket_start[i]);
    free_pagealigned(BA.bucket_start, BA.n_bucket*sizeof(bucket_update_t *));
    free(BA.bucket_write);
    BA.bucket_write = NULL;
    free(BA.bucket_read);
    BA.bucket_read = NULL;
    free(BA.logp_val);
    BA.logp_val = NULL;
    free(BA.logp_idx);
    BA.logp_idx = NULL;
}

static inline void
push_bucket_update(bucket_array_t BA, const int i, 
                   const bucket_update_t update)
{
    *(BA.bucket_write[i])++ = update; /* Pretty! */
#ifdef SAFE_BUCKETS
    if (BA.bucket_start[i] + BA.bucket_size <= BA.bucket_write[i]) {
        fprintf(stderr, "# Warning: hit end of bucket nb %d\n", i);
        BA.bucket_write[i]--;
    }
#endif
}

static inline void
bucket_new_logp (bucket_array_t *BA, const unsigned char logp)
{
  int i, j;

  if (BA->last_logp == logp)
    {
      /* printf ("bucket_new_logp called unnecessarily\n"); */
      return;
    }

  /* printf ("bucket_new_logp: need new logp, old logp = %u, "
          "old nr_logp = %d, new logp = %u\n",
          (unsigned) BA->last_logp, BA->nr_logp, (unsigned) logp); */
  BA->last_logp = logp;
  j = BA->nr_logp;
  BA->nr_logp++;
  BA->logp_val = (unsigned char *) realloc (BA->logp_val, 
                 BA->nr_logp * sizeof (unsigned char));
  BA->logp_idx = (bucket_update_t **) 
                 realloc (BA->logp_idx, BA->nr_logp * BA->n_bucket * 
                          sizeof (bucket_update_t *));
  BA->logp_val[j] = logp;
  for (i = 0; i < BA->n_bucket; ++i)
    BA->logp_idx[j * BA->n_bucket + i] = BA->bucket_write[i];
}

static inline void
rewind_bucket(bucket_array_t BA, const int i)
{
    BA.bucket_read[i] = BA.bucket_start[i];
}

static inline void
rewind_bucket_by_1 (bucket_array_t BA, const int i)
{
  if (BA.bucket_read[i] > BA.bucket_start[i])
    BA.bucket_read[i]--;
}

static inline bucket_update_t
get_next_bucket_update(bucket_array_t BA, const int i)
{
    bucket_update_t rep = *(BA.bucket_read[i])++;
#ifdef SAFE_BUCKETS
    if (BA.bucket_read[i] > BA.bucket_write[i]) {
        fprintf(stderr, "# Warning: reading too many updates in bucket nb %d\n", i);
        BA.bucket_read[i]--;
        return LAST_UPDATE;
    }
#endif
    return rep;
}

static inline bucket_update_t
get_kth_bucket_update(const bucket_array_t BA, const int i, const int k)
{
    bucket_update_t rep = (BA.bucket_start[i])[k];
#ifdef SAFE_BUCKETS
    if (BA.bucket_start[i] + k >= BA.bucket_write[i]) {
        fprintf(stderr, "# Warning: reading outside valid updates in bucket nb %d\n", i);
        return LAST_UPDATE;
    }
#endif
    return rep;
}

static inline int
nb_of_updates(const bucket_array_t BA, const int i)
{
    return (BA.bucket_write[i] - BA.bucket_start[i]);
}

/* Returns how full the fullest bucket is */
static double
buckets_max_full (const bucket_array_t BA)
{
  int i, max = 0;
  for (i = 0; i < BA.n_bucket; ++i)
    {
      int j = nb_of_updates (BA, i);
      if (max < j)
        max = j;
    }
  return (double) max / (double) BA.bucket_size;
}

static inline void
push_sentinel(bucket_array_t BA, const int i)
{
    push_bucket_update(BA, i, LAST_UPDATE);
}

static inline int
is_end(const bucket_array_t BA, const int i)
{
    return (BA.bucket_read[i] == BA.bucket_write[i]);
}


static inline bucket_primes_t
init_bucket_primes (const int size)
{
  bucket_primes_t BP;
  BP.size = size;
  BP.start = (bucket_prime_t *) malloc_check (size * sizeof(bucket_prime_t));
  BP.read = BP.start;
  BP.write = BP.start;
  return BP;
}

static inline void
clear_bucket_primes (bucket_primes_t *BP)
{
  free (BP->start);
  BP->start = NULL;
  BP->read = NULL;
  BP->write = NULL;
  BP->size = 0;
}


static inline bucket_prime_t
get_next_bucket_prime (bucket_primes_t *BP)
{
  return *BP->read++;
}

static inline void
push_bucket_prime (bucket_primes_t *BP, const bucket_prime_t p)
{
  *BP->write++ = p;
}
                   

static inline int
bucket_primes_is_end(const bucket_primes_t *BP)
{
  return (BP->read == BP->write);
}

static inline void
rewind_primes_by_1 (bucket_primes_t *BP)
{
  if (BP->read > BP->start)
    BP->read--;
}


/* A compare function suitable for sorting updates in order of ascending x
   with qsort() */
static int
bucket_cmp_x (const bucket_prime_t *a, const bucket_prime_t *b)
{
  if (a->x < b->x)
    return -1;
  if (a->x == b->x)
    return 0;
  return 1;
}

static void
bucket_sortbucket (bucket_primes_t *BP)
{
  qsort (BP->start, BP->write - BP->start, sizeof (bucket_prime_t), 
	 (int(*)(const void *, const void *)) &bucket_cmp_x);
}


/* Remove some redundancy form the stored primes, e.g., remove the low
   bit which is always 1. We might also store p/6 and one bit telling
   whether it was 1 or 5 (mod 6). */
static inline uint16_t
bucket_encode_prime (uint32_t p)
{
  return (uint16_t)(p/2U);
}

/* Copy only those bucket entries where x yields a sieve report.
 * These entries get sorted, to speed up trial division. 
 * Due to the purging and sorting, it will not be possible to
 * reconstruct the correct p from its low 16 bits, so the
 * reconstruction is done here and the full p is stored in the output.
 */

void
purge_bucket (bucket_primes_t *BP, bucket_array_t BA, 
              const int i, const unsigned char *S)
{
  bucket_update_t *u;
  uint16_t last_p = 0;
  uint32_t phigh = 0;
  bucket_prime_t bp;

  for (u = BA.bucket_start[i] ; u < BA.bucket_write[i]; u++)
    {
      if (u->p < last_p)
	phigh += BUCKET_P_WRAP;
      last_p = u->p;
      if (S[u->x] != 255)
        {
	  bp.p = phigh + (uint32_t)(u->p) * 2U + 1U; /* Reconstruct prime */
          bp.x = u->x;
          push_bucket_prime (BP, bp);
	}
    }
}

