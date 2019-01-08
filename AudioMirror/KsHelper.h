#pragma once

#include "Globals.h"

class KsHelper
{
private:
	KsHelper();
	~KsHelper();
public:
	static NTSTATUS PropertyHandler_BasicSupport(
		_In_ PPCPROPERTY_REQUEST         PropertyRequest,
		_In_ ULONG                       Flags,
		_In_ DWORD                       PropTypeSetId
	);

	static NTSTATUS PropertyHandler_BasicSupportPeakMeter2(
		_In_ PPCPROPERTY_REQUEST   PropertyRequest,
		_In_ ULONG                 MaxChannels
	);

	static NTSTATUS PropertyHandler_BasicSupportMute(
		_In_ PPCPROPERTY_REQUEST PropertyRequest,
		_In_ ULONG MaxChannels
	);

	static NTSTATUS PropertyHandler_BasicSupportVolume(
		_In_ PPCPROPERTY_REQUEST PropertyRequest,
		_In_ ULONG MaxChannels
	);

	static NTSTATUS GetAttributesFromAttributeList
	(
		_In_ const KSMULTIPLE_ITEM *_pAttributes,
		_In_ size_t _Size,
		_Out_ GUID* _pSignalProcessingMode
	);

	static NTSTATUS ValidatePropertyParams(
		_In_ PPCPROPERTY_REQUEST      PropertyRequest,
		_In_ ULONG                    cbValueSize,
		_In_ ULONG                    cbInstanceSize = 0
	);
	static PWAVEFORMATEX GetWaveFormatEx(_In_ PKSDATAFORMAT pDataFormat);
};

