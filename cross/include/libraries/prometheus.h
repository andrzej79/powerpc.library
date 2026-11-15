/* Recreated for powerpc.library 17.15. Tag base read from the shipped binary. */
#ifndef LIBRARIES_PROMETHEUS_H
#define LIBRARIES_PROMETHEUS_H
#include <exec/types.h>
#include <utility/tagitem.h>
#ifndef PRM_BASE
#define PRM_BASE 0x6EDA0000
#endif
typedef struct PCIBoard PCIBoard;
#define PRM_Vendor      (PRM_BASE)
#define PRM_Device      (PRM_BASE+1)
#define PRM_Revision    (PRM_BASE+2)
#define PRM_BoardOwner  (PRM_BASE+5)
#define PRM_MemoryAddr0 (PRM_BASE+0x10)
#define PRM_MemoryAddr1 (PRM_BASE+0x11)
#define PRM_MemoryAddr2 (PRM_BASE+0x12)
#define PRM_MemoryAddr3 (PRM_BASE+0x13)
#define PRM_MemorySize0 (PRM_BASE+0x20)
#define PRM_MemorySize1 (PRM_BASE+0x21)
#endif
