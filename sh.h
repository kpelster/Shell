#include "get_path.h"

int pid;
char *which(char *command, struct pathelement *p);
char *where(char *command, struct pathelement *p);
void list(char *dir);
void printenv(char **envp);

#define PROMPTMAX 64
#define MAXARGS   64
#define MAXLINE   128

typedef struct Users{
    char *name;
    struct Users *next;
    struct Users *prev;
} Users_t;
