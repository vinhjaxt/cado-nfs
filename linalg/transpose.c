#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include "cado_config.h"
#include "macros.h"
#include "readmat.h"

/* Number of unsigned ints that fit in RAM */
#define	MEM_LIMIT	(1UL << 27)	

/* Rows processed simultaneously when outputting the result */
#define NROWS_CORE      20

/* Maximal length of temporary file names */
#define TEMP_FILE_LENGTH 256

unsigned int nfiles = 0;

const char * tmpdir;
const char * filename_in;
const char * filename_out;

struct vec_uint_struct {
    unsigned int * data;
    unsigned int size;
    unsigned int alloc;
};

typedef struct vec_uint_struct vec_uint_t[1];

filter_matrix_t mat;
vec_uint_t * transposed;

unsigned int vec_next_size(unsigned int x)
{
    return MAX(x * 2, x + 16);
}

void vec_uint_init(vec_uint_t x)
{
    memset(x, 0, sizeof(vec_uint_t));
}
void vec_uint_clear(vec_uint_t x)
{
    free(x->data);
    memset(x, 0, sizeof(vec_uint_t));
}
void vec_uint_init_array(vec_uint_t ** x, unsigned int n)
{
    *x = (vec_uint_t *) malloc(n * sizeof(vec_uint_t));
    memset(*x, 0, n * sizeof(vec_uint_t));
}
void vec_uint_clear_array(vec_uint_t ** x, unsigned int n)
{
    unsigned int j;
    for(j = 0 ; j < n ; j++) {
        free((*x)[j]->data);
    }
    memset((*x), 0, n * sizeof(vec_uint_t));
}
void vec_uint_push_back(vec_uint_t ptr, unsigned int x)
{
    if (ptr->size >= ptr->alloc) {
        ptr->alloc = vec_next_size(ptr->alloc);
        ptr->data = realloc(ptr->data, ptr->alloc * sizeof(unsigned int));
        ASSERT_ALWAYS(ptr->data != NULL);
    }
    ptr->data[ptr->size++] = x;
}

void transpose_and_save(unsigned int i0, unsigned int i1)
{
    char dstfile[TEMP_FILE_LENGTH];
    unsigned int ret;

    const unsigned int * src = mat->data;
    unsigned int i, j, c;

    for(i = i0 ; i < i1 ; i++) {
        unsigned int nc = * src++;
        for(j = 0 ; j < nc ; j++) {
            c = *src++;
            ASSERT(c < mat->ncols);
            vec_uint_push_back(transposed[c], i);
            ASSERT(transposed[c]->size < mat->nrows);
        }
    }

    ret = snprintf(dstfile, sizeof(dstfile), "%s/temp.%u.%04d",
            tmpdir, getpid(), nfiles);
    ASSERT_ALWAYS(ret < sizeof(dstfile));

    FILE * out;
    out = fopen(dstfile, "w"); ASSERT_ALWAYS(out);
    for(j = 0 ; j < mat->ncols ; j++) {
        fprintf(out, "%d", transposed[j]->size);
        for(c = 0 ; c < transposed[j]->size ; c++) {
            fprintf(out, " %d", transposed[j]->data[c]);
        }
        fprintf(out, "\n");
        // reset the size pointer for next chunk
        transposed[j]->size = 0;
    }
    fclose(out);

    /*
    fprintf(stderr, "Written %s (rows %u to %u, transposed)\n",
            dstfile, i0, i1);
            */
}

// skip first 'skip' (heavy) rows, corresponding to heaviest rows in original
// matrix.
void rebuild(int skip)
{
    FILE * fout;
    FILE ** fin_vchunks;
    unsigned int i, j, lastsize = 0, minskip = UINT_MAX, maxskip = 0;
    int ret, skip0 = skip;

    fout = fopen(filename_out, "w");
    ASSERT_ALWAYS(fout != NULL); //  "Cannot open output matrix"

    fprintf(fout, "%u %u\n", mat->ncols - skip, mat->nrows);

    fin_vchunks = malloc(nfiles * sizeof(FILE *));
    for(i = 0 ; i < nfiles ; i++) {
        char dstfile[TEMP_FILE_LENGTH];
        unsigned int ret;
        ret = snprintf(dstfile, sizeof(dstfile), "%s/temp.%u.%04d",
                       tmpdir, getpid(), i);
        ASSERT_ALWAYS(ret < sizeof(dstfile));
        fin_vchunks[i] = fopen(dstfile, "r");
        ASSERT_ALWAYS(fin_vchunks[i]);
    }

    vec_uint_t * rows;
    vec_uint_init_array(&rows, NROWS_CORE);

    for(j = 0 ; j < mat->ncols ; j+= NROWS_CORE) {
        unsigned int k;
        unsigned int kmax = MIN(NROWS_CORE, mat->ncols - j);
        for(k = 0 ; k < NROWS_CORE ; k++) {
            rows[k]->size = 0;
        }
        for(i = 0 ; i < nfiles ; i++) {
            unsigned int nc;
            unsigned int c;
            for(k = 0 ; k < kmax ; k++) {
                ret = fscanf(fin_vchunks[i], "%u", &nc);
                ASSERT_ALWAYS(ret == 1);
                ret = 0;
                for(c = 0 ; c < nc ; c++) {
                    unsigned int idx;
                    ret += fscanf(fin_vchunks[i], "%u", &idx);
                    vec_uint_push_back(rows[k], idx);
                }
                ASSERT_ALWAYS(ret == (int) nc);
            }
        }
        for(k = 0 ; k < kmax ; k++) {
            unsigned int c;
            /* If we're not skipping, it's not a problem to have an
             * unbalanced matrix */
            if (k > 0 && skip > 0 && rows[k]->size > lastsize)
              {
                fprintf (stderr, "Error, matrix rows are not in decreasing weight\n");
                exit (1);
              }
            lastsize = rows[k]->size;
	    if(skip > 0){
                if (lastsize > maxskip)
                  maxskip = lastsize;
                if (lastsize < minskip)
                  minskip = lastsize;
		skip--;
		continue;
	    }
            fprintf(fout, "%d", rows[k]->size);
            for(c = 0 ; c < rows[k]->size ; c++) {
                fprintf(fout, " %d", rows[k]->data[c]);
            }
            fprintf(fout, "\n");
        }
    }
    vec_uint_clear_array(&rows, NROWS_CORE);
    for(i = 0 ; i < nfiles ; i++) {
        fclose(fin_vchunks[i]);
        char dstfile[TEMP_FILE_LENGTH];
        unsigned int ret;
        ret = snprintf(dstfile, sizeof(dstfile), "%s/temp.%u.%04d",
                tmpdir, getpid(), i);
        ASSERT_ALWAYS(ret < sizeof(dstfile));
        unlink(dstfile);
    }
    free(fin_vchunks);
    if (skip0)
        fprintf (stderr, "Skipped %d rows of weight %d to %d\n",
                skip0, minskip, maxskip);
}

void usage()
{
    fprintf(stderr, "usage: ./transpose -in <input matrix> -out <output matrix> [ -T <tmpdir> ] [ -skip <nrows to skip> ]\n");
    exit(1);
}

int main(int argc, char * argv[])
{

    FILE * fin;
    int skip = 0;

    {
      int i;
      /* print the command line */
      fprintf (stderr, "%s.r%s", argv[0], CADO_REV);
      for (i = 1; i < argc; i++)
        fprintf (stderr, " %s", argv[i]);
      fprintf (stderr, "\n");
    }

    tmpdir = "/tmp";
    while(argc > 1 && argv[1][0] == '-'){
        if(argc > 2 && strcmp (argv[1], "-in") == 0){
            filename_in = argv[2];
            argc -= 2;
            argv += 2;
            continue;
        }
        if(argc > 2 && strcmp (argv[1], "-out") == 0){
            filename_out = argv[2];
            argc -= 2;
            argv += 2;
            continue;
        }
        if(argc > 2 && strcmp (argv[1], "-T") == 0){
            tmpdir = argv[2];
            argc -= 2;
            argv += 2;
            continue;
        }
        if(argc > 2 && strcmp (argv[1], "-skip") == 0){
            skip = atoi(argv[2]);
            argc -= 2;
            argv += 2;
            continue;
        }
        usage();
    }

    fin = fopen(filename_in, "r");
    ASSERT_ALWAYS(fin);

    filter_matrix_init(mat);

    read_matrix_header(fin, mat);

    unsigned int i;
    unsigned int i0;
    unsigned int * dst = mat->data;
    unsigned int wt0 = 0;

    vec_uint_init_array(&transposed, mat->ncols);

    for(i = i0 = 0 ; i < mat->nrows ; ) {
        dst = read_matrix_row(fin, mat, dst, 1);
        i++;

        if (mat->wt >  wt0 + MEM_LIMIT) {
            fprintf(stderr, "Read %u rows and %u coeffs into core,"
                    " saving to tempfile\n", i, mat->wt - wt0);
            transpose_and_save(i0, i);
            i0 = i;
            nfiles++;
            dst = mat->data;
            wt0 = mat->wt;
        }
    }
    if (i != i0) {
        transpose_and_save(i0, i);
        i0 = i;
        nfiles++;
        dst = mat->data;
    }
    vec_uint_clear_array(&transposed, mat->ncols);

    rebuild(skip);

    return 0;
}
/* vim: set sw=4 sta et: */
