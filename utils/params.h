#ifndef PARAMS_H_
#define PARAMS_H_

#include <stdio.h>
#include <stdint.h>
#include <gmp.h>

#include "macros.h"

struct param_list_doc_s {
    char * key;
    char * doc;
};
typedef struct param_list_doc_s param_list_doc[1];
typedef struct param_list_doc_s * param_list_doc_ptr;
typedef struct param_list_doc_s const * param_list_doc_srcptr;

/* This is by increasing order of priority */
enum parameter_origin { PARAMETER_FROM_FILE, PARAMETER_FROM_CMDLINE };

struct parameter_s {
    char * key;
    char * value;
    enum parameter_origin origin;
    int parsed;
    int seen;
};
typedef struct parameter_s parameter[1];
typedef struct parameter_s * parameter_ptr;
typedef struct parameter_s const * parameter_srcptr;

struct param_list_alias_s {
    char * alias;
    const char * key;
};

typedef struct param_list_alias_s param_list_alias[1];

struct param_list_switch_s {
    char * switchname;
    int * ptr;
};

typedef struct param_list_switch_s param_list_switch[1];

struct param_list_s {
    // documented parameters
    char * usage_hdr;
    int ndocs;
    int ndocs_alloc;
    param_list_doc * docs;
    // parameters given by user
    unsigned int alloc;
    unsigned int size;
    parameter * p;
    int consolidated;
    // aliases
    param_list_alias * aliases;
    int naliases;
    int naliases_alloc;
    // switches
    param_list_switch * switches;
    int nswitches;
    int nswitches_alloc;
    /* We use this to remember the first command line pointer which have
     * been given to us */
    int cmdline_argc0;
    char ** cmdline_argv0;
    // did the user use the doc functionality ?
    int use_doc;
};

typedef struct param_list_s param_list[1];
typedef struct param_list_s * param_list_ptr;
typedef struct param_list_s const * param_list_srcptr;

#ifdef __cplusplus
extern "C" {
#endif

// in any case, calls to param_list functions overwrite the previously
// set parameters in the parameter list.

extern void param_list_init(param_list pl);
extern void param_list_clear(param_list pl);

// document the usage of a parameter.
extern void param_list_decl_usage(param_list pl, const char * key,
        const char * doc);
extern void param_list_print_usage(param_list pl, const char * argv0, FILE *f);
extern void param_list_usage_header(param_list pl, const char * hdr);

// takes a file, in the Cado-NFS params format, and stores the dictionary
// of parameters to pl.
extern int param_list_read_stream(param_list pl, FILE *f);
extern int param_list_read_file(param_list pl, const char * name);

// sees whether the arguments pointed to by argv[0] and (possibly)
// argv[1] correspond to either -<key> <value>, --<key> <value> or
// <key>=<value> ; configured switches and aliases for the param list are
// also checked.
extern int param_list_update_cmdline(param_list pl,
        int * p_argc, char *** p_argv);

#if 0
// This one allows shorthands. Notice that the alias string has to
// contain the exact form of the wanted alias, which may be either "-x",
// "--x", or "x=" (a terminating = tells the program that the option is
// wanted all in one go, like in ./a.out m=42, in contrast to ./a.out -m
// 42).
extern int param_list_update_cmdline_alias(param_list pl, const char * key,
        const char * alias, int * p_argc, char *** p_argv);
#endif

extern int param_list_parse_int(param_list, const char *, int *);
extern int param_list_parse_long(param_list, const char *, long *);
extern int param_list_parse_uint(param_list, const char *, unsigned int *);
extern int param_list_parse_ulong(param_list, const char *, unsigned long *);
extern int param_list_parse_int64(param_list, const char *, int64_t *);
extern int param_list_parse_uint64(param_list, const char *, uint64_t *);
extern int param_list_parse_double(param_list, const char *, double *);
extern int param_list_parse_string(param_list, const char *, char *, size_t);
extern int param_list_parse_mpz(param_list, const char *, mpz_ptr);
extern int param_list_parse_intxint(param_list pl, const char * key, int * r);
extern int param_list_parse_int_and_int(param_list pl, const char * key, int * r, const char * sep);
extern int param_list_parse_int_list(param_list pl, const char * key, int * r, size_t n, const char * sep);
extern int param_list_parse_size_t(param_list pl, const char * key, size_t * r);
extern int param_list_parse_switch(param_list pl, const char * key);

extern const char * param_list_lookup_string(param_list pl, const char * key);

extern void param_list_save(param_list pl, const char * filename);

// This one allows shorthands. Notice that the alias string has to
// contain the exact form of the wanted alias, which may be either "-x",
// "--x", or "x=" (a terminating = tells the program that the option is
// wanted all in one go, like in ./a.out m=42, in contrast to ./a.out -m
// 42).
extern int param_list_configure_alias(param_list pl, const char * key, const char * alias);

// A switch is a command-line argument which sets a value by its mere
// presence. Could be for instance --verbose, or --use-smart-algorithm
extern int param_list_configure_switch(param_list pl, const char * key, int * ptr);

// tells whether everything has been consumed. Otherwise, return the key
// of the first unconsumed argument.
extern int param_list_all_consumed(param_list pl, char ** extraneous);

// warns against unused command-line parameters. This normally indicates
// a user error. parameters ignored from config files are considered
// normal (although note that in some cases, it could be bad as well).
extern int param_list_warn_unused(param_list pl);

// this one is the ``joker'' call. Return type is for internal use.
extern int param_list_add_key(param_list pl,
        const char *, const char *, enum parameter_origin);
// removing a key can be handy before savin g a config file. Some options
// are relevant only for one particular invokation, and not for saving.
extern void param_list_remove_key(param_list pl, const char * key);

// for debugging.
extern void param_list_display(param_list pl, FILE *f);

// quick way to reinject parameters in the param_list (presumably before
// saving)
extern int param_list_save_parameter(param_list pl, enum parameter_origin o, 
        const char * key, const char * format, ...) ATTR_PRINTF(4,5);

// Some miscellany really. Why here more than somewhere else, not clear.
// But why not...
extern void print_command_line(FILE * stream, int argc, char * argv[]);

// This function is a shorthand which does employ some hackery put into
// param lists, which remember their oldest argv, argc pair.
extern void param_list_print_command_line(FILE * stream, param_list pl);
#ifdef __cplusplus
}
#endif

#endif	/* PARAMS_H_ */
