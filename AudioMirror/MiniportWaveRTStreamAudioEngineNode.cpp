/*++

Module Name:

	MiniportStreamAudioEngineNode.cpp

Abstract:

	Implementation of IMiniportstreamAudioEngineNode interface for wavert steam.

--*/
#include "MiniportWaveRTStream.h"
#include "MiniportWaveRT.h"

#define MINWAVERTSTREAM_POOLTAG 'SRWM'
#define HNSTIME_PER_MILLISECOND 10000

// Default volume settings.
#define VOLUME_STEPPING_DELTA       0x8000
#define VOLUME_SIGNED_MAXIMUM       0x00000000
#define VOLUME_SIGNED_MINIMUM       (-96 * 0x10000)

// Default peak meter settings
#define PEAKMETER_STEPPING_DELTA    0x1000
#define PEAKMETER_SIGNED_MAXIMUM    LONG_MAX
#define PEAKMETER_SIGNED_MINIMUM    LONG_MIN

#define VALUE_NORMALIZE_P(v, step) \
    ((((v) + (step)/2) / (step)) * (step))

#define VALUE_NORMALIZE(v, step) \
    ((v) > 0 ? VALUE_NORMALIZE_P((v), (step)) : -(VALUE_NORMALIZE_P(-(v), (step))))

#define VALUE_NORMALIZE_IN_RANGE_EX(v, min, max, step) \
    ((v) > (max) ? (max) : \
     (v) < (min) ? (min) : \
     VALUE_NORMALIZE((v), (step)))

// to normalize volume values.
#define VOLUME_NORMALIZE_IN_RANGE(v) \
    VALUE_NORMALIZE_IN_RANGE_EX((v), VOLUME_SIGNED_MINIMUM, VOLUME_SIGNED_MAXIMUM, VOLUME_STEPPING_DELTA)

#pragma warning (disable : 4127)

#pragma code_seg("PAGE")
/*-----------------------------------------------------------------------------
IMiniportStreamAudioEngineNode::GetLfxState

Description:

	Portcls calls this method to get a offload stream's Lfx state

Parameters

	_Out_ pbEnable: a pointer to a BOOL value for receieving the returned LFX state

Return Value:

   Appropriate NTSTATUS code

Called at PASSIVE_LEVEL

Remarks
	The Lfx operations (on the offload stream) inside HW Audio Engine (such as src, dsp, and other special effects) are hidden from the software audio stack.
So, the driver should return TRUE if any one of the effects is on and returns FALSE when all the opertations are off.
-------------------------------------------------------------------------------------------------------------------------*/
NTSTATUS MiniportWaveRTStream::GetLfxState(_Out_ BOOL *_pbEnable)
{
	PAGED_CODE();

	*_pbEnable = m_bLfxEnabled;
	return STATUS_SUCCESS;
}
#pragma code_seg("PAGE")
/*-----------------------------------------------------------------------------
IMiniportStreamAudioEngineNode::SetLfxState

Decscription:

	Portcls calls this method to set a offload stream's Lfx state

Parameters:

	_In_ bEnable: a BOOL value. TRU: to enable Lfx, FALSE: to disable Lfx

Return Value:

   Appropriate NTSTATUS code

Called at PASSIVE_LEVEL

Remarks
	The local operations (on the offload stream) inside HW Audio Engine (such as src, dsp, and other special effects) are hidden from the software audio stack.
So, when handling disabling Lfx, the driver should ALL the effects. When enabling Lfx, it's up the driver+HW Audio Engine to decide what opertations to turn on
when they see appropriate.
-------------------------------------------------------------------------------------------------------------------------*/
NTSTATUS MiniportWaveRTStream::SetLfxState(_In_ BOOL _bEnable)
{
	PAGED_CODE();


	UNREFERENCED_PARAMETER(_bEnable);
	m_bLfxEnabled = _bEnable;
	return STATUS_SUCCESS;
}
#pragma code_seg("PAGE")
/*-----------------------------------------------------------------------------
IMiniportStreamAudioEngineNode::GetStreamChannelVolume

Decscription:

	When handling GET volume KS property for the device, Portcls calls
	this method, inside its property handlers, to get the current setting on the specific channel.

Parameters:

		_In_ _uiChannel:  the target channel for this GET volume operation
		_Out_ _pVolume: a pointer to a LONG variable for receiving returned information

Return Value:

   Appropriate NTSTATUS code

Called at PASSIVE_LEVEL

Remarks

-------------------------------------------------------------------------------------------------------------------------*/
NTSTATUS MiniportWaveRTStream::GetStreamChannelVolume(_In_ UINT32 _uiChannel, _Out_ LONG *_pVolume)
{
	PAGED_CODE();


	*_pVolume = m_plVolumeLevel[_uiChannel];

	return STATUS_SUCCESS;
}
#pragma code_seg("PAGE")
/*-----------------------------------------------------------------------------
IMiniportStreamAudioEngineNode::GetStreamChannelMute

Decscription:

	When handling GET mute KS property for the device, Portcls calls
	this method, inside its property handlers, to get the current setting on the specific channel.

Parameters:

		_In_ _uiChannel:  the target channel for this GET volume operation
		_Out_ _pbMute: a pointer to a BOOL variable for receiving returned information

Return Value:

   Appropriate NTSTATUS code

Called at PASSIVE_LEVEL

Remarks

-------------------------------------------------------------------------------------------------------------------------*/
STDMETHODIMP_(NTSTATUS) MiniportWaveRTStream::GetStreamChannelMute(_In_ UINT32 _uiChannel, _Out_  BOOL  *_pbMute)
{
	NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

	PAGED_CODE();
	ASSERT(_pbMute);

	ntStatus = GetChannelMute(_uiChannel, _pbMute);

	return ntStatus;
}
#pragma code_seg("PAGE")
/*-----------------------------------------------------------------------------
IMiniportStreamAudioEngineNode::GetStreamAttributeSteppings

Decscription:

	When handling volume, mute, and meter related KS properties, Portcls calls
	this method, inside its property handlers, to know the property stepping information for the
	corresponding KS property.

Parameters:

		_In_ _targetType:  the query target (volume. mute, or peak meter)
		_Out_ _pKsPropMembHead: a pointer to a PKSPROPERTY_STEPPING_LONG variable for receiving returned channel count information
		_In_ ulBufferSize: a pointer to a ULONG variable that has the size of the buffer pointed by _pKsPropMembHead

Return Value:

   Appropriate NTSTATUS code

Called at PASSIVE_LEVEL

Remarks

-------------------------------------------------------------------------------------------------------------------------*/
NTSTATUS MiniportWaveRTStream::GetStreamAttributeSteppings(_In_  eChannelTargetType _targetType, _Out_ PKSPROPERTY_STEPPING_LONG _pKsPropMembHead, _In_  UINT32 _ui32DataSize)
{
	NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

	PAGED_CODE();


	switch (_targetType)
	{
	case eVolumeAttribute:
		ntStatus = GetVolumeSteppings(_pKsPropMembHead, _ui32DataSize);;
		break;
	case eMuteAttribute:
		ntStatus = GetMuteSteppings(_pKsPropMembHead, _ui32DataSize);;
		break;
	case ePeakMeterAttribute:
		ntStatus = GetPeakMeterSteppings(_pKsPropMembHead, _ui32DataSize);;
		break;
	default:
		ntStatus = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}
	return ntStatus;

}
#pragma code_seg("PAGE")
/*-----------------------------------------------------------------------------
IMiniportStreamAudioEngineNode::SetStreamChannelVolume

Decscription:

	When handling SET volume KS property for the device, Portcls calls
	this method, inside its property handlers, to set the current setting on the specific channel.

Parameters:

		_In_ Channel:  the target channel for this GET volume operation
		_In_ TargetVolume: volume value to set
		_In_ CurveType: type of curve to apply to the ramp
		_In_ CurveDuration: amount of time in hns over which to ramp the volume

Return Value:

   Appropriate NTSTATUS code

Called at PASSIVE_LEVEL

Remarks

-------------------------------------------------------------------------------------------------------------------------*/
STDMETHODIMP_(NTSTATUS) MiniportWaveRTStream::SetStreamChannelVolume
(
	_In_ UINT32             Channel,
	_In_ LONG                TargetVolume,
	_In_ AUDIO_CURVE_TYPE   CurveType,
	_In_ ULONGLONG          CurveDuration
)
{
	UNREFERENCED_PARAMETER(CurveType);
	UNREFERENCED_PARAMETER(CurveDuration);
	NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

	PAGED_CODE();


	// Snap the volume level to our range of steppings.
	LONG lVolume = VOLUME_NORMALIZE_IN_RANGE(TargetVolume);

	// If Channel is ALL_CHANNELS_ID, then set the level on all channels
	if (ALL_CHANNELS_ID == Channel)
	{
		for (UINT32 i = 0; i < m_pWfExt->Format.nChannels; i++)
		{
			ntStatus = SetChannelVolume(i, lVolume);
		}
	}
	else
	{
		ntStatus = SetChannelVolume(Channel, lVolume);
	}

	return ntStatus;
}

#pragma code_seg("PAGE")
/*-----------------------------------------------------------------------------
IMiniportStreamAudioEngineNode::GetStreamChannelPeakMeter

Decscription:

	When handling GET peak meter KS property for the device, Portcls calls
	this method, inside its property handlers, to get the current setting on the specific channel.

Parameters:

		_In_ _uiChannel:  the target channel for this GET peak meter operation
	_Out_ _pPeakMeterValue: a pointer to a LONG variable for receiving returned information

Return Value:

   Appropriate NTSTATUS code

Called at PASSIVE_LEVEL

Remarks

-------------------------------------------------------------------------------------------------------------------------*/
STDMETHODIMP_(NTSTATUS) MiniportWaveRTStream::GetStreamChannelPeakMeter(_In_ UINT32 _uiChannel, _Out_  LONG  *_pPeakMeterValue)
{
	NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

	PAGED_CODE();


	ntStatus = GetChannelPeakMeter(_uiChannel, _pPeakMeterValue);

	return ntStatus;
}

#pragma code_seg("PAGE")
/*-----------------------------------------------------------------------------
IMiniportStreamAudioEngineNode::SetStreamChannelMute

Decscription:

	When handling SET mute KS property for the device, Portcls calls
	this method, inside its property handlers, to set the current setting on the specific channel.

Parameters:

		_In_ _uiChannel:  the target channel for this Set mute operation
		_In_ _bMute: mute value to set

Return Value:

   Appropriate NTSTATUS code

Called at PASSIVE_LEVEL

Remarks

-------------------------------------------------------------------------------------------------------------------------*/
NTSTATUS MiniportWaveRTStream::SetStreamChannelMute(_In_ UINT32 _uiChannel, _In_  BOOL  _bMute)
{
	NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

	PAGED_CODE();

	// If Channel is ALL_CHANNELS_ID, then set the mute info on all channels
	if (ALL_CHANNELS_ID == _uiChannel)
	{
		for (UINT32 i = 0; i < m_pWfExt->Format.nChannels; i++)
		{
			ntStatus = SetChannelMute(i, _bMute);
		}
	}
	else
	{
		ntStatus = SetChannelMute(_uiChannel, _bMute);
	}

	return ntStatus;
}
//presentation
#pragma code_seg("PAGE")
/*-----------------------------------------------------------------------------
IMiniportStreamAudioEngineNode::GetStreamPresentationPosition

Decscription:

	Portcls calls this method, inside its property handlers, to get a stream's presentation
	postion.

Parameters:

		_Out_ pPresentationPosition: a pointer to a KSAUDIO_PRESENTATION_POSITION variable for receiving returned information

Return Value:

   Appropriate NTSTATUS code

Called at PASSIVE_LEVEL

Remarks

-------------------------------------------------------------------------------------------------------------------------*/
NTSTATUS MiniportWaveRTStream::GetStreamPresentationPosition(_Out_ KSAUDIO_PRESENTATION_POSITION *_pPresentationPosition)
{
	NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

	PAGED_CODE();
	ASSERT(_pPresentationPosition);

	ntStatus = GetPresentationPosition(_pPresentationPosition);

	return ntStatus;
}
//StreamCurrentWritePosition
#pragma code_seg("PAGE")
/*-----------------------------------------------------------------------------
IMiniportStreamAudioEngineNode::SetStreamCurrentWritePosition

Decscription:

	Portcls calls this method, inside its property handlers, to set a stream's write position
	postion.

Parameters:

		_In_ ulCurrentWritePosition: a position value indicating the last valid byte in the buffer

Return Value:

   Appropriate NTSTATUS code

Called at PASSIVE_LEVEL

Remarks

_ulCurrentWritePosition specifies the current write position in bytes, which an offload driver can use to know how much valid data in the WaveRT buffer.
Let say, BufferByteSize is the WaveRT buffer size,  the valid value range for _ulCurrentWritePosition be 0... BufferByteSize (inclusive)

0: no valid data in the buffer
n: the last valid data byte is at the offset n from the beginning of the buffer. If this is a value smaller than a previous write position,
it means a wrap-around has happened, in the case, the driver would need to take that in to consider when calculating how many bytes are valid for fetching.
BufferByteSize: the last valid data byte is at the end of the buffer

After a pin is instantiated and before any KSPROPERTY_AUDIO_WAVERT_CURRENT_WRITE_POSITION is received, a driver should assume its current write position is zero.

-------------------------------------------------------------------------------------------------------------------------*/
NTSTATUS MiniportWaveRTStream::SetStreamCurrentWritePosition(_In_ ULONG _ulCurrentWritePosition)
{
	NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

	PAGED_CODE();

	ntStatus = SetCurrentWritePosition(_ulCurrentWritePosition);

	return ntStatus;
}
#pragma code_seg("PAGE")
/*-----------------------------------------------------------------------------
IMiniportStreamAudioEngineNode::GetStreamLinearBufferPosition

Decscription:

	Portcls calls this method, inside its property handlers, to set a stream's write position
	postion.

Parameters:

		_Out_ pullLinearBufferPosition: a pointer to a ULONGLONG variable for receiving returned information


Return Value:

   Appropriate NTSTATUS code

Called at PASSIVE_LEVEL

Remarks
The returned  value is the number of bytes that the DMA has fetched from the audio buffer since the beginning of the stream
-------------------------------------------------------------------------------------------------------------------------*/
NTSTATUS MiniportWaveRTStream::GetStreamLinearBufferPosition(_Out_ ULONGLONG *_pullLinearBufferPosition)
{
	NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

	PAGED_CODE();
	ASSERT(_pullLinearBufferPosition);

	ntStatus = GetPositions(_pullLinearBufferPosition, NULL, NULL);

	return ntStatus;
}
//SetStreamLoopbackProtection
#pragma code_seg("PAGE")
/*-----------------------------------------------------------------------------
IMiniportStreamAudioEngineNode::SetStreamLoopbackProtection

Decscription:

	Portcls calls this method, inside its property handlers, to set the loopback
	protection option

Parameters:
		_In_ protectionOption: protection option
								 CONSTRICTOR_OPTION_DISABLE: Turn protection  off.
								 CONSTRICTOR_OPTION_MUTE: Mute the loopback stream
Return Value:

	Appropriate NTSTATUS code

Called at PASSIVE_LEVEL

Remarks
	This content protection settings requestion could come in from the host process
	pin or offload pin. The miniport needs to mute its loopback stream contents for
	for this request from either host process pin or offload pin.
-------------------------------------------------------------------------------------------------------------------------*/
NTSTATUS MiniportWaveRTStream::SetStreamLoopbackProtection(_In_ CONSTRICTOR_OPTION protectionOption)
{
	NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

	UNREFERENCED_PARAMETER(protectionOption);
	PAGED_CODE();

	return ntStatus;
}
#pragma code_seg("PAGE")
/*-----------------------------------------------------------------------------
IMiniportStreamAudioEngineNode::GetStreamChannelCount

Decscription:

	When handling volume, mute, and meter related KS properties, Portcls calls
	this method, inside its property handlers, to know the number of channels for the
	corresponding KS property.

Parameters:

		_In_ _targetType:  the query target (volume, mute, or peak meter)
		_Out_ _pulChannelCount: a pointer to a UINT32 variable for receiving returned channel count information

Return Value:

   Appropriate NTSTATUS code

Called at PASSIVE_LEVEL

Remarks

-------------------------------------------------------------------------------------------------------------------------*/
NTSTATUS MiniportWaveRTStream::GetStreamChannelCount(_In_  eChannelTargetType    _targetType, _Out_  UINT32 *_pulChannelCount)
{
	NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

	PAGED_CODE();


	switch (_targetType)
	{
	case eVolumeAttribute:
		ntStatus = GetVolumeChannelCount(_pulChannelCount);
		break;
	case eMuteAttribute:
		ntStatus = GetMuteChannelCount(_pulChannelCount);
		break;
	case ePeakMeterAttribute:
		ntStatus = GetPeakMeterChannelCount(_pulChannelCount);
		break;
	default:
		ntStatus = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	return ntStatus;
}


//------------------------------------------------------------------------------------------------------------------------
//MiniportWaveRTStream private supporting functions
//------------------------------------------------------------------------------------------------------------------------
#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::GetVolumeChannelCount(_Out_ UINT32 *_puiChannelCount)
{
	PAGED_CODE();


	ASSERT(_puiChannelCount);
	ASSERT(m_pWfExt);

	NTSTATUS ntStatus = STATUS_SUCCESS;
	*_puiChannelCount = m_pWfExt->Format.nChannels;
	return ntStatus;
}

#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::GetVolumeSteppings(_Out_writes_bytes_(_ui32DataSize)  PKSPROPERTY_STEPPING_LONG _pKsPropStepLong, _In_  UINT32 _ui32DataSize)
{
	PAGED_CODE();
	UINT32 ulChannelCount = _ui32DataSize / sizeof(KSPROPERTY_STEPPING_LONG);
	ASSERT(_pKsPropStepLong);

	if (ulChannelCount != m_pWfExt->Format.nChannels)
	{
		return STATUS_INVALID_PARAMETER;
	}

	for (UINT i = 0; i < ulChannelCount; i++)
	{
		_pKsPropStepLong[i].SteppingDelta = VOLUME_STEPPING_DELTA;
		_pKsPropStepLong[i].Bounds.SignedMaximum = VOLUME_SIGNED_MAXIMUM;
		_pKsPropStepLong[i].Bounds.SignedMinimum = VOLUME_SIGNED_MINIMUM;
	}

	return STATUS_SUCCESS;
}
#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::GetChannelVolume(_In_  UINT32 _uiChannel, _Out_  LONG *_pVolume)
{
	PAGED_CODE();
	ASSERT(_pVolume);

	*_pVolume = m_plVolumeLevel[_uiChannel];

	return STATUS_SUCCESS;
}
#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::SetChannelVolume(_In_  UINT32 _uiChannel, _In_  LONG _Volume)
{
	PAGED_CODE();

	m_plVolumeLevel[_uiChannel] = _Volume;

	return STATUS_SUCCESS;
}
///- metering
#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::GetPeakMeterChannelCount(_Out_ UINT32 *puiChannelCount)
{
	PAGED_CODE();

	ASSERT(puiChannelCount);
	ASSERT(m_pWfExt);

	NTSTATUS ntStatus = STATUS_SUCCESS;
	*puiChannelCount = m_pWfExt->Format.nChannels;
	return ntStatus;
}

#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::GetPeakMeterSteppings(_Out_writes_bytes_(_ui32DataSize) PKSPROPERTY_STEPPING_LONG _pKsPropStepLong, _In_  UINT32 _ui32DataSize)
{
	PAGED_CODE();
	UINT32 ulChannelCount = _ui32DataSize / sizeof(KSPROPERTY_STEPPING_LONG);

	ASSERT(_pKsPropStepLong);

	if (ulChannelCount != m_pWfExt->Format.nChannels)
	{
		return STATUS_INVALID_PARAMETER;
	}

	for (UINT i = 0; i < ulChannelCount; i++)
	{
		_pKsPropStepLong[i].SteppingDelta = PEAKMETER_STEPPING_DELTA;
		_pKsPropStepLong[i].Bounds.SignedMaximum = PEAKMETER_SIGNED_MAXIMUM;
		_pKsPropStepLong[i].Bounds.SignedMinimum = PEAKMETER_SIGNED_MINIMUM;
	}

	return STATUS_SUCCESS;
}
#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::GetChannelPeakMeter(_In_  UINT32 _uiChannel, _Out_  LONG *_plPeakMeter)
{
	PAGED_CODE();
	ASSERT(_plPeakMeter);
	UNREFERENCED_PARAMETER(_uiChannel);

	*_plPeakMeter = PEAKMETER_NORMALIZE_IN_RANGE(PEAKMETER_SIGNED_MAXIMUM / 2);

	return STATUS_SUCCESS;
}

//=============================================================================
// stream Mute
#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::GetMuteChannelCount(_Out_ UINT32 *puiChannelCount)
{
	PAGED_CODE();

	ASSERT(puiChannelCount);
	ASSERT(m_pWfExt);

	NTSTATUS ntStatus = STATUS_SUCCESS;
	*puiChannelCount = m_pWfExt->Format.nChannels;
	return ntStatus;
}

#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::GetMuteSteppings(_Out_writes_bytes_(_ui32DataSize) PKSPROPERTY_STEPPING_LONG _pKsPropStepLong, _In_  UINT32 _ui32DataSize)
{
	PAGED_CODE();
	UINT32 ulChannelCount = _ui32DataSize / sizeof(KSPROPERTY_STEPPING_LONG);

	ASSERT(_pKsPropStepLong);

	if (ulChannelCount != m_pWfExt->Format.nChannels)
	{
		return STATUS_INVALID_PARAMETER;
	}

	for (UINT i = 0; i < ulChannelCount; i++)
	{
		_pKsPropStepLong[i].SteppingDelta = 1;
		_pKsPropStepLong[i].Bounds.SignedMaximum = TRUE;
		_pKsPropStepLong[i].Bounds.SignedMinimum = FALSE;
	}

	return STATUS_SUCCESS;
}
#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::GetChannelMute(_In_  UINT32 _uiChannel, _Out_  BOOL *_pbMute)
{
	PAGED_CODE();
	ASSERT(_pbMute);

	*_pbMute = m_pbMuted[_uiChannel];
	return STATUS_SUCCESS;
}
#pragma code_seg("PAGE")
NTSTATUS MiniportWaveRTStream::SetChannelMute(_In_  UINT32 _uiChannel, _In_  BOOL _bMute)
{
	PAGED_CODE();

	m_pbMuted[_uiChannel] = _bMute;

	return STATUS_SUCCESS;
}
//presentation
#pragma code_seg()
//
//  UINT64 u64PositionInBlocks; // The block offset from the start of the stream to the current post-decoded uncompressed 
//                              // position in the stream, where a block is the group of channels in the same sample; for a PCM stream, 
//                              // a block is same as a frame. For compressed formats, a block is a single sample within a frame 
//                              // (eg. each MP3 frame has 1152 samples or 1152 blocks) 
//  UINT64 u64QPCPosition;      // The value of the performance counter at the time that the audio endpoint device read the device 
//                              // position (*pu64Position) in response to the KSAUDIO_PRESENTATION_POSITION call.

NTSTATUS MiniportWaveRTStream::GetPresentationPosition(_Out_  KSAUDIO_PRESENTATION_POSITION *_pPresentationPosition)
{
	ASSERT(_pPresentationPosition);
	LARGE_INTEGER timeStamp;
	IAdapterCommon* pAdapterComm = m_pMiniport->GetAdapter();

	ULONGLONG ullLinearPosition = { 0 };
	ULONGLONG ullPresentationPosition = { 0 };
	NTSTATUS status = STATUS_SUCCESS;

	status = GetPositions(&ullLinearPosition, &ullPresentationPosition, &timeStamp);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	_pPresentationPosition->u64PositionInBlocks = ullPresentationPosition * m_pWfExt->Format.nSamplesPerSec / m_pWfExt->Format.nAvgBytesPerSec;
	_pPresentationPosition->u64QPCPosition = (UINT64)timeStamp.QuadPart;

	//Event type: eMINIPORT_GET_PRESENTATION_POSITION
	//Parameter 1: Current linear buffer position    
	//Parameter 2: Previous WaveRtBufferWritePosition that the driver received    
	//Parameter 3: Presentation position
	//Parameter 4: 0
	pAdapterComm->WriteEtwEvent(eMINIPORT_GET_PRESENTATION_POSITION,
		ullLinearPosition, // replace with the correct "Current linear buffer position"    
		m_ulCurrentWritePosition,
		_pPresentationPosition->u64PositionInBlocks,
		0);  // always zero
	return STATUS_SUCCESS;
}

#pragma code_seg()
NTSTATUS MiniportWaveRTStream::SetCurrentWritePosition(_In_  ULONG _ulCurrentWritePosition)
{

	NTSTATUS ntStatus;

#if defined(SYSVAD_BTH_BYPASS)
	if (m_SidebandStarted)
	{
		ntStatus = GetSidebandStreamNtStatus();
		IF_FAILED_JUMP(ntStatus, Done);
	}
#endif // defined(SYSVAD_BTH_BYPASS)

	//
	// Basic validation. WritePosition indicates the position (1-based) of the last valid byte.
	//
	if (_ulCurrentWritePosition == 0)
	{
		ntStatus = STATUS_INVALID_DEVICE_REQUEST;
		goto Done;
	}

	KIRQL oldIrql;
	KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);
	ntStatus = SetCurrentWritePositionInternal(_ulCurrentWritePosition);
	KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);

Done:
	return ntStatus;
}

#pragma code_seg()
NTSTATUS MiniportWaveRTStream::SetCurrentWritePositionInternal(_In_  ULONG _ulCurrentWritePosition)
{

	ASSERT(m_bEoSReceived == FALSE);

	if (m_bEoSReceived)
	{
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	if (_ulCurrentWritePosition > m_ulDmaBufferSize)
	{
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	IAdapterCommon* pAdapterComm = m_pMiniport->GetAdapter();

	//Event type: eMINIPORT_SET_WAVERT_BUFFER_WRITE_POSITION
	//Parameter 1: Current linear buffer position    
	//Parameter 2: Previous WaveRtBufferWritePosition that the driver received    
	//Parameter 3: Target WaveRtBufferWritePosition received from portcls
	//Parameter 4: 0
	pAdapterComm->WriteEtwEvent(eMINIPORT_SET_WAVERT_BUFFER_WRITE_POSITION,
		m_ullLinearPosition, // replace with the correct "Current linear buffer position"    
		m_ulCurrentWritePosition,
		_ulCurrentWritePosition, // this is new write position
		0); // always zero

//
// Check for eMINIPORT_GLITCH_REPORT - Same WaveRT buffer write during event driven mode.
//
	if (m_ulNotificationIntervalMs > 0)
	{
		if (m_ulCurrentWritePosition == _ulCurrentWritePosition)
		{
			//Event type: eMINIPORT_GLITCH_REPORT
			//Parameter 1: Current linear buffer position 
			//Parameter 2: Previous WaveRtBufferWritePosition that the driver received 
			//Parameter 3: Major glitch code: 3: Received same WaveRT buffer twice in a row during event driven mode
			//Parameter 4: Minor code for the glitch cause
			pAdapterComm->WriteEtwEvent(eMINIPORT_GLITCH_REPORT,
				m_ullLinearPosition, // replace with the correct "Current linear buffer position"
				m_ulCurrentWritePosition,
				3, // received same WaveRT buffer twice in a row during event driven mode
				_ulCurrentWritePosition);
		}
	}

	m_ulCurrentWritePosition = _ulCurrentWritePosition;
	InterlockedExchange(&m_IsCurrentWritePositionUpdated, 1);

	return STATUS_SUCCESS;
}

//linear and presentation positions
#pragma code_seg()
NTSTATUS MiniportWaveRTStream::GetPositions
(
	_Out_opt_  ULONGLONG *      _pullLinearBufferPosition,
	_Out_opt_  ULONGLONG *      _pullPresentationPosition,
	_Out_opt_  LARGE_INTEGER *  _pliQPCTime
)
{

	NTSTATUS        ntStatus;
	LARGE_INTEGER   ilQPC;
	KIRQL           oldIrql;
#if defined(SYSVAD_BTH_BYPASS)
	if (m_SidebandStarted)
	{
		ntStatus = GetSidebandStreamNtStatus();
		IF_FAILED_JUMP(ntStatus, Done);
	}
#endif // defined(SYSVAD_BTH_BYPASS)

	// Update *_pullLinearBufferPosition with the the number of bytes fetched from waveRT ever since a stream got set into RUN
	// state.
	// Once the stream is set to STOP state, any further read on this call would return zero.

	//
	// Get the current time and update position.
	//
	KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);
	ilQPC = KeQueryPerformanceCounter(NULL);
	if (m_KsState == KSSTATE_RUN)
	{
		UpdatePosition(ilQPC);
	}
	if (_pullLinearBufferPosition)
	{
		*_pullLinearBufferPosition = m_ullLinearPosition;
	}
	if (_pullPresentationPosition)
	{
		*_pullPresentationPosition = m_ullPresentationPosition;
	}
	KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);
	if (_pliQPCTime)
	{
		*_pliQPCTime = ilQPC;
	}

	ntStatus = STATUS_SUCCESS;

#if defined(SYSVAD_BTH_BYPASS)
	Done:
#endif // defined(SYSVAD_BTH_BYPASS)
		return ntStatus;
}

#pragma code_seg("PAGE")
/*-----------------------------------------------------------------------------
MiniportWaveRTStream::SetLoopbackProtection

Decscription:

	Portcls calls this method, inside its property handlers, to set the loopback
	protection option

Parameters:
		_In_ protectionOption: protection option
								 CONSTRICTOR_OPTION_DISABLE: Turn protection  off.
								 CONSTRICTOR_OPTION_MUTE: Mute the loopback stream
Return Value:

	Appropriate NTSTATUS code

Called at PASSIVE_LEVEL

Remarks
	This content protection settings requestion could come in from the host process
	pin or offload pin. The miniport needs to mute its loopback stream contents for
	for this request from either host process pin or offload pin.
-------------------------------------------------------------------------------------------------------------------------*/
NTSTATUS MiniportWaveRTStream::SetLoopbackProtection(_In_ CONSTRICTOR_OPTION protectionOption)
{
	PAGED_CODE();

	UNREFERENCED_PARAMETER(protectionOption);
	//
	// Miniport driver mutes/unmutes the loopback here.
	// 
	//m_ToneGenerator.SetMute(protectionOption == CONSTRICTOR_OPTION_MUTE);

	return STATUS_SUCCESS;
}

#pragma code_seg()
//----------------------------------------------------------------------------------
// Description:
//      Set current write position for the very last buffer in a stream
//
// Parameters:
//      ULONG _ulWritePosition: point to the last valid data byte
//
// Remark:
//
//      In Win 8, for offload streams, double buffering mechanism is used and the buffer completion contact
//      between the audio stack and driver is: a driver signals buffer completion only when it reaches the
//      end of either buffer. Thus, there is no concept of partial buffer (not all the data in a buffer are  
//      valid, only the first N frames are valid and the rest of the buffer might contain invalid or silence 
//      data. So, when the very last buffer is partially filled, its completion will not get singled until 
//      all data is consumed, which might become undesirable when the size of a buffer is large and valid frame
//      count is low.
//      In WinBlue, a driver implements IMiniportStreamAudioEngineNode2 to claim being able to handle the very 
//      last buffer that's partial filled. 
//      For a driver claiming supporting IMiniportStreamAudioEngineNode2, for the very last buffer position write,
//      it will receive the very last buffer write via SetStreamCurrentWritePositionForLastBuffer() call instead of 
//      SetStreamCurrentWritePosition(). A driver should check _ulWritePosition parameter to see how much valid data
//      in the buffer (instead of blindly assuming all the data are valid) before signaling buffer completion.
//      No more SetStreamCurrentWritePositionForLastBuffer or SetStreamCurrentWritePosition will be called after this 
//      SetStreamCurrentWritePositionForLastBuffer() call.
//-----------------------------------------------------------------------------------------------------------------------

NTSTATUS MiniportWaveRTStream::SetStreamCurrentWritePositionForLastBuffer(_In_ ULONG _ulWritePosition)
{
	NTSTATUS        ntStatus;
	KIRQL           oldIrql;

	KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);

	// Miniport driver needs to prepare to signal buffer completion event
	// when it's done with reading the last valid byte - an _ulWritePosition offset from the beginning WaveRT buffer
	// Note: _ulWritePosition will be smaller than buffer size in most of the cases 
	ntStatus = SetCurrentWritePositionInternal(_ulWritePosition);
	if (NT_SUCCESS(ntStatus))
	{
		m_bEoSReceived = TRUE;
	}

	KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);

	return ntStatus;
}

