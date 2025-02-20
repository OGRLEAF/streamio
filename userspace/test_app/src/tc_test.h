#ifndef _H_TC_TEST
#define _H_TC_TEST

struct arguments
{
    char * input_file_path;
    char * output_file_path;
};


int txrx_test_thread(int argc, char **argv);

#endif
