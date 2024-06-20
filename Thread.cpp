

#include "Driver.h"

VOID DriverThread(
    PVOID StartContext
    )
{
    ULONG Status;
    ULONG ClientId;
    CRegistry Registry;
    ULONG64 Address;
    PBOOLEAN Running;

    UNREFERENCED_PARAMETER(StartContext);

    Registry.DeleteValue(Encrypt(L"Client ID"));
    Registry.DeleteValue(Encrypt(L"Address"));
    Registry.WriteValue(Encrypt(L"Status"), STATUS_REQUEST_ACTIVE);

    LogPrint(
        "Driver Main Entry...");

    Running = (PBOOLEAN)StartContext;
    while (*Running)
    {
        if (!*Running) break;

        RtlSecureZeroMemory(&Status, sizeof(Status));
        RtlSecureZeroMemory(&ClientId, sizeof(ClientId));

        Status = Registry.ReadValue<ULONG>(Encrypt(L"Status"));
        
        if (!Status || Status != STATUS_REQUEST_WORKING)
        {
            Utility::DelayExecutionThread(10);
            continue;
        }

        ClientId    = Registry.ReadValue<ULONG>(Encrypt(L"Client ID"));
        Address     = Registry.ReadValue<ULONG64>(Encrypt(L"Address"));

        if (!ClientId || !Address)
        {            
            Registry.DeleteValue(Encrypt(L"Client ID"));
            Registry.DeleteValue(Encrypt(L"Address"));
            Registry.WriteValue(Encrypt(L"Status"), STATUS_REQUEST_ACTIVE);
            continue;
        }

        // Dispatch Time

        PEPROCESS Client;
        NTSTATUS ntStatus;
        KAPC_STATE APC;
        PKERNEL_REQUEST Request;

        ntStatus = PsLookupProcessByProcessId((HANDLE)ClientId, &Client);

        if (!NT_SUCCESS(ntStatus)) {
            Registry.DeleteValue(Encrypt(L"Client ID"));
            Registry.DeleteValue(Encrypt(L"Address"));
            Registry.WriteValue(Encrypt(L"Status"), STATUS_REQUEST_ACTIVE);
            continue;
        }

        // Attach To Client
        KeStackAttachProcess(Client, &APC);
        Request = (PKERNEL_REQUEST)Address;

		switch (Request->Instruction)
		{
		case REQUEST_RUNNING_STATUS:
			if (Request->NumberOfBytes == sizeof(BOOLEAN) &&
				*(BOOLEAN*)Request->Buffer == FALSE)
				*(BOOLEAN*)Request->Buffer = TRUE;

			break;

		case REQUEST_PROCESS_ADDRESS:
			if (Request->NumberOfBytes == sizeof(PROCESS_ADDRESS_REQUEST))
			{
				PEPROCESS Process;
				PPROCESS_ADDRESS_REQUEST SubReq = (PPROCESS_ADDRESS_REQUEST)Request->Buffer;

				Status = PsLookupProcessByProcessId(
					(HANDLE)SubReq->ProcessId,
					&Process);

				if (Status == STATUS_SUCCESS) {

					SubReq->BaseAddress = (ULONG64)PsGetProcessSectionBaseAddress(Process);
					SubReq->ProcessCR3	= Process::GetProcessDirectoryBase(Process);

					ObDereferenceObject(Process);
				}
			}
			break;

		case REQUEST_READ_MEMORY:
			if (Request->NumberOfBytes == sizeof(COPY_MEMORY_REQUEST))
			{
				PCOPY_MEMORY_REQUEST SubReq = (PCOPY_MEMORY_REQUEST)Request->Buffer;
				
                if (!Process::ReadProcessMemory(SubReq->ProcessCR3,
                                                reinterpret_cast<ULONG64>(SubReq->Source),
                                                SubReq->Destination,
                                                SubReq->NumberOfBytes,
                                                NULL))
                {
                    LogPrint(
                        "Error! Failed to read process memory.");
                }
			}
			break;

		case REQUEST_WRITE_MEMORY:
			if (Request->NumberOfBytes == sizeof(COPY_MEMORY_REQUEST))
			{
                PCOPY_MEMORY_REQUEST SubReq = (PCOPY_MEMORY_REQUEST)Request->Buffer;

                if (!Process::WriteProcessMemory(SubReq->ProcessCR3,
                                                 reinterpret_cast<ULONG64>(SubReq->Destination),
                                                 SubReq->Source,
                                                 SubReq->NumberOfBytes,
                                                 NULL))
                {
                    LogPrint(
                        "Error! Failed to write process memory.");
                }
			}
			break;

        case REQUEST_MOUSE_MOVE:
            struct POINT {
                LONG DeltaX;
                LONG DeltaY;
            } *Point;
            if (Request->NumberOfBytes == sizeof(POINT))
            {
                Point = (POINT*)Request->Buffer;
                MoveMouse(Point->DeltaX, Point->DeltaY, 0);
            }
            break;

		default:

            LogPrint(
				"Unspecified Request Instruction: 0x%lX",
				Request->Instruction);

			break;
		}

        KeUnstackDetachProcess(&APC);
        ObDereferenceObject(Client);

        // Ignore any error.
        Registry.DeleteValue(Encrypt(L"Client ID"));
        Registry.DeleteValue(Encrypt(L"Address"));
        Registry.WriteValue(Encrypt(L"Status"), STATUS_REQUEST_COMPLETED);
    }

    Registry.DeleteValue(Encrypt(L"Status"));
    return;
}