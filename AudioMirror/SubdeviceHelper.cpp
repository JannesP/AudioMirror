#include "SubdeviceHelper.h"

#include "RegistryHelper.h"

SubdeviceHelper::SubdeviceHelper(IAdapterCommon* adapter) : m_Adapter(adapter)
{
	
}
SubdeviceHelper::~SubdeviceHelper()
{
}

/*
	This method copies all of the properties from the template interface,
	which is specified in the inf, to the actual interface being used which
	may be dynamically generated at run time. This allows for a driver
	to reuse a single inf entry for multiple audio endpoints. The primary
	purpose for this is to allow for sideband audio endpoints to dynamically
	generate the reference string at run time, tied to the peripheral connected,
	while still having a simple static inf entry for setting up apo's or other
	parameters.

	For example, if you have an interface in your inf defined with reference string
	"SpeakerWave". At runtime you could generate "SpeakerWave-1234ABCDE", and specify
	"SpeakerWave" as the template name. When "SpeakerWave-1234ABCDE" is installed
	we will copy all of the parameters that were specified in the inf for "SpeakerWave"
	over to "SpeakerWave-1234ABCDE". You simply need to specify "SpeakerWave" as the
	"TemplateName" in the ENDPOINT_MINIPAIRS.

	By default, the first level of registry keys are not copied. Only the 2nd level and
	deeper are copied. This way the friendly name and other PNP properties will not
	be modified, but the EP and FX properties will be copied.

	Return Value:

	NT status code.

*/
NTSTATUS SubdeviceHelper::MigrateDeviceInterfaceTemplateParameters
(
	_In_ PUNICODE_STRING    SymbolicLinkName,
	_In_ PDEVICE_OBJECT		pPhysicalDeviceObject,
	_In_opt_ PCWSTR         TemplateReferenceString
)
{
	NTSTATUS            ntStatus = STATUS_SUCCESS;
	HANDLE              hDeviceInterfaceParametersKey(NULL);
	HANDLE              hTemplateDeviceInterfaceParametersKey(NULL);
	UNICODE_STRING      TemplateSymbolicLinkName;
	UNICODE_STRING      referenceString;

	RtlInitUnicodeString(&TemplateSymbolicLinkName, NULL);
	RtlInitUnicodeString(&referenceString, TemplateReferenceString);

	//
	// Register an audio interface if not already present for the template interface, so we can access
	// the registry path. If it's already registered, this simply returns the symbolic link name. 
	// No need to unregister it (there is no mechanism to), and we'll never make it active.
	//
	ntStatus = IoRegisterDeviceInterface(
		pPhysicalDeviceObject,
		&KSCATEGORY_AUDIO,
		&referenceString,
		&TemplateSymbolicLinkName);

	// Open the template device interface's registry key path
	ntStatus = IoOpenDeviceInterfaceRegistryKey(&TemplateSymbolicLinkName, GENERIC_READ, &hTemplateDeviceInterfaceParametersKey);
	IF_FAILED_JUMP(ntStatus, Exit);

	// Open the new device interface's registry key path that we plan to activate
	ntStatus = IoOpenDeviceInterfaceRegistryKey(SymbolicLinkName, GENERIC_WRITE, &hDeviceInterfaceParametersKey);
	IF_FAILED_JUMP(ntStatus, Exit);

	// Copy the template device parameters key to the device interface key
	ntStatus = RegistryHelper::CopyRegistryKey(hTemplateDeviceInterfaceParametersKey, hDeviceInterfaceParametersKey);
	IF_FAILED_JUMP(ntStatus, Exit);

Exit:
	RtlFreeUnicodeString(&TemplateSymbolicLinkName);

	if (hTemplateDeviceInterfaceParametersKey)
	{
		ZwClose(hTemplateDeviceInterfaceParametersKey);
	}

	if (hDeviceInterfaceParametersKey)
	{
		ZwClose(hDeviceInterfaceParametersKey);
	}

	return ntStatus;
}


/*
	Creates an audio interface (in disabled mode).
*/
NTSTATUS SubdeviceHelper::CreateAudioInterfaceWithProperties
(
	_In_ PCWSTR ReferenceString,
	_In_opt_ PCWSTR TemplateReferenceString,
	_In_ ULONG cPropertyCount,
	_In_reads_opt_(cPropertyCount) const SYSVAD_DEVPROPERTY *pProperties,
	_In_ PDEVICE_OBJECT PhysicalDeviceObject,
	_Out_ _At_(AudioSymbolicLinkName->Buffer, __drv_allocatesMem(Mem)) UNICODE_STRING* AudioSymbolicLinkName
)

{
	PAGED_CODE();
	DPF_ENTER(("[CAdapterCommon::CreateAudioInterfaceWithProperties]"));

	NTSTATUS        ntStatus;
	UNICODE_STRING  referenceString;

	RtlInitUnicodeString(&referenceString, ReferenceString);

	//
	// Reset output value.
	//
	RtlZeroMemory(AudioSymbolicLinkName, sizeof(UNICODE_STRING));

	//
	// Register an audio interface if not already present.
	//
	ntStatus = IoRegisterDeviceInterface(
		PhysicalDeviceObject,
		&KSCATEGORY_AUDIO,
		&referenceString,
		AudioSymbolicLinkName);

	IF_FAILED_ACTION_JUMP(
		ntStatus,
		DPF(D_ERROR, ("CreateAudioInterfaceWithProperties: IoRegisterDeviceInterface(KSCATEGORY_AUDIO): failed, 0x%x", ntStatus)),
		Done);

	//
	// Migrate optional device interface parameters from the template if it exists
	// This is done first, so that any additional parameters in pProperties will override the defaults.
	//
	if (NULL != TemplateReferenceString)
	{
		ntStatus = MigrateDeviceInterfaceTemplateParameters(AudioSymbolicLinkName, PhysicalDeviceObject, TemplateReferenceString);

		IF_FAILED_ACTION_JUMP(
			ntStatus,
			DPF(D_ERROR, ("MigrateDeviceInterfaceTempalteParameters: MigrateDeviceInterfaceTemplateParameters(...): failed, 0x%x", ntStatus)),
			Done);
	}

	//
	// Set properties on the interface
	//
	ntStatus = SysvadIoSetDeviceInterfacePropertyDataMultiple(AudioSymbolicLinkName, cPropertyCount, pProperties);

	IF_FAILED_ACTION_JUMP(
		ntStatus,
		DPF(D_ERROR, ("CreateAudioInterfaceWithProperties: SysvadIoSetDeviceInterfacePropertyDataMultiple(...): failed, 0x%x", ntStatus)),
		Done);

	//
	// All done.
	//
	ntStatus = STATUS_SUCCESS;

Done:
	if (!NT_SUCCESS(ntStatus))
	{
		RtlFreeUnicodeString(AudioSymbolicLinkName);
		RtlZeroMemory(AudioSymbolicLinkName, sizeof(UNICODE_STRING));
	}
	return ntStatus;
}


NTSTATUS SubdeviceHelper::SysvadIoSetDeviceInterfacePropertyDataMultiple
(
	_In_ PUNICODE_STRING                                        SymbolicLinkName,
	_In_ ULONG                                                  cPropertyCount,
	_In_reads_opt_(cPropertyCount) const SYSVAD_DEVPROPERTY     *pProperties
)
{
	NTSTATUS ntStatus;

	PAGED_CODE();

	if (pProperties)
	{
		for (ULONG i = 0; i < cPropertyCount; i++)
		{
			ntStatus = IoSetDeviceInterfacePropertyData(
				SymbolicLinkName,
				pProperties[i].PropertyKey,
				LOCALE_NEUTRAL,
				PLUGPLAY_PROPERTY_PERSISTENT,
				pProperties[i].Type,
				pProperties[i].BufferSize,
				pProperties[i].Buffer);

			if (!NT_SUCCESS(ntStatus))
			{
				return ntStatus;
			}
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS __stdcall SubdeviceHelper::InstallMinipair
(
	_In_opt_    PIRP                Irp,
	_In_        PENDPOINT_MINIPAIR  MiniportPair,
	_In_opt_    PVOID               DeviceContext,
	_Out_opt_   PUNKNOWN *          UnknownTopology,
	_Out_opt_   PUNKNOWN *          UnknownWave,
	_Out_opt_   PUNKNOWN *          UnknownMiniportTopology,
	_Out_opt_   PUNKNOWN *          UnknownMiniportWave
)
{
	PAGED_CODE();
	DPF_ENTER(("[SubdeviceHelper::InstallMinipair]"));

	NTSTATUS            ntStatus = STATUS_SUCCESS;
	PUNKNOWN            unknownTopology = NULL;
	PUNKNOWN            unknownWave = NULL;
	BOOL                bTopologyCreated = FALSE;
	BOOL                bWaveCreated = FALSE;
	PUNKNOWN            unknownMiniTopo = NULL;
	PUNKNOWN            unknownMiniWave = NULL;

	// Initialize output optional parameters if needed
	if (UnknownTopology)
	{
		*UnknownTopology = NULL;
	}

	if (UnknownWave)
	{
		*UnknownWave = NULL;
	}

	if (UnknownMiniportTopology)
	{
		*UnknownMiniportTopology = NULL;
	}

	if (UnknownMiniportWave)
	{
		*UnknownMiniportWave = NULL;
	}

	ntStatus = m_DeviceCache.Get(MiniportPair->TopoName, &unknownTopology, &unknownMiniTopo);
	if (!NT_SUCCESS(ntStatus) || NULL == unknownTopology || NULL == unknownMiniTopo)
	{
		bTopologyCreated = TRUE;

		// Install SYSVAD topology miniport for the render endpoint.
		//
		ntStatus = InstallSubdevice(Irp,
			MiniportPair->TopoName, // make sure this name matches with SYSVAD.<TopoName>.szPname in the inf's [Strings] section
			MiniportPair->TemplateTopoName,
			CLSID_PortTopology,
			CLSID_PortTopology,
			MiniportPair->TopoCreateCallback,
			MiniportPair->TopoInterfacePropertyCount,
			MiniportPair->TopoInterfaceProperties,
			DeviceContext,
			MiniportPair,
			NULL,
			IID_IPortTopology,
			NULL,
			&unknownTopology,
			&unknownMiniTopo
		);
		if (NT_SUCCESS(ntStatus))
		{
			ntStatus = m_DeviceCache.Put(MiniportPair->TopoName, unknownTopology, unknownMiniTopo);
		}
	}

	ntStatus = m_DeviceCache.Get(MiniportPair->WaveName, &unknownWave, &unknownMiniWave);
	if (!NT_SUCCESS(ntStatus) || NULL == unknownWave || NULL == unknownMiniWave)
	{
		bWaveCreated = TRUE;

		// Install SYSVAD wave miniport for the render endpoint.
		//
		ntStatus = InstallSubdevice(Irp,
			MiniportPair->WaveName, // make sure this name matches with SYSVAD.<WaveName>.szPname in the inf's [Strings] section
			MiniportPair->TemplateWaveName,
			CLSID_PortWaveRT,
			CLSID_PortWaveRT,
			MiniportPair->WaveCreateCallback,
			MiniportPair->WaveInterfacePropertyCount,
			MiniportPair->WaveInterfaceProperties,
			DeviceContext,
			MiniportPair,
			NULL,
			IID_IPortWaveRT,
			NULL,
			&unknownWave,
			&unknownMiniWave
		);

		if (NT_SUCCESS(ntStatus))
		{
			ntStatus = m_DeviceCache.Put(MiniportPair->WaveName, unknownWave, unknownMiniWave);
		}
	}

	if (unknownTopology && unknownWave)
	{
		//
		// register wave <=> topology connections
		// This will connect bridge pins of wave and topology
		// miniports.
		//
		ntStatus = ConnectTopologies(
			unknownTopology,
			unknownWave,
			MiniportPair->PhysicalConnections,
			MiniportPair->PhysicalConnectionCount);
	}

	if (NT_SUCCESS(ntStatus))
	{
		//
		// Set output parameters.
		//
		if (UnknownTopology != NULL && unknownTopology != NULL)
		{
			unknownTopology->AddRef();
			*UnknownTopology = unknownTopology;
		}

		if (UnknownWave != NULL && unknownWave != NULL)
		{
			unknownWave->AddRef();
			*UnknownWave = unknownWave;
		}
		if (UnknownMiniportTopology != NULL && unknownMiniTopo != NULL)
		{
			unknownMiniTopo->AddRef();
			*UnknownMiniportTopology = unknownMiniTopo;
		}

		if (UnknownMiniportWave != NULL && unknownMiniWave != NULL)
		{
			unknownMiniWave->AddRef();
			*UnknownMiniportWave = unknownMiniWave;
		}

	}
	else
	{
		if (bTopologyCreated && unknownTopology != NULL)
		{
			//UnregisterSubdevice(unknownTopology);
			m_DeviceCache.Remove(MiniportPair->TopoName);
		}

		if (bWaveCreated && unknownWave != NULL)
		{
			//UnregisterSubdevice(unknownWave);
			m_DeviceCache.Remove(MiniportPair->WaveName);
		}
	}

	SAFE_RELEASE(unknownMiniTopo);
	SAFE_RELEASE(unknownTopology);
	SAFE_RELEASE(unknownMiniWave);
	SAFE_RELEASE(unknownWave);

	return ntStatus;
}

void __stdcall SubdeviceHelper::Clean()
{
	m_DeviceCache.Clear();
}

NTSTATUS __stdcall SubdeviceHelper::InstallSubdevice
(
	_In_opt_        PIRP                                    Irp,
	_In_            PWSTR                                   Name,
	_In_opt_        PWSTR                                   TemplateName,
	_In_            REFGUID                                 PortClassId,
	_In_            REFGUID                                 MiniportClassId,
	_In_opt_        PFNCREATEMINIPORT                       MiniportCreate,
	_In_            ULONG                                   cPropertyCount,
	_In_reads_opt_(cPropertyCount) const SYSVAD_DEVPROPERTY* pProperties,
	_In_opt_        PVOID                                   DeviceContext,
	_In_            PENDPOINT_MINIPAIR                      MiniportPair,
	_In_opt_        PRESOURCELIST                           ResourceList,
	_In_            REFGUID                                 PortInterfaceId,
	_Out_opt_       PUNKNOWN*								OutPortInterface,
	_Out_opt_       PUNKNOWN*								OutPortUnknown,
	_Out_opt_       PUNKNOWN*								OutMiniportUnknown
)
{
	/*
		This function creates and registers a subdevice consisting of a port
		driver, a minport driver and a set of resources bound together.  It will
		also optionally place a pointer to an interface on the port driver in a
		specified location before initializing the port driver.  This is done so
		that a common ISR can have access to the port driver during
		initialization, when the ISR might fire.

	Arguments:

		Irp - pointer to the irp object.

		Name - name of the miniport. Passes to PcRegisterSubDevice

		PortClassId - port class id. Passed to PcNewPort.

		MiniportClassId - miniport class id. Passed to PcNewMiniport.

		MiniportCreate - pointer to a miniport creation function. If NULL,
						 PcNewMiniport is used.

		DeviceContext - deviceType specific.

		MiniportPair - endpoint configuration info.

		ResourceList - pointer to the resource list.

		PortInterfaceId - GUID that represents the port interface.

		OutPortInterface - pointer to store the port interface

		OutPortUnknown - pointer to store the unknown port interface.

		OutMiniportUnknown - pointer to store the unknown miniport interface

	Return Value:

		NT status code.

	*/
	PAGED_CODE();
	DPF_ENTER(("[InstallSubDevice %S]", Name));

	ASSERT(Name != NULL);
	ASSERT(m_Adapter->GetPhysicalDeviceObject() != NULL);

	NTSTATUS                    ntStatus;
	PPORT                       port = NULL;
	PUNKNOWN                    miniport = NULL;
	UNICODE_STRING              symbolicLink = { 0 };

	ntStatus = SubdeviceHelper::CreateAudioInterfaceWithProperties(
		Name, TemplateName, cPropertyCount, pProperties, m_Adapter->GetPhysicalDeviceObject(), &symbolicLink);
	if (NT_SUCCESS(ntStatus))
	{
		// Currently have no use for the symbolic link
		RtlFreeUnicodeString(&symbolicLink);

		// Create the port driver object
		//
		ntStatus = PcNewPort(&port, PortClassId);
	}

	// Create the miniport object
	//
	if (NT_SUCCESS(ntStatus))
	{
		if (MiniportCreate)
		{
			ntStatus =
				MiniportCreate
				(
					&miniport,
					MiniportClassId,
					NULL,
					NonPagedPoolNx,
					m_Adapter,
					DeviceContext,
					MiniportPair
				);
		}
		else
		{
			ntStatus =
				PcNewMiniport
				(
				(PMINIPORT*)&miniport,
					MiniportClassId
				);
		}
	}

	// Init the port driver and miniport in one go.
	//
	if (NT_SUCCESS(ntStatus))
	{
#pragma warning(push)
		// IPort::Init's annotation on ResourceList requires it to be non-NULL.  However,
		// for dynamic devices, we may no longer have the resource list and this should
		// still succeed.
		//
#pragma warning(disable:6387)
		ntStatus =
			port->Init
			(
				m_Adapter->GetDeviceObject(),
				Irp,
				miniport,
				m_Adapter,
				ResourceList
			);
#pragma warning (pop)

		if (NT_SUCCESS(ntStatus))
		{
			// Register the subdevice (port/miniport combination).
			//
			ntStatus =
				PcRegisterSubdevice
				(
					m_Adapter->GetDeviceObject(),
					Name,
					port
				);
		}
	}

	// Deposit the port interfaces if it's needed.
	//
	if (NT_SUCCESS(ntStatus))
	{
		if (OutPortUnknown)
		{
			ntStatus =
				port->QueryInterface
				(
					IID_IUnknown,
					(PVOID *)OutPortUnknown
				);
		}

		if (OutPortInterface)
		{
			ntStatus =
				port->QueryInterface
				(
					PortInterfaceId,
					(PVOID *)OutPortInterface
				);
		}

		if (OutMiniportUnknown)
		{
			ntStatus =
				miniport->QueryInterface
				(
					IID_IUnknown,
					(PVOID *)OutMiniportUnknown
				);
		}

	}

	if (port)
	{
		port->Release();
	}

	if (miniport)
	{
		miniport->Release();
	}

	return ntStatus;
} // InstallSubDevice

NTSTATUS __stdcall SubdeviceHelper::ConnectTopologies
(
	_In_ PUNKNOWN                   UnknownTopology,
	_In_ PUNKNOWN                   UnknownWave,
	_In_ PHYSICALCONNECTIONTABLE*   PhysicalConnections,
	_In_ ULONG                      PhysicalConnectionCount
)
/*++

Routine Description:

  Connects the bridge pins between the wave and mixer topologies.

Arguments:

Return Value:

  NTSTATUS

--*/
{
	PAGED_CODE();
	DPF_ENTER(("[CAdapterCommon::ConnectTopologies]"));

	ASSERT(m_Adapter->GetDeviceObject() != NULL);

	NTSTATUS        ntStatus = STATUS_SUCCESS;

	//
	// register wave <=> topology connections
	// This will connect bridge pins of wave and topology
	// miniports.
	//
	for (ULONG i = 0; i < PhysicalConnectionCount && NT_SUCCESS(ntStatus); i++)
	{

		switch (PhysicalConnections[i].eType)
		{
		case PhysicalConnectionOrientation::TopologyToWave:
			ntStatus =
				PcRegisterPhysicalConnection
				(
					m_Adapter->GetDeviceObject(),
					UnknownTopology,
					PhysicalConnections[i].ulTopology,
					UnknownWave,
					PhysicalConnections[i].ulWave
				);
			if (!NT_SUCCESS(ntStatus))
			{
				DPF(D_TERSE, ("ConnectTopologies: PcRegisterPhysicalConnection(render) failed, 0x%x", ntStatus));
			}
			break;
		case PhysicalConnectionOrientation::WaveToTopology:
			ntStatus =
				PcRegisterPhysicalConnection
				(
					m_Adapter->GetDeviceObject(),
					UnknownWave,
					PhysicalConnections[i].ulWave,
					UnknownTopology,
					PhysicalConnections[i].ulTopology
				);
			if (!NT_SUCCESS(ntStatus))
			{
				DPF(D_TERSE, ("ConnectTopologies: PcRegisterPhysicalConnection(capture) failed, 0x%x", ntStatus));
			}
			break;
		}
	}

	//
	// Cleanup in case of error.
	//
	if (!NT_SUCCESS(ntStatus))
	{
		// disconnect all connections on error, ignore error code because not all
		// connections may have been made
		DisconnectTopologies(UnknownTopology, UnknownWave, PhysicalConnections, PhysicalConnectionCount);
	}

	return ntStatus;
}

NTSTATUS __stdcall SubdeviceHelper::DisconnectTopologies
(
	_In_ PUNKNOWN                   UnknownTopology,
	_In_ PUNKNOWN                   UnknownWave,
	_In_ PHYSICALCONNECTIONTABLE*   PhysicalConnections,
	_In_ ULONG                      PhysicalConnectionCount
)
/*++

Routine Description:

  Disconnects the bridge pins between the wave and mixer topologies.

Arguments:

Return Value:

  NTSTATUS

--*/
{
	PAGED_CODE();
	DPF_ENTER(("[CAdapterCommon::DisconnectTopologies]"));

	ASSERT(m_Adapter->GetDeviceObject() != NULL);

	NTSTATUS                        ntStatus = STATUS_SUCCESS;
	NTSTATUS                        ntStatus2 = STATUS_SUCCESS;
	PUNREGISTERPHYSICALCONNECTION   unregisterPhysicalConnection = NULL;

	//
	// Get the IUnregisterPhysicalConnection interface
	//
	ntStatus = UnknownTopology->QueryInterface(
		IID_IUnregisterPhysicalConnection,
		(PVOID *)&unregisterPhysicalConnection);

	if (NT_SUCCESS(ntStatus))
	{
		for (ULONG i = 0; i < PhysicalConnectionCount; i++)
		{
			switch (PhysicalConnections[i].eType)
			{
			case PhysicalConnectionOrientation::TopologyToWave:
				ntStatus =
					unregisterPhysicalConnection->UnregisterPhysicalConnection(
						m_Adapter->GetDeviceObject(),
						UnknownTopology,
						PhysicalConnections[i].ulTopology,
						UnknownWave,
						PhysicalConnections[i].ulWave
					);

				if (!NT_SUCCESS(ntStatus))
				{
					DPF(D_TERSE, ("DisconnectTopologies: UnregisterPhysicalConnection(render) failed, 0x%x", ntStatus));
				}
				break;
			case PhysicalConnectionOrientation::WaveToTopology:
				ntStatus =
					unregisterPhysicalConnection->UnregisterPhysicalConnection(
						m_Adapter->GetDeviceObject(),
						UnknownWave,
						PhysicalConnections[i].ulWave,
						UnknownTopology,
						PhysicalConnections[i].ulTopology
					);
				if (!NT_SUCCESS(ntStatus2))
				{
					DPF(D_TERSE, ("DisconnectTopologies: UnregisterPhysicalConnection(capture) failed, 0x%x", ntStatus2));
				}
				break;
			}

			// cache and return the first error encountered, as it's likely the most relevent
			if (NT_SUCCESS(ntStatus))
			{
				ntStatus = ntStatus2;
			}
		}
	}

	//
	// Release the IUnregisterPhysicalConnection interface.
	//
	SAFE_RELEASE(unregisterPhysicalConnection);

	return ntStatus;
}