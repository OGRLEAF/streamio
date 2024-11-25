#include <stdlib.h>
#include <argp.h>
#include <string.h>
#include "tc_test.h"

const char *argp_program_version =
    "argp-ex3 1.0";
const char *argp_program_bug_address =
    "<bug-gnu-utils@gnu.org>";

/* Program documentation. */
static char doc[] =
    "";

/* A description of the arguments we accept. */
static char args_doc[] = "-i input_file -o output_file";

/* The options we understand. */
static struct argp_option options[] = {
    {"input", 'i', "WAVE_FILE", 0, "Input waveform file "},    
    {"output", 'o', "WAVE_FILE", 0, "Output waveform file "},
    {0}};

error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = state->input;

    switch(key) 
    {
        case 'i':
            arguments->input_file_path = arg;
            break;
        case 'o':
            arguments->output_file_path = arg;
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

struct argp argp = {options, parse_opt, args_doc, doc};
