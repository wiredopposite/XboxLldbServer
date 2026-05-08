#pragma once

#include "XboxDmAdapter/Types.h"

#pragma ms_struct on

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)
typedef struct _DM_TELL_ALLOCATION_ENTRY
{
    PVOID AllocPtr;
    ULONG AllocSize;
    USHORT AllocType;
} DM_TELL_ALLOCATION_ENTRY, *PDM_TELL_ALLOCATION_ENTRY;
_Static_assert(sizeof(DM_TELL_ALLOCATION_ENTRY) == 10, "DM_TELL_ALLOCATION_ENTRY size mismatch");
#pragma pack(pop)

typedef struct _DM_TELL_MODULE_PROC_ADDRESS
{
    PCHAR ModuleName;
    PCHAR ProcName;
} DM_TELL_MODULE_PROC_ADDRESS, *PDM_TELL_MODULE_PROC_ADDRESS;
_Static_assert(sizeof(DM_TELL_MODULE_PROC_ADDRESS) == 0x8, "DM_TELL_MODULE_PROC_ADDRESS size mismatch");

typedef struct _DMDRIVE
{
    CHAR Drive;
    PCHAR Path;
} DMDRIVE, *PDMDRIVE;
_Static_assert(sizeof(DMDRIVE) == 0x8, "DMDRIVE size mismatch");

#ifdef __cplusplus
}
#endif

#pragma ms_struct reset
