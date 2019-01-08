#pragma once
#include "Globals.h"
#include "RingBuffer.h"

/*++

Copyright (c) 1997-2010  Microsoft Corporation All Rights Reserved

Module Name:

	minwavertstream.h

Abstract:

	Definition of wavert miniport class.

--*/


//
// Structure to store notifications events in a protected list
//
typedef struct _NotificationListEntry
{
	LIST_ENTRY  ListEntry;
	PKEVENT     NotificationEvent;
} NotificationListEntry;

EXT_CALLBACK   TimerNotifyRT;

//=============================================================================
// Referenced Forward
//=============================================================================
class MiniportWaveRT;
typedef MiniportWaveRT *PMiniportWaveRT;

//=============================================================================
// Classes
//=============================================================================
///////////////////////////////////////////////////////////////////////////////
// CMiniportWaveRTStream 
// 
class MiniportWaveRTStream :
	public IMiniportWaveRTStreamNotification,
	public IMiniportWaveRTInputStream,
	public IMiniportWaveRTOutputStream,
	public IMiniportStreamAudioEngineNode,
	public IMiniportStreamAudioEngineNode2,
	public CUnknown
{
protected:
	PPORTWAVERTSTREAM           m_pPortStream;
	LIST_ENTRY                  m_NotificationList;
	PEX_TIMER                   m_pNotificationTimer;
	ULONG                       m_ulNotificationIntervalMs;
	ULONG                       m_ulCurrentWritePosition;
	LONG                        m_IsCurrentWritePositionUpdated;

public:
	DECLARE_STD_UNKNOWN();
	DEFINE_STD_CONSTRUCTOR(MiniportWaveRTStream);
	~MiniportWaveRTStream();

	IMP_IMiniportWaveRTStream;
	IMP_IMiniportWaveRTStreamNotification;
	IMP_IMiniportWaveRTInputStream;
	IMP_IMiniportWaveRTOutputStream;
	IMP_IMiniportStreamAudioEngineNode;
	IMP_IMiniportStreamAudioEngineNode2;

	NTSTATUS                    Init
	(
		_In_  PMiniportWaveRT     Miniport,
		_In_  PPORTWAVERTSTREAM   Stream,
		_In_  ULONG               Channel,
		_In_  BOOLEAN             Capture,
		_In_  PKSDATAFORMAT       DataFormat,
		_In_  GUID                SignalProcessingMode
	);

	// Friends
	friend EXT_CALLBACK         TimerNotifyRT;
protected:
	MiniportWaveRT*            m_pMiniport;
	ULONG                       m_ulPin;
	BOOLEAN                     m_bCapture;
	BOOLEAN                     m_bUnregisterStream;
	ULONG                       m_ulDmaBufferSize;
	BYTE*                       m_pDmaBuffer;
	ULONG                       m_ulNotificationsPerBuffer;
	KSSTATE                     m_KsState;
	PKTIMER                     m_pTimer;
	PRKDPC                      m_pDpc;
	ULONGLONG                   m_ullPlayPosition;
	ULONGLONG                   m_ullWritePosition;
	ULONGLONG                   m_ullLinearPosition;
	ULONGLONG                   m_ullPresentationPosition;
	ULONG                       m_ulLastOsReadPacket;
	ULONG                       m_ulLastOsWritePacket;
	LONGLONG                    m_llPacketCounter;
	ULONGLONG                   m_ullDmaTimeStamp;
	LARGE_INTEGER               m_ullPerformanceCounterFrequency;
	ULONGLONG                   m_hnsElapsedTimeCarryForward;
	ULONGLONG                   m_ullLastDPCTimeStamp;
	ULONGLONG                   m_hnsDPCTimeCarryForward;
	LONG                        m_byteDisplacementCarryForward;
	ULONG                       m_ulDmaMovementRate;
	BOOL                        m_bLfxEnabled;
	PBOOL                       m_pbMuted;
	PLONG                       m_plVolumeLevel;
	PLONG                       m_plPeakMeter;
	PWAVEFORMATEXTENSIBLE       m_pWfExt;
	ULONG                       m_ulContentId;
	GUID                        m_SignalProcessingMode;
	BOOLEAN                     m_bEoSReceived;
	BOOLEAN                     m_bLastBufferRendered;
	KSPIN_LOCK                  m_PositionSpinLock;
	ULONG                       m_AudioModuleCount;

	MiniportWaveRTStream*		m_PairedStream;
	RingBuffer*					m_RingBuffer;
public:

	NTSTATUS GetVolumeChannelCount
	(
		_Out_ UINT32 *puiChannelCount
	);

	NTSTATUS GetVolumeSteppings
	(
		_Out_writes_bytes_(_ui32DataSize) PKSPROPERTY_STEPPING_LONG _pKsPropStepLong,
		_In_  UINT32 _ui32DataSize
	);

	NTSTATUS GetChannelVolume
	(
		_In_  UINT32 _uiChannel,
		_Out_  LONG *_pVolume
	);

	NTSTATUS SetChannelVolume
	(
		_In_  UINT32 _uiChannel,
		_In_  LONG _Volume
	);

	NTSTATUS GetPeakMeterChannelCount
	(
		_Out_ UINT32 *puiChannelCount
	);

	NTSTATUS GetPeakMeterSteppings
	(
		_Out_writes_bytes_(_ui32DataSize)  PKSPROPERTY_STEPPING_LONG _pKsPropStepLong,
		_In_  UINT32 _ui32DataSize
	);

	NTSTATUS GetChannelPeakMeter
	(
		_In_  UINT32 _uiChannel,
		_Out_  LONG *_plPeakMeter
	);

	NTSTATUS GetMuteChannelCount
	(
		_Out_ UINT32 *puiChannelCount
	);

	NTSTATUS GetMuteSteppings
	(
		_Out_writes_bytes_(_ui32DataSize)  PKSPROPERTY_STEPPING_LONG _pKsPropStepLong,
		_In_  UINT32 _ui32DataSize
	);

	NTSTATUS GetChannelMute
	(
		_In_  UINT32 _uiChannel,
		_Out_  BOOL *_pbMute
	);

	NTSTATUS SetChannelMute
	(
		_In_  UINT32 _uiChannel,
		_In_  BOOL _bMute
	);

	//presentation
	NTSTATUS GetPresentationPosition
	(
		_Out_  KSAUDIO_PRESENTATION_POSITION *_pPresentationPosition
	);

	NTSTATUS SetCurrentWritePosition
	(
		_In_  ULONG ulCurrentWritePosition
	);

public:
	NTSTATUS SetLoopbackProtection
	(
		_In_ CONSTRICTOR_OPTION ulProtectionOption
	);

	ULONG GetCurrentWaveRTWritePosition()
	{
		return m_ulCurrentWritePosition;
	};

	// To support simple underrun validation.
	BOOL IsCurrentWaveRTWritePositionUpdated()
	{
		return InterlockedExchange(&m_IsCurrentWritePositionUpdated, 0) ? TRUE : FALSE;
	};

	GUID GetSignalProcessingMode()
	{
		return m_SignalProcessingMode;
	}

	NTSTATUS PropertyHandlerModulesListRequest
	(
		_In_ PPCPROPERTY_REQUEST PropertyRequest
	);

	NTSTATUS PropertyHandlerModuleCommand
	(
		_In_ PPCPROPERTY_REQUEST PropertyRequest
	);

	void SetPairedStream(MiniportWaveRTStream* stream);
private:

	//
	// Helper functions.
	//

#pragma code_seg()
	ULONG
		GetAudioModuleListCount()
	{
		return m_AudioModuleCount;
	}

	VOID WriteBytes
	(
		_In_ ULONG ByteDisplacement
	);

	VOID ReadBytes
	(
		_In_ ULONG ByteDisplacement
	);

	NTSTATUS WriteAudioPacket(BYTE * buffer, ULONG packetSize, BOOL eos);

	VOID UpdatePosition
	(
		_In_ LARGE_INTEGER ilQPC
	);

	NTSTATUS SetCurrentWritePositionInternal
	(
		_In_  ULONG ulCurrentWritePosition
	);

	NTSTATUS GetPositions
	(
		_Out_opt_  ULONGLONG *      _pullLinearBufferPosition,
		_Out_opt_  ULONGLONG *      _pullPresentationPosition,
		_Out_opt_  LARGE_INTEGER *  _pliQPCTime
	);

};
typedef MiniportWaveRTStream *PMiniportWaveRTStream;