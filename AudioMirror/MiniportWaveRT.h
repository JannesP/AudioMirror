#pragma once
#include "Globals.h"

#include "EndpointMinipair.h"
#include "IAdapterCommon.h"
#include "MiniportWaveRTStream.h"

DEFINE_GUID(IID_MiniportWaveRT,
	0xebbe60f7, 0xe725, 0x4be9, 0xbc, 0x3e, 0x6e, 0xd5, 0x6e, 0xee, 0x37, 0x2e);

class MiniportWaveRT :
	public IMiniportWaveRT,
	public CUnknown
{
private:
	MiniportWaveRT* m_pPairedMiniport;
	MiniportWaveRTStream* m_SystemStream;
	MiniportWaveRTStream* GetStream();

	ULONG m_ulMaxSystemStreams;
	ULONG m_ulSystemAllocated;

	MiniportWaveRTStream**          m_SystemStreams;

	DeviceType m_DeviceType;
	PVOID m_DeviceContext;
	FAST_MUTEX m_DeviceFormatsAndModesLock;
	PIN_DEVICE_FORMATS_AND_MODES* m_DeviceFormatsAndModes;
	ULONG m_DeviceFormatsAndModesCount;
	USHORT m_DeviceMaxChannels;
	ULONG m_DeviceFlags;
	ENDPOINT_MINIPAIR* m_pMiniportPair;
	IAdapterCommon* m_pAdapterCommon;
	PCFILTER_DESCRIPTOR m_FilterDesc;

	BOOL IsRenderDevice();
	ULONG GetSystemPinId();

	NTSTATUS PropertyHandlerProposedFormat(PPCPROPERTY_REQUEST PropertyRequest);
	NTSTATUS PropertyHandlerProposedFormat2(PPCPROPERTY_REQUEST PropertyRequest);
	NTSTATUS IsFormatSupported(ULONG _ulPin, BOOLEAN _bCapture, PKSDATAFORMAT _pDataFormat);
	ULONG GetPinSupportedDeviceFormats(ULONG PinId, KSDATAFORMAT_WAVEFORMATEXTENSIBLE** ppFormats);
	ULONG GetPinSupportedDeviceModes(ULONG PinId, MODE_AND_DEFAULT_FORMAT ** ppModes);
	ULONG GetAudioEngineSupportedDeviceFormats(KSDATAFORMAT_WAVEFORMATEXTENSIBLE** ppFormats);
	NTSTATUS ValidateStreamCreate(ULONG   _Pin, BOOLEAN _Capture);
	const GUID* GetAudioModuleNotificationDeviceId();
	ULONG GetAudioModuleDescriptorListCount();
	ULONG GetAudioModuleListCount();

	NTSTATUS VerifyPinInstanceResourcesAvailable(ULONG allocated, ULONG max);

	PinType GetPinTypeForPinNum(ULONG nPin);
public:
	DECLARE_STD_UNKNOWN();

	MiniportWaveRT(
		_In_            PUNKNOWN                                UnknownAdapter,
		_In_            ENDPOINT_MINIPAIR*                      MiniportPair,
		_In_opt_        PVOID                                   DeviceContext
	);
	
	~MiniportWaveRT();

	IMP_IMiniportWaveRT;

	/*
		Redirects general property request to miniport object
	*/
	static NTSTATUS PropertyHandler_WaveFilter(_In_ PPCPROPERTY_REQUEST PropertyRequest);
	static NTSTATUS Create(PUNKNOWN * Unknown, REFCLSID, PUNKNOWN UnknownOuter, POOL_TYPE PoolType, PUNKNOWN UnknownAdapter, PVOID DeviceContext, PENDPOINT_MINIPAIR MiniportPair);

	IAdapterCommon* GetAdapter();

	NTSTATUS MiniportWaveRT::StreamClosed(ULONG pin, MiniportWaveRTStream* stream);
	NTSTATUS MiniportWaveRT::StreamCreated(_In_ ULONG _Pin, _In_ MiniportWaveRTStream* _Stream);
	void SetPairedMiniport(MiniportWaveRT* miniport);
	BOOL IsSystemRenderPin(ULONG nPinId);
	BOOL IsSystemCapturePin(ULONG nPinId);
	BOOL IsBridgePin(ULONG nPinId);
};

