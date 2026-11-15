#ifndef PROTO_PROMETHEUS_H
#define PROTO_PROMETHEUS_H
#include <libraries/prometheus.h>
#include <exec/interrupts.h>
PCIBoard* __Prm_FindBoardTags(__reg("a6") void*, __reg("a0") void* prev, ULONG tags, ...)="\tmove.l\ta1,-(a7)\n\tlea\t4(a7),a1\n\tjsr\t-30(a6)\n\tmove.l\t(a7)+,a1";
#define Prm_FindBoardTags(a,...) __Prm_FindBoardTags(PrometheusBase,(a),__VA_ARGS__)
void __Prm_GetBoardAttrsTagList(__reg("a6") void*, __reg("a0") void* b, __reg("a1") struct TagItem* t)="\tjsr\t-36(a6)";
#define Prm_GetBoardAttrsTagList(a,b) __Prm_GetBoardAttrsTagList(PrometheusBase,(a),(b))
void __Prm_GetBoardAttrsTags(__reg("a6") void*, __reg("a0") void* b, ULONG tags, ...)="\tmove.l\ta1,-(a7)\n\tlea\t4(a7),a1\n\tjsr\t-36(a6)\n\tmove.l\t(a7)+,a1";
#define Prm_GetBoardAttrsTags(a,...) __Prm_GetBoardAttrsTags(PrometheusBase,(a),__VA_ARGS__)
ULONG __Prm_ReadConfigLong(__reg("a6") void*, __reg("a0") void* b, __reg("d0") UBYTE o)="\tjsr\t-42(a6)";
#define Prm_ReadConfigLong(a,b) __Prm_ReadConfigLong(PrometheusBase,(a),(b))
UWORD __Prm_ReadConfigWord(__reg("a6") void*, __reg("a0") void* b, __reg("d0") UBYTE o)="\tjsr\t-48(a6)";
#define Prm_ReadConfigWord(a,b) __Prm_ReadConfigWord(PrometheusBase,(a),(b))
UBYTE __Prm_ReadConfigByte(__reg("a6") void*, __reg("a0") void* b, __reg("d0") UBYTE o)="\tjsr\t-54(a6)";
#define Prm_ReadConfigByte(a,b) __Prm_ReadConfigByte(PrometheusBase,(a),(b))
void __Prm_WriteConfigLong(__reg("a6") void*, __reg("a0") void* b, __reg("d0") ULONG v, __reg("d1") UBYTE o)="\tjsr\t-60(a6)";
#define Prm_WriteConfigLong(a,b,c) __Prm_WriteConfigLong(PrometheusBase,(a),(b),(c))
void __Prm_WriteConfigWord(__reg("a6") void*, __reg("a0") void* b, __reg("d0") UWORD v, __reg("d1") UBYTE o)="\tjsr\t-66(a6)";
#define Prm_WriteConfigWord(a,b,c) __Prm_WriteConfigWord(PrometheusBase,(a),(b),(c))
void __Prm_WriteConfigByte(__reg("a6") void*, __reg("a0") void* b, __reg("d0") UBYTE v, __reg("d1") UBYTE o)="\tjsr\t-72(a6)";
#define Prm_WriteConfigByte(a,b,c) __Prm_WriteConfigByte(PrometheusBase,(a),(b),(c))
ULONG __Prm_SetBoardAttrsTags(__reg("a6") void*, __reg("a0") void* b, ULONG tags, ...)="\tmove.l\ta1,-(a7)\n\tlea\t4(a7),a1\n\tjsr\t-78(a6)\n\tmove.l\t(a7)+,a1";
#define Prm_SetBoardAttrsTags(a,...) __Prm_SetBoardAttrsTags(PrometheusBase,(a),__VA_ARGS__)
BOOL __Prm_AddIntServer(__reg("a6") void*, __reg("a0") void* b, __reg("a1") struct Interrupt* i)="\tjsr\t-84(a6)";
#define Prm_AddIntServer(a,b) __Prm_AddIntServer(PrometheusBase,(a),(b))
APTR __Prm_AllocDMABuffer(__reg("a6") void*, __reg("d0") ULONG s)="\tjsr\t-96(a6)";
#define Prm_AllocDMABuffer(a) __Prm_AllocDMABuffer(PrometheusBase,(a))
APTR __Prm_AllocPCIAddressSpace(__reg("a6") void*, __reg("a0") void* b, __reg("d0") ULONG s, __reg("d1") ULONG bar)="\tjsr\t-120(a6)";
#define Prm_AllocPCIAddressSpace(a,b,c) __Prm_AllocPCIAddressSpace(PrometheusBase,(a),(b),(c))
void __Prm_FreePCIAddressSpace(__reg("a6") void*, __reg("a0") void* b, __reg("d0") ULONG bar)="\tjsr\t-126(a6)";
#define Prm_FreePCIAddressSpace(a,b) __Prm_FreePCIAddressSpace(PrometheusBase,(a),(b))
#endif
