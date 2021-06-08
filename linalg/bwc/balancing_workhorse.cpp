#include "cado.h" // IWYU pragma: keep
// IWYU pragma: no_include <ext/alloc_traits.h>
// IWYU pragma: no_include <memory>
#include <cstddef>              // for ptrdiff_t
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cinttypes>

#include <vector>
#include <mutex>
#include <algorithm>
#include <string>                // for string
#include <utility>               // for move
#include <sys/stat.h>
#include <unistd.h>

#include "balancing.h"
#include "balancing_workhorse.h"
#include "fmt/core.h"            // for check_format_string
#include "fmt/format.h"          // for basic_buffer::append, basic_parse_co...
#include "fmt/printf.h" // IWYU pragma: keep
#include "macros.h"              // for ASSERT_ALWAYS, ASSERT
#include "misc.h"                // for size_disp, derived_filename
#include "parallelizing_info.h"
#include "params.h"     // param_list
#include "select_mpi.h"
#include "timing.h" // wct_seconds
#include "verbose.h" // verbose_enabled

#define xxxRELY_ON_MPI_THREAD_MULTIPLE

/* The entry point of this code is balancing_get_matrix_u32 ; called in a
 * parallel context, it fills the provided matrix_u32 parameter with the
 * sub-matrix that is relevant for the required balancing (also passed
 * from the matrix_u32_ptr parameter.
 *
 */
void balancing_decl_usage(param_list_ptr pl)
{
    param_list_decl_usage(pl, "sanity_check_vector",
            "while dispatching the matrix, store a fixed matrix times vector product in the given file");
}

void balancing_lookup_parameters(param_list_ptr pl)
{
    param_list_lookup_string(pl, "sanity_check_vector");
}

bool has_mpi_thread_multiple()
{
    int prov;
    MPI_Query_thread(&prov);
    return prov >= MPI_THREAD_MULTIPLE;
}

struct dispatcher {/*{{{*/
    parallelizing_info_ptr pi;
    std::string mfile;
    std::string bfile;
    std::string check_vector_filename;
    int withcoeffs;
    int transpose;
    /* one matrix_u32_ptr per thread of this endpoint */
    matrix_u32_ptr * args_per_thread;
    std::vector<int> is_reader_map;
    std::vector<int> readers_index;
    int nreaders;
    balancing bal;
    unsigned int nhjobs;
    unsigned int nvjobs;
    uint32_t rows_chunk_big;
    uint32_t cols_chunk_big;
    uint32_t rows_chunk_small;
    uint32_t cols_chunk_small;

    MPI_Comm reader_comm;

    bool is_reader() const { return is_reader_map[pi->m->jrank]; }
    bool is_reader(int i) const { return is_reader_map[i]; }

    template<typename T> T integrate(std::vector<T> & v) const/*{{{*/
    {
        T s = 0;
        for(T & x : v) {
            T y = x;
            x = s;
            s += y;
        }
        return s;
    }/*}}}*/
    dispatcher(parallelizing_info_ptr pi,/*{{{*/
            param_list_ptr pl,
            matrix_u32_ptr * args_per_thread)
        : pi(pi)
        , mfile(args_per_thread[0]->mfile)
        , bfile(args_per_thread[0]->bfile)
        , withcoeffs(args_per_thread[0]->withcoeffs)
        , transpose(args_per_thread[0]->transpose)
        , args_per_thread(args_per_thread)
        , is_reader_map(pi->m->njobs, 0)
    {
        const char * tmp = param_list_lookup_string(pl, "sanity_check_vector");
        if (tmp) check_vector_filename = tmp;
        // Assume we are reading an N-rows matrix.  Assume we have n0*n1
        // nodes, t0*t1 threads.
        //
        // Determine the number of readers by finding out which of the n0*n1
        // nodes have read access from the matrix.

        is_reader_map[pi->m->jrank] = access(mfile.c_str(), R_OK) == 0;
#ifdef RELY_ON_MPI_THREAD_MULTIPLE
        if (!has_mpi_thread_multiple()) {
            if (pi->m->jrank > 0)
                is_reader_map[pi->m->jrank] = 0;
            else
                ASSERT_ALWAYS(is_reader_map[pi->m->jrank]);
        }
#endif

        MPI_Allgather(MPI_IN_PLACE, 0, 0, is_reader_map.data(), 1, MPI_INT, pi->m->pals);
        readers_index = is_reader_map;
        nreaders = integrate(readers_index);

        if (pi->m->jrank == 0) {
            printf("Beginning balancing with %d readers for file %s\n",
                    nreaders, mfile.c_str());
            for(unsigned int i = 0 ; i < pi->m->njobs ; i++) {
                if (is_reader(i))
                    printf("Job %d is reader number %d\n", i, readers_index[i]);
            }
        }
        ASSERT_ALWAYS(nreaders);
        MPI_Comm_split(MPI_COMM_WORLD, is_reader(), pi->m->jrank, &reader_comm);

        balancing_init(bal);

        /* Reading the _full_ bfile is not useful. Readers do need the
         * colperm. As for the row perm, we only need a fraction of it,
         * but cuting hair might well end up costing more memory (if we
         * have FLAG_REPLICATE for instance).
         */
        if (pi->m->jrank == 0)
            balancing_read_header(bal, bfile.c_str());
        MPI_Bcast(bal, sizeof(balancing), MPI_BYTE, 0, pi->m->pals);

        nhjobs = pi->wr[1]->njobs;
        nvjobs = pi->wr[0]->njobs;
        rows_chunk_big = bal->trows / nhjobs;
        cols_chunk_big = bal->tcols / nvjobs;
        rows_chunk_small = bal->trows / bal->h->nh;
        cols_chunk_small = bal->tcols / bal->h->nv;

        /* these are set in other member functions. The values here are
         * just silly placeholders
         */
        pass_number = -1;
        row0 = row1 = UINT_MAX;
    }/*}}}*/
    ~dispatcher() {/*{{{*/
        MPI_Comm_free(&reader_comm);
        balancing_clear(bal);
    }/*}}}*/

    void main();
    void stats();

    int pass_number;

    /* reader stuff */
    std::vector<size_t> bytes_per_reader;
    std::vector<size_t> offset_per_reader;
    std::vector<uint32_t> row0_per_reader;
    std::vector<uint32_t> fw_rowperm; // fragmented among all readers.
    std::vector<uint32_t> fw_colperm; // identical at all nodes
    void reader_compute_offsets();
    unsigned int row0, row1;
    void reader_fill_index_maps();
    void reader_thread();

    /* MPI send from readers to endpoints */
    std::vector<MPI_Request> outstanding;
    std::vector<std::vector<uint32_t>> outstanding_queues;
    std::vector<std::vector<uint32_t>> avail_queues;
    std::vector<int> indices;
    // std::vector<MPI_Status> statuses;
    void post_send(std::vector<uint32_t> &, unsigned int);
    void progress(bool wait = false);
    void post_semaphore_blocking(unsigned int k);
    void post_semaphore_nonblocking(unsigned int k);

    /* endpoint stuff */

    /* used in pass 1 for the weights per row */
    std::vector<std::vector<uint32_t>> thread_row_weights;

    /* used on pass 2 to give the row-beginning pointers. */
    std::vector<std::vector<size_t>> thread_row_positions;

    void prepare_pass();
    std::mutex incoming_mutex;
    void endpoint_handle_incoming(std::vector<uint32_t> & Q);
    void watch_incoming_on_reader(int & active_peers);
    void endpoint_thread();
};/*}}}*/

/* balancing_get_matrix_u32 -- This is our entry point. {{{
 * arg is thread private
 * We read:
 *      arg->mfile
 *      arg->bfile
 *      arg->withcoeffs
 *      arg->transpose  (abide by the preference of the inner mm layer)
 * On output, we set:
 *      arg->size
 *      arg->p
 */
void balancing_get_matrix_u32(parallelizing_info_ptr pi, param_list pl,
        matrix_u32_ptr arg)
{
    // REQUIRED: arg->mfile      -- URLs no longer supported.
    ASSERT_ALWAYS(arg->mfile);
    ASSERT_ALWAYS(arg->bfile);
    ASSERT_ALWAYS(!arg->p);
    ASSERT_ALWAYS(!arg->size);

    matrix_u32_ptr * args_per_thread = (matrix_u32_ptr *) shared_malloc_set_zero(pi->m, pi->m->ncores * sizeof(matrix_u32_ptr));
    args_per_thread[pi->m->trank] = arg;
    serialize_threads(pi->m);

    if (pi->m->trank == 0) {
        dispatcher D(pi, pl, args_per_thread);
        D.main();
        D.stats();
    }

    serialize_threads(pi->m);

    shared_free(pi->m, args_per_thread);
}
/* }}} */

/* This sends the data in vector Q, and replaces Q with a fresh vector of
 * size zero, which might be drawn from a pool of queues that were
 * attached to recently completed sends.
 */
void dispatcher::post_send(std::vector<uint32_t> & Q, unsigned int k)/*{{{*/
{
    if (k == pi->m->jrank) {
        endpoint_handle_incoming(Q);
        Q.clear();
        return;
    }
    MPI_Request req;
    MPI_Isend(Q.data(), Q.size(), CADO_MPI_UINT32_T, k, 0, pi->m->pals, &req);
    outstanding.push_back(req);
    outstanding_queues.emplace_back(std::move(Q));
    if (!avail_queues.empty()) {
        /* Do this to allow re-use of queues that we used recently, and
         * that might have useful allocation to re-use */
        Q = std::move(avail_queues.back());
        avail_queues.pop_back();
    }
}/*}}}*/

void dispatcher::post_semaphore_blocking(unsigned int k)/*{{{*/
{
    if (k != pi->m->jrank) {
        /* we might as well do it in a blocking way */
        uint32_t z = UINT32_MAX;
        MPI_Send(&z, 1, CADO_MPI_UINT32_T, k, 0, pi->m->pals);
    }
}/*}}}*/
void dispatcher::post_semaphore_nonblocking(unsigned int k)/*{{{*/
{
    if (k == pi->m->jrank) return;
    /* do it non-blocking */
    uint32_t z = UINT32_MAX;
    MPI_Request req;
    MPI_Isend(&z, 1, CADO_MPI_UINT32_T, k, 0, pi->m->pals, &req);
    outstanding.push_back(req);
    /* just for the show. We need progress() to find data that is
     * compatible with what it expects */
    outstanding_queues.emplace_back();
}/*}}}*/

void dispatcher::progress(bool wait)/*{{{*/
{
    int n_in, n_out;
    n_in = n_out = outstanding.size();
    if (!n_in) return;
#ifndef RELY_ON_MPI_THREAD_MULTIPLE
    /* If we're supposed to deal with progress by ourselves, then it's
     * handled in the caller */
    ASSERT_ALWAYS(!wait);
#else
    if (wait) {
        MPI_Waitall(n_in, outstanding.data(), MPI_STATUSES_IGNORE);
        outstanding.clear();
        outstanding_queues.clear();
        avail_queues.clear();
        return;
    }
#endif
    indices.assign(n_in, 0);
    // statuses.assign(n_in, 0);
    int err = MPI_Testsome(n_in, outstanding.data(),
            &n_out, indices.data(),
            MPI_STATUSES_IGNORE);
    ASSERT_ALWAYS(!err);
    ASSERT_ALWAYS(n_out != MPI_UNDEFINED);
    indices.erase(indices.begin()+n_out, indices.end());
    std::sort(indices.begin(), indices.end());
    for(int i = 0, j = 0 ; i < n_in ; i++) {
        if (j >= (int) indices.size() || i < indices[j]) {
            /* request has not completed */
            if (j == 0) continue;
            outstanding[i-j] = outstanding[i];
            std::swap(outstanding_queues[i-j], outstanding_queues[i]);
        } else {
            ASSERT_ALWAYS(i == indices[j]);
            /* request *has* completed. Do nothing, then. We'll keep the
             * completed request and associated queues at the end, and
             * trim then in one go when we're done */
            j++;
        }
    }
    for(int j = n_in - n_out ; j < n_in ; j++) {
        auto & Q = outstanding_queues[j];
        Q.clear();
        avail_queues.emplace_back(std::move(Q));
    }
    outstanding.erase(outstanding.begin() + n_in-n_out, outstanding.end());
    outstanding_queues.erase(outstanding_queues.begin() + n_in-n_out, outstanding_queues.end());
}/*}}}*/

void dispatcher::reader_compute_offsets()/*{{{*/
{
    ASSERT_ALWAYS(is_reader());
    unsigned int ridx = readers_index[pi->m->jrank];

    // Let R == nreaders.
    // All R nodes read from the rw file and deduce the byte size of the
    // orginal submatrix that has 1/R-th of the rows.
    std::string rwfile;
    {
        char * tmp = derived_filename(mfile.c_str(), "rw", ".bin");
        rwfile = tmp;
        free(tmp);
    }
    bool can_read_rw = access(rwfile.c_str(), R_OK) == 0;

    bytes_per_reader.assign(nreaders, 0);

#if 0
    /* This the naive-optimistic approach where we expect all row
     * fragments in the matrix to have equal size */
    subdivision readers_rows(bal->h->nrows, nreaders);
    unsigned int row0 = readers_rows.nth_block_start(ridx);
    unsigned int row1 = readers_rows.nth_block_end(ridx);
    ASSERT_ALWAYS(!is_reader() || can_read_rw);
    FILE * frw = fopen(rwfile.c_str(), "rb");
    ASSERT_ALWAYS(frw);
    fseek(frw, row0 * sizeof(uint32_t), SEEK_SET);
    std::vector<uint32_t> rw(row1-row0,0);
    int r = fread(&rw[0], sizeof(uint32_t), row1-row0, frw);
    ASSERT_ALWAYS(r == (int) (row1 - row0));
    fclose(frw);
    for(unsigned int i = row0 ; i < row1 ; i++) {
        bytes_per_reader[ridx] += sizeof(uint32_t) * (1 + rw[i - row0] * (1 + withcoeffs));
    }
    // This data is then allgathered into an array of R integers. Each
    // node thus determines at which offset it should read from the main
    // matrix.
    //
    MPI_Allgather(MPI_IN_PLACE, 0, 0,
            bytes_per_reader.data(), 1, CADO_MPI_SIZE_T,
            reader_comm);
#else

    row0_per_reader.assign(nreaders, 0);
    row0_per_reader.push_back(bal->h->nrows);

    if (ridx == 0) {
        int rc;
        struct stat sbuf[1];
        rc = stat(mfile.c_str(), sbuf);
        ASSERT_ALWAYS(rc == 0);
        size_t matsize = sbuf->st_size;
        ASSERT_ALWAYS(can_read_rw);
        FILE * frw = fopen(rwfile.c_str(), "rb");
        ASSERT_ALWAYS(frw);
        rc = fseek(frw, 0, SEEK_END);
        ASSERT_ALWAYS(rc == 0);
        long endpos = ftell(frw);
        ASSERT_ALWAYS(endpos >= 0);
        ASSERT_ALWAYS((size_t) endpos == bal->h->nrows * sizeof(uint32_t));
        rc = fseek(frw, 0, SEEK_SET);
        ASSERT_ALWAYS(rc == 0);
        std::vector<uint32_t> rw(bal->h->nrows,0);
        rc = fread(rw.data(), sizeof(uint32_t), bal->h->nrows, frw);
        ASSERT_ALWAYS(rc == (int) bal->h->nrows);
        fclose(frw);

        uint32_t r = 0;
        size_t s = 0;
        size_t last_s = 0;
        size_t qsize = matsize / nreaders;
        int rsize = matsize % nreaders;
        for(int i = 1 ; i < nreaders ; i++) {
            /* want to find first row for reader i */
            size_t want = i * qsize + (i < rsize);
            for( ; r < bal->h->nrows && s < want ; )
                s += sizeof(uint32_t) * (1 + rw[r++] * (1 + withcoeffs));
            /* start it at row r */
            row0_per_reader[i] = r;
            bytes_per_reader[i-1] = s-last_s;
            last_s = s;
        }
        /* finish the table for consistency */
        for( ; r < bal->h->nrows ; )
            s += sizeof(uint32_t) * (1 + rw[r++] * (1 + withcoeffs));
        ASSERT_ALWAYS(s == matsize);
        bytes_per_reader[nreaders-1] = matsize-last_s;

        for(unsigned int i = 0 ; i < pi->m->njobs ; i++) {
            if (!is_reader(i)) continue;
            int r = readers_index[i];
            fmt::printf("Job %d (reader number %d) reads rows %" PRIu32 " to %" PRIu32 " and expects %s\n",
                    i, r,
                    row0_per_reader[r],
                    row0_per_reader[r+1],
                    size_disp(bytes_per_reader[r]));
        }
    }
    MPI_Bcast(row0_per_reader.data(), row0_per_reader.size(), CADO_MPI_UINT32_T, 0, reader_comm);
    MPI_Bcast(bytes_per_reader.data(), bytes_per_reader.size(), CADO_MPI_SIZE_T, 0, reader_comm);
    row0 = row0_per_reader[ridx];
    row1 = row0_per_reader[ridx+1];
#endif

    offset_per_reader = bytes_per_reader;
    integrate(offset_per_reader);
}/*}}}*/

void dispatcher::reader_thread()/*{{{*/
{
    if (!is_reader()) return;

    unsigned int ridx = readers_index[pi->m->jrank];

    FILE * f = fopen(mfile.c_str(), "rb");
    ASSERT_ALWAYS(f);
    int rc = fseek(f, offset_per_reader[ridx], SEEK_SET);
    ASSERT_ALWAYS(rc == 0);

    std::vector<uint32_t> row;
    std::vector<std::vector<uint32_t>> nodedata(nvjobs);
    std::vector<std::vector<uint32_t>> queues(pi->m->njobs);

    size_t queue_size_per_peer = 1 << 16;

    std::vector<uint64_t> check_vector;
    if (pass_number == 2 && !check_vector_filename.empty())
        check_vector.assign(row1 - row0, 0);

    /* display logarithmically-spaced reports until 1/100-th, and then
     * split that evenly so that we print at most 20+log_2(size/nreaders/2^14)
     * lines
     */
    size_t z = 0;
    size_t last_z = 0;
    size_t disp_z = 1 << 14;
    for( ; disp_z * 20 < bytes_per_reader[0] ; disp_z <<= 1);
    size_t disp_zx = 1 << 14;

    double t0 = wct_seconds();

#ifndef RELY_ON_MPI_THREAD_MULTIPLE
    int active_peers = nreaders-1;
#endif

    for(unsigned int i = row0 ; i < row1 ; i++) {
#ifndef RELY_ON_MPI_THREAD_MULTIPLE
        watch_incoming_on_reader(active_peers);
#endif
        uint32_t rr = fw_rowperm[i - row0];
        // Readers read full lines from the matrix.
        uint32_t w;
        rc = fread(&w, sizeof(uint32_t), 1, f);
        ASSERT_ALWAYS(w <= (1 + withcoeffs) * fw_colperm.size());
        if (rc != 1) {
            fprintf(stderr, "%s: short read\n", mfile.c_str());
            exit(EXIT_FAILURE);
        }
        z += rc * sizeof(uint32_t);
        row.assign(w * (1 + withcoeffs), 0);
        int ww = w * (1 + withcoeffs);
        rc = fread(row.data(), sizeof(uint32_t), ww, f);
        if (rc != ww) {
            fprintf(stderr, "%s: short read\n", mfile.c_str());
            exit(EXIT_FAILURE);
        }
        z += ww * sizeof(uint32_t);
        // Column indices are transformed.
        for(int j = 0 ; j < ww ; j += 1 + withcoeffs) {
            uint32_t cc = fw_colperm[row[j]];
            unsigned int k = cc / cols_chunk_big;
            ASSERT_ALWAYS(k < nodedata.size());
            nodedata[k].push_back(cc);
            if (pass_number == 2 && withcoeffs)
                nodedata[k].push_back(row[j+1]);
            if (pass_number == 2 && !check_vector_filename.empty()) {
                uint32_t q = balancing_pre_shuffle(bal, row[j]);
                check_vector[i - row0] ^= DUMMY_VECTOR_COORD_VALUE(q);
            }
        }

        // Queues to all nodes are filled
        for(unsigned int k = 0 ; k < nodedata.size() ; k++) {
            auto & C = nodedata[k];
            if (C.empty()) continue;

            unsigned int group = rr / rows_chunk_big * nvjobs + k;
            auto & Q = queues[group];
            Q.push_back(rr);
            Q.push_back(C.size());
            Q.insert(Q.end(), C.begin(), C.end());

            /* very important */
            C.clear();

            // and sends are posted every once in a while.
            if (Q.size() >= queue_size_per_peer)
                post_send(Q, group);
        }
        if ((z - last_z) > disp_zx && ridx == 0) {
            double dt = wct_seconds()-t0;
            if (dt <= 0) dt = 1e-9;
            fmt::printf("pass %d, J%u (reader 0/%d): %s in %.1fs, %s/s\n",
                    pass_number, pi->m->jrank, nreaders,
                    size_disp(z), dt, size_disp(z / dt));
            fflush(stdout);
            if (disp_zx < disp_z) {
                disp_zx <<= 1;
            } else {
                last_z = z;
            }
        }
        progress();
    }
    {
        double dt = wct_seconds()-t0;
        fmt::printf("pass %d, J%u (reader 0/%d): %s in %.1fs, %s/s (done)\n",
                pass_number, pi->m->jrank, nreaders,
                size_disp(z), dt, size_disp(z / dt));
        fflush(stdout);
    }
    for(unsigned int kk = 0 ; kk < pi->m->njobs ; kk++) {
        auto & Q = queues[kk];
        if (!Q.empty())
            post_send(Q, kk);
    }
#ifdef RELY_ON_MPI_THREAD_MULTIPLE
    progress(true);
    for(unsigned int kk = 0 ; kk < pi->m->njobs ; kk++) {
        post_semaphore_blocking(kk);
    }
#else   /* RELY_ON_MPI_THREAD_MULTIPLE */
    for( ; outstanding.size() ; ) {
        watch_incoming_on_reader(active_peers);
        progress();
    }
    for(unsigned int kk = 0 ; kk < pi->m->njobs ; kk++) {
        post_semaphore_nonblocking(kk);
    }
    for( ; outstanding.size() || active_peers ; )  {
        watch_incoming_on_reader(active_peers);
        progress();
    }
#endif  /* RELY_ON_MPI_THREAD_MULTIPLE */
    fclose(f);
    if (pass_number == 2 && !check_vector_filename.empty()) {
        /* Allocate a full vector on the leader node */
        std::vector<uint64_t> full;
        full.assign(bal->trows, 0);
        std::vector<int> sizes(nreaders, 0);
        std::vector<int> displs(nreaders, 0);
        for(int ridx = 0, d = 0 ; ridx < nreaders ; ridx++) {
            sizes[ridx] = row0_per_reader[ridx+1]-row0_per_reader[ridx];
            displs[ridx] = d;
            d += sizes[ridx];
        }
        std::copy(check_vector.begin(), check_vector.end(),
                full.begin() + displs[readers_index[pi->m->jrank]]);
        MPI_Allgatherv(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                full.data(), sizes.data(), displs.data(),
                CADO_MPI_UINT64_T, reader_comm);

        if (readers_index[pi->m->jrank] == 0) {
            FILE * f = fopen(check_vector_filename.c_str(), "wb");
            int rc = fwrite(full.data(), sizeof(uint64_t), bal->h->nrows, f);
            ASSERT_ALWAYS(rc == (int) bal->h->nrows);
            fclose(f);
        }
    }
}/*}}}*/

void dispatcher::reader_fill_index_maps()/*{{{*/
{
    balancing xbal;
    balancing_init(xbal);
    balancing_read(xbal, bfile.c_str());
    uint32_t quo_r = xbal->trows / xbal->h->nh;
    ASSERT_ALWAYS(xbal->trows % xbal->h->nh == 0);

    fw_colperm.assign(xbal->tcols, -1);
    fw_rowperm.assign(xbal->trows, -1);

    if (xbal->h->flags & FLAG_REPLICATE) {
        uint32_t * xc = xbal->colperm;
        uint32_t * xr = xbal->rowperm;
        ASSERT_ALWAYS(xbal->tcols == xbal->trows);
        /* currently we seem to be supporting this only in case the
         * column permutation is authoritative. */
        if (!xc) {
            fprintf(stderr, "The current code expects a column permutation replicated on rows, not the converse. There is little adaptation work, but yet to be done. Maybe you could pass \"--reorder columns\" to mf_bal ?\n");
            abort();
        }
        ASSERT_ALWAYS(xc && !xr);
        xr = xc;
        for (uint32_t i = 0; i < xbal->tcols; i++) {
            ASSERT_ALWAYS(xc[i] < xbal->tcols);
            uint32_t q = balancing_pre_unshuffle(bal, xc[i]);
            ASSERT_ALWAYS(fw_colperm[q] == UINT32_MAX);
            fw_colperm[q] = i;
        }
        /* In this case we arrange so that the replicated permutation is so
         * that eventually, we are still computing iterates of a matrix
         * which is conjugate to the one we're interested in */

        uint32_t nh = xbal->h->nh;
        uint32_t nv = xbal->h->nv;
        ASSERT_ALWAYS(xbal->trows % (nh * nv) == 0);
        uint32_t elem = xbal->trows / (nh * nv);
        uint32_t ix = 0;
        uint32_t iy = 0;
        for(uint32_t i = 0 ; i < nh ; i++) {
            for(uint32_t j = 0 ; j < nv ; j++) {
                ix = (i * nv + j) * elem;
                iy = (j * nh + i) * elem;
                for(uint32_t k = 0 ; k < elem ; k++) {
                    ASSERT(fw_rowperm[xr[iy+k]] == UINT32_MAX);
                    fw_rowperm[xr[iy+k]] = ix+k;
                }
            }
        }
    } else {
        /* In this case, because the row and column permutations depend
         * on the splitting, if we happen to be in block Wiedemann
         * context, and compute iterates of the form M^i, then we will
         * compute something which depends on the splitting. There is no
         * way we can reconcile what we're doing in a consistent way.
         * Therefore we don't bother trying to undo the effect of the
         * shuffled product.
         */

        uint32_t * xc = xbal->colperm;
        for (uint32_t i = 0; i < xbal->tcols; i++) {
            uint32_t j = xc ? xc[i] : i;
            ASSERT_ALWAYS(j < xbal->tcols);
            ASSERT_ALWAYS(fw_colperm[j] == UINT32_MAX);
            fw_colperm[j] = i;
        }

        uint32_t * xr = xbal->rowperm;
        for (uint32_t i = 0; i < xbal->trows; i++) {
            uint32_t j = xr ? xr[i] : i;
            ASSERT_ALWAYS(j < xbal->trows);
            ASSERT(fw_rowperm[j] == UINT32_MAX);
            fw_rowperm[j] = i;
        }
    }
    /* one more check. The cost is tiny compared to what we do in other
     * parts of the code. This does not deserve output. */
    std::vector<uint32_t> ttab(xbal->h->nh, 0);
    for (uint32_t j = 0; j < xbal->trows; j++) {
        ttab[fw_rowperm[j] / rows_chunk_small]++;
    }
    ASSERT_ALWAYS(xbal->h->nh == pi->wr[1]->totalsize);
    for (uint32_t k = 0; k < xbal->h->nh; k++) {
        ASSERT_ALWAYS(ttab[k] == quo_r);
    }

    fw_rowperm.erase(fw_rowperm.begin() + row1, fw_rowperm.end());
    fw_rowperm.erase(fw_rowperm.begin(), fw_rowperm.begin() + row0);


#if 0
    char buf[16];
    size_t sz = xbal->h->ncols * sizeof(*colmap);
    printf("Creating column map of size %s ...", size_disp(sz, buf));
    fflush(stdout);
    colmap = malloc(sz);
    for(uint32_t i = 0 ; i < xbal->h->ncols ; i++) {
        uint32_t q = balancing_pre_shuffle(xbal, i);
        ASSERT_ALWAYS(q < xbal->h->ncols);
        uint32_t c = fw_colperm[q];
        colmap[i].w = DUMMY_VECTOR_COORD_VALUE(q);
        colmap[i].c = fw_colperm[q];
        colmap[i].n = who_has_col(m, c);
    }
    printf(" done\n");
#endif
#if 0
    free(m->colmap);
#endif

    balancing_clear(xbal);
}/*}}}*/

void dispatcher::prepare_pass()/*{{{*/
{
    for(unsigned int i = 0 ; i < pi->m->ncores ; i++) {
        ASSERT_ALWAYS(args_per_thread[i]->p == NULL);
        ASSERT_ALWAYS(args_per_thread[i]->size == 0);
    }
    if (pass_number == 1) {
        // Each node allocates a local row weight info for each of its
        // theads.
        decltype(thread_row_weights)::value_type v;
        v.assign(transpose ? cols_chunk_small : rows_chunk_small, 0);
        thread_row_weights = { pi->m->ncores, v };
    } else if (pass_number == 2) {
        for(unsigned int i = 0 ; i < pi->m->ncores ; i++) {
            auto & C = thread_row_weights[i];

            decltype(thread_row_positions)::value_type Cint(C.begin(), C.end());

            /* take into account the subrow length before taking the
             * integral */
            if (withcoeffs)
                for(auto & x : Cint) x = 1 + x * 2;
            else
                for(auto & x : Cint) x++;
            uint32_t w = integrate(Cint);

            args_per_thread[i]->size = w;
            args_per_thread[i]->p = (uint32_t *) malloc(w * sizeof(uint32_t));
            memset(args_per_thread[i]->p, 0, w * sizeof(uint32_t));
            /* place markers for the row weights.  */
            for(unsigned int j = 0 ; j < C.size() ; j++) {
                args_per_thread[i]->p[Cint[j]] = C[j];
                Cint[j]++;
            }

            thread_row_positions.emplace_back(std::move(Cint));
            C.clear();
        }
        thread_row_weights.clear();
    }
}/*}}}*/

void dispatcher::endpoint_handle_incoming(std::vector<uint32_t> & Q)/*{{{*/
{
    std::lock_guard<std::mutex> dummy(incoming_mutex);

    for(auto next = Q.begin() ; next != Q.end() ; ) {
        ASSERT_ALWAYS(next < Q.end());
        uint32_t rr = *next++;
        uint32_t rs = *next++;
        if (pass_number == 2 && withcoeffs) rs/=2;

        unsigned int n_row_groups = pi->wr[1]->ncores;
        unsigned int n_col_groups = pi->wr[0]->ncores;
        unsigned int row_group = (rr / rows_chunk_small) % n_row_groups;
        unsigned int row_index = rr % rows_chunk_small;

        /* Does it make sense anyway ? In the transposed case we
         * don't do this...
         * When transposing, we're receiving fragments of columns,
         * which begin with a _column_ index. No real point in sorting
         * what we put in the rows (if done, it must be at the end).
         */

        if (!transpose && pass_number == 2) {
            if (!withcoeffs) {
                std::sort(next, next + rs);
            } else {
                /* ugly */
                struct cv {
                    uint32_t c;
                    int32_t v;
                    bool operator<(cv const & a) const { return c < a.c; }
                };
                cv * Q0 = (cv *) &next[0];
                cv * Q1 = (cv *) (&next[0] + 2*rs);
                std::sort(Q0, Q1);
            }
        }

        if (pass_number == 1) {
            for(unsigned int j = 0 ; j < rs ; j ++) {
                uint32_t cc = *next++;
                unsigned int col_group = (cc / cols_chunk_small) % n_col_groups;
                unsigned int col_index = cc % cols_chunk_small;
                unsigned int group = row_group * n_col_groups + col_group;
                if (!transpose)
                    thread_row_weights[group][row_index]++;
                else
                    thread_row_weights[group][col_index]++;
                /* on pass 1 we don't do next++ */
            }
        } else if (pass_number == 2) {
            if (!transpose) {
                std::vector<uint32_t *> pointers;
                pointers.reserve(n_col_groups);
                for(unsigned int i = 0 ; i < n_col_groups ; i++) {
                    unsigned int col_group = i;
                    uint32_t group = row_group * n_col_groups + col_group;
                    uint32_t * matrix = args_per_thread[group]->p;
                    size_t pos0 = thread_row_positions[group][row_index];
                    uint32_t * p0 = matrix + pos0;
                    pointers.push_back(p0);
                }
                for(unsigned int j = 0 ; j < rs ; j ++) {
                    uint32_t cc = *next++;
                    unsigned int col_group = (cc / cols_chunk_small) % n_col_groups;
                    uint32_t * & p = pointers[col_group];
                    ASSERT(*p == 0);
                    *p++ = cc % cols_chunk_small;
                    if (withcoeffs) {
                        ASSERT(*p == 0);
                        *p++ = *next++;
                    }
                }
                for(unsigned int i = 0 ; i < n_col_groups ; i++) {
                    unsigned int col_group = i;
                    uint32_t group = row_group * n_col_groups + col_group;
                    uint32_t * matrix = args_per_thread[group]->p;
                    size_t pos0 = thread_row_positions[group][row_index];
                    uint32_t * p0 = matrix + pos0;
                    /* verify consistency with the first pass */
                    ASSERT_ALWAYS((pointers[col_group] - p0) == (ptrdiff_t) ((1 + withcoeffs) * p0[-1]));
                }
            } else {
                /* It's slightly harder than in the non-transposed case.
                 * We'll adjust the thread_row_positions each time we
                 * receive new stuff, and there's little sanity checking
                 * that we can effectively do.
                 */
                /* for clarity -- we might as well do a swap() */
                for(unsigned int j = 0 ; j < rs ; j ++) {
                    uint32_t cc = *next++;
                    unsigned int col_group = (cc / cols_chunk_small) % n_col_groups;
                    unsigned int col_index = cc % cols_chunk_small;
                    unsigned int group = row_group * n_col_groups + col_group;
                    uint32_t * matrix = args_per_thread[group]->p;
                    size_t & x = thread_row_positions[group][col_index];
                    ASSERT(matrix[x] == 0);
                    matrix[x++] = row_index;
                    if (withcoeffs) {
                        ASSERT(matrix[x] == 0);
                        matrix[x++] = *next++;
                    }
                }
            }
        }
    }
}/*}}}*/

void dispatcher::endpoint_thread()/*{{{*/
{
    int Qs;
    std::vector<uint32_t> Q;

    /* We must loop as long as there are active readers that want to talk
     * to us. */
    int active_peers = nreaders;
    if (is_reader())
        active_peers--;
    for(;active_peers;) {
        // On pass 1, endpoint threads do Recv from any source, and update the
        // local row weight for all threads.
        MPI_Status status;
        MPI_Probe(MPI_ANY_SOURCE, 0, pi->m->pals, &status);
        MPI_Get_count(&status, CADO_MPI_UINT32_T, &Qs);
        Q.assign(Qs, 0);
        MPI_Recv(Q.data(), Qs, CADO_MPI_UINT32_T, status.MPI_SOURCE, 0, pi->m->pals, MPI_STATUS_IGNORE);
        if (Qs == 1 && Q[0] == UINT32_MAX) {
            active_peers--;
            continue;
        }
        endpoint_handle_incoming(Q);
    }
}/*}}}*/

void dispatcher::watch_incoming_on_reader(int &active_peers)
{
    if (!active_peers) return;

    MPI_Status status;
    int flag = 0;
    MPI_Iprobe(MPI_ANY_SOURCE, 0, pi->m->pals, &flag, &status);
    if (!flag) return;

    int Qs;
    std::vector<uint32_t> Q;

    // On pass 1, endpoint threads do Recv from any source, and update the
    // local row weight for all threads.
    MPI_Get_count(&status, CADO_MPI_UINT32_T, &Qs);
    Q.assign(Qs, 0);
    MPI_Recv(Q.data(), Qs, CADO_MPI_UINT32_T, status.MPI_SOURCE, 0, pi->m->pals, MPI_STATUS_IGNORE);
    if (Qs == 1 && Q[0] == UINT32_MAX) {
        active_peers--;
        return;
    }
    endpoint_handle_incoming(Q);
}


void dispatcher::stats()
{
    if (!verbose_enabled(CADO_VERBOSE_PRINT_BWC_DISPATCH_OUTER)) return;
    uint32_t quo_r = bal->trows / bal->h->nh;
    for(unsigned int k = 0 ; k < pi->m->ncores ; k++) {
        printf("[J%uT%u] N=%" PRIu32 " W=%zu\n",
                pi->m->jrank, k,
                quo_r,
                (args_per_thread[k]->size-quo_r)/(1+withcoeffs));
    }
    /*
    uint32_t * row_weights = s->row_weights;
    uint32_t my_nrows = s->s->my_nrows;
    uint32_t * col_weights = s->col_weights;
    uint32_t my_ncols = s->s->my_ncols;
    // some stats
    double s1 = 0;
    double s2 = 0;
    uint64_t tw = 0;
    uint32_t row_min = UINT32_MAX, row_max = 0;
    double row_avg, row_dev;
    s1 = s2 = 0;
    for (uint32_t i = 0; i < my_nrows; i++) {
       double d = row_weights[i];
       if (row_weights[i] > row_max)
           row_max = row_weights[i];
       if (row_weights[i] < row_min)
           row_min = row_weights[i];
       s1 += d;
       s2 += d * d;
       tw += row_weights[i];
    }
    row_avg = s1 / my_nrows;
    row_dev = sqrt(s2 / my_nrows - row_avg * row_avg);

    uint32_t col_min = UINT32_MAX, col_max = 0;
    double col_avg, col_dev;
    s1 = s2 = 0;
    for (uint32_t j = 0; j < my_ncols; j++) {
       double d = col_weights[j];
       if (col_weights[j] > col_max)
           col_max = col_weights[j];
       if (col_weights[j] < col_min)
           col_min = col_weights[j];
       s1 += d;
       s2 += d * d;
    }
    col_avg = s1 / my_ncols;
    col_dev = sqrt(s2 / my_ncols - col_avg * col_avg);

    printf("[J%uT%u] N=%" PRIu32 " W=%" PRIu64 " "
          "R[%" PRIu32 "..%" PRIu32 "],%.1f~%.1f "
          "C[%" PRIu32 "..%" PRIu32 "],%.1f~%.1f.\n",
          s->s->pi->m->jrank, s->s->pi->m->trank,
           my_nrows, tw,
          row_min, row_max, row_avg, row_dev,
          col_min, col_max, col_avg, col_dev);
    */
}


//
// Once this is all done, the reading threads adjust pointers for the
// row beginnings of each local matrix.
//
// A second pass is then done.
//
// The hard question is how we deal with MPI. What do we post in a
// blocking way, what do we post in a non-blockiing way, etc. It
// seems non trivial.
//

void dispatcher::main() {
    if (is_reader()) {
        reader_compute_offsets();
        // Each reader reads a fragment of N/R row index transformations.
        // Each reader reads the full set if N col index transformations.
        reader_fill_index_maps();
    }

    for(pass_number = 1 ; pass_number <= 2 ; pass_number++) {
        prepare_pass();
#ifdef RELY_ON_MPI_THREAD_MULTIPLE
        /* Note that if we don't have MPI_THREAD_MULTIPLE, then we always
         * have nreaders==1 no matter what
         */
        if (nreaders == 1) {
            if (is_reader())
                reader_thread();
            else
                endpoint_thread();
        } else {
            // Each reader spawns a writing (to MPI) thread (reading from disk).
            // (non-readers exit immediately)
            std::thread reader([this] { reader_thread(); });

            // Each node spawns a reading (from MPI) thread.
            std::thread endpoint([this] { endpoint_thread(); });

            reader.join();
            endpoint.join();
        }
#else   /* RELY_ON_MPI_THREAD_MULTIPLE */
        /* then the reader thread has to incorporate special code so as
         * to play both roles
         */
            if (is_reader())
                reader_thread();
            else
                endpoint_thread();
#endif  /* RELY_ON_MPI_THREAD_MULTIPLE */
    }
}
