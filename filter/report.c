/* report.c --- auxiliary merge program

Copyright 2008-2009 Francois Morain, Paul Zimmermann

This file is part of CADO-NFS.

CADO-NFS is free software; you can redistribute it and/or modify it under the
terms of the GNU Lesser General Public License as published by the Free
Software Foundation; either version 2.1 of the License, or (at your option)
any later version.

CADO-NFS is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
details.

You should have received a copy of the GNU Lesser General Public License
along with CADO-NFS; see the file COPYING.  If not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include "utils.h"
#include "gzip.h"
#include "sparse.h"
#include "dclist.h"
#include "filter_matrix.h"
#include "report.h"

/* initialize the rep data structure
   outname is the name of the output (history) file
   mat is the input matrix
   type = 2 does nothing
*/
void
init_rep (report_t *rep, char *outname, filter_matrix_t *mat, int type,
	  int bufsize)
{
    int32_t** tmp, i;

    rep->type = type;
    if (type == 2) /* do nothing...! */
      return;
    rep->outfile = gzip_open (outname, "w");
    switch (type)
      {
      case 0: /* classical one: output to a file */
	break;
      case 1: /* mostly for MPI */
	/* FIXME: probably a clear_rep function is needed to free tmp */
	tmp = (int32_t **) malloc (mat->nrows * sizeof(int32_t *));
	for(i = 0; i < mat->nrows; i++)
	    tmp[i] = NULL;
	rep->history = tmp;
	rep->mark = -1;
	rep->bufsize = bufsize;
	break;
      default:
	{
	  fprintf(stderr, "Unknown type: %d\n", type);
	  exit (1);
	}
      }
}

/* terrific hack: everybody on the same line
   the first is to be destroyed in replay!!! */
void
reportn (report_t *rep, int32_t *ind, int n)
{
    int i;

#if DEBUG >= 1
    fprintf(stderr, "Reporting for n=%d\n", n);
#endif
    if(rep->type == 2)
	return;
    if(rep->type == 0){
	for(i = 0; i < n; i++){
	    fprintf(rep->outfile, "%ld", (long int) ind[i]);
	    if(i < n-1)
		fprintf(rep->outfile, " ");
	}
	fprintf(rep->outfile, "\n");
    }
    else if((rep->type == 1) && (rep->mark != -2)){
	// mark == -2 => we are probably resuming and we don't care
	// to consume a lot of memory that will not be used, anyway.
	rep->mark += 1;
	if(rep->history[rep->mark] == NULL)
	    rep->history[rep->mark] = 
		(int32_t *)malloc((rep->bufsize+1)*sizeof(int32_t));
	rep->history[rep->mark][0] = n;
	for(i = 0; i < n; i++)
	    rep->history[rep->mark][i+1] = ind[i];
    }
}

/* print a new line "i" in the history file */
void
report1 (report_t *rep, int32_t i)
{
  reportn (rep, &i, 1);
}

/* print a new line "i1 i2" in the history file */
void
report2 (report_t *rep, int32_t i1, int32_t i2)
{
    int32_t tmp[2];

    tmp[0] = i1;
    tmp[1] = i2;
    reportn (rep, tmp, 2);
}

