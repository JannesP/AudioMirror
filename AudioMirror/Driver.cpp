#include "Globals.h"

#include "IAdapterCommon.h"
#include "AdapterCommon.h"

#define MAX_ADAPTERS				10 * 2


//-----------------------------------------------------------------------------
// Referenced forward.
//-----------------------------------------------------------------------------
DRIVER_ADD_DEVICE AddDevice;

NTSTATUS StartDevice(_In_  PDEVICE_OBJECT, _In_  PIRP, _In_  PRESOURCELIST);

_Dispatch_type_(IRP_MJ_PNP)
DRIVER_DISPATCH PnpHandler;

extern "C" void DriverUnload(_In_ PDRIVER_OBJECT DriverObject);

typedef void(*fnPcDriverUnload) (PDRIVER_OBJECT);
fnPcDriverUnload gPCDriverUnloadRoutine = NULL;

typedef struct _PortClassDeviceContext              // 32       64      Byte offsets for 32 and 64 bit architectures
{
	ULONG_PTR m_pulReserved1[2];                    // 0-7      0-15    First two pointers are reserved.
	PDEVICE_OBJECT m_DoNotUsePhysicalDeviceObject;  // 8-11     16-23   Reserved pointer to our Physical Device Object (PDO).
	PVOID m_pvReserved2;                            // 12-15    24-31   Reserved pointer to our Start Device function.
	PVOID m_pvReserved3;                            // 16-19    32-39   "Out Memory" according to DDK.  
	IAdapterCommon* m_pCommon;                      // 20-23    40-47   Pointer to our adapter common object.
#ifdef _USE_SingleComponentMultiFxStates
	POHANDLE m_poHandle;                            // 24-27    48-55   PoFxDevice handle.
#else
	PVOID m_pvUnused1;                              // 24-27    48-55   Unused space.
#endif // _USE_SingleComponentMultiFxStates
	PVOID m_pvUnused2;                              // 28-31    56-63   Unused space.

	// Anything after above line should not be used.
	// This actually goes on for (64*sizeof(ULONG_PTR)) but it is all opaque.
} PortClassDeviceContext;

extern "C" DRIVER_INITIALIZE DriverEntry;
extern "C" NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT		DriverObject,
	_In_ PUNICODE_STRING	RegistryPath
)
{
	NTSTATUS status = STATUS_SUCCESS;

	WDF_DRIVER_CONFIG config;
	
	//print hello world
	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "AudioMirror: DriverEntry\n"));

	// Initialize the driver configuration object to register the
	// entry point for the EvtDeviceAdd callback, KmdfHelloWorldEvtDeviceAdd
	WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);

	//
	// Set WdfDriverInitNoDispatchOverride flag to tell the framework
	// not to provide dispatch routines for the driver. In other words,
	// the framework must not intercept IRPs that the I/O manager has
	// directed to the driver. In this case, they will be handled by Audio
	// port driver.
	//
	config.DriverInitFlags |= WdfDriverInitNoDispatchOverride;
	config.DriverPoolTag = MINIADAPTER_POOLTAG;

	status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
	IF_FAILED_LOG_RETURN(status, "WdfDriverCreate failed, 0x%x", status);

	//
	// Tell the class driver to initialize the driver.
	//
	status = PcInitializeAdapterDriver(DriverObject,
		RegistryPath,
		PDRIVER_ADD_DEVICE(AddDevice));
	IF_FAILED_LOG_RETURN(status, "PcInitializeAdapterDriver failed, 0x%x", status);

	//
	// To intercept stop/remove/surprise-remove.
	//
	DriverObject->MajorFunction[IRP_MJ_PNP] = PnpHandler;

	//
	// Hook the port class unload function
	//
	gPCDriverUnloadRoutine = DriverObject->DriverUnload;
	DriverObject->DriverUnload = DriverUnload;

	return status;
}

extern "C" void DriverUnload
(
	_In_ PDRIVER_OBJECT DriverObject
)
/*++

Routine Description:

  Our driver unload routine. This just frees the WDF driver object.

Arguments:

  DriverObject - pointer to the driver object

Environment:

	PASSIVE_LEVEL

--*/
{
	PAGED_CODE();

	DPF(D_TERSE, ("[DriverUnload]"));

	if (DriverObject == NULL)
	{
		goto Done;
	}

	//
	// Invoke first the port unload.
	//
	if (gPCDriverUnloadRoutine != NULL)
	{
		gPCDriverUnloadRoutine(DriverObject);
	}

	//
	// Unload WDF driver object. 
	//
	if (WdfGetDriver() != NULL)
	{
		WdfDriverMiniportUnload(WdfGetDriver());
	}


Done:
	return;
}

NTSTATUS AddDevice
(
	_In_  PDRIVER_OBJECT    DriverObject,
	_In_  PDEVICE_OBJECT    PhysicalDeviceObject
)
/*++

Routine Description:

  The Plug & Play subsystem is handing us a brand new PDO, for which we
  (by means of INF registration) have been asked to provide a driver.

  We need to determine if we need to be in the driver stack for the device.
  Create a function device object to attach to the stack
  Initialize that device object
  Return status success.

  All audio adapter drivers can use this code without change.

Arguments:

  DriverObject - pointer to a driver object

  PhysicalDeviceObject -  pointer to a device object created by the
							underlying bus driver.

Return Value:

  NT status code.

--*/
{
	PAGED_CODE();

	NTSTATUS        ntStatus;

	DPF(D_TERSE, ("[AddDevice]"));

	// Tell the class driver to add the device.
	//
	ntStatus =
		PcAddAdapterDevice
		(
			DriverObject,
			PhysicalDeviceObject,
			PCPFNSTARTDEVICE(StartDevice),
			MAX_ADAPTERS,
			0
		);



	return ntStatus;
} // AddDevice

NTSTATUS StartDevice
(
	_In_  PDEVICE_OBJECT          DeviceObject,
	_In_  PIRP                    Irp,
	_In_  PRESOURCELIST           ResourceList
)
{
	/*++

	Routine Description:

	  This function is called by the operating system when the device is
	  started.
	  It is responsible for starting the miniports.  This code is specific to
	  the adapter because it calls out miniports for functions that are specific
	  to the adapter.

	Arguments:

	  DeviceObject - pointer to the driver object

	  Irp - pointer to the irp

	  ResourceList - pointer to the resource list assigned by PnP manager

	Return Value:

	  NT status code.

	--*/
	UNREFERENCED_PARAMETER(ResourceList);

	PAGED_CODE();

	ASSERT(DeviceObject);
	ASSERT(Irp);
	ASSERT(ResourceList);
	NTSTATUS                    ntStatus = STATUS_SUCCESS;

	IAdapterCommon*             pAdapterCommon = NULL;
	PUNKNOWN                    pUnknownCommon = NULL;
	PortClassDeviceContext*     pExtension = static_cast<PortClassDeviceContext*>(DeviceObject->DeviceExtension);

	DPF_ENTER(("[StartDevice]"));

	//
	// create a new adapter common object
	//
	ntStatus = AdapterCommon::Create(
		&pUnknownCommon,
		IID_IAdapterCommon,
		NULL,
		NonPagedPoolNx,
		DeviceObject,
		Irp
	);
	IF_FAILED_JUMP(ntStatus, Exit);

	ntStatus = pUnknownCommon->QueryInterface(IID_IAdapterCommon, (PVOID *)&pAdapterCommon);
	IF_FAILED_JUMP(ntStatus, Exit);
	
	//DISABLED CAUSE I PROBABLY DONT NEED IT
	//to elaborate: we don't have hardware that could use less power
	// register with PortCls for power-management services
	//ntStatus = PcRegisterAdapterPowerManagement(PUNKNOWN(pAdapterCommon), DeviceObject);
	//IF_FAILED_JUMP(ntStatus, Exit);

Exit:

	//
	// Stash the adapter common object in the device extension so
	// we can access it for cleanup on stop/removal.
	//
	if (pAdapterCommon)
	{
		ASSERT(pExtension != NULL);
		pExtension->m_pCommon = pAdapterCommon;
	}

	//
	// Release the adapter IUnknown interface.
	//
	if (pUnknownCommon) pUnknownCommon->Release();

	return ntStatus;
} // StartDevice

NTSTATUS PnpHandler
(
	_In_ DEVICE_OBJECT* _DeviceObject,
	_Inout_ IRP *_Irp
)
/*++

Routine Description:

  Handles PnP IRPs

Arguments:

  _DeviceObject - Functional Device object pointer.

  _Irp - The Irp being passed

Return Value:

  NT status code.

--*/
{
	NTSTATUS                ntStatus = STATUS_UNSUCCESSFUL;
	IO_STACK_LOCATION      *stack;
	PortClassDeviceContext *ext;

	// Documented https://msdn.microsoft.com/en-us/library/windows/hardware/ff544039(v=vs.85).aspx
	// This method will be called in IRQL PASSIVE_LEVEL
#pragma warning(suppress: 28118)
	PAGED_CODE();

	ASSERT(_DeviceObject);
	ASSERT(_Irp);

	//
	// Check for the REMOVE_DEVICE irp.  If we're being unloaded, 
	// uninstantiate our devices and release the adapter common
	// object.
	//
	stack = IoGetCurrentIrpStackLocation(_Irp);


	if ((IRP_MN_REMOVE_DEVICE == stack->MinorFunction) ||
		(IRP_MN_SURPRISE_REMOVAL == stack->MinorFunction) ||
		(IRP_MN_STOP_DEVICE == stack->MinorFunction))
	{
		ext = static_cast<PortClassDeviceContext*>(_DeviceObject->DeviceExtension);

		if (ext->m_pCommon != NULL)
		{
			ext->m_pCommon->Cleanup();

			ext->m_pCommon->Release();
			
			ext->m_pCommon = NULL;
			
		}
	}

	ntStatus = PcDispatchIrp(_DeviceObject, _Irp);

	return ntStatus;
}