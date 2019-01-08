#pragma once
#include "Globals.h"

class SubdeviceCache
{
private:
	LIST_ENTRY m_DeviceList;

public:
	SubdeviceCache();
	~SubdeviceCache();

	NTSTATUS __stdcall Put(_In_ PWSTR Name, _In_  PUNKNOWN Port, _In_ PUNKNOWN Miniport);
	NTSTATUS __stdcall Get(_In_ PWSTR Name, _Out_opt_ PUNKNOWN* UnknownPort, _Out_opt_ PUNKNOWN* UnknownMiniport);
	NTSTATUS __stdcall Remove(_In_ PWSTR Name);
	void __stdcall Clear();
};

