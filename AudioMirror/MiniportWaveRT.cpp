#include "MiniportWaveRT.h"

#include "KsAudioProcessingAttribute.h"
#include "KsHelper.h"

#define WAVERT_POOLTAG	'tRaW'

MiniportWaveRT::MiniportWaveRT(
	_In_            PUNKNOWN                                UnknownAdapter,
	_In_            ENDPOINT_MINIPAIR*                      MiniportPair,
	_In_opt_        PVOID                                   DeviceContext
)
	:CUnknown(0),
	m_DeviceType(MiniportPair->DeviceType),
	m_DeviceContext(DeviceContext),
	m_DeviceFormatsAndModes(MiniportPair->PinDeviceFormatsAndModes),
	m_DeviceFormatsAndModesCount(MiniportPair->PinDeviceFormatsAndModesCount),
	m_DeviceMaxChannels(MiniportPair->DeviceMaxChannels),
	m_DeviceFlags(MiniportPair->DeviceFlags),
	m_pMiniportPair(MiniportPair)
{
	PAGED_CODE();
	m_pAdapterCommon = (IAdapterCommon*)UnknownAdapter; // weak ref.
	ExInitializeFastMutex(&m_DeviceFormatsAndModesLock);

	if (MiniportPair->WaveDescriptor)
	{
		m_FilterDesc = *MiniportPair->WaveDescriptor;
		//RtlCopyMemory(&m_FilterDesc, MiniportPair->WaveDescriptor, sizeof(m_FilterDesc));

		//
		// Get the max # of pin instances.
		//
		if (IsRenderDevice())
		{
			m_ulMaxSystemStreams = m_FilterDesc.Pins[(int)WaveRenderPins::SINK_SYSTEM].MaxFilterInstanceCount;
		}
		else
		{
			m_ulMaxSystemStreams = m_FilterDesc.Pins[(int)WaveCapturePins::KSPIN_WAVEIN_HOST].MaxFilterInstanceCount;
		}
	}
}

STDMETHODIMP_(NTSTATUS) MiniportWaveRT::Init
(
	_In_  PUNKNOWN                UnknownAdapter_,
	_In_  PRESOURCELIST           ResourceList_,
	_In_  PPORTWAVERT             Port_
)
/*++

Routine Description:

  The Init function initializes the miniport. Callers of this function
  should run at IRQL PASSIVE_LEVEL

Arguments:

  UnknownAdapter - A pointer to the Iuknown interface of the adapter object.

  ResourceList - Pointer to the resource list to be supplied to the miniport
				 during initialization. The port driver is free to examine the
				 contents of the ResourceList. The port driver will not be
				 modify the ResourceList contents.

  Port - Pointer to the topology port object that is linked with this miniport.

Return Value:

  NT status code.

--*/
{
	UNREFERENCED_PARAMETER(UnknownAdapter_);
	UNREFERENCED_PARAMETER(ResourceList_);
	UNREFERENCED_PARAMETER(Port_);
	PAGED_CODE();

	ASSERT(UnknownAdapter_);
	ASSERT(Port_);

	DPF_ENTER(("[CMiniportWaveRT::Init]"));

	NTSTATUS ntStatus = STATUS_SUCCESS;
	size_t   size;

	//
	// Init class data members
	//
	m_ulSystemAllocated = 0;

	if (m_ulMaxSystemStreams == 0)
	{
		return STATUS_INVALID_DEVICE_STATE;
	}
	// System streams.
	size = sizeof(MiniportWaveRTStream*) * m_ulMaxSystemStreams;
	m_SystemStreams = (MiniportWaveRTStream**)ExAllocatePoolWithTag(NonPagedPoolNx, size, WAVERT_POOLTAG);
	if (m_SystemStreams == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(m_SystemStreams, size);
	return ntStatus;
} // Init

#pragma code_seg("PAGE")
NTSTATUS __stdcall MiniportWaveRT::DataRangeIntersection
(
	_In_        ULONG                       PinId,
	_In_        PKSDATARANGE                ClientDataRange,
	_In_        PKSDATARANGE                MyDataRange,
	_In_        ULONG                       OutputBufferLength,
	_Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength)
	PVOID                       ResultantFormat,
	_Out_       PULONG                      ResultantFormatLength
)
/*++

Routine Description:

  The DataRangeIntersection function determines the highest quality
  intersection of two data ranges.

  This sample just validates the # of channels and lets the class handler
  do the rest.

Arguments:

  PinId -           Pin for which data intersection is being determined.

  ClientDataRange - Pointer to KSDATARANGE structure which contains the data
					range submitted by client in the data range intersection
					property request.

  MyDataRange -         Pin's data range to be compared with client's data
						range. In this case we actually ignore our own data
						range, because we know that we only support one range.

  OutputBufferLength -  Size of the buffer pointed to by the resultant format
						parameter.

  ResultantFormat -     Pointer to value where the resultant format should be
						returned.

  ResultantFormatLength -   Actual length of the resultant format placed in
							ResultantFormat. This should be less than or equal
							to OutputBufferLength.

  Return Value:

	NT status code.

  Remarks:

	This sample driver's custom data intersection handler handles all the
	audio endpoints defined in this driver. Some endpoints support mono formats
	while others do not. The handler is written such that it requires an exact
	match in MaximumChannels. This simplifies the handler but requires the pin
	data ranges to include a separate data range for mono formats if the pin
	supports mono formats.

--*/
{
	ULONG                   requiredSize;

	UNREFERENCED_PARAMETER(PinId);
	UNREFERENCED_PARAMETER(ResultantFormat);

	PAGED_CODE();

	if (!IsEqualGUIDAligned(ClientDataRange->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))
	{
		return STATUS_NOT_IMPLEMENTED;
	}

	requiredSize = sizeof(KSDATAFORMAT_WAVEFORMATEX);

	//
	// Validate return buffer size, if the request is only for the
	// size of the resultant structure, return it now before
	// returning other types of errors.
	//
	if (!OutputBufferLength)
	{
		*ResultantFormatLength = requiredSize;
		return STATUS_BUFFER_OVERFLOW;
	}
	else if (OutputBufferLength < requiredSize)
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

	// Verify channel count is supported. This routine assumes a separate data
	// range for each supported channel count.
	if (((PKSDATARANGE_AUDIO)MyDataRange)->MaximumChannels != ((PKSDATARANGE_AUDIO)ClientDataRange)->MaximumChannels)
	{
		return STATUS_NO_MATCH;
	}

	//
	// Ok, let the class handler do the rest.
	//
	return STATUS_NOT_IMPLEMENTED;
} // DataRangeIntersection

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS __stdcall MiniportWaveRT::GetDescription
(
	_Out_ PPCFILTER_DESCRIPTOR * OutFilterDescriptor
)
/*++

Routine Description:

  The GetDescription function gets a pointer to a filter description.
  It provides a location to deposit a pointer in miniport's description
  structure.

Arguments:

  OutFilterDescriptor - Pointer to the filter description.

Return Value:

  NT status code.

--*/
{
	PAGED_CODE();

	ASSERT(OutFilterDescriptor);
	*OutFilterDescriptor = &m_FilterDesc;

	return STATUS_SUCCESS;
} // GetDescription

#pragma code_seg("PAGE")
NTSTATUS __stdcall MiniportWaveRT::GetDeviceDescription(_Out_ PDEVICE_DESCRIPTION DmaDeviceDescription)
{
	PAGED_CODE();

	ASSERT(DmaDeviceDescription);

	DPF_ENTER(("[CMiniportWaveRT::GetDeviceDescription]"));

	RtlZeroMemory(DmaDeviceDescription, sizeof(DEVICE_DESCRIPTION));

	//
	// Init device description. This sample is using the same info for all m_DeviceType(s).
	// 

	DmaDeviceDescription->Master = TRUE;
	DmaDeviceDescription->ScatterGather = TRUE;
	DmaDeviceDescription->Dma32BitAddresses = TRUE;
	DmaDeviceDescription->InterfaceType = PCIBus;
	DmaDeviceDescription->MaximumLength = 0xFFFFFFFF;

	return STATUS_SUCCESS;
}

MiniportWaveRTStream * MiniportWaveRT::GetStream()
{
	return m_SystemStream;
}

BOOL MiniportWaveRT::IsRenderDevice()
{
	return m_DeviceType == DeviceType::RenderDevice;
}

ULONG MiniportWaveRT::GetSystemPinId()
{
	PAGED_CODE();
	ASSERT(IsRenderDevice());
	return (int)WaveRenderPins::SINK_SYSTEM;
}

#pragma code_seg()
IAdapterCommon* MiniportWaveRT::GetAdapter()
{
	return m_pAdapterCommon;
}

STDMETHODIMP_(NTSTATUS) MiniportWaveRT::NewStream
(
	_Out_ PMINIPORTWAVERTSTREAM * OutStream,
	_In_  PPORTWAVERTSTREAM       OuterUnknown,
	_In_  ULONG                   Pin,
	_In_  BOOLEAN                 Capture,
	_In_  PKSDATAFORMAT           DataFormat
)
/*++

Routine Description:

  The NewStream function creates a new instance of a logical stream
  associated with a specified physical channel. Callers of NewStream should
  run at IRQL PASSIVE_LEVEL.

Arguments:

  OutStream -

  OuterUnknown -

  Pin -

  Capture -

  DataFormat -

Return Value:

  NT status code.

--*/
{
	PAGED_CODE();
	ASSERT(OutStream);
	ASSERT(DataFormat);

	DPF_ENTER(("[MiniportWaveRT::NewStream]"));

	NTSTATUS                    ntStatus = STATUS_SUCCESS;
	MiniportWaveRTStream*	    stream = NULL;
	GUID                        signalProcessingMode = AUDIO_SIGNALPROCESSINGMODE_DEFAULT;

	*OutStream = NULL;

	//
   // If the data format attributes were specified, extract them.
   //
	if (DataFormat->Flags & KSDATAFORMAT_ATTRIBUTES)
	{
		// The attributes are aligned (QWORD alignment) after the data format
		PKSMULTIPLE_ITEM attributes = (PKSMULTIPLE_ITEM)(((PBYTE)DataFormat) + ((DataFormat->FormatSize + FILE_QUAD_ALIGNMENT) & ~FILE_QUAD_ALIGNMENT));
		ntStatus = KsHelper::GetAttributesFromAttributeList(attributes, attributes->Size, &signalProcessingMode);
	}

	// Check if we have enough streams.
	//
	if (NT_SUCCESS(ntStatus))
	{
		ntStatus = ValidateStreamCreate(Pin, Capture);
	}

	// Determine if the format is valid.
	//
	if (NT_SUCCESS(ntStatus))
	{
		ntStatus = IsFormatSupported(Pin, Capture, DataFormat);
	}

	// Instantiate a stream. Stream must be in
	// NonPagedPool(Nx) because of file saving.
	//
	if (NT_SUCCESS(ntStatus))
	{
		stream = new(NonPagedPoolNx, WAVERT_POOLTAG) MiniportWaveRTStream(NULL);

		if (stream)
		{
			stream->AddRef();

			ntStatus =
				stream->Init
				(
					this,
					OuterUnknown,
					Pin,
					Capture,
					DataFormat,
					signalProcessingMode
				);
		}
		else
		{
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		}
	}

	if (NT_SUCCESS(ntStatus))
	{
		*OutStream = PMINIPORTWAVERTSTREAM(stream);
		(*OutStream)->AddRef();

		// The stream has references now for the caller.  The caller expects these
		// references to be there.
	}

	// This is our private reference to the stream.  The caller has
	// its own, so we can release in any case.
	//
	if (stream)
	{
		stream->Release();
	}

	return ntStatus;
} // NewStream

#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRT::StreamCreated
(
	_In_ ULONG                  _Pin,
	_In_ MiniportWaveRTStream* _Stream
)
{
	PAGED_CODE();

	MiniportWaveRTStream**	streams = NULL;
	ULONG                   count = 0;

	DPF_ENTER(("[CMiniportWaveRT::StreamCreated]"));

	if (IsSystemCapturePin(_Pin))
	{
		m_ulSystemAllocated++;
		streams = m_SystemStreams;
		count = m_ulMaxSystemStreams;
		if (IsRenderDevice()) { DPF(D_TERSE, ("SPEAKER: Created %u th system capture stream.", m_ulSystemAllocated)); }
		else { DPF(D_TERSE, ("MIC: Created %u th system capture stream.", m_ulSystemAllocated)); }

		if (m_ulSystemAllocated == 1 && m_pPairedMiniport && m_pPairedMiniport->m_ulSystemAllocated == 1)
		{
			_Stream->SetPairedStream(m_pPairedMiniport->m_SystemStreams[0]);
			m_pPairedMiniport->m_SystemStreams[0]->SetPairedStream(_Stream);
		}
	}
	else if (IsSystemRenderPin(_Pin))
	{
		m_ulSystemAllocated++;
		streams = m_SystemStreams;
		count = m_ulMaxSystemStreams;

		if (m_ulSystemAllocated == 1 && m_pPairedMiniport && m_pPairedMiniport->m_ulSystemAllocated == 1)
		{
			_Stream->SetPairedStream(m_pPairedMiniport->m_SystemStreams[0]);
			m_pPairedMiniport->m_SystemStreams[0]->SetPairedStream(_Stream);
		}

		if (IsRenderDevice()) { DPF(D_TERSE, ("SPEAKER: Created %u th system render stream.", m_ulSystemAllocated)); }
		else { DPF(D_TERSE, ("MIC: Created %u th system render stream.", m_ulSystemAllocated)); }
	}
	else 
	{
		if (IsRenderDevice()) { DPF(D_TERSE, ("SPEAKER: Created pin %n stream with type: %n.", _Pin, (int)GetPinTypeForPinNum(_Pin))); }
		else { DPF(D_TERSE, ("MIC: Created pin %n stream with type: %n.", _Pin, (int)GetPinTypeForPinNum(_Pin))); }
	}

	//
	// Cache this stream's ptr.
	//
	if (streams != NULL)
	{
		ULONG i = 0;
		for (; i < count; ++i)
		{
			if (streams[i] == NULL)
			{
				streams[i] = _Stream;
				break;
			}
		}
		ASSERT(i != count);
	}

	return STATUS_SUCCESS;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
MiniportWaveRT::StreamClosed
(
	_In_ ULONG                  _Pin,
	_In_ MiniportWaveRTStream* _Stream
)
{
	PAGED_CODE();

	MiniportWaveRTStream**		streams = NULL;
	ULONG						count = 0;

	DPF_ENTER(("[MiniportWaveRT::StreamClosed]"));

	if (IsSystemCapturePin(_Pin) || IsSystemRenderPin(_Pin))
	{
		m_ulSystemAllocated--;
		streams = m_SystemStreams;
		count = m_ulMaxSystemStreams;

		if (m_pPairedMiniport && m_pPairedMiniport->m_ulSystemAllocated > 1)
		{
			ULONG i = 0;
			for (; i < m_pPairedMiniport->m_ulMaxSystemStreams; ++i)
			{
				if (m_pPairedMiniport->m_SystemStreams[i] != NULL)
				{
					streams[i]->SetPairedStream(NULL);
					break;
				}
			}
		}
	}

	//
	// Cleanup.
	//
	if (streams != NULL)
	{
		ULONG i = 0;
		for (; i < count; ++i)
		{
			if (streams[i] == _Stream)
			{
				streams[i] = NULL;
				break;
			}
		}
		ASSERT(i != count);
	}

	return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRT::Create
(
	_Out_           PUNKNOWN                              * Unknown,
	_In_            REFCLSID,
	_In_opt_        PUNKNOWN                                UnknownOuter,
	_When_((PoolType & NonPagedPoolMustSucceed) != 0,
		__drv_reportError("Must succeed pool allocations are forbidden. "
			"Allocation failures cause a system crash"))
	_In_            POOL_TYPE                               PoolType,
	_In_            PUNKNOWN                                UnknownAdapter,
	_In_opt_        PVOID                                   DeviceContext,
	_In_            PENDPOINT_MINIPAIR                      MiniportPair
)
{
	UNREFERENCED_PARAMETER(UnknownOuter);

	PAGED_CODE();

	ASSERT(Unknown);
	ASSERT(MiniportPair);

	MiniportWaveRT *obj = new (PoolType, WAVERT_POOLTAG) MiniportWaveRT
	(
		UnknownAdapter,
		MiniportPair,
		DeviceContext
	);
	if (NULL == obj)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	obj->AddRef();
	*Unknown = reinterpret_cast<IUnknown*>(obj);

	return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
STDMETHODIMP_(NTSTATUS) MiniportWaveRT::NonDelegatingQueryInterface
(
	_In_ REFIID  Interface,
	_COM_Outptr_ PVOID * Object
)
/*++

Routine Description:

  QueryInterface

Arguments:

  Interface - GUID

  Object - interface pointer to be returned.

Return Value:

  NT status code.

--*/
{
	PAGED_CODE();

	ASSERT(Object);

	if (IsEqualGUIDAligned(Interface, IID_MiniportWaveRT))
	{
		*Object = PVOID((MiniportWaveRT*)(this));
	}
	else if (IsEqualGUIDAligned(Interface, IID_IUnknown))
	{
		*Object = PVOID(PUNKNOWN(PMINIPORTWAVERT(this)));
	}
	else if (IsEqualGUIDAligned(Interface, IID_IMiniport))
	{
		*Object = PVOID(PMINIPORT(this));
	}
	else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRT))
	{
		*Object = PVOID(PMINIPORTWAVERT(this));
	}
	else
	{
		*Object = NULL;
	}

	if (*Object)
	{
		// We reference the interface for the caller.

		PUNKNOWN(*Object)->AddRef();
		return STATUS_SUCCESS;
	}

	return STATUS_INVALID_PARAMETER;
} // NonDelegatingQueryInterface

void MiniportWaveRT::SetPairedMiniport(MiniportWaveRT* miniport)
{
	//clear all current pairs
	if (m_pPairedMiniport) 
	{
		if (m_pPairedMiniport->m_SystemStream) m_pPairedMiniport->m_SystemStream->SetPairedStream(NULL);
		m_pPairedMiniport->m_pPairedMiniport = NULL;
	}
	if (m_SystemStream) m_SystemStream->SetPairedStream(NULL);
	m_pPairedMiniport = NULL;

	//setup new pairs
	m_pPairedMiniport = miniport;
	if (m_pPairedMiniport) m_pPairedMiniport->m_pPairedMiniport = this;
	if (m_pPairedMiniport->m_SystemStream && m_SystemStream)
	{
		m_SystemStream->SetPairedStream(m_pPairedMiniport->m_SystemStream);
		m_pPairedMiniport->m_SystemStream->SetPairedStream(m_SystemStream);
	}
}

/*
  Return mode information for a given pin.

  Return value
      The number of MODE_AND_DEFAULT_FORMAT items or 0 if none.

  Remarks
      Supported formats index array follows same order as filter's pin
      descriptor list.
*/
_Success_(return != 0)
ULONG MiniportWaveRT::GetPinSupportedDeviceModes(_In_ ULONG PinId, _Outptr_opt_result_buffer_(return) _On_failure_(_Deref_post_null_) MODE_AND_DEFAULT_FORMAT **ppModes)
{
	PMODE_AND_DEFAULT_FORMAT modes;
	ULONG numModes;

	PAGED_CODE();

	ExAcquireFastMutex(&m_DeviceFormatsAndModesLock);

	ASSERT(m_DeviceFormatsAndModesCount > PinId);
	ASSERT((m_DeviceFormatsAndModes[PinId].ModeAndDefaultFormatCount == 0) == (m_DeviceFormatsAndModes[PinId].ModeAndDefaultFormat == NULL));

	modes = m_DeviceFormatsAndModes[PinId].ModeAndDefaultFormat;
	numModes = m_DeviceFormatsAndModes[PinId].ModeAndDefaultFormatCount;

	if (ppModes != NULL)
	{
		if (numModes > 0)
		{
			*ppModes = modes;
		}
		else
		{
			// ensure that the returned pointer is NULL
			// in the event of failure (SAL annotation above
			// indicates that it must be NULL, and OACR sees a possibility
			// that it might not be).
			*ppModes = NULL;
		}
	}

	ExReleaseFastMutex(&m_DeviceFormatsAndModesLock);
	return numModes;
}

/*
	Return supported device formats for the audio engine node.
	
	Return value
	    The number of KSDATAFORMAT_WAVEFORMATEXTENSIBLE items.
	
	Remarks
	    Supported formats index array follows same order as filter's pin
	    descriptor list. This routine assumes the engine formats are the
	    last item in the filter's array of PIN_DEVICE_FORMATS_AND_MODES.
*/
_Post_satisfies_(return > 0)
ULONG MiniportWaveRT::GetAudioEngineSupportedDeviceFormats(_Outptr_opt_result_buffer_(return) KSDATAFORMAT_WAVEFORMATEXTENSIBLE **ppFormats)
{
	ULONG i;
	PPIN_DEVICE_FORMATS_AND_MODES pDeviceFormatsAndModes = NULL;

	PAGED_CODE();

	ExAcquireFastMutex(&m_DeviceFormatsAndModesLock);

	pDeviceFormatsAndModes = m_DeviceFormatsAndModes;

	// By convention, the audio engine node's device formats are the last
	// entry in the PIN_DEVICE_FORMATS_AND_MODES list.

	// Since this endpoint apparently supports offload, there must be at least a system,
	// offload, and loopback pin, plus the entry for the device formats.
	ASSERT(m_DeviceFormatsAndModesCount > 3);

	i = m_DeviceFormatsAndModesCount - 1;                       // Index of last list entry

	ASSERT(pDeviceFormatsAndModes[i].PinType == PinType::NoPin);
	ASSERT(pDeviceFormatsAndModes[i].WaveFormats != NULL);
	ASSERT(pDeviceFormatsAndModes[i].WaveFormatsCount > 0);

	if (ppFormats != NULL)
	{
		*ppFormats = pDeviceFormatsAndModes[i].WaveFormats;
	}

	ExReleaseFastMutex(&m_DeviceFormatsAndModesLock);
	return pDeviceFormatsAndModes[i].WaveFormatsCount;
}

MiniportWaveRT::~MiniportWaveRT()
{
	PAGED_CODE();

	DPF_ENTER(("[CMiniportWaveRT::~CMiniportWaveRT]"));

	if (m_pPairedMiniport)
	{
		SetPairedMiniport(NULL);
	}

	if (m_SystemStreams)
	{
		ExFreePoolWithTag(m_SystemStreams, WAVERT_POOLTAG);
		m_SystemStreams = NULL;
	}
}

NTSTATUS MiniportWaveRT::PropertyHandler_WaveFilter(PPCPROPERTY_REQUEST PropertyRequest)
{
	PAGED_CODE();

	NTSTATUS            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
	MiniportWaveRT*    pWaveHelper = reinterpret_cast<MiniportWaveRT*>(PropertyRequest->MajorTarget);

	if (pWaveHelper == NULL)
	{
		return STATUS_INVALID_PARAMETER;
	}

	pWaveHelper->AddRef();

	if (IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_Pin))
	{
		switch (PropertyRequest->PropertyItem->Id)
		{
		case KSPROPERTY_PIN_PROPOSEDATAFORMAT:
			ntStatus = pWaveHelper->PropertyHandlerProposedFormat(PropertyRequest);
			break;

		case KSPROPERTY_PIN_PROPOSEDATAFORMAT2:
			ntStatus = pWaveHelper->PropertyHandlerProposedFormat2(PropertyRequest);
			break;

		default:
			DPF(D_TERSE, ("[PropertyHandler_WaveFilter: Invalid Device Request]"));
		}
	}
	else if (IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_Audio))
	{
		DPF(D_TERSE, ("[PropertyHandler_WaveFilter: Invalid Device Request]"));
	}

	pWaveHelper->Release();

	return ntStatus;
}

NTSTATUS MiniportWaveRT::PropertyHandlerProposedFormat
(
	_In_ PPCPROPERTY_REQUEST      PropertyRequest
)
{
	PKSP_PIN                kspPin = NULL;
	PKSDATAFORMAT           pKsFormat = NULL;
	ULONG                   cbMinSize = 0;
	NTSTATUS                ntStatus = STATUS_INVALID_PARAMETER;

	PAGED_CODE();

	//DPF_ENTER(("[CMiniportWaveRT::PropertyHandlerProposedFormat]"));

	// All properties handled by this handler require at least a KSP_PIN descriptor.

	// Verify instance data stores at least KSP_PIN fields beyond KSPPROPERTY.
	if (PropertyRequest->InstanceSize < (sizeof(KSP_PIN) - RTL_SIZEOF_THROUGH_FIELD(KSP_PIN, Property)))
	{
		return STATUS_INVALID_PARAMETER;
	}

	// Extract property descriptor from property request instance data
	kspPin = CONTAINING_RECORD(PropertyRequest->Instance, KSP_PIN, PinId);

	//
	// This method is valid only on streaming pins.
	//
	if (IsSystemRenderPin(kspPin->PinId) ||
		IsSystemCapturePin(kspPin->PinId))
	{
		ntStatus = STATUS_SUCCESS;
	}
	else if (IsBridgePin(kspPin->PinId))
	{
		ntStatus = STATUS_NOT_SUPPORTED;
	}
	else
	{
		ntStatus = STATUS_INVALID_PARAMETER;
	}

	if (!NT_SUCCESS(ntStatus))
	{
		return ntStatus;
	}

	cbMinSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);

	// Handle KSPROPERTY_TYPE_BASICSUPPORT query
	if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
	{
		ULONG flags = PropertyRequest->PropertyItem->Flags;

		return KsHelper::PropertyHandler_BasicSupport(PropertyRequest, flags, VT_ILLEGAL);
	}

	// Verify value size
	if (PropertyRequest->ValueSize == 0)
	{
		PropertyRequest->ValueSize = cbMinSize;
		return STATUS_BUFFER_OVERFLOW;
	}
	if (PropertyRequest->ValueSize < cbMinSize)
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

#if 0
	// Only SET is supported for this property
	if ((PropertyRequest->Verb & KSPROPERTY_TYPE_SET) == 0)
	{
		return STATUS_INVALID_DEVICE_REQUEST;
	}
#endif

	if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
	{
		ntStatus = STATUS_INVALID_DEVICE_REQUEST;
	}
	else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
	{
		pKsFormat = (PKSDATAFORMAT)PropertyRequest->Value;
		ntStatus = IsFormatSupported(kspPin->PinId, BOOLEAN(IsSystemCapturePin(kspPin->PinId)),
			pKsFormat);
		if (!NT_SUCCESS(ntStatus))
		{
			return ntStatus;
		}
	}

	return ntStatus;
} // PropertyHandlerProposedFormat

#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRT::PropertyHandlerProposedFormat2
(
	_In_ PPCPROPERTY_REQUEST      PropertyRequest
)
{
	PKSP_PIN                kspPin = NULL;
	ULONG                   cbMinSize = 0;
	NTSTATUS                ntStatus = STATUS_INVALID_PARAMETER;
	ULONG                   numModes = 0;
	MODE_AND_DEFAULT_FORMAT *modeInfo = NULL;
	MODE_AND_DEFAULT_FORMAT *modeTemp = NULL;
	PKSMULTIPLE_ITEM        pKsItemsHeader = NULL;
	PKSMULTIPLE_ITEM        pKsItemsHeaderOut = NULL;
	size_t                  cbItemsList = 0;
	GUID                    signalProcessingMode = { 0 };
	BOOLEAN                 bFound = FALSE;
	ULONG                   i;

	PAGED_CODE();

	//DPF_ENTER(("[CMiniportWaveRT::PropertyHandlerProposedFormat2]"));

	// All properties handled by this handler require at least a KSP_PIN descriptor.

	// Verify instance data stores at least KSP_PIN fields beyond KSPPROPERTY.
	if (PropertyRequest->InstanceSize < (sizeof(KSP_PIN) - RTL_SIZEOF_THROUGH_FIELD(KSP_PIN, Property)))
	{
		return STATUS_INVALID_PARAMETER;
	}

	// Extract property descriptor from property request instance data
	kspPin = CONTAINING_RECORD(PropertyRequest->Instance, KSP_PIN, PinId);

	if (kspPin->PinId >= m_pMiniportPair->WaveDescriptor->PinCount)
	{
		return STATUS_INVALID_PARAMETER;
	}

	//
	// This property is supported only on some streaming pins.
	//
	numModes = GetPinSupportedDeviceModes(kspPin->PinId, &modeInfo);

	ASSERT((modeInfo != NULL && numModes > 0) || (modeInfo == NULL && numModes == 0));

	if (modeInfo == NULL)
	{
		return STATUS_NOT_SUPPORTED;
	}

	//
	// Even for pins that support modes, the pin might not support proposed formats
	//
	bFound = FALSE;
	for (i = 0, modeTemp = modeInfo; i < numModes; ++i, ++modeTemp)
	{
		if (modeTemp->DefaultFormat != NULL)
		{
			bFound = TRUE;
			break;
		}
	}

	if (!bFound)
	{
		return STATUS_NOT_SUPPORTED;
	}

	//
	// The property is generally supported on this pin. Handle basic support request.
	//
	if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
	{
		return KsHelper::PropertyHandler_BasicSupport(PropertyRequest, PropertyRequest->PropertyItem->Flags, VT_ILLEGAL);
	}

	//
	// Get the mode if specified.
	//
	pKsItemsHeader = (PKSMULTIPLE_ITEM)(kspPin + 1);
	cbItemsList = (((PBYTE)PropertyRequest->Instance) + PropertyRequest->InstanceSize) - (PBYTE)pKsItemsHeader;

	ntStatus = KsHelper::GetAttributesFromAttributeList(pKsItemsHeader, cbItemsList, &signalProcessingMode);
	if (!NT_SUCCESS(ntStatus))
	{
		return ntStatus;
	}

	//
	// Get the info associated with this mode.
	//
	bFound = FALSE;
	for (i = 0; i < numModes; ++i, ++modeInfo)
	{
		if (modeInfo->Mode == signalProcessingMode)
		{
			bFound = TRUE;
			break;
		}
	}

	// Either the mode isn't supported, or the driver doesn't support a
	// proprosed format for this specific mode.
	if (!bFound || modeInfo->DefaultFormat == NULL)
	{
		return STATUS_NOT_SUPPORTED;
	}

	//
	// Compute output data buffer.
	//
	cbMinSize = modeInfo->DefaultFormat->FormatSize;
	cbMinSize = (cbMinSize + 7) & ~7;

	pKsItemsHeaderOut = (PKSMULTIPLE_ITEM)((PBYTE)PropertyRequest->Value + cbMinSize);

	if (cbItemsList > MAXULONG)
	{
		return STATUS_INVALID_PARAMETER;
	}

	// Total # of bytes.
	ntStatus = RtlULongAdd(cbMinSize, (ULONG)cbItemsList, &cbMinSize);
	if (!NT_SUCCESS(ntStatus))
	{
		return STATUS_INVALID_PARAMETER;
	}

	// Property not supported.
	if (cbMinSize == 0)
	{
		return STATUS_NOT_SUPPORTED;
	}

	// Verify value size
	if (PropertyRequest->ValueSize == 0)
	{
		PropertyRequest->ValueSize = cbMinSize;
		return STATUS_BUFFER_OVERFLOW;
	}
	if (PropertyRequest->ValueSize < cbMinSize)
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

	// Only GET is supported for this property
	if ((PropertyRequest->Verb & KSPROPERTY_TYPE_GET) == 0)
	{
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	// Copy the proposed default format.
	RtlCopyMemory(PropertyRequest->Value, modeInfo->DefaultFormat, modeInfo->DefaultFormat->FormatSize);

	// Copy back the attribute list.
	ASSERT(cbItemsList > 0);
	((KSDATAFORMAT*)PropertyRequest->Value)->Flags = KSDATAFORMAT_ATTRIBUTES;
	RtlCopyMemory(pKsItemsHeaderOut, pKsItemsHeader, cbItemsList);

	PropertyRequest->ValueSize = cbMinSize;

	return STATUS_SUCCESS;
} // PropertyHandlerProposedFormat

NTSTATUS MiniportWaveRT::IsFormatSupported
(
	_In_ ULONG          _ulPin,
	_In_ BOOLEAN        _bCapture,
	_In_ PKSDATAFORMAT  _pDataFormat
)
{
	PAGED_CODE();

	//DPF_ENTER(("[CMiniportWaveRT::IsFormatSupported]"));

	NTSTATUS                            ntStatus = STATUS_NO_MATCH;
	PKSDATAFORMAT_WAVEFORMATEXTENSIBLE  pPinFormats = NULL;
	ULONG                               cPinFormats = 0;

	UNREFERENCED_PARAMETER(_bCapture);

	if (_ulPin >= m_pMiniportPair->WaveDescriptor->PinCount)
	{
		return STATUS_INVALID_PARAMETER;
	}

	cPinFormats = GetPinSupportedDeviceFormats(_ulPin, &pPinFormats);

	for (UINT iFormat = 0; iFormat < cPinFormats; iFormat++)
	{
		PKSDATAFORMAT_WAVEFORMATEXTENSIBLE pFormat = &pPinFormats[iFormat];
		// KSDATAFORMAT VALIDATION
		if (!IsEqualGUIDAligned(pFormat->DataFormat.MajorFormat, _pDataFormat->MajorFormat)) { continue; }
		if (!IsEqualGUIDAligned(pFormat->DataFormat.SubFormat, _pDataFormat->SubFormat)) { continue; }
		if (!IsEqualGUIDAligned(pFormat->DataFormat.Specifier, _pDataFormat->Specifier)) { continue; }
		if (pFormat->DataFormat.FormatSize < sizeof(KSDATAFORMAT_WAVEFORMATEX)) { continue; }

		// WAVEFORMATEX VALIDATION
		PWAVEFORMATEX pWaveFormat = reinterpret_cast<PWAVEFORMATEX>(_pDataFormat + 1);

		if (pWaveFormat->wFormatTag != WAVE_FORMAT_EXTENSIBLE)
		{
			if (pWaveFormat->wFormatTag != EXTRACT_WAVEFORMATEX_ID(&(pFormat->WaveFormatExt.SubFormat))) { continue; }
		}
		if (pWaveFormat->nChannels != pFormat->WaveFormatExt.Format.nChannels) { continue; }
		if (pWaveFormat->nSamplesPerSec != pFormat->WaveFormatExt.Format.nSamplesPerSec) { continue; }
		if (pWaveFormat->nBlockAlign != pFormat->WaveFormatExt.Format.nBlockAlign) { continue; }
		if (pWaveFormat->wBitsPerSample != pFormat->WaveFormatExt.Format.wBitsPerSample) { continue; }

		if (pWaveFormat->wFormatTag != WAVE_FORMAT_EXTENSIBLE)
		{
			ntStatus = STATUS_SUCCESS;
			break;
		}

		// WAVEFORMATEXTENSIBLE VALIDATION
		if (pWaveFormat->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) { continue; }

		PWAVEFORMATEXTENSIBLE pWaveFormatExt = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pWaveFormat);
		if (pWaveFormatExt->Samples.wValidBitsPerSample != pFormat->WaveFormatExt.Samples.wValidBitsPerSample) { continue; }
		if (pWaveFormatExt->dwChannelMask != pFormat->WaveFormatExt.dwChannelMask) { continue; }
		if (!IsEqualGUIDAligned(pWaveFormatExt->SubFormat, pFormat->WaveFormatExt.SubFormat)) { continue; }

		ntStatus = STATUS_SUCCESS;
		break;
	}

	return ntStatus;
}

_Post_satisfies_(return > 0)
ULONG MiniportWaveRT::GetPinSupportedDeviceFormats(_In_ ULONG PinId, _Outptr_opt_result_buffer_(return) KSDATAFORMAT_WAVEFORMATEXTENSIBLE** ppFormats)
{
	PAGED_CODE();

	PPIN_DEVICE_FORMATS_AND_MODES pDeviceFormatsAndModes = NULL;

	ExAcquireFastMutex(&m_DeviceFormatsAndModesLock);

	pDeviceFormatsAndModes = m_DeviceFormatsAndModes;
	ASSERT(m_DeviceFormatsAndModesCount > PinId);
	ASSERT(pDeviceFormatsAndModes[PinId].WaveFormats != NULL);
	ASSERT(pDeviceFormatsAndModes[PinId].WaveFormatsCount > 0);

	if (ppFormats != NULL)
	{
		*ppFormats = pDeviceFormatsAndModes[PinId].WaveFormats;
	}

	ExReleaseFastMutex(&m_DeviceFormatsAndModesLock);

	return pDeviceFormatsAndModes[PinId].WaveFormatsCount;
}

NTSTATUS MiniportWaveRT::ValidateStreamCreate
(
	_In_    ULONG   _Pin,
	_In_    BOOLEAN _Capture
)
{
	PAGED_CODE();

	//DPF_ENTER(("[MiniportWaveRT::ValidateStreamCreate]"));

	NTSTATUS ntStatus = STATUS_NOT_SUPPORTED;

	if (_Capture)
	{
		if (IsSystemCapturePin(_Pin))
		{
			ntStatus = VerifyPinInstanceResourcesAvailable(m_ulSystemAllocated , m_ulMaxSystemStreams);
		}
	}
	else
	{
		if (IsSystemRenderPin(_Pin))
		{
			ntStatus = VerifyPinInstanceResourcesAvailable(m_ulSystemAllocated, m_ulMaxSystemStreams);
		}
	}

	return ntStatus;
}

const GUID * MiniportWaveRT::GetAudioModuleNotificationDeviceId()
{
	return m_pMiniportPair->ModuleNotificationDeviceId;
}

ULONG MiniportWaveRT::GetAudioModuleDescriptorListCount()
{
	return m_pMiniportPair->ModuleListCount;
}

ULONG MiniportWaveRT::GetAudioModuleListCount()
{
	return GetAudioModuleDescriptorListCount();
}

NTSTATUS MiniportWaveRT::VerifyPinInstanceResourcesAvailable(ULONG allocated, ULONG max)
{
	return (allocated < max) ? STATUS_SUCCESS : STATUS_INSUFFICIENT_RESOURCES;
}

PinType MiniportWaveRT::GetPinTypeForPinNum(ULONG nPin)
{
	ExAcquireFastMutex(&m_DeviceFormatsAndModesLock);
	PinType pinType = m_DeviceFormatsAndModes[nPin].PinType;
	ExReleaseFastMutex(&m_DeviceFormatsAndModesLock);
	return pinType;
}

BOOL MiniportWaveRT::IsSystemCapturePin(ULONG nPinId)
{
	PAGED_CODE();
	return (GetPinTypeForPinNum(nPinId) == PinType::SystemCapturePin);
}

BOOL MiniportWaveRT::IsSystemRenderPin(ULONG nPinId)
{
	PAGED_CODE();
	return (GetPinTypeForPinNum(nPinId) == PinType::SystemRenderPin);
}

BOOL MiniportWaveRT::IsBridgePin(ULONG nPinId)
{
	PAGED_CODE();
	return (GetPinTypeForPinNum(nPinId) == PinType::BridgePin);
}