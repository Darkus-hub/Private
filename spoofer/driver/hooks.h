#pragma once
#include "spoof.h"

typedef struct _FILTER_EXT { ULONG Index; PDEVICE_OBJECT Lower; } FILTER_EXT;

NTSTATUS StorageDispatch(PDEVICE_OBJECT DevObj, PIRP Irp);
NTSTATUS ControlDispatch(PDEVICE_OBJECT DevObj, PIRP Irp);
NTSTATUS AttachAll(VOID);
VOID DetachAll(VOID);
NTSTATUS RegHookInstall(VOID);
VOID RegHookRemove(VOID);
