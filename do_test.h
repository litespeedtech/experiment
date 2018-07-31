// Splice vs. non-splice testing include file
// All real programs are required to provide the following:
typedef struct s_test {
    long size;
    long count;
    int  validate;
} t_test;
extern const char *program_name;
int do_test(int sock, char *buffer, long size, long count, int validate);




