#pragma once
#include <ntddk.h>
#include <ntstrsafe.h>
#include <ntddstor.h>
#include <ntddscsi.h>
#include <ntdddisk.h>

extern PDRIVER_OBJECT g_DriverObject;

typedef struct _SPOOF_MODEL {
    char serial[2][21];
    char fw[2][9];
    char model[2][41];
    GUID machine_guid;
    char product_id[21];
    GUID hw_profile_guid;
} SPOOF_MODEL;

extern SPOOF_MODEL g_Model;
extern ULONG64 g_Seed;

static __forceinline ULONG64 mix64(ULONG64 x)
{
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31; return x;
}
#define TAG(t) mix64(g_Seed ^ (0xA5A5A5A500000000ULL | (ULONG64)(t)))

static __forceinline void fill_set(char* d, SIZE_T len, ULONG64 r, const char* set)
{
    SIZE_T i, sl = 0; while (set[sl]) sl++;
    for (i = 0; i < len; i++) d[i] = set[(r >> ((i % 8) * 6)) % sl];
    if (len > 4) d[2] = set[(r >> 3) % sl];
    d[len] = 0;
}

static __forceinline VOID build_model(VOID)
{
    static const char S[] = "ABCDEFGHJKLMNPQRSTUVWXYZ0123456789";
    static const char F[] = "0123456789";
    static const char P[] = "BCDFGHJKMPQRTVWXY2346789";
    char raw[21]; int i;

    for (i = 0; i < 2; i++) {
        fill_set(g_Model.serial[i], 20, TAG(0x10 + i), S);
        fill_set(g_Model.fw[i], 8, TAG(0x20 + i), F);
    }
    fill_set(raw, 20, TAG(0x40), P);
    RtlStringCchPrintfA(g_Model.product_id, 21, "%.5s-%.5s-%.5s-%.5s",
                        raw, raw + 5, raw + 10, raw + 15);
}

static __forceinline VOID make_guid(GUID* g, ULONG64 tag)
{
    ULONG64 a = mix64(tag), b = mix64(tag ^ 0x9E3779B97F4A7C15ULL);
    RtlCopyMemory(g, &a, 8); RtlCopyMemory((PUCHAR)g + 8, &b, 8);
    g->Data3 = (USHORT)((g->Data3 & 0x0FFF) | 0x4000);
    g->Data4[0] = (UCHAR)((g->Data4[0] & 0x3F) | 0x80);
}
