#include "hooks.h"

typedef struct _PATCH_CTX { ULONG Index; ULONG Code; ULONG OutLen; ULONG Property; } PATCH_CTX;

#define W_ASCII(b, room, src, max)                                   \
    do { ULONG _n = 0; while (_n < (max) && (src)[_n]) _n++;         \
         if (_n + 1 > (room)) _n = (room) ? (room) - 1 : 0;          \
         if (_n) RtlCopyMemory((b), (src), _n); (b)[_n] = 0; } while (0)

static VOID AtaPut(PUCHAR buf, ULONG off, ULONG len, const char* s)
{
    ULONG i, sl = 0; while (s[sl]) sl++;
    for (i = 0; i < len; i += 2) {
        buf[off + i]     = (i + 1 < sl) ? (UCHAR)s[i + 1] : ' ';
        buf[off + i + 1] = (i     < sl) ? (UCHAR)s[i]     : ' ';
    }
}

static VOID PatchIdentify(PUCHAR d, ULONG idx)
{
    AtaPut(d, 20, 20, g_Model.serial[idx]);
    AtaPut(d, 46,  8, g_Model.fw[idx]);
    AtaPut(d, 54, 40, g_Model.model[idx]);
}

static VOID PatchDesc(PSTORAGE_DEVICE_DESCRIPTOR d, ULONG outLen, ULONG idx)
{
    if (d->Size < sizeof(*d)) return;
    if (d->SerialNumberOffset && d->SerialNumberOffset < outLen)
        W_ASCII((PUCHAR)d + d->SerialNumberOffset,
                outLen - d->SerialNumberOffset, g_Model.serial[idx], 20);
    if (d->ProductIdOffset && d->ProductIdOffset < outLen)
        W_ASCII((PUCHAR)d + d->ProductIdOffset,
                outLen - d->ProductIdOffset, g_Model.model[idx], 16);
    if (d->ProductRevisionOffset && d->ProductRevisionOffset < outLen)
        W_ASCII((PUCHAR)d + d->ProductRevisionOffset,
                outLen - d->ProductRevisionOffset, g_Model.fw[idx], 8);
}

static NTSTATUS PatchCompletion(PDEVICE_OBJECT DevObj, PIRP Irp, PVOID Ctx)
{
    PATCH_CTX* c = (PATCH_CTX*)Ctx;
    UNREFERENCED_PARAMETER(DevObj);

    if (NT_SUCCESS(Irp->IoStatus.Status) && Irp->AssociatedIrp.SystemBuffer) {
        PVOID b = Irp->AssociatedIrp.SystemBuffer;
        if (c->Code == IOCTL_STORAGE_QUERY_PROPERTY && c->Property == StorageDeviceProperty)
            PatchDesc((PSTORAGE_DEVICE_DESCRIPTOR)b, c->OutLen, c->Index);
        else if (c->Code == IOCTL_ATA_PASS_THROUGH) {
            PATA_PASS_THROUGH_EX a = (PATA_PASS_THROUGH_EX)b;
            if (a->CurrentTaskFile[6] == 0xEC && a->DataTransferLength >= 512 &&
                a->DataBufferOffset + 512 <= c->OutLen)
                PatchIdentify((PUCHAR)b + a->DataBufferOffset, c->Index);
        } else if (c->Code == IOCTL_ATA_PASS_THROUGH_DIRECT) {
            PATA_PASS_THROUGH_EX a = (PATA_PASS_THROUGH_EX)b;
            if (a->CurrentTaskFile[6] == 0xEC && a->DataTransferLength >= 512 && a->DataBuffer)
                PatchIdentify((PUCHAR)a->DataBuffer, c->Index);
        } else if (c->Code == IOCTL_SMART_RECEIVE_DRIVE_DATA) {
            PSENDCMDOUTPARAMS o = (PSENDCMDOUTPARAMS)b;
            if (o->cBufferSize >= 512) PatchIdentify(o->bBuffer, c->Index);
        }
    }
    ExFreePoolWithTag(c, 'xhcp');
    return STATUS_SUCCESS;
}

NTSTATUS StorageDispatch(PDEVICE_OBJECT DevObj, PIRP Irp)
{
    PIO_STACK_LOCATION s = IoGetCurrentIrpStackLocation(Irp);
    FILTER_EXT* e = (FILTER_EXT*)DevObj->DeviceExtension;
    ULONG code = s->Parameters.DeviceIoControl.IoControlCode;
    PATCH_CTX* c;

    if (!e) return ControlDispatch(DevObj, Irp);

    if (code != IOCTL_STORAGE_QUERY_PROPERTY && code != IOCTL_ATA_PASS_THROUGH &&
        code != IOCTL_ATA_PASS_THROUGH_DIRECT && code != IOCTL_SMART_RECEIVE_DRIVE_DATA) {
        IoSkipCurrentIrpStackLocation(Irp);
        return IoCallDriver(e->Lower, Irp);
    }

    c = (PATCH_CTX*)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(*c), 'xhcp');
    if (!c) { IoSkipCurrentIrpStackLocation(Irp); return IoCallDriver(e->Lower, Irp); }

    c->Index = e->Index;
    c->Code  = code;
    c->OutLen = s->Parameters.DeviceIoControl.OutputBufferLength;
    c->Property = 0;
    if (code == IOCTL_STORAGE_QUERY_PROPERTY && Irp->AssociatedIrp.SystemBuffer)
        c->Property = ((PSTORAGE_PROPERTY_QUERY)Irp->AssociatedIrp.SystemBuffer)->PropertyId;

    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(Irp, PatchCompletion, c, TRUE, TRUE, TRUE);
    return IoCallDriver(e->Lower, Irp);
}
