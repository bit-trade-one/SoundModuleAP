#ifdef AMIGA
#include "stddef.h" 
#endif

extern long *mem_free_list[];

void *memget(register size_t size);
void memfree(register void *ptr, register size_t size);

void meminit(void);

#define MEM
