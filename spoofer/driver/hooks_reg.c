#include "hooks.h"

static PVOID  g_Cookie;
static HANDLE g_CryptoKey, g_ProfileKey, g_VersionKey;

static BOOLEAN KeyIs(HANDLE h, PCWSTR suffix)
{
    UCHAR buf[512]; OBJECT_NAME_INFORMATION* ni = (OBJECT_NAME_INFORMATION*)buf;
    ULONG need = 0; SIZE_T n;
    if (!h) return FALSE;
    n = wcslen(suffix) * sizeof(WCHAR);
    if (!NT_SUCCESS(ZwQueryObject(h, ObjectNameInformation, ni, sizeof(buf), &need))) return FALSE;
    if (ni->Name.Length < n) return FALSE;
    return RtlEqualMemory((PUCHAR)ni->Name.Buffer + ni->Name.Length - n, suffix, n);
}

static VOID PutGuid(PKEY_VALUE_PARTIAL_INFORMATION* out, PULONG outLen,
                    PCWSTR fmt, const GUID* g)
{
    static WCHAR buf[48];
    ULONG len;
    RtlStringCchPrintfW(buf, 48, fmt,
        g->Data1, g->Data2, g->Data3, g->Data4[0], g->Data4[1], g->Data4[2],
        g->Data4[3], g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
    len = (ULONG)(wcslen(buf) + 1) * sizeof(WCHAR);
    *out = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED,
             sizeof(**out) + len, 'xhpr');
    if (!*out) return;
    (*out)->TitleIndex = 0;
    (*out)->Type = REG_SZ;
    (*out)->DataLength = len;
    RtlCopyMemory((*out)->Data, buf, len);
    *outLen = sizeof(**out) + len;
}

static NTSTATUS QueryCb(PVOID ctx, PVOID info)
{
    REG_QUERY_KEY_INFORMATION* q = (REG_QUERY_KEY_INFORMATION*)info;
    PKEY_VALUE_PARTIAL_INFORMATION out = NULL;
    ULONG len = 0;
    UNREFERENCED_PARAMETER(ctx);

    if (q->KeyValueInformationClass != KeyValuePartialInformation) return STATUS_SUCCESS;

    if (KeyIs(g_CryptoKey, L"Cryptography") &&
        RtlEqualUnicodeString(q->ValueName, &(UNICODE_STRING)RTL_CONSTANT_STRING(L"MachineGuid"), TRUE))
        PutGuid(&out, &len, L"%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X", &g_Model.machine_guid);
    else if (KeyIs(g_ProfileKey, L"0001") &&
        RtlEqualUnicodeString(q->ValueName, &(UNICODE_STRING)RTL_CONSTANT_STRING(L"HwProfileGuid"), TRUE))
        PutGuid(&out, &len, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", &g_Model.hw_profile_guid);
    else if (KeyIs(g_VersionKey, L"CurrentVersion") &&
        RtlEqualUnicodeString(q->ValueName, &(UNICODE_STRING)RTL_CONSTANT_STRING(L"ProductId"), TRUE)) {
        len = (ULONG)strlen(g_Model.product_id) + 1;
        out = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED,
                sizeof(*out) + len, 'xhpr');
        if (out) {
            out->TitleIndex = 0; out->Type = REG_SZ; out->DataLength = len;
            RtlCopyMemory(out->Data, g_Model.product_id, len);
            len += sizeof(*out);
        }
    }

    if (!out) return STATUS_SUCCESS;
    *(PKEY_VALUE_PARTIAL_INFORMATION*)q->KeyValueInformation = out;
    *q->Length = len;
    *q->ResultLength = len;
    return STATUS_SUCCESS;
}

static NTSTATUS RegCb(PVOID ctx, PVOID reg, PVOID info)
{
    if ((REG_NOTIFY_CLASS)(ULONG_PTR)reg == RegNtPreQueryValueKey) return QueryCb(ctx, info);
    return STATUS_SUCCESS;
}

static HANDLE OpenKey(PCWSTR p)
{
    UNICODE_STRING u; OBJECT_ATTRIBUTES oa; HANDLE h = NULL;
    RtlInitUnicodeString(&u, p);
    InitializeObjectAttributes(&oa, &u, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    if (!NT_SUCCESS(ZwOpenKey(&h, KEY_READ, &oa))) return NULL;
    return h;
}

NTSTATUS RegHookInstall(VOID)
{
    UNICODE_STRING alt;
    RtlInitUnicodeString(&alt, L"380000");
    g_CryptoKey  = OpenKey(L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Cryptography");
    g_ProfileKey = OpenKey(L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\IDConfigDB\\Hardware Profiles\\0001");
    g_VersionKey = OpenKey(L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion");
    return CmRegisterCallbackEx(RegCb, &alt, g_DriverObject, NULL, &g_Cookie, NULL);
}

VOID RegHookRemove(VOID)
{
    if (g_Cookie) { CmUnRegisterCallback(g_Cookie); g_Cookie = NULL; }
    if (g_CryptoKey)  { ZwClose(g_CryptoKey);  g_CryptoKey = NULL; }
    if (g_ProfileKey) { ZwClose(g_ProfileKey); g_ProfileKey = NULL; }
    if (g_VersionKey) { ZwClose(g_VersionKey); g_VersionKey = NULL; }
}
