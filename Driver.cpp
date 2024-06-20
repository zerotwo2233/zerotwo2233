
#include "Driver.h"

HANDLE Thread;
BOOLEAN Running;

DRIVER_UNLOAD       DriverUnload;
DRIVER_INITIALIZE   DriverMain;
KSTART_ROUTINE      DriverThread;

NTSTATUS DriverMain(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);
#ifdef DBG
    DriverObject->DriverUnload = DriverUnload;
#endif
    Running = TRUE;

    #define USE_SYSTEM_THREAD
    #ifdef USE_SYSTEM_THREAD
    Status  = PsCreateSystemThread(&Thread, 0, NULL, NULL, NULL, DriverThread, &Running);
    #else

    #endif


    if (!NT_SUCCESS(Status))
        LogPrint("Driver Entry Failed.");
    else
        LogPrint("Driver Loaded Successfully.");

    InitializeMouseControl();
	return Status;
}

VOID DriverUnload(
    PDRIVER_OBJECT  DriverObject
    )
{
    UNREFERENCED_PARAMETER(DriverObject);
    
    Running = FALSE;
    ZwWaitForSingleObject(Thread, FALSE, NULL);
    ZwClose(Thread);

    LogPrint("Driver Unloaded.");
}
