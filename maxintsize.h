#ifndef maxintsize_h
#define maxintsize_h

#include <limits.h>

#define MAXINTSIZE2(x) sizeof(# x)
#define MAXINTSIZE1(x) x
//! Define the size of a char buffer long enough for any int

#define MAXINTSIZE	(MAXINTSIZE2(MAXINTSIZE1(ULLONG_MAX)) + ULLONG_MAX*0+1)

#endif
