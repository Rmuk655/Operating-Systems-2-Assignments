#ifndef VMSTRUCT_H
#define VMSTRUCT_H

#include "types.h"

struct swap {
    int used;
#ifndef USER
    // The user-space compiler will skip this line
    uint64 *page_table; 
#else
    // The user-space compiler sees a generic pointer or padding
    uint64 page_table_ptr; 
#endif
    uint64 va;
};

struct proc;

struct frame
{
    int used;          // whether the frame is in use
    struct proc *proc; // process using this frame
    uint64 va;         // virtual address mapped to this frame
    uint64 pa;         // physical address of the frame
};

#endif