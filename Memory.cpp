
#include "Driver.h"

namespace Memory
{
    PHYSICAL_ADDRESS
        TranslateLinearAddress(
            ULONG64 CR3,
            ULONG64 VirtualAddress
            )
    {
		ULONG64 pageOffset, pte, pt, pd, pdp, pdpe, pde, pteAddr;
		SIZE_T ReadSize;
		PHYSICAL_ADDRESS Result;
		RtlSecureZeroMemory(&Result, sizeof(Result));

		CR3 &= ~0xf;
		pageOffset = VirtualAddress & ~(~0ul << PAGE_OFFSET_SIZE);
		pte = ((VirtualAddress >> 12) & (0x1ffll));
		pt = ((VirtualAddress >> 21) & (0x1ffll));
		pd = ((VirtualAddress >> 30) & (0x1ffll));
		pdp = ((VirtualAddress >> 39) & (0x1ffll));
		ReadSize = 0;
		pdpe = 0;
		
		Memory::CopyMemory(CR3 + 8 * pdp, &pdpe, sizeof(pdpe), &ReadSize);
		if (~pdpe & 1)
		{
			return Result;
		}

		pde = 0;
		Memory::CopyMemory((pdpe & PMASK) + 8 * pd, &pde, sizeof(pde), &ReadSize);
		if (~pde & 1)
		{
			return Result;
		}

		//
		// 1GB large page, use pde's 12-34 bits 
		//

		if (pde & 0x80)
		{
			Result.QuadPart=(pde & (~0ull << 42 >> 12)) + (VirtualAddress & ~(~0ull << 30));
			return Result;
		}

		pteAddr = 0;
		Memory::CopyMemory((pde & PMASK) + 8 * pt, &pteAddr, sizeof(pteAddr), &ReadSize);
		if (~pteAddr & 1)
		{
			return Result;
		}

		//
		// 2MB large page
		//

		if (pteAddr & 0x80)
		{
			Result.QuadPart=(pteAddr & PMASK) + (VirtualAddress & ~(~0ull << 21));
			return Result;
		}

		VirtualAddress = 0;
		Memory::CopyMemory((pteAddr & PMASK) + 8 * pte, &VirtualAddress, sizeof(VirtualAddress), &ReadSize);
		VirtualAddress &= PMASK;

		if (VirtualAddress)
		{
			Result.QuadPart= VirtualAddress + pageOffset;
			return Result;
		}

		return Result;
    }

	BOOLEAN
		CopyMemory(
			ULONG64 Source,
			PVOID Destination,
			SIZE_T BytesToCopy,
			PSIZE_T BytesCopied
			)
	{
		MM_COPY_ADDRESS Address;
		NTSTATUS Status;
		BOOLEAN Result;

		RtlSecureZeroMemory(&Address, sizeof(Address));
		Address.PhysicalAddress.QuadPart = Source;
		Status = MmCopyMemory(Destination, Address, BytesToCopy, MM_COPY_MEMORY_PHYSICAL, BytesCopied);
		Result = TRUE;

		if (!NT_SUCCESS(Status)) {

			LogPrint(
				"Failed to Copy Memory from 0x%llX to 0x%p\r\n",
				Source, Destination
				);

			Result = FALSE;
		}

		return Result;
	}

}