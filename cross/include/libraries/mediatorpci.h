/* Reconstructed from SonnetAmiga common/pci.i + pci_lib.fd. Names are those used by powerpc.library 17.15. */
#ifndef LIBRARIES_MEDIATORPCI_H
#define LIBRARIES_MEDIATORPCI_H
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/nodes.h>
struct PciBase {
    struct Library pb_LibNode;   /* 0x00, size 0x22 */
    UBYTE  pb_Pad[8];            /* 0x22 */
    ULONG  pb_MemAddress;        /* 0x2A pcibase_MemWindow */
};
struct PciDevice {
    struct Node pd_Node;         /* 0 */
    UWORD  pd_VendorID;          /* 14 */
    UWORD  pd_DeviceID;          /* 16 */
    ULONG  pd_ClassCode;         /* 18 */
    UBYTE  pd_RevisionID;        /* 22 */
    UBYTE  pd_InterruptPin;      /* 23 */
    UWORD  pd_SubVendorID;       /* 24 */
    UWORD  pd_SubID;             /* 26 */
    ULONG  pd_ABaseAddress0;     /* 28 */
    ULONG  pd_Size0;             /* 32 */
    ULONG  pd_ABaseAddress1;     /* 36 */
    ULONG  pd_Size1;             /* 40 */
    ULONG  pd_ABaseAddress2;     /* 44 */
    ULONG  pd_Size2;             /* 48 */
    ULONG  pd_ABaseAddress3;     /* 52 */
    ULONG  pd_Size3;             /* 56 */
    ULONG  pd_ABaseAddress4;     /* 60 */
    ULONG  pd_Size4;             /* 64 */
    ULONG  pd_ABaseAddress5;     /* 68 */
    ULONG  pd_Size5;             /* 72 */
};
#endif
