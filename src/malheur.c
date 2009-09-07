/*
 * MALHEUR - Automatic Malware Analysis on Steroids
 * Copyright (c) 2009 Konrad Rieck (rieck@cs.tu-berlin.de)
 * Berlin Institute of Technology (TU Berlin).
 * --
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.  This program is distributed without any
 * warranty. See the GNU General Public License for more details. 
 * --
 */

#include "config.h"
#include "malheur.h"
#include "common.h"
#include "mconfig.h"
#include "ftable.h"
#include "fmath.h"
#include "proto.h"
#include "util.h"
#include "export.h"

/* Global variables */
int verbose = 0;
config_t cfg;

/* Local variables */
static char *config_file = CONFIG_FILE;
static char *result_file = NULL;
static char *input_file = NULL;
static char *proto_file = NULL;
static malheur_task_t task = PROTOTYPE;
static int lookup_table = FALSE;

/**
 * Print usage of command line tool
 * @param argc Number of arguments
 * @param argv Argument values
 */
void print_usage(int argc, char **argv)
{
    printf("Usage: malheur [options] <task> <input>\n"
           "Tasks:\n"
           "  kernel        Compute a kernel matrix from malware reports\n"
           "  prototype     Extract prototypes from malware reports\n"
           "  cluster       Cluster malware reports into similar groups\n"
           "Options:\n"
           "  -c <file>     Set configuration file.\n"
           "  -r <file>     Save analysis results to file.\n"   
           "  -l <file>     Load feature vectors of prototypes from file.\n"  
           "  -s <file>     Save feature vectors of prototypes to file.\n"  
           "  -t            Enable feature lookup table.\n"
           "  -v            Increase verbosity.\n"
           "  -V            Print version and copyright.\n"
           "  -h            Print this help screen.\n");
}

/**
 * Print version and copyright information
 */
void print_version()
{
    printf(" MALHEUR - Automatic Malware Analysis on Steroids\n"
           " Copyright (c) 2009 Konrad Rieck (rieck@cs.tu-berlin.de)\n"
           " Berlin Institute of Technology (TU Berlin).\n");
}

/**
 * Parse command line options
 * @param argc Number of arguments
 * @param argv Argument values
 */
void parse_options(int argc, char **argv)
{
    int ch;
    while ((ch = getopt(argc, argv, "l:s:tr:c:hvV")) != -1) {
        switch (ch) {
        case 'v':
            verbose++;
            break;
        case 'c':
            config_file = optarg;
            break;
        case 'r':
            result_file = optarg;
            break;
        case 'l':
        case 's':
            proto_file = optarg;
            break;
        case 't':
            lookup_table = TRUE;
            break;
        case 'V':
            print_version();
            exit(EXIT_SUCCESS);
            break;
        case 'h':
        case '?':
            print_usage(argc, argv);
            exit(EXIT_SUCCESS);
            break;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc != 2)
        fatal("<task> and <input> arguments are required");

    /* Argument: Task */
    if (!strcasecmp(argv[0], "prototype"))
        task = PROTOTYPE;
    else if (!strcasecmp(argv[0], "kernel"))
        task = KERNEL;
    else if (!strcasecmp(argv[0], "cluster"))
        task = CLUSTER;
    else
        fatal("Unknown analysis task '%s' for Malheur", argv[0]);

    /* Argument: Input */
    input_file = argv[1];
    if (access(input_file, R_OK))
        fatal("Could not access '%s'", input_file); 
    
    /* Sanity checks */
    switch(task) {
    case PROTOTYPE:
        if (!proto_file && !result_file)
            fatal("No output specified. See options '-s' and/or '-r'");
        break;
    case KERNEL:
        if (!result_file)
            fatal("No output specified. See option '-r'");
        if (proto_file)
            warning("Prototypes will not be extracted in this task"); 
        break;
    case CLUSTER:
        break;
    }
}

/**
 * Determines prototypes for the given malware reports
 */
static void malheur_prototype()
{
    /* Load data */
    farray_t *fa = farray_extract(input_file);
    proto_t *pr = proto_extract(fa);
    
    if (verbose > 1)
        proto_print(pr);

    /* Export prototypes */
    if (result_file)
        export_proto(pr, fa, result_file);

    /* Save prototype vectors */
    if (proto_file) 
        proto_save_file(pr, proto_file);

    /* Clean up */
    proto_destroy(pr);
    farray_destroy(fa);
}

/**
 * Clusters the given malware reports
 */
static void malheur_cluster()
{
    /* Load data */
    farray_t *fa = farray_extract(input_file);        

    farray_destroy(fa);        
}

/**
 * Computes a kernel matrix and saves the result to afile
 */
static void malheur_kernel()
{
    /* Load data */
    farray_t *fa = farray_extract(input_file);
                
    /* Compute similarity matrix */
    double *d = malloc(fa->len * fa->len * sizeof(double));
    if (!d)
        fatal("Could not allocate similarity matrix");
    farray_dot(fa, fa, d);
        
    /* Save kernel matrix */
    export_kernel(d, fa, result_file);

    /* Clean up */
    free(d);
    farray_destroy(fa);
}

/**
 * Initialize malheur tool
 * @param argc Number of arguments
 * @param argv Argument values
 */
static void malheur_init(int argc, char **argv)
{
    /* Parse options */
    parse_options(argc, argv);

    /* Init and load configuration */
    config_init(&cfg);
    if (config_read_file(&cfg, config_file) != CONFIG_TRUE)
        fatal("Could not read configuration (%s in line %d)",
              config_error_text(&cfg), config_error_line(&cfg));
    
    /* Check configuration */
    config_check(&cfg);
    if (verbose > 1)
        config_print(&cfg);
    
    /* Init feature lookup table */
    if (lookup_table)
        ftable_init();
}

/**
 * Exits the malheur tool.
 */
static void malheur_exit()
{
    if (lookup_table)
        ftable_destroy();

    /* Destroy configuration */
    config_destroy(&cfg);
}
 
/**
 * Main function of Malheur
 * @param argc Number of arguments
 * @param argv Argument values
 * @return Exit code
 */
int main(int argc, char **argv)
{
    malheur_init(argc, argv);

    /* Perform task */
    switch (task) {
    case KERNEL:
        malheur_kernel();
        break;
    case PROTOTYPE:
        malheur_prototype();
        break;
    case CLUSTER:
        malheur_cluster();
        break;
    }

    malheur_exit();
    return EXIT_SUCCESS;
}
