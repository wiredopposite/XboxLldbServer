#pragma once

#include "XboxDmAdapter/Types.h"

#define DMEXT extern "C"

#ifdef DM_SHARED_LIBRARY
#define DMAPI DMEXT
#else
#define DMAPI DMEXT __declspec(dllimport)
#endif

DMAPI HRESULT NTAPI DmEnableGPUCounter(BOOL Enable);
DMAPI HRESULT NTAPI DmRegisterPerformanceCounter(LPCSTR Name, DWORD Type, PVOID);
DMAPI HRESULT NTAPI DmInsertAllocationEntry(PVOID AllocPtr, SIZE_T AllocSize, USHORT AllocType);
DMAPI HRESULT NTAPI DmRemoveAllocationEntry(PVOID AllocPtr, SIZE_T AllocSize, USHORT AllocType);
DMAPI HRESULT NTAPI DmGetProcAddress(HANDLE Module, LPCSTR ProcName, PVOID *Ret);

DMAPI PVOID NTAPI DmAllocatePool(SIZE_T size);
DMAPI PVOID NTAPI DmAllocatePoolWithTag(SIZE_T size, ULONG Tag);
DMAPI VOID NTAPI DmFreePool(PVOID p);