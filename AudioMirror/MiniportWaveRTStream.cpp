#include "MiniportWaveRTStream.h"
#include "MiniportWaveRT.h"
#include "KsHelper.h"
#define MINWAVERTSTREAM_POOLTAG 'SRWM'
#define HNSTIME_PER_MILLISECOND 10000
#pragma warning (disable : 4127)

//=============================================================================
// MiniportWaveRTStream
//=============================================================================

//=============================================================================
#pragma code_seg("PAGE")
MiniportWaveRTStream::~MiniportWaveRTStream
(
	void
)
/*++

Routine Description:

  Destructor for wavertstream

Arguments:

Return Value:

  NT status code.

--*/
{
	PAGED_CODE();
	if (NULL != m_pMiniport)
	{

		if (m_bUnregisterStream)
		{
			m_pMiniport->StreamClosed(m_ulPin, this);
			m_bUnregisterStream = FALSE;
		}

		m_pMiniport->Release();
		m_pMiniport = NULL;
	}

	if (m_pDpc)
	{
		ExFreePoolWithTag(m_pDpc, MINWAVERTSTREAM_POOLTAG);
		m_pDpc = NULL;
	}

	if (m_pTimer)
	{
		ExFreePoolWithTag(m_pTimer, MINWAVERTSTREAM_POOLTAG);
		m_pTimer = NULL;
	}

	if (m_pbMuted)
	{
		ExFreePoolWithTag(m_pbMuted, MINWAVERTSTREAM_POOLTAG);
		m_pbMuted = NULL;
	}

	if (m_plVolumeLevel)
	{
		ExFreePoolWithTag(m_plVolumeLevel, MINWAVERTSTREAM_POOLTAG);
		m_plVolumeLevel = NULL;
	}

	if (m_plPeakMeter)
	{
		ExFreePoolWithTag(m_plPeakMeter, MINWAVERTSTREAM_POOLTAG);
		m_plPeakMeter = NULL;
	}

	if (m_pWfExt)
	{
		ExFreePoolWithTag(m_pWfExt, MINWAVERTSTREAM_POOLTAG);
		m_pWfExt = NULL;
	}
	if (m_RingBuffer)
	{
		ExFreePoolWithTag(m_RingBuffer, MINWAVERTSTREAM_POOLTAG);
		m_RingBuffer = NULL;
	}
	if (m_pNotificationTimer)
	{
		ExDeleteTimer
		(
			m_pNotificationTimer,
			TRUE, // Cancel the timer if it is currently set.
			TRUE, // Wait for the timer to finish expiring and for any callback to a ExTimerCallback routine to finish.
			NULL
		);
		// Since we just cancelled the notification timer, wait for all queued 
		// DPCs to complete before we free the notification DPC.
		//
		KeFlushQueuedDpcs();
	}

	DPF_ENTER(("[MiniportWaveRTStream::~MiniportWaveRTStream]"));
} // ~MiniportWaveRTStream

//=============================================================================

NTSTATUS
MiniportWaveRTStream::Init
(
	_In_ PMiniportWaveRT            Miniport_,
	_In_ PPORTWAVERTSTREAM          PortStream_,
	_In_ ULONG                      Pin_,
	_In_ BOOLEAN                    Capture_,
	_In_ PKSDATAFORMAT              DataFormat_,
	_In_ GUID                       SignalProcessingMode
)
/*++

Routine Description:

  Initializes the stream object.

Arguments:

  Miniport_ -

  Pin_ -

  Capture_ -

  DataFormat -

  SignalProcessingMode - The driver uses the signalProcessingMode to configure
	driver and/or hardware specific signal processing to be applied to this new
	stream.

Return Value:

  NT status code.

--*/
{
	PAGED_CODE();

	PWAVEFORMATEX pWfEx = NULL;
	NTSTATUS ntStatus = STATUS_SUCCESS;

	m_pMiniport = NULL;
	m_ulPin = 0;
	m_bUnregisterStream = FALSE;
	m_bCapture = FALSE;
	m_ulDmaBufferSize = 0;
	m_pDmaBuffer = NULL;
	m_ulNotificationsPerBuffer = 0;
	m_KsState = KSSTATE_STOP;
	m_pTimer = NULL;
	m_pDpc = NULL;
	m_llPacketCounter = 0;
	m_ullPlayPosition = 0;
	m_ullWritePosition = 0;
	m_ullDmaTimeStamp = 0;
	m_hnsElapsedTimeCarryForward = 0;
	m_ullLastDPCTimeStamp = 0;
	m_hnsDPCTimeCarryForward = 0;
	m_ulDmaMovementRate = 0;
	m_byteDisplacementCarryForward = 0;
	m_bLfxEnabled = FALSE;
	m_pbMuted = NULL;
	m_plVolumeLevel = NULL;
	m_plPeakMeter = NULL;
	m_pWfExt = NULL;
	m_ullLinearPosition = 0;
	m_ullPresentationPosition = 0;
	m_ulContentId = 0;
	m_ulCurrentWritePosition = 0;
	m_ulLastOsReadPacket = ULONG_MAX;
	m_ulLastOsWritePacket = ULONG_MAX;
	m_IsCurrentWritePositionUpdated = 0;
	m_SignalProcessingMode = SignalProcessingMode;
	m_bEoSReceived = FALSE;
	m_bLastBufferRendered = FALSE;
	m_AudioModuleCount = 0;

	m_pPortStream = PortStream_;
	InitializeListHead(&m_NotificationList);
	m_ulNotificationIntervalMs = 0;

	// Initialize the spinlock to synchronize position updates
	KeInitializeSpinLock(&m_PositionSpinLock);

	m_pNotificationTimer = ExAllocateTimer(
		TimerNotifyRT,
		this,
		EX_TIMER_HIGH_RESOLUTION
	);
	if (!m_pNotificationTimer)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	pWfEx = KsHelper::GetWaveFormatEx(DataFormat_);
	if (NULL == pWfEx)
	{
		return STATUS_UNSUCCESSFUL;
	}

	m_pMiniport = Miniport_;
	if (m_pMiniport == NULL)
	{
		return STATUS_INVALID_PARAMETER;
	}
	m_pMiniport->AddRef();
	if (!NT_SUCCESS(ntStatus))
	{
		return ntStatus;
	}
	m_ulPin = Pin_;
	m_bCapture = Capture_;
	m_ulDmaMovementRate = pWfEx->nAvgBytesPerSec;

	m_pDpc = (PRKDPC)ExAllocatePoolWithTag(NonPagedPoolNx, sizeof(KDPC), MINWAVERTSTREAM_POOLTAG);
	if (!m_pDpc)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	m_pWfExt = (PWAVEFORMATEXTENSIBLE)ExAllocatePoolWithTag(NonPagedPoolNx, sizeof(WAVEFORMATEX) + pWfEx->cbSize, MINWAVERTSTREAM_POOLTAG);
	if (m_pWfExt == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlCopyMemory(m_pWfExt, pWfEx, sizeof(WAVEFORMATEX) + pWfEx->cbSize);

	m_pbMuted = (PBOOL)ExAllocatePoolWithTag(NonPagedPoolNx, m_pWfExt->Format.nChannels * sizeof(BOOL), MINWAVERTSTREAM_POOLTAG);
	if (m_pbMuted == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(m_pbMuted, m_pWfExt->Format.nChannels * sizeof(BOOL));

	m_plVolumeLevel = (PLONG)ExAllocatePoolWithTag(NonPagedPoolNx, m_pWfExt->Format.nChannels * sizeof(LONG), MINWAVERTSTREAM_POOLTAG);
	if (m_plVolumeLevel == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(m_plVolumeLevel, m_pWfExt->Format.nChannels * sizeof(LONG));

	m_plPeakMeter = (PLONG)ExAllocatePoolWithTag(NonPagedPoolNx, m_pWfExt->Format.nChannels * sizeof(LONG), MINWAVERTSTREAM_POOLTAG);
	if (m_plPeakMeter == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(m_plPeakMeter, m_pWfExt->Format.nChannels * sizeof(LONG));

	//
	// Register this stream.
	//
	ntStatus = m_pMiniport->StreamCreated(m_ulPin, this);
	if (NT_SUCCESS(ntStatus))
	{
		m_bUnregisterStream = TRUE;
	}

	return ntStatus;
} // Init

//=============================================================================
#pragma code_seg("PAGE")
STDMETHODIMP_(NTSTATUS)
MiniportWaveRTStream::NonDelegatingQueryInterface
(
	_In_ REFIID  Interface,
	_COM_Outptr_ PVOID * Object
)
/*++

Routine Description:

  QueryInterface

Arguments:

  Interface - GUID

  Object - interface pointer to be returned

Return Value:

  NT status code.

--*/
{
	PAGED_CODE();

	ASSERT(Object);

	if (IsEqualGUIDAligned(Interface, IID_IUnknown))
	{
		*Object = PVOID(PUNKNOWN(PMINIPORTWAVERTSTREAM(this)));
	}
	else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRTStream))
	{
		*Object = PVOID(PMINIPORTWAVERTSTREAM(this));
	}
	else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRTStreamNotification))
	{
		*Object = PVOID(PMINIPORTWAVERTSTREAMNOTIFICATION(this));
	}
	else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRTInputStream) && (this->m_bCapture))
	{
		// This interface is supported only on capture streams
		*Object = PVOID(PMINIPORTWAVERTINPUTSTREAM(this));
	}
	else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRTOutputStream) && (!this->m_bCapture))
	{
		// This interface is supported only on host render streams
		*Object = PVOID(PMINIPORTWAVERTOUTPUTSTREAM(this));
	}
	else if (IsEqualGUIDAligned(Interface, IID_IMiniportStreamAudioEngineNode))
	{
		*Object = (PVOID)(IMiniportStreamAudioEngineNode*)this;
	}
	else if (IsEqualGUIDAligned(Interface, IID_IMiniportStreamAudioEngineNode2))
	{
		*Object = (PVOID)(IMiniportStreamAudioEngineNode2*)this;
	}
	else if (IsEqualGUIDAligned(Interface, IID_IDrmAudioStream))
	{
		*Object = (PVOID)(IDrmAudioStream*)this;
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

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::AllocateBufferWithNotification
(
	_In_    ULONG               NotificationCount_,
	_In_    ULONG               RequestedSize_,
	_Out_   PMDL                *AudioBufferMdl_,
	_Out_   ULONG               *ActualSize_,
	_Out_   ULONG               *OffsetFromFirstPage_,
	_Out_   MEMORY_CACHING_TYPE *CacheType_
)
{
	PAGED_CODE();

	ULONG ulBufferDurationMs = 0;

	if ((0 == RequestedSize_) || (RequestedSize_ < m_pWfExt->Format.nBlockAlign))
	{
		return STATUS_UNSUCCESSFUL;
	}

	if ((NotificationCount_ == 0) || (RequestedSize_ % NotificationCount_ != 0))
	{
		return STATUS_INVALID_PARAMETER;
	}

	RequestedSize_ -= RequestedSize_ % (m_pWfExt->Format.nBlockAlign);

	PHYSICAL_ADDRESS highAddress;
	highAddress.HighPart = 0;
	highAddress.LowPart = MAXULONG;

	PMDL pBufferMdl = m_pPortStream->AllocatePagesForMdl(highAddress, RequestedSize_);

	if (NULL == pBufferMdl)
	{
		return STATUS_UNSUCCESSFUL;
	}

	// From MSDN: 
	// "Since the Windows audio stack does not support a mechanism to express memory access 
	//  alignment requirements for buffers, audio drivers must select a caching type for mapped
	//  memory buffers that does not impose platform-specific alignment requirements. In other 
	//  words, the caching type used by the audio driver for mapped memory buffers, must not make 
	//  assumptions about the memory alignment requirements for any specific platform.
	//
	//  This method maps the physical memory pages in the MDL into kernel-mode virtual memory. 
	//  Typically, the miniport driver calls this method if it requires software access to the 
	//  scatter-gather list for an audio buffer. In this case, the storage for the scatter-gather 
	//  list must have been allocated by the IPortWaveRTStream::AllocatePagesForMdl or 
	//  IPortWaveRTStream::AllocateContiguousPagesForMdl method. 
	//
	//  A WaveRT miniport driver should not require software access to the audio buffer itself."
	//   
	m_pDmaBuffer = (BYTE*)m_pPortStream->MapAllocatedPages(pBufferMdl, MmCached);
	m_ulNotificationsPerBuffer = NotificationCount_;
	m_ulDmaBufferSize = RequestedSize_;
	ulBufferDurationMs = (RequestedSize_ * 1000) / m_ulDmaMovementRate;
	m_ulNotificationIntervalMs = ulBufferDurationMs / NotificationCount_;

	m_RingBuffer = new(NonPagedPoolNx, MINWAVERTSTREAM_POOLTAG)RingBuffer;
	m_RingBuffer->Init(m_ulDmaBufferSize * 4, m_pWfExt->Format.nBlockAlign);

	*AudioBufferMdl_ = pBufferMdl;
	*ActualSize_ = RequestedSize_;
	*OffsetFromFirstPage_ = 0;
	*CacheType_ = MmCached;

	return STATUS_SUCCESS;
}

//=============================================================================
#pragma code_seg("PAGE")
VOID MiniportWaveRTStream::FreeBufferWithNotification
(
	_In_        PMDL    Mdl_,
	_In_        ULONG   Size_
)
{
	UNREFERENCED_PARAMETER(Size_);

	PAGED_CODE();

	if (Mdl_ != NULL)
	{
		if (m_pDmaBuffer != NULL)
		{
			m_pPortStream->UnmapAllocatedPages(m_pDmaBuffer, Mdl_);
			m_pDmaBuffer = NULL;
		}

		m_pPortStream->FreePagesFromMdl(Mdl_);
	}

	m_ulDmaBufferSize = 0;
	m_ulNotificationsPerBuffer = 0;

	return;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::RegisterNotificationEvent
(
	_In_ PKEVENT NotificationEvent_
)
{
	UNREFERENCED_PARAMETER(NotificationEvent_);

	PAGED_CODE();

	NotificationListEntry *nleNew = (NotificationListEntry*)ExAllocatePoolWithTag(
		NonPagedPoolNx,
		sizeof(NotificationListEntry),
		MINWAVERTSTREAM_POOLTAG);
	if (NULL == nleNew)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	nleNew->NotificationEvent = NotificationEvent_;

	// Fail if the notification event already exists in our list.
	if (!IsListEmpty(&m_NotificationList))
	{
		PLIST_ENTRY leCurrent = m_NotificationList.Flink;
		while (leCurrent != &m_NotificationList)
		{
			NotificationListEntry* nleCurrent = CONTAINING_RECORD(leCurrent, NotificationListEntry, ListEntry);
			if (nleCurrent->NotificationEvent == NotificationEvent_)
			{
				ExFreePoolWithTag(nleNew, MINWAVERTSTREAM_POOLTAG);
				return STATUS_UNSUCCESSFUL;
			}

			leCurrent = leCurrent->Flink;
		}
	}

	InsertTailList(&m_NotificationList, &(nleNew->ListEntry));

	return STATUS_SUCCESS;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::UnregisterNotificationEvent
(
	_In_ PKEVENT NotificationEvent_
)
{
	UNREFERENCED_PARAMETER(NotificationEvent_);

	PAGED_CODE();

	if (!IsListEmpty(&m_NotificationList))
	{
		PLIST_ENTRY leCurrent = m_NotificationList.Flink;
		while (leCurrent != &m_NotificationList)
		{
			NotificationListEntry* nleCurrent = CONTAINING_RECORD(leCurrent, NotificationListEntry, ListEntry);
			if (nleCurrent->NotificationEvent == NotificationEvent_)
			{
				RemoveEntryList(leCurrent);
				ExFreePoolWithTag(nleCurrent, MINWAVERTSTREAM_POOLTAG);
				return STATUS_SUCCESS;
			}

			leCurrent = leCurrent->Flink;
		}
	}

	return STATUS_NOT_FOUND;
}


//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::GetClockRegister
(
	_Out_ PKSRTAUDIO_HWREGISTER Register_
)
{
	UNREFERENCED_PARAMETER(Register_);

	PAGED_CODE();

	return STATUS_NOT_IMPLEMENTED;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::GetPositionRegister
(
	_Out_ PKSRTAUDIO_HWREGISTER Register_
)
{
	UNREFERENCED_PARAMETER(Register_);

	PAGED_CODE();

	return STATUS_NOT_IMPLEMENTED;
}

//=============================================================================
#pragma code_seg("PAGE")
VOID MiniportWaveRTStream::GetHWLatency
(
	_Out_ PKSRTAUDIO_HWLATENCY  Latency_
)
{
	PAGED_CODE();

	ASSERT(Latency_);

	Latency_->ChipsetDelay = 0;
	Latency_->CodecDelay = 0;
	Latency_->FifoSize = 0;
}

//=============================================================================
#pragma code_seg("PAGE")
VOID MiniportWaveRTStream::FreeAudioBuffer
(
	_In_opt_    PMDL        Mdl_,
	_In_        ULONG       Size_
)
{
	UNREFERENCED_PARAMETER(Size_);

	PAGED_CODE();

	if (Mdl_ != NULL)
	{
		if (m_pDmaBuffer != NULL)
		{
			m_pPortStream->UnmapAllocatedPages(m_pDmaBuffer, Mdl_);
			m_pDmaBuffer = NULL;
		}

		m_pPortStream->FreePagesFromMdl(Mdl_);
	}

	m_ulDmaBufferSize = 0;
	m_ulNotificationsPerBuffer = 0;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::AllocateAudioBuffer
(
	_In_    ULONG                   RequestedSize_,
	_Out_   PMDL                   *AudioBufferMdl_,
	_Out_   ULONG                  *ActualSize_,
	_Out_   ULONG                  *OffsetFromFirstPage_,
	_Out_   MEMORY_CACHING_TYPE    *CacheType_
)
{
	PAGED_CODE();

	if ((0 == RequestedSize_) || (RequestedSize_ < m_pWfExt->Format.nBlockAlign))
	{
		return STATUS_UNSUCCESSFUL;
	}

	RequestedSize_ -= RequestedSize_ % (m_pWfExt->Format.nBlockAlign);

	PHYSICAL_ADDRESS highAddress;
	highAddress.HighPart = 0;
	highAddress.LowPart = MAXULONG;

	PMDL pBufferMdl = m_pPortStream->AllocatePagesForMdl(highAddress, RequestedSize_);

	if (NULL == pBufferMdl)
	{
		return STATUS_UNSUCCESSFUL;
	}

	// From MSDN: 
	// "Since the Windows audio stack does not support a mechanism to express memory access 
	//  alignment requirements for buffers, audio drivers must select a caching type for mapped
	//  memory buffers that does not impose platform-specific alignment requirements. In other 
	//  words, the caching type used by the audio driver for mapped memory buffers, must not make 
	//  assumptions about the memory alignment requirements for any specific platform.
	//
	//  This method maps the physical memory pages in the MDL into kernel-mode virtual memory. 
	//  Typically, the miniport driver calls this method if it requires software access to the 
	//  scatter-gather list for an audio buffer. In this case, the storage for the scatter-gather 
	//  list must have been allocated by the IPortWaveRTStream::AllocatePagesForMdl or 
	//  IPortWaveRTStream::AllocateContiguousPagesForMdl method. 
	//
	//  A WaveRT miniport driver should not require software access to the audio buffer itself."
	//   
	m_pDmaBuffer = (BYTE*)m_pPortStream->MapAllocatedPages(pBufferMdl, MmCached);

	m_ulDmaBufferSize = RequestedSize_;
	m_ulNotificationsPerBuffer = 0;

	*AudioBufferMdl_ = pBufferMdl;
	*ActualSize_ = RequestedSize_;
	*OffsetFromFirstPage_ = 0;
	*CacheType_ = MmCached;

	return STATUS_SUCCESS;
}

//=============================================================================
#pragma code_seg()
NTSTATUS MiniportWaveRTStream::GetPosition
(
	_Out_   KSAUDIO_POSITION    *Position_
)
{
	NTSTATUS ntStatus;

	KIRQL oldIrql;
	KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);

	if (m_KsState == KSSTATE_RUN)
	{
		//
		// Get the current time and update position.
		//
		LARGE_INTEGER ilQPC = KeQueryPerformanceCounter(NULL);
		UpdatePosition(ilQPC);
	}

	Position_->PlayOffset = m_ullPlayPosition;
	Position_->WriteOffset = m_ullWritePosition;

	KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);

	ntStatus = STATUS_SUCCESS;

#if defined(SYSVAD_BTH_BYPASS)
	Done:
#endif // defined(SYSVAD_BTH_BYPASS)
		return ntStatus;
}

//=============================================================================
// MiniportWaveRTStream::GetReadPacket
//
//  Returns information about the next packet for the OS to read.
//
// Return value
//
//  Returns STATUS_OPERATION_IN_PROGRESS if no new packets are available and
//  the next packet is in progress.
//
// IRQL - PASSIVE_LEVEL
//
// Remarks
//  Although called at passive level, this routine is non-paged code because
//  it is called in the streaming path where page faults should be avoided.
//
// ISSUE-2014/10/4 Will this work correctly across pause/play?
#pragma code_seg()
NTSTATUS MiniportWaveRTStream::GetReadPacket
(
	_Out_ ULONG     *PacketNumber,
	_Out_ DWORD     *Flags,
	_Out_ ULONG64   *PerformanceCounterValue,
	_Out_ BOOL      *MoreData
)
{
	ULONG availablePacketNumber;
	ULONG droppedPackets;

	// The call must be from event driven mode
	if (m_ulNotificationsPerBuffer == 0)
	{
		return STATUS_NOT_SUPPORTED;
	}

	*Flags = 0;

	if (m_KsState < KSSTATE_PAUSE)
	{
		return STATUS_INVALID_DEVICE_STATE;
	}

	KIRQL oldIrql;
	KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);

	LONGLONG packetCounter = m_llPacketCounter;
	ULONGLONG ullLinearPosition = m_ullLinearPosition;
	ULONGLONG hnsElapsedTimeCarryForward = m_hnsElapsedTimeCarryForward;
	ULONGLONG ullDmaTimeStamp = m_ullDmaTimeStamp;

	KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);

	// The 0-based number of the last completed packet
	// FUTURE-2014/10/27 Update to allow different numbers of packets per WaveRT buffer
	availablePacketNumber = LODWORD(packetCounter - 1);  // Note this might be ULONG_MAX if called during the first packet

	// If no new packets are available...
	if (availablePacketNumber == m_ulLastOsReadPacket)
	{
		return STATUS_DEVICE_NOT_READY;
	}

	// If more than one packet has transferred since the last packet read by
	// the OS, then those were dropped. That is, a glitch occurred.
	droppedPackets = availablePacketNumber - m_ulLastOsReadPacket - 1;
	if (droppedPackets > 0)
	{
		// Trace a glitch
	}

	// Return next packet number to be read
	*PacketNumber = availablePacketNumber;

	// Compute and return timestamp corresponding to start of the available packet. In a real hardware
	// driver, the timestamp would be computed in a driver and hardware specific manner. In this sample
	// driver, it is extrapolated from the sample driver's internal simulated position correlation
	// [m_ullLinearPosition @ m_ullDmaTimeStamp] and the sample's internal 64-bit packet counter, subtracting
	// 1 from the packet counter to compute the time at the start of that last completed packet.
	ULONGLONG linearPositionOfAvailablePacket = (packetCounter - 1) * (m_ulDmaBufferSize / m_ulNotificationsPerBuffer);
	// Need to divide by (1000 * 10000 because m_ulDmaMovementRate is average bytes per sec
	ULONGLONG carryForwardBytes = (hnsElapsedTimeCarryForward * m_ulDmaMovementRate) / 10000000;
	ULONGLONG deltaLinearPosition = ullLinearPosition + carryForwardBytes - linearPositionOfAvailablePacket;
	ULONGLONG deltaTimeInHns = deltaLinearPosition * 10000000 / m_ulDmaMovementRate;
	ULONGLONG timeOfAvailablePacketInHns = ullDmaTimeStamp - deltaTimeInHns;
	ULONGLONG timeOfAvailablePacketInQpc = timeOfAvailablePacketInHns * m_ullPerformanceCounterFrequency.QuadPart / 10000000;

	*PerformanceCounterValue = timeOfAvailablePacketInQpc;

	// No flags are defined yet
	*Flags = 0;

	// This sample does not internally buffer data so there is never more data
	// than revealed by the results from this routine.
	*MoreData = FALSE;

	// Update the last packet read by the OS
	m_ulLastOsReadPacket = availablePacketNumber;

#if 0
	// For test, embed packet number and timestamp into first two LONGLONGs of the packet
	LONG packetIndex = availablePacketNumber % m_ulNotificationsPerBuffer;
	SIZE_T packetSize = m_ulDmaBufferSize / m_ulNotificationsPerBuffer;
	BYTE *packetDataAsBytes = m_pDmaBuffer + (packetIndex * packetSize);
	LONGLONG *packetDataAsLonglongs = (LONGLONG*)packetDataAsBytes;
	for (int i = 0; i < packetSize / sizeof(LONGLONG); i++)
	{
		packetDataAsLonglongs[i] = i;
	}
	packetDataAsLonglongs[0] = availablePacketNumber;
	packetDataAsLonglongs[1] = timeOfAvailablePacketInQpc;
#endif

	return STATUS_SUCCESS;
}

#pragma code_seg()
NTSTATUS MiniportWaveRTStream::SetWritePacket
(
	_In_ ULONG      PacketNumber,
	_In_ DWORD      Flags,
	_In_ ULONG      EosPacketLength
)
{
	NTSTATUS ntStatus;

	// The call must be from event driven mode
	if (m_ulNotificationsPerBuffer == 0)
	{
		return STATUS_NOT_SUPPORTED;
	}

	ULONG oldLastOsWritePacket = m_ulLastOsWritePacket;

	// This function should not be called once EoS has been set.
	if (m_bEoSReceived)
	{
		return STATUS_INVALID_DEVICE_STATE;
	}

	KIRQL oldIrql;
	KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);
	// 1-based count of completed packets, 0-based packet number of current packet
	LONGLONG currentPacket = m_llPacketCounter;
	KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);

	// If not running, the current packet hasn't actually started transfering so OS should be writing
	// to the current packet. If running, then the current packing is already transfering to hardware
	// so the OS should write the packet after the current packet.
	ULONG expectedPacket = LODWORD(currentPacket);
	if (m_KsState == KSSTATE_RUN)
	{
		expectedPacket++;
	}

	// Check if OS PacketNumber is behind or too far ahead of current packet
	LONG deltaFromExpectedPacket = PacketNumber - expectedPacket;   // Modulo arithemetic
	if (deltaFromExpectedPacket < 0)
	{
		return STATUS_DATA_LATE_ERROR;
	}
	else if (deltaFromExpectedPacket > 0)
	{
		return STATUS_DATA_OVERRUN;
	}

	ULONG packetSize = (m_ulDmaBufferSize / m_ulNotificationsPerBuffer);
	ULONG packetIndex = PacketNumber % m_ulNotificationsPerBuffer;
	ULONG ulCurrentWritePosition = packetIndex * packetSize;

	// Check if EOS flag was passed
	if (Flags & KSSTREAM_HEADER_OPTIONSF_ENDOFSTREAM)
	{
		if (EosPacketLength > packetSize)
		{
			return STATUS_INVALID_PARAMETER;
		}
		else {
			// EOS position will be after the total completed packets, plus the packet in progress,
			// plus this EOS packet length
			m_ulLastOsWritePacket = PacketNumber;
			ulCurrentWritePosition += EosPacketLength;
			ntStatus = SetStreamCurrentWritePositionForLastBuffer(ulCurrentWritePosition);
		}
	}
	else
	{
		m_ulLastOsWritePacket = PacketNumber;

		// This function sets the current write position to the specified byte in the DMA buffer.
		// Will check if the write position is smaller than the DMA buffer size.
		// Will not return an error when the passed in parameter is 0.
		// Will also check if this function was called with the same write position(in event mode only)
		// Underruning will also be checked via timer mechanism
		KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);
		ntStatus = SetCurrentWritePositionInternal(ulCurrentWritePosition);
		KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);
	}

	if (!NT_SUCCESS(ntStatus))
	{
		m_ulLastOsWritePacket = oldLastOsWritePacket;
	}

	return ntStatus;
}

//=============================================================================
#pragma code_seg()
NTSTATUS MiniportWaveRTStream::GetOutputStreamPresentationPosition
(
	_Out_ KSAUDIO_PRESENTATION_POSITION *pPresentationPosition
)
{
	ASSERT(pPresentationPosition);

	// The call must be from event driven mode
	if (m_ulNotificationsPerBuffer == 0)
	{
		return STATUS_NOT_SUPPORTED;
	}

	return GetPresentationPosition(pPresentationPosition);
}

//=============================================================================
#pragma code_seg()
NTSTATUS MiniportWaveRTStream::GetPacketCount
(
	_Out_ ULONG *pPacketCount
)
{
	ASSERT(pPacketCount);

	// The call must be from event driven mode
	if (m_ulNotificationsPerBuffer == 0)
	{
		return STATUS_NOT_SUPPORTED;
	}

	KIRQL oldIrql;
	KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);

	if (m_KsState == KSSTATE_RUN)
	{
		// Get the current time and update simulated position.
		LARGE_INTEGER ilQPC = KeQueryPerformanceCounter(NULL);
		UpdatePosition(ilQPC);
	}

	*pPacketCount = LODWORD(m_llPacketCounter);
	KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);

	return STATUS_SUCCESS;
}

//=============================================================================
#pragma code_seg()
NTSTATUS MiniportWaveRTStream::SetState
(
	_In_    KSSTATE State_
)
{
	NTSTATUS        ntStatus = STATUS_SUCCESS;
	IAdapterCommon*  pAdapterComm = m_pMiniport->GetAdapter();
	KIRQL oldIrql;

	// Spew an event for a pin state change request from portcls
	//Event type: eMINIPORT_PIN_STATE
	//Parameter 1: Current linear buffer position
	//Parameter 2: Current WaveRtBufferWritePosition
	//Parameter 3: Pin State 0->KS_STOP, 1->KS_ACQUIRE, 2->KS_PAUSE, 3->KS_RUN 
	//Parameter 4: 0
	pAdapterComm->WriteEtwEvent(eMINIPORT_PIN_STATE,
		m_ullLinearPosition, // replace with the correct "Current linear buffer position"
		m_ulCurrentWritePosition, // replace with the previous WaveRtBufferWritePosition that the driver received
		State_, // replace with the correct "Data length completed"
		0); // always zero
	switch (State_)
	{
	case KSSTATE_STOP:
		if (m_KsState == KSSTATE_ACQUIRE)
		{
		}
		KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);
		// Reset DMA
		m_llPacketCounter = 0;
		m_ullPlayPosition = 0;
		m_ullWritePosition = 0;
		m_ullLinearPosition = 0;
		m_ullPresentationPosition = 0;

		// Reset OS read/write positions
		m_ulLastOsReadPacket = ULONG_MAX;
		m_ulCurrentWritePosition = 0;
		m_ulLastOsWritePacket = ULONG_MAX;
		m_bEoSReceived = FALSE;
		m_bLastBufferRendered = FALSE;

		KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);
		break;

	case KSSTATE_ACQUIRE:
		if (m_KsState == KSSTATE_STOP)
		{
		}
		break;

	case KSSTATE_PAUSE:

		if (m_KsState > KSSTATE_PAUSE)
		{
			//
			// Run -> Pause
			//

			// Pause DMA
			if (m_ulNotificationIntervalMs > 0)
			{
				ExCancelTimer(m_pNotificationTimer, NULL);
				KeFlushQueuedDpcs();

				// If pin is transitioning from RUN, save the time since last buffer completion event was sent 
				// so if the pin goes to RUN state again we can send the buffer completion event at correct time.
				if (m_ullLastDPCTimeStamp > 0)
				{
					LARGE_INTEGER qpc;
					LARGE_INTEGER qpcFrequency;
					LONGLONG  hnsCurrentTime;

					qpc = KeQueryPerformanceCounter(&qpcFrequency);

					// Convert ticks to 100ns units.
					hnsCurrentTime = KSCONVERT_PERFORMANCE_TIME(m_ullPerformanceCounterFrequency.QuadPart, qpc);
					m_hnsDPCTimeCarryForward = hnsCurrentTime - m_ullLastDPCTimeStamp + m_hnsDPCTimeCarryForward;
				}
			}
		}
		// This call updates the linear buffer and presentation positions.
		GetPositions(NULL, NULL, NULL);
		break;

	case KSSTATE_RUN:

		// Start DMA
		LARGE_INTEGER ullPerfCounterTemp;
		
		ullPerfCounterTemp = KeQueryPerformanceCounter(&m_ullPerformanceCounterFrequency);
		m_ullLastDPCTimeStamp = m_ullDmaTimeStamp = KSCONVERT_PERFORMANCE_TIME(m_ullPerformanceCounterFrequency.QuadPart, ullPerfCounterTemp);
		m_RingBuffer->Clear();

		if (m_ulNotificationIntervalMs > 0)
		{
			// Set timer for 1 ms. This will cause DPC to run every 1 ms but driver will send out 
			// notification events only after notification interval. This timer is used by Sysvad to 
			// emulate hardware and send out notification event. Real hardware should not use this
			// timer to fire notification event as it will drain power if the timer is running at 1 msec.
			ExSetTimer
			(
				m_pNotificationTimer,
				(-1) * HNSTIME_PER_MILLISECOND,
				HNSTIME_PER_MILLISECOND, // 1 ms 
				NULL
			);

		}

		break;
	}

	m_KsState = State_;

#if defined(SYSVAD_BTH_BYPASS)
	Done:
#endif  // defined(SYSVAD_BTH_BYPASS)
		return ntStatus;
}

#pragma code_seg("PAGE")
void MiniportWaveRTStream::SetPairedStream(MiniportWaveRTStream* stream)
{
	PAGED_CODE();
	if (m_RingBuffer) m_RingBuffer->Clear();
	m_PairedStream = stream;
}

#pragma code_seg()
NTSTATUS MiniportWaveRTStream::WriteAudioPacket(BYTE* buffer, ULONG packetSize, BOOL eos)
{
	UNREFERENCED_PARAMETER(buffer);
	UNREFERENCED_PARAMETER(packetSize);
	UNREFERENCED_PARAMETER(eos);

	//only allowed on capture stream
	if (!m_bCapture) return STATUS_NOT_IMPLEMENTED;
	//if we dont have a paired stream this is not allowed
	if (m_PairedStream == NULL) return STATUS_INVALID_DEVICE_STATE;
	if (m_RingBuffer == NULL) return STATUS_DEVICE_NOT_READY;
	if (packetSize > m_RingBuffer->GetSize()) return STATUS_BUFFER_TOO_SMALL;

	NTSTATUS state = m_RingBuffer->Put(buffer, packetSize);
	switch (state)
	{
	case STATUS_BUFFER_TOO_SMALL:
		return state;
	case STATUS_BUFFER_OVERFLOW:
	default:
		return STATUS_SUCCESS;
		break;
	}
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::SetFormat
(
	_In_    KSDATAFORMAT    *DataFormat_
)
{
	UNREFERENCED_PARAMETER(DataFormat_);

	PAGED_CODE();

	//if (!m_fCapture && !g_DoNotCreateDataFiles)
	//{
	//    ntStatus = m_SaveData.SetDataFormat(Format);
	//}

	return STATUS_NOT_SUPPORTED;
}

//=============================================================================
#pragma code_seg()
VOID MiniportWaveRTStream::UpdatePosition
(
	_In_ LARGE_INTEGER ilQPC
)
{
	// Convert ticks to 100ns units.
	LONGLONG  hnsCurrentTime = KSCONVERT_PERFORMANCE_TIME(m_ullPerformanceCounterFrequency.QuadPart, ilQPC);

	// Calculate the time elapsed since the last call to GetPosition() or since the
	// DMA engine started.  Note that the division by 10000 to convert to milliseconds
	// may cause us to lose some of the time, so we will carry the remainder forward 
	// to the next GetPosition() call.
	//
	ULONG TimeElapsedInMS = (ULONG)(hnsCurrentTime - m_ullDmaTimeStamp + m_hnsElapsedTimeCarryForward) / 10000;

	// Carry forward the remainder of this division so we don't fall behind with our position too much.
	//
	m_hnsElapsedTimeCarryForward = (hnsCurrentTime - m_ullDmaTimeStamp + m_hnsElapsedTimeCarryForward) % 10000;

	// Calculate how many bytes in the DMA buffer would have been processed in the elapsed
	// time.  Note that the division by 1000 to convert to milliseconds may cause us to 
	// lose some bytes, so we will carry the remainder forward to the next GetPosition() call.
	//
	// need to divide by 1000 because m_ulDmaMovementRate is average bytes per sec.

	ULONG ByteDisplacement = ((m_ulDmaMovementRate * TimeElapsedInMS) + m_byteDisplacementCarryForward) / 1000;
	m_byteDisplacementCarryForward = ((m_ulDmaMovementRate * TimeElapsedInMS) + m_byteDisplacementCarryForward) % 1000;

	// Increment presentation position even after last buffer is rendered.
	m_ullPresentationPosition += ByteDisplacement;

	if (m_bCapture)
	{
		//ULONG fillerBytes = m_pWfExt->Format.nBlockAlign - (ByteDisplacement % m_pWfExt->Format.nBlockAlign);
		//ULONG subtractBytes = ByteDisplacement - (ByteDisplacement % m_pWfExt->Format.nBlockAlign);
		//m_byteDisplacementCarryForward -= fillerBytes;
		//m_ullPresentationPosition += fillerBytes;
		//ULONG bytesToWrite = ByteDisplacement + fillerBytes;
		// Write sine wave to buffer.
		WriteBytes(ByteDisplacement);
	}
	else
	{
		if (m_bEoSReceived)
		{
			// since EoS flag is set, we'll need to make sure not to read data beyond EOS position.
			// If driver's current position is less than EoS position, then make sure not to read data beyond EoS.
			if (m_ullWritePosition <= m_ulCurrentWritePosition)
			{
				ByteDisplacement = min(ByteDisplacement, m_ulCurrentWritePosition - (ULONG)m_ullWritePosition);
			}
			// If our current position is ahead of EoS position and we'll wrap around after new position then adjust
			// new position if it crosses EoS.
			else if ((m_ullWritePosition + ByteDisplacement) % m_ulDmaBufferSize < m_ullWritePosition)
			{
				if ((m_ullWritePosition + ByteDisplacement) % m_ulDmaBufferSize > m_ulCurrentWritePosition)
				{
					ByteDisplacement = ByteDisplacement - (((ULONG)m_ullWritePosition + ByteDisplacement) % m_ulDmaBufferSize - m_ulCurrentWritePosition);
				}
			}
		}

		// If the last packet was rendered(read in the sample driver's case), send out an etw event.
		if (m_bEoSReceived && !m_bLastBufferRendered
			&& (m_ullWritePosition + ByteDisplacement) % m_ulDmaBufferSize == m_ulCurrentWritePosition)
		{
			m_bLastBufferRendered = TRUE;
			IAdapterCommon* pAdapterComm = m_pMiniport->GetAdapter();
			//Event type : eMINIPORT_LAST_BUFFER_RENDERED
			//Parameter 1 : Current linear buffer position
			//Parameter 2 : the very last WaveRtBufferWritePosition that the driver received
			//Parameter 3 : 0
			//Parameter 4 : 0
			pAdapterComm->WriteEtwEvent(eMINIPORT_LAST_BUFFER_RENDERED,
				m_ullLinearPosition + ByteDisplacement, // Current linear buffer position  
				m_ulCurrentWritePosition, // The very last WaveRtBufferWritePosition that the driver received
				0,
				0);
		}
		// Read from buffer and write to a file.
		ReadBytes(ByteDisplacement);
	}

	// Increment the DMA position by the number of bytes displaced since the last
	// call to UpdatePosition() and ensure we properly wrap at buffer length.
	//
	m_ullPlayPosition = m_ullWritePosition =
		(m_ullWritePosition + ByteDisplacement) % m_ulDmaBufferSize;

	// m_ullDmaTimeStamp is updated in both GetPostion and GetLinearPosition calls
	// so m_ullLinearPosition needs to be updated accordingly here
	//
	m_ullLinearPosition += ByteDisplacement;

	// Update the DMA time stamp for the next call to GetPosition()
	//
	m_ullDmaTimeStamp = hnsCurrentTime;
}

//=============================================================================
#pragma code_seg()
VOID MiniportWaveRTStream::WriteBytes
(
	_In_ ULONG ByteDisplacement
)
/*++

Routine Description:

This function writes the audio buffer using a sine wave generator
Arguments:

ByteDisplacement - # of bytes to process.

--*/
{
	ULONG bufferOffset = m_ullLinearPosition % m_ulDmaBufferSize;

	// Normally this will loop no more than once for a single wrap, but if
	// many bytes have been displaced then this may loops many times.
	while (ByteDisplacement > 0)
	{
		ULONG runWrite = min(ByteDisplacement, m_ulDmaBufferSize - bufferOffset);
		SIZE_T actuallyWritten;
		
		m_RingBuffer->Take(m_pDmaBuffer + bufferOffset, runWrite, &actuallyWritten);
		if (actuallyWritten < runWrite)
		{
			RtlZeroMemory(m_pDmaBuffer + bufferOffset + actuallyWritten, runWrite - actuallyWritten);
		}
		
		bufferOffset = (bufferOffset + runWrite) % m_ulDmaBufferSize;
		ByteDisplacement -= runWrite;
	}
}

//=============================================================================
#pragma code_seg()
VOID MiniportWaveRTStream::ReadBytes
(
	_In_ ULONG ByteDisplacement
)
/*++

Routine Description:

This function reads the audio buffer and saves the data in a file.

Arguments:

ByteDisplacement - # of bytes to process.

--*/
{
	ULONG bufferOffset = m_ullLinearPosition % m_ulDmaBufferSize;

	// Normally this will loop no more than once for a single wrap, but if
	// many bytes have been displaced then this may loops many times.
	while (ByteDisplacement > 0)
	{
		ULONG runWrite = min(ByteDisplacement, m_ulDmaBufferSize - bufferOffset);
		if (m_PairedStream) m_PairedStream->WriteAudioPacket(m_pDmaBuffer + bufferOffset, runWrite, false);
		bufferOffset = (bufferOffset + runWrite) % m_ulDmaBufferSize;
		ByteDisplacement -= runWrite;
	}
}

//=============================================================================
#pragma code_seg()
void
TimerNotifyRT
(
	_In_      PEX_TIMER    Timer,
	_In_opt_  PVOID        DeferredContext
)
{
	LARGE_INTEGER qpc;
	LARGE_INTEGER qpcFrequency;
	BOOL bufferCompleted = FALSE;

	UNREFERENCED_PARAMETER(Timer);

	_IRQL_limited_to_(DISPATCH_LEVEL);

	MiniportWaveRTStream* _this = (MiniportWaveRTStream*)DeferredContext;

	if (NULL == _this)
	{
		return;
	}

	KIRQL oldIrql;
	KeAcquireSpinLock(&_this->m_PositionSpinLock, &oldIrql);

	qpc = KeQueryPerformanceCounter(&qpcFrequency);

	// Convert ticks to 100ns units.
	LONGLONG  hnsCurrentTime = KSCONVERT_PERFORMANCE_TIME(_this->m_ullPerformanceCounterFrequency.QuadPart, qpc);

	// Calculate the time elapsed since the last we ran DPC that matched Notification interval. Note that the division by 10000 
	// to convert to milliseconds may cause us to lose some of the time, so we will carry the remainder forward.

	ULONG TimeElapsedInMS = (ULONG)(hnsCurrentTime - _this->m_ullLastDPCTimeStamp + _this->m_hnsDPCTimeCarryForward) / 10000;

	if (TimeElapsedInMS >= _this->m_ulNotificationIntervalMs)
	{
		// Carry forward the time greater than notification interval to adjust time to signal next buffer completion event accordingly.
		_this->m_hnsDPCTimeCarryForward = hnsCurrentTime - _this->m_ullLastDPCTimeStamp + _this->m_hnsDPCTimeCarryForward - (_this->m_ulNotificationIntervalMs * 10000);
		// Save the last time DPC ran at notification interval
		_this->m_ullLastDPCTimeStamp = hnsCurrentTime;
		bufferCompleted = TRUE;
	}

	if (!bufferCompleted && !_this->m_bEoSReceived)
	{
		goto End;
	}

	_this->UpdatePosition(qpc);

	if (!_this->m_bEoSReceived)
	{
		_this->m_llPacketCounter++;
	}

	if (_this->m_KsState != KSSTATE_RUN)
	{
		goto End;
	}

	IAdapterCommon*  pAdapterComm = _this->m_pMiniport->GetAdapter();

	// Simple buffer underrun detection.
	if (!_this->IsCurrentWaveRTWritePositionUpdated() && !_this->m_bEoSReceived)
	{
		//Event type: eMINIPORT_GLITCH_REPORT
		//Parameter 1: Current linear buffer position 
		//Parameter 2: Previous WaveRtBufferWritePosition that the driver received 
		//Parameter 3: Major glitch code: 1:WaveRT buffer is underrun
		//Parameter 4: Minor code for the glitch cause
		pAdapterComm->WriteEtwEvent(eMINIPORT_GLITCH_REPORT,
			_this->m_ullLinearPosition,
			_this->GetCurrentWaveRTWritePosition(),
			1,      // WaveRT buffer is underrun
			0);
	}

	// Send buffer completion event if either of the following is true
	// 1. Driver consumed a complete buffer for this stream
	// 2. Driver consumed a partial buffer containing EoS for this stream

	if (!IsListEmpty(&_this->m_NotificationList) &&
		(bufferCompleted || _this->m_bLastBufferRendered))
	{
		PLIST_ENTRY leCurrent = _this->m_NotificationList.Flink;
		while (leCurrent != &_this->m_NotificationList)
		{
			NotificationListEntry* nleCurrent = CONTAINING_RECORD(leCurrent, NotificationListEntry, ListEntry);
			//Event type: eMINIPORT_BUFFER_COMPLETE
			//Parameter 1: Current linear buffer position
			//Parameter 2: Previous WaveRtBufferWritePosition that the driver received
			//Parameter 3: Data length completed
			//Parameter 4: 0
			pAdapterComm->WriteEtwEvent(eMINIPORT_BUFFER_COMPLETE,
				_this->m_ullLinearPosition,
				_this->GetCurrentWaveRTWritePosition(),
				_this->m_ulDmaBufferSize / _this->m_ulNotificationsPerBuffer, // replace with the correct "Data length completed"
				0); // always zero
			KeSetEvent(nleCurrent->NotificationEvent, 0, 0);

			leCurrent = leCurrent->Flink;
		}
	}

	if (_this->m_bLastBufferRendered)
	{
		ExCancelTimer(_this->m_pNotificationTimer, NULL);
	}

End:
	KeReleaseSpinLock(&_this->m_PositionSpinLock, oldIrql);
	return;
}
//=============================================================================


