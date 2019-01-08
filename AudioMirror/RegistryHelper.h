#pragma once
#include "Globals.h"

class RegistryHelper
{
private:
	RegistryHelper();
	~RegistryHelper();
public:
	static NTSTATUS CopyRegistryValues(_In_ HANDLE _hSourceKey, _In_ HANDLE _hDestinationKey);
	static NTSTATUS CopyRegistryKey(_In_ HANDLE _hSourceKey, _In_ HANDLE _hDestinationKey, _In_ BOOL _bOverwrite = FALSE);
};

