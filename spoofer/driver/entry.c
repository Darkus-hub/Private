#include "hooks.h"

PDRIVER_OBJECT g_DriverObject;
static PDEVICE_OBJECT g_Control;
static PDEVICE_OBJECT g_Filter[8];
static BOOLEAN g_Attached[8];
SPOOF_MODEL g_Model;
ULONG64 g_Seed;

NTSTATUS AttachAll(VOID)
{
    ULONG i;
    for (i = 0; i < 8; i++) {
        WCHAR p[64]; UNICODE_STRING u; PFILE_OBJECT fo = NULL;
        PDEVICE_OBJECT tgt = NULL, flt = NULL, lower = NULL;
        NTSTATUS st;

        RtlStringCchPrintfW(p, 64, L"\\Device\\Harddisk%u\\DR%u", i, i);
        RtlInitUnicodeString(&u, p);
        if (!NT_SUCCESS(IoGetDeviceObjectPointer(&u, FILE_ALL_ACCESS, &fo, &tgt))) continue;

        st = IoCreateDevice(g_DriverObject, sizeof(FILTER_EXT), NULL,
                            tgt->DeviceType, tgt->Characteristics, FALSE, &flt);
        if (NT_SUCCESS(st)) {
            ((FILTER_EXT*)flt->DeviceExtension)->Index = i;
            flt->Flags |= tgt->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE);
            st = IoAttachDeviceToDeviceStackSafe(flt, tgt, &lower);
            if (NT_SUCCESS(st)) {
                ((FILTER_EXT*)flt->DeviceExtension)->Lower = lower;
                flt->Flags &= ~DO_DEVICE_INITIALIZING;
                g_Filter[i] = flt; g_Attached[i] = TRUE;
            } else IoDeleteDevice(flt);
        }
        ObDereferenceObject(fo);
    }
    return STATUS_SUCCESS;
}

VOID DetachAll(VOID)
{
    ULONG i;
    for (i = 0; i < 8; i++) {
        if (!g_Attached[i]) continue;
        IoDetachDevice(((FILTER_EXT*)g_Filter[i]->DeviceExtension)->Lower);
        IoDeleteDevice(g_Filter[i]);
        g_Filter[i] = NULL; g_Attached[i] = FALSE;
    }
}

NTSTATUS ControlDispatch(PDEVICE_OBJECT d, PIRP irp)
{
    UNREFERENCED_PARAMETER(d);
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static VOID Unload(PDRIVER_OBJECT d)
{
    UNREFERENCED_PARAMETER(d);
    RegHookRemove();
    DetachAll();
    if (g_Control) {
        UNICODE_STRING l; RtlInitUnicodeString(&l, L"\\DosDevices\\Spoof");
        IoDeleteSymbolicLink(&l);
        IoDeleteDevice(g_Control);
    }
}

NTSTATUS DriverEntry(PDRIVER_OBJECT drv, PUNICODE_STRING reg)
{
    UNICODE_STRING dev, lnk;
    RTL_QUERY_REGISTRY_TABLE tab[2];
    ULONG i;
    NTSTATUS st;

    UNREFERENCED_PARAMETER(reg);
    g_DriverObject = drv;
    g_Seed = 0;

    RtlZeroMemory(tab, sizeof(tab));
    tab[0].Name = L"Seed";
    tab[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
    tab[0].EntryContext = &g_Seed;
    tab[0].DefaultType = REG_BINARY;
    tab[0].DefaultData = &g_Seed;
    tab[0].DefaultLength = sizeof(g_Seed);
    RtlQueryRegistryValues(RTL_REGISTRY_SERVICES, L"spoof\\Parameters", tab, NULL, NULL);
    if (!g_Seed) g_Seed = (ULONG64)KeQuerySystemTimePrecise();

    build_model();
    make_guid(&g_Model.machine_guid, TAG(0x50));
    make_guid(&g_Model.hw_profile_guid, TAG(0x51));

    RtlInitUnicodeString(&dev, L"\\Device\\Spoof");
    st = IoCreateDevice(drv, 0, &dev, FILE_DEVICE_UNKNOWN, 0, FALSE, &g_Control);
    if (!NT_SUCCESS(st)) return st;
    RtlInitUnicodeString(&lnk, L"\\DosDevices\\Spoof");
    IoCreateSymbolicLink(&lnk, &dev);

    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) drv->MajorFunction[i] = StorageDispatch;
    drv->DriverUnload = Unload;

    AttachAll();
    RegHookInstall();
    return STATUS_SUCCESS;
}
