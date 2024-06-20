
#include "Driver.h"

namespace Process
{
    ULONG64
        GetProcessDirectoryBase(
            PEPROCESS Process
            )
    {
        PNT_KPROCESS SystemProcess = (PNT_KPROCESS)(Process);
        return SystemProcess->DirectoryTableBase;
    }

    BOOLEAN
        ReadProcessMemory(
            ULONG64 ProcessCR3,
            ULONG64 VirtualAddress,
            PVOID   Destination,
            SIZE_T  NumberOfBytes,
            PSIZE_T NumberOfBytesCopied
            )
    {
        if (NumberOfBytes < VADDR_ADDRESS_MASK_4KB_PAGES)
        {
            ULONG64 PhysAddress = Memory::TranslateLinearAddress(ProcessCR3, VirtualAddress).QuadPart;
            SIZE_T  BytesCopied = 0;

            if (Memory::CopyMemory(PhysAddress,
                                   Destination,
                                   NumberOfBytes,
                                   &BytesCopied))
            {
                if (ARGUMENT_PRESENT(NumberOfBytesCopied))
                {
                    __try {

                        *NumberOfBytesCopied = BytesCopied;
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER) {

                        NOTHING;
                    }
                }

                return TRUE;
            }
        }

        return FALSE;
    }

    BOOLEAN
        WriteProcessMemory(
            ULONG64 ProcessCR3,
            ULONG64 VirtualAddress,
            PVOID   Source,
            SIZE_T  NumberOfBytes,
            PSIZE_T NumberOfBytesCopied
            )
    {
        PVOID MappedMemory;
        
        MappedMemory = Memory::MapPhysical(Memory::TranslateLinearAddress(
                                           ProcessCR3, VirtualAddress),
                                           NumberOfBytes,
                                           PAGE_READWRITE);

        if (MappedMemory) {

            RtlCopyMemory(MappedMemory, Source, NumberOfBytes);
            MmUnmapIoSpace(MappedMemory, NumberOfBytes);

            if (ARGUMENT_PRESENT(NumberOfBytesCopied)) {
                __try {
                    *NumberOfBytesCopied = NumberOfBytes;
                }
                __except(EXCEPTION_EXECUTE_HANDLER) {
                    NOTHING;
                }
            }

            return TRUE;
        }

        return FALSE;
    }

}