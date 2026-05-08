#include <xboxkrnl/xboxkrnl.h>
#include "XboxDmAdapter/Private/XboxDmAdapter.h"

#define DmPoolAllocNoTag 'epoN'
#define DmPoolAllocTag 'pmdX'

struct DmMemoryHeader {
    union {
        struct {
            UCHAR PrevSize;
            UCHAR Index;
            UCHAR Type;
            UCHAR BLockSize;
        };
        ULONG Ul;
    };
    ULONG Tag;
};

struct DmPoolState {
};

PVOID DmAllocatePool(SIZE_T size) {
    return ExAllocatePoolWithTag(size, DmPoolAllocNoTag);
}

PVOID DmAllocatePoolWithTag(SIZE_T size, ULONG Tag) {
    KIRQL oldIrql = KeRaiseIrqlToDpcLevel();

    auto result = ExAllocatePoolWithTag(size, Tag);

    KfLowerIrql(oldIrql);
    return result;
}

VOID DmFreePool(PVOID p) {
    ExFreePool(p);
}