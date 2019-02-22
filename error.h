#include <stdio.h>			//printf
#include <stdlib.h>			//exit

#define general_check_err(cond, msg) if (cond){perror(msg); exit(1);}
#define check_err(err, msg) general_check_err(err==-1, msg);

#define assert(cond, msg) if (!(cond)){perror(msg); exit(1);}
