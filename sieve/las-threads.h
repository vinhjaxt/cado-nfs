#ifndef LAS_THREADS_H_
#define LAS_THREADS_H_

#include <pthread.h>
#include <algorithm>
#include "threadpool.h"
#include "las-forwardtypes.h"
#include "bucket.h"
#include "fb.h"
#include "las-report-stats.h"
#include "las-base.hpp"

class thread_workspaces;

/* {{{ thread-related defines */
/* All of this exists _for each thread_ */
struct thread_side_data : private NonCopyable {
  const fb_factorbase *fb;
  /* For small sieve */
  int64_t * ssdpos;
  int64_t * rsdpos;

  /* The real array where we apply the sieve.
     This has size BUCKET_REGION_0 and should be close to L1 cache size. */
  unsigned char *bucket_region;
  sieve_checksum checksum_post_sieve;

  thread_side_data();
  ~thread_side_data();
  
  void set_fb(const fb_factorbase *_fb) {fb = _fb;}
  void update_checksum(){checksum_post_sieve.update(bucket_region, BUCKET_REGION);}
};

struct thread_data : private NonCopyable {
  const thread_workspaces *ws;
  int id;
  thread_side_data sides[2];
  las_info_srcptr las;
  sieve_info_ptr si;
  las_report rep;
  unsigned char *SS;
  bool is_initialized;
  uint32_t first_region0_index;
  thread_data();
  ~thread_data();
  void init(const thread_workspaces &_ws, int id, las_info_srcptr las);
  void pickup_si(sieve_info_ptr si);
  void update_checksums();
};

/* A set of n bucket arrays, all of the same type, and methods to reserve one
   of them for exclusive use and to release it again. */
template <typename T>
class reservation_array : private NonCopyable, private monitor {
  T * const BAs;
  bool * const in_use;
  const size_t n;
  condition_variable cv;
  /* Return the index of the first entry that's currently not in use, or the
     first index out of array bounds if all are in use */
  size_t find_free() const {
    return std::find(&in_use[0], &in_use[n], false) - &in_use[0];
  }
public:
  reservation_array(size_t n)
    : BAs(new T[n]), in_use(new bool[n]), n(n)
  {
    for (size_t i = 0; i < n; i++) {
      in_use[i] = false;
    }
  }
  ~reservation_array() {
    delete[] BAs;
    delete[] in_use;
  }

  /* Allocate enough memory to be able to store at least n_bucket buckets,
     each of size at least fill_ratio * bucket region size. */
  void allocate_buckets(const uint32_t n_bucket, double fill_ratio);
  const T* cbegin() const {return &BAs[0];}
  const T* cend() const {return &BAs[n];}

  T &reserve();
  void release(T &BA);
};

/* A group of reservation arrays, one for each possible update type.
   Also defines a getter function, templated by the desired type of
   update, that returns the corresponding reservation array, i.e.,
   it provides a type -> object mapping. */
class reservation_group {
  friend class thread_workspaces;
  reservation_array<bucket_array_t<1, shorthint_t> > RA1_short;
  reservation_array<bucket_array_t<2, shorthint_t> > RA2_short;
  reservation_array<bucket_array_t<3, shorthint_t> > RA3_short;
  reservation_array<bucket_array_t<1, longhint_t> > RA1_long;
  reservation_array<bucket_array_t<2, longhint_t> > RA2_long;
protected:
  template<int LEVEL, typename HINT>
  reservation_array<bucket_array_t<LEVEL, HINT> > &
  get();

  template <int LEVEL, typename HINT>
  const reservation_array<bucket_array_t<LEVEL, HINT> > &
  cget() const;
public:
  reservation_group(size_t nr_bucket_arrays);
  void allocate_buckets(const uint32_t *n_bucket, const double *fill_ratio);
};

class thread_workspaces : private NonCopyable {
  typedef reservation_group * reservation_group_ptr;
  const size_t nr_workspaces;
  sieve_info_ptr si;
  const unsigned int nr_sides; /* Usually 2 */
  reservation_group_ptr *groups; /* one per side. Need pointer array due to
    lacking new[] without default constructor prior to C++11 :( */

public:
  // FIXME: thrs should be private!
  thread_data *thrs;

  thread_workspaces(size_t nr_workspaces, unsigned int nr_sides, las_info_ptr _las);
  ~thread_workspaces();
  void pickup_si(sieve_info_ptr si);
  void thread_do(void * (*) (thread_data *));
  void buckets_alloc();
  void buckets_free();
  template <int LEVEL, typename HINT>
  double buckets_max_full();
  void accumulate(las_report_ptr, sieve_checksum *);

  template <int LEVEL, typename HINT>
  bucket_array_t<LEVEL, HINT> &
  reserve_BA(const int side) {return groups[side]->get<LEVEL, HINT>().reserve();}

  template <int LEVEL, typename HINT>
  void
  release_BA(const int side, bucket_array_t<LEVEL, HINT> &BA) {
    return groups[side]->get<LEVEL, HINT>().release(BA);
  }

  /* Iterator over all the bucket arrays of a given type on a given side */
  template <int LEVEL, typename HINT>
  const bucket_array_t<LEVEL, HINT> *
  cbegin_BA(const int side) const {return groups[side]->cget<LEVEL, HINT>().cbegin();}

  template <int LEVEL, typename HINT>
  const bucket_array_t<LEVEL, HINT> *
  cend_BA(const int side) const {return groups[side]->cget<LEVEL, HINT>().cend();}
};

#endif
