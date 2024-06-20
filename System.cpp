#include "Driver.h"

namespace System
{
	PVOID GetSystemModuleBase(LPCWSTR ModuleName)
	{
		UNICODE_STRING wzName;
		PLIST_ENTRY LoadedModuleList;

		LoadedModuleList = (PLIST_ENTRY)PsLoadedModuleList;
		if (LoadedModuleList) {

			RtlInitUnicodeString(&wzName, ModuleName);

			for (PLIST_ENTRY NextEntry = LoadedModuleList;
				NextEntry != LoadedModuleList->Blink;
				NextEntry = NextEntry->Flink)
			{
				PLDR_DATA_TABLE_ENTRY DataTableEntry = CONTAINING_RECORD(NextEntry,
					LDR_DATA_TABLE_ENTRY,
					InLoadOrderLinks);

				if (RtlEqualUnicodeString(
					(PCUNICODE_STRING)&DataTableEntry->BaseDllName,
					(PCUNICODE_STRING)&wzName,
					TRUE))
				{
					return (PVOID)DataTableEntry->DllBase;
				}
			}
		}

		return (PVOID)NULL;
	}

	NTSTATUS
		QuerySystemModule(
			const char* ModuleName,
			ULONG64* ModuleBase,
			SIZE_T* ModuleSize
			)
	{
		if (!ModuleBase || !ModuleSize)
			return STATUS_INVALID_PARAMETER;

		ULONG NumberOfBytes{};
		ZwQuerySystemInformation(0xB, nullptr, NumberOfBytes, reinterpret_cast<PULONG>(&NumberOfBytes));

		const auto listHeader = Memory::AllocPoolMemory(NumberOfBytes);

        if (listHeader)	
        {
            if (const auto status = ZwQuerySystemInformation(0xB, listHeader, NumberOfBytes, reinterpret_cast<PULONG>(&NumberOfBytes)))
                return status;

            auto currentModule = reinterpret_cast<PSYSTEM_MODULE_INFORMATION>(listHeader)->Module;
            for (SIZE_T i{}; i < reinterpret_cast<PSYSTEM_MODULE_INFORMATION>(listHeader)->Count; ++i, ++currentModule)
            {
                const auto currentModuleName = reinterpret_cast<const char*>(currentModule->FullPathName + currentModule->OffsetToFileName);
                if (!strcmp(ModuleName, currentModuleName))
                {
                    *ModuleBase = reinterpret_cast<ULONG64>(currentModule->ImageBase);
                    *ModuleSize = currentModule->ImageSize;
                    Memory::FreePoolMemory(listHeader);
                    return STATUS_SUCCESS;
                }
            }
            Memory::FreePoolMemory(listHeader);
        }
        else
        {
            return STATUS_MEMORY_NOT_ALLOCATED;
        }

		return STATUS_NOT_FOUND;
	}
}
