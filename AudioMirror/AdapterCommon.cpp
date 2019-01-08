#include "AdapterCommon.h"

#include "RegistryHelper.h"
#include "SubdeviceHelper.h"
#include "MinipairDescriptorFactory.h"
#include "MiniportWaveRT.h"

LONG AdapterCommon::m_Instances = 0;

AdapterCommon::~AdapterCommon()
{
	PAGED_CODE();
	DPF_ENTER(("[CAdapterCommon::~CAdapterCommon]"));

	if (m_pDeviceHelper)
	{
		ExFreePoolWithTag(m_pDeviceHelper, MINIADAPTER_POOLTAG);
	}

	InterlockedDecrement(&AdapterCommon::m_Instances);
	ASSERT(AdapterCommon::m_Instances == 0);
}

/*
	Creates a new AdapterCommon
	Arguments:

	  Unknown -

	  UnknownOuter -

	  PoolType
*/
NTSTATUS AdapterCommon::Create
(
	_Out_       PUNKNOWN *              Unknown,
	_In_        REFCLSID,
	_In_opt_    PUNKNOWN                UnknownOuter,
	_When_((PoolType & NonPagedPoolMustSucceed) != 0,
		__drv_reportError("Must succeed pool allocations are forbidden. "
			"Allocation failures cause a system crash"))
	_In_        POOL_TYPE               PoolType,
	_In_		PDEVICE_OBJECT DeviceObject,
	_In_		PIRP StartupIrp
)
{
	PAGED_CODE();
	DPF_ENTER(("[CAdapterCommon::Create]"));
	ASSERT(Unknown);
	ASSERT(StartupIrp);

	NTSTATUS ntStatus;

	//
	// This is a singleton, check before creating instance.
	//
	if (InterlockedCompareExchange(&AdapterCommon::m_Instances, 1, 0) != 0)
	{
		ntStatus = STATUS_DEVICE_BUSY;
		DPF(D_TERSE, ("NewAdapterCommon failed, adapter already created."));
		goto Done;
	}

	//
	// Allocate an adapter object.
	//
	AdapterCommon* p = new(PoolType, MINIADAPTER_POOLTAG) AdapterCommon(UnknownOuter);
	if (p == NULL)
	{
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		DPF(D_TERSE, ("NewAdapterCommon failed, 0x%x", ntStatus));
		goto Done;
	}

	ntStatus = p->Init(StartupIrp, DeviceObject);
	IF_FAILED_ACTION_RETURN(ntStatus, DPF(D_TERSE, ("Error initializing Adapter, 0x%x", ntStatus)));

	// 
	// Success.
	//
	*Unknown = PUNKNOWN((IAdapterCommon*)(p));
	(*Unknown)->AddRef();
	ntStatus = STATUS_SUCCESS;

Done:
	return ntStatus;
} // NewAdapterCommon


/*
	Initialize the Adapter.
*/
NTSTATUS __stdcall AdapterCommon::Init(IRP* StartupIrp, PDEVICE_OBJECT DeviceObject)
{
	PAGED_CODE();
	DPF_ENTER(("[CAdapterCommon::Init]"));
	ASSERT(DeviceObject);

	NTSTATUS        ntStatus = STATUS_SUCCESS;

	m_pDeviceObject = DeviceObject;
	m_pDeviceHelper = new(NonPagedPoolNx, MINIADAPTER_POOLTAG) SubdeviceHelper(this);
	if (!m_pDeviceHelper)
	{
		m_pDeviceHelper = NULL;
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
	}

	if (NT_SUCCESS(ntStatus))
	{
		ntStatus = PcGetPhysicalDeviceObject(DeviceObject, &m_pPhysicalDeviceObject);
		if (!NT_SUCCESS(ntStatus)) DPF(D_TERSE, ("PcGetPhysicalDeviceObject failed, 0x%x", ntStatus));
	}

	ntStatus = InstallVirtualCable(StartupIrp);
	IF_FAILED_ACTION_RETURN(ntStatus, DPF(D_TERSE, ("InstallVirtualCable failed, 0x%x", ntStatus)));

	if (!NT_SUCCESS(ntStatus))
	{
		m_pDeviceObject = NULL;
		if (m_pDeviceHelper) ExFreePoolWithTag(m_pDeviceHelper, MINIADAPTER_POOLTAG);
		m_pPhysicalDeviceObject = NULL;
	}

	return ntStatus;
}

NTSTATUS AdapterCommon::InstallVirtualCable(IRP * irp)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	IUnknown* unknownMic;
	IUnknown* unknownSpeaker;

	ntStatus = InstallVirtualMic(irp, &unknownMic);
	IF_FAILED_ACTION_RETURN(ntStatus, DPF(D_TERSE, ("InstallVirtualMic failed, 0x%x", ntStatus)));
	ntStatus = InstallVirtualSpeaker(irp, &unknownSpeaker);
	IF_FAILED_ACTION_RETURN(ntStatus, DPF(D_TERSE, ("InstallVirtualSpeaker failed, 0x%x", ntStatus)));

	MiniportWaveRT* microphone;
	ntStatus = unknownMic->QueryInterface(IID_MiniportWaveRT, (PVOID*)&microphone);
	MiniportWaveRT* speaker;
	ntStatus = unknownSpeaker->QueryInterface(IID_MiniportWaveRT, (PVOID*)&speaker);
	microphone->SetPairedMiniport(speaker);

	return STATUS_SUCCESS;
}

NTSTATUS AdapterCommon::InstallVirtualMic(IRP* Irp, IUnknown** unknownMiniport)
{
	PAGED_CODE();
	NTSTATUS ntStatus = STATUS_NOT_IMPLEMENTED;

	ENDPOINT_MINIPAIR* pCaptureMiniport = NULL;
	ntStatus = MinipairDescriptorFactory::CreateMicrophone(&pCaptureMiniport);
	if (!NT_SUCCESS(ntStatus)) 
	{
		return ntStatus;
	}
	m_pDeviceHelper->InstallMinipair(Irp, pCaptureMiniport, NULL, NULL, NULL, NULL, unknownMiniport);

	return ntStatus;
}

NTSTATUS AdapterCommon::InstallVirtualSpeaker(IRP* Irp, IUnknown** unknownMiniport)
{
	PAGED_CODE();
	NTSTATUS ntStatus = STATUS_NOT_IMPLEMENTED;

	ENDPOINT_MINIPAIR* pRenderMiniport = NULL;
	ntStatus = MinipairDescriptorFactory::CreateSpeaker(&pRenderMiniport);
	if (!NT_SUCCESS(ntStatus))
	{
		return ntStatus;
	}
	m_pDeviceHelper->InstallMinipair(Irp, pRenderMiniport, NULL, NULL, NULL, NULL, unknownMiniport);

	return ntStatus;
}



STDMETHODIMP AdapterCommon::NonDelegatingQueryInterface
(
	_In_ REFIID                      Interface,
	_COM_Outptr_ PVOID *        Object
)
/*++

Routine Description:

  QueryInterface routine for AdapterCommon

Arguments:

  Interface -

  Object -

Return Value:

  NT status code.

--*/
{
	PAGED_CODE();

	ASSERT(Object);

	if (IsEqualGUIDAligned(Interface, IID_IUnknown))
	{
		*Object = PVOID(PUNKNOWN((IAdapterCommon*)(this)));
	}
	else if (IsEqualGUIDAligned(Interface, IID_IAdapterCommon))
	{
		*Object = PVOID((IAdapterCommon*)(this));
	}
	else if (IsEqualGUIDAligned(Interface, IID_IAdapterPowerManagement))
	{
		*Object = PVOID(PADAPTERPOWERMANAGEMENT(this));
	}
	else
	{
		*Object = NULL;
	}

	if (*Object)
	{
		PUNKNOWN(*Object)->AddRef();
		return STATUS_SUCCESS;
	}

	return STATUS_INVALID_PARAMETER;
} // NonDelegatingQueryInterface

PDEVICE_OBJECT __stdcall AdapterCommon::GetDeviceObject(void)
{
	return m_pDeviceObject;
}

PDEVICE_OBJECT __stdcall AdapterCommon::GetPhysicalDeviceObject(void)
{
	return m_pPhysicalDeviceObject;
}

#pragma code_seg()
NTSTATUS __stdcall AdapterCommon::WriteEtwEvent(EPcMiniportEngineEvent miniportEventType, ULONGLONG ullData1, ULONGLONG ullData2, ULONGLONG ullData3, ULONGLONG ullData4)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;

	if (m_pPortClsEtwHelper)
	{
		ntStatus = m_pPortClsEtwHelper->MiniportWriteEtwEvent(miniportEventType, ullData1, ullData2, ullData3, ullData4);
	}
	return ntStatus;
}

#pragma code_seg("PAGE")
void __stdcall AdapterCommon::SetEtwHelper(PPORTCLSETWHELPER _pPortClsEtwHelper)
{
	PAGED_CODE();

	SAFE_RELEASE(m_pPortClsEtwHelper);

	m_pPortClsEtwHelper = _pPortClsEtwHelper;

	if (m_pPortClsEtwHelper)
	{
		m_pPortClsEtwHelper->AddRef();
	}
}

void __stdcall AdapterCommon::Cleanup()
{
	PAGED_CODE();
	DPF_ENTER(("[AdapterCommon::Cleanup]"));

	if (m_pDeviceHelper) m_pDeviceHelper->Clean();
}
