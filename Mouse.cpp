#include "Driver.h"

MOUSE_OBJECT gMouseObject;

//
// Used in MouseAsm.ASM
//

extern "C" {
	ULONG64 _KeAcquireSpinLockAtDpcLevel;
	ULONG64 _KeReleaseSpinLockFromDpcLevel;
	ULONG64 _IofCompleteRequest;
	ULONG64 _IoReleaseRemoveLockEx;
}

//VOID RSSetCursorPosition(LONG DeltaX, LONG DeltaY)
//{
//	PEPROCESS WinLogon;
//	KAPC_STATE APC = { 0 };
//	ULONG64 gptCursorAsync;
//	POINT Position;
//
//	WinLogon = RSGetProcessByName("winlogon.exe");
//
//	if (WinLogon)
//	{
//		KeStackAttachProcess(WinLogon, &APC);
//
//		gptCursorAsync = (ULONG64)RSGetSystemModuleExport(L"win32kbase.sys", "gptCursorAsync");
//		Position = *(POINT*)(gptCursorAsync);
//		Position.x = DeltaX;
//		Position.y = DeltaY;
//
//		*(POINT*)(gptCursorAsync) = Position;
//
//		KeUnstackDetachProcess(&APC);
//		ObDereferenceObject(WinLogon);
//	}
//}
//
//VOID RSSetCursorRelative(LONG DeltaX, LONG DeltaY)
//{
//	PEPROCESS WinLogon;
//	KAPC_STATE APC = { 0 };
//	ULONG64 gptCursorAsync;
//	POINT Position;
//
//	WinLogon = RSGetProcessByName("winlogon.exe");
//
//	if (WinLogon)
//	{
//		KeStackAttachProcess(WinLogon, &APC);
//
//		gptCursorAsync = (ULONG64)RSGetSystemModuleExport(L"win32kbase.sys", "gptCursorAsync");
//		Position = *(POINT*)(gptCursorAsync);
//
//		Position.x += DeltaX;
//		Position.y += DeltaY;
//
//		*(POINT*)(gptCursorAsync) = Position;
//
//		KeUnstackDetachProcess(&APC);
//		ObDereferenceObject(WinLogon);
//	}
//}

VOID InitializeMouseControl(VOID)
{
	/* Microsoft compiler is sometimes retarded, thats why we have to do this non sense */
	/* It would otherwise generate wrapper functions around, and it would cause system BSOD */

	_KeAcquireSpinLockAtDpcLevel	= (ULONG64)KeAcquireSpinLockAtDpcLevel;
	_KeReleaseSpinLockFromDpcLevel	= (ULONG64)KeReleaseSpinLockFromDpcLevel;
	_IofCompleteRequest				= (ULONG64)IofCompleteRequest;
	_IoReleaseRemoveLockEx			= (ULONG64)IoReleaseRemoveLockEx;
}

BOOLEAN OpenMouse(VOID)
{
	// https://github.com/nbqofficial/norsefire

	if (gMouseObject.UseMouse == 0) {

		UNICODE_STRING class_string;
		RtlInitUnicodeString(&class_string, Encrypt(L"\\Driver\\MouClass"));

		PDRIVER_OBJECT ClassDriverObject = NULL;
		NTSTATUS status = ObReferenceObjectByName(&class_string, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&ClassDriverObject);
		if (!NT_SUCCESS(status)) {
			gMouseObject.UseMouse = 0;
			return 0;
		}

		UNICODE_STRING wzHidString;
		RtlInitUnicodeString(&wzHidString, Encrypt(L"\\Driver\\MouHID"));

		PDRIVER_OBJECT HidDriverObject = NULL;

		status = ObReferenceObjectByName(&wzHidString, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&HidDriverObject);
		if (!NT_SUCCESS(status))
		{
			if (ClassDriverObject) {
				ObDereferenceObject(ClassDriverObject);
			}
			gMouseObject.UseMouse = 0;
			return 0;
		}

		PVOID class_driver_base = NULL;

		PDEVICE_OBJECT HidDeviceObject = HidDriverObject->DeviceObject;
		while (HidDeviceObject && !gMouseObject.ServiceCallback)
		{
			PDEVICE_OBJECT ClassDeviceObject = ClassDriverObject->DeviceObject;
			while (ClassDeviceObject && !gMouseObject.ServiceCallback)
			{
				if (!ClassDeviceObject->NextDevice && !gMouseObject.DeviceObject)
				{
					gMouseObject.DeviceObject = ClassDeviceObject;
				}

				PULONG64 device_extension = (PULONG64)HidDeviceObject->DeviceExtension;
				ULONG64 device_ext_size = ((ULONG64)HidDeviceObject->DeviceObjectExtension - (ULONG64)HidDeviceObject->DeviceExtension) / 4;
				class_driver_base = ClassDriverObject->DriverStart;
				for (ULONG64 i = 0; i < device_ext_size; i++)
				{
					if (device_extension[i] == (ULONG64)ClassDeviceObject && device_extension[i + 1] > (ULONG64)ClassDriverObject)
					{
						gMouseObject.ServiceCallback = (MouseClassServiceCallbackFn)(device_extension[i + 1]);

						break;
					}
				}
				ClassDeviceObject = ClassDeviceObject->NextDevice;
			}
			HidDeviceObject = HidDeviceObject->AttachedDevice;
		}

		if (!gMouseObject.DeviceObject)
		{
			PDEVICE_OBJECT TargetDeviceObject = ClassDriverObject->DeviceObject;
			while (TargetDeviceObject)
			{
				if (!TargetDeviceObject->NextDevice)
				{
					gMouseObject.DeviceObject = TargetDeviceObject;
					break;
				}
				TargetDeviceObject = TargetDeviceObject->NextDevice;
			}
		}

		ObDereferenceObject(ClassDriverObject);
		ObDereferenceObject(HidDriverObject);

		if (gMouseObject.DeviceObject && gMouseObject.ServiceCallback) {
			gMouseObject.UseMouse = 1;
		}

	}

	return gMouseObject.DeviceObject && gMouseObject.ServiceCallback;
}

VOID MoveMouse(LONG DeltaX, LONG DeltaY, USHORT ButtonFlags)
{
	KIRQL irql;
	ULONG input_data;
	MOUSE_INPUT_DATA mid = { 0 };
	mid.LastX = DeltaX;
	mid.LastY = DeltaY;
	mid.ButtonFlags = ButtonFlags;
	mid.UnitId = 1;
	irql = KeGetCurrentIrql();
	if (!OpenMouse()) {
		return;
	}
	KeMRaiseIrql(DISPATCH_LEVEL, &irql);
	MouseClassServiceCallback(gMouseObject.DeviceObject, &mid, (PMOUSE_INPUT_DATA)&mid + 1, &input_data);
	KeLowerIrql(irql);
}
