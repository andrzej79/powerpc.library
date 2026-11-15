#ifndef PROTO_MEDIATORPCI_H
#define PROTO_MEDIATORPCI_H
#include <libraries/mediatorpci.h>
struct PciDevice* __FindPciDevice(__reg("d0") ULONG vendor, __reg("d1") ULONG device, __reg("d2") ULONG n, __reg("a6") void*)="\tjsr\t-30(a6)";
#define FindPciDevice(a,b,c) __FindPciDevice((a),(b),(c),MediatorPCIBase)
struct PciDevice* __FindPciDevFunc(__reg("d0") ULONG devfn, __reg("a6") void*)="\tjsr\t-42(a6)";
#define FindPciDevFunc(a) __FindPciDevFunc((a),MediatorPCIBase)
void __SetInterrupt(__reg("a0") struct PciDevice* d, __reg("a6") void*)="\tjsr\t-48(a6)";
#define SetInterrupt(a) __SetInterrupt((a),MediatorPCIBase)
void __AddInterrupt(__reg("a0") struct PciDevice* d, __reg("a1") struct Interrupt* i, __reg("a6") void*)="\tjsr\t-78(a6)";
#define AddInterrupt(a,b) __AddInterrupt((a),(b),MediatorPCIBase)
UBYTE __ReadConfigurationByte(__reg("d0") UWORD df, __reg("d1") UBYTE o, __reg("a6") void*)="\tjsr\t-90(a6)";
#define ReadConfigurationByte(a,b) __ReadConfigurationByte((a),(b),MediatorPCIBase)
UWORD __ReadConfigurationWord(__reg("d0") UWORD df, __reg("d1") UBYTE o, __reg("a6") void*)="\tjsr\t-96(a6)";
#define ReadConfigurationWord(a,b) __ReadConfigurationWord((a),(b),MediatorPCIBase)
ULONG __ReadConfigurationLong(__reg("d0") UWORD df, __reg("d1") UBYTE o, __reg("a6") void*)="\tjsr\t-102(a6)";
#define ReadConfigurationLong(a,b) __ReadConfigurationLong((a),(b),MediatorPCIBase)
void __WriteConfigurationByte(__reg("d0") UWORD df, __reg("d1") UBYTE o, __reg("d2") UBYTE v, __reg("a6") void*)="\tjsr\t-108(a6)";
#define WriteConfigurationByte(a,b,c) __WriteConfigurationByte((a),(b),(c),MediatorPCIBase)
void __WriteConfigurationWord(__reg("d0") UWORD df, __reg("d1") UBYTE o, __reg("d2") UWORD v, __reg("a6") void*)="\tjsr\t-114(a6)";
#define WriteConfigurationWord(a,b,c) __WriteConfigurationWord((a),(b),(c),MediatorPCIBase)
void __WriteConfigurationLong(__reg("d0") UWORD df, __reg("d1") UBYTE o, __reg("d2") ULONG v, __reg("a6") void*)="\tjsr\t-120(a6)";
#define WriteConfigurationLong(a,b,c) __WriteConfigurationLong((a),(b),(c),MediatorPCIBase)
ULONG __AllocPCIBAR(__reg("d0") ULONG size, __reg("d1") ULONG bar, __reg("a0") struct PciDevice* d, __reg("a6") void*)="\tjsr\t-150(a6)";
#define AllocPCIBAR(a,b,c) __AllocPCIBAR((a),(b),(c),MediatorPCIBase)
void __FreePCIBAR(__reg("d0") ULONG bar, __reg("a0") struct PciDevice* d, __reg("a6") void*)="\tjsr\t-156(a6)";
#define FreePCIBAR(a,b) __FreePCIBAR((a),(b),MediatorPCIBase)
#endif
