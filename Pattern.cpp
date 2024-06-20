#include "Driver.h"

namespace Utility
{
    BOOLEAN __DataCompare(LPCSTR pdata, LPCSTR bmask, LPCSTR szmask)
    {
        for (; *szmask; ++szmask, ++pdata, ++bmask)
        {
            if (*szmask == 'x' && *pdata != *bmask)
                return FALSE;
        }

        return !*szmask;
    }

    ULONG64 __FindPattern(ULONG64 base, SIZE_T size, LPCSTR bmask, LPCSTR szmask)
    {
        for (SIZE_T i = 0; i < size; ++i)
            if (__DataCompare((LPCSTR)(base + i), bmask, szmask))
                return base + i;

        return 0;
    }

    NTSTATUS BBSearchPattern(IN PCUCHAR pattern, IN UCHAR wildcard, IN ULONG64 len, IN const VOID* base, IN ULONG64 size, OUT PVOID* ppFound)
    {
        NT_ASSERT(ppFound != NULL && pattern != NULL && base != NULL);
        if (ppFound == NULL || pattern == NULL || base == NULL)
            return STATUS_INVALID_PARAMETER;

        __try
        {
            for (ULONG64 i = 0; i < size - len; i++)
            {
                BOOLEAN found = TRUE;
                for (ULONG64 j = 0; j < len; j++)
                {
                    if (pattern[j] != wildcard && pattern[j] != ((PCUCHAR)base)[i + j])
                    {
                        found = FALSE;
                        break;
                    }
                }

                if (found != FALSE)
                {
                    *ppFound = (PUCHAR)base + i;
                    return STATUS_SUCCESS;
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return STATUS_UNHANDLED_EXCEPTION;
        }

        return STATUS_NOT_FOUND;
    }

    NTSTATUS BBScanSection(IN PCCHAR section, IN PCUCHAR pattern, IN UCHAR wildcard, IN ULONG_PTR len, OUT PVOID* ppFound)
    {
        ASSERT(ppFound != NULL);
        if (ppFound == NULL)
            return STATUS_INVALID_PARAMETER;

        PVOID SystemBase = System::GetSystemModuleBase(Encrypt(L"ntoskrnl.exe"));
        if (!SystemBase)
            return STATUS_NOT_FOUND;

        PIMAGE_NT_HEADERS64 pHdr = RtlImageNtHeader(SystemBase);
        if (!pHdr)
            return STATUS_INVALID_IMAGE_FORMAT;

        PIMAGE_SECTION_HEADER pFirstSection = (PIMAGE_SECTION_HEADER)(pHdr + 1);
        for (PIMAGE_SECTION_HEADER pSection = pFirstSection; pSection < pFirstSection + pHdr->FileHeader.NumberOfSections; pSection++)
        {
            ANSI_STRING s1, s2;
            RtlInitAnsiString(&s1, section);
            RtlInitAnsiString(&s2, (PCCHAR)pSection->Name);
            if (RtlCompareString(&s1, &s2, TRUE) == 0)
            {
                PVOID ptr = NULL;
                NTSTATUS status = BBSearchPattern(pattern, wildcard, len, (PUCHAR)SystemBase + pSection->VirtualAddress, pSection->Misc.VirtualSize, &ptr);
                if (NT_SUCCESS(status))
                    *(PULONG)ppFound = (ULONG)((PUCHAR)ptr - (PUCHAR)SystemBase);

                return status;
            }
        }

        return STATUS_NOT_FOUND;
    }

}