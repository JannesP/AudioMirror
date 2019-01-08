#include "RegistryHelper.h"

RegistryHelper::RegistryHelper()
{
}

RegistryHelper::~RegistryHelper()
{
}

NTSTATUS RegistryHelper::CopyRegistryKey(HANDLE _hSourceKey, HANDLE _hDestinationKey, BOOL _bOverwrite)
/*++

Routine Description:

  This method recursively copies the registry values in _hSourceKey to _hDestinationKey.
  Set _bOverwrite to indicate whether the first level values are copied or not.
  Normal use is to set false for the initial call, and then all sub paths will be copied.

Return Value:

  NT status code.

--*/
{
	NTSTATUS                ntStatus = STATUS_UNSUCCESSFUL;
	PKEY_BASIC_INFORMATION  kBasicInfo = NULL;
	ULONG                   ulBasicInfoLength = 0;
	ULONG                   ulBasicInfoResultLength = 0;
	ULONG                   ulDisposition = 0;
	PWSTR                   pwstrKeyName = NULL;
	UNICODE_STRING          strKeyName;
	OBJECT_ATTRIBUTES       hCurrentSourceKeyAttributes;
	OBJECT_ATTRIBUTES       hNewDestinationKeyAttributes;
	HANDLE                  hCurrentSourceKey = NULL;
	HANDLE                  hNewDestinationKey = NULL;
	PAGED_CODE();
	// Validate parameters
	IF_TRUE_ACTION_JUMP(_hSourceKey == nullptr, ntStatus = STATUS_INVALID_PARAMETER, Exit);
	IF_TRUE_ACTION_JUMP(_hDestinationKey == nullptr, ntStatus = STATUS_INVALID_PARAMETER, Exit);

	// Allocate the KEY_BASIC_INFORMATION structure
	ulBasicInfoLength = sizeof(KEY_BASIC_INFORMATION) + MAX_DEVICE_REG_KEY_LENGTH;
	kBasicInfo = (PKEY_BASIC_INFORMATION)ExAllocatePoolWithTag(NonPagedPoolNx, ulBasicInfoLength, MINIADAPTER_POOLTAG);
	IF_TRUE_ACTION_JUMP(kBasicInfo == NULL, ntStatus = STATUS_INSUFFICIENT_RESOURCES, Exit);

	ntStatus = STATUS_SUCCESS;
	// Iterate over each key and copy it
	for (UINT i = 0; NT_SUCCESS(ntStatus); i++)
	{
		// Enumerate the next key
		ntStatus = ZwEnumerateKey(_hSourceKey, i, KeyBasicInformation, kBasicInfo, ulBasicInfoLength, &ulBasicInfoResultLength);

		// Jump out of this loop if there are no more keys
		IF_TRUE_ACTION_JUMP(ntStatus == STATUS_NO_MORE_ENTRIES, ntStatus = STATUS_SUCCESS, copy_values);

		// Handle incorrect buffer size
		if (ntStatus == STATUS_BUFFER_TOO_SMALL || ntStatus == STATUS_BUFFER_OVERFLOW)
		{
			// Free and re-allocate the KEY_BASIC_INFORMATION structure with the correct size.
			ExFreePoolWithTag(kBasicInfo, MINIADAPTER_POOLTAG);
			ulBasicInfoLength = ulBasicInfoResultLength;
			kBasicInfo = (PKEY_BASIC_INFORMATION)ExAllocatePoolWithTag(NonPagedPoolNx, ulBasicInfoLength, MINIADAPTER_POOLTAG);
			IF_TRUE_ACTION_JUMP(kBasicInfo == NULL, ntStatus = STATUS_INSUFFICIENT_RESOURCES, loop_exit);

			// Try to enumerate the current key again.
			ntStatus = ZwEnumerateKey(_hSourceKey, i, KeyBasicInformation, kBasicInfo, ulBasicInfoLength, &ulBasicInfoResultLength);

			// Jump out of this loop if there are no more keys
			IF_TRUE_ACTION_JUMP(ntStatus == STATUS_NO_MORE_ENTRIES, ntStatus = STATUS_SUCCESS, copy_values);
			IF_FAILED_JUMP(ntStatus, loop_exit);
		}
		else
		{
			IF_FAILED_JUMP(ntStatus, loop_exit);
		}

		// Allocate the key name string 
		pwstrKeyName = (PWSTR)ExAllocatePoolWithTag(NonPagedPoolNx, kBasicInfo->NameLength + sizeof(WCHAR), MINIADAPTER_POOLTAG);
		IF_TRUE_ACTION_JUMP(kBasicInfo == NULL, ntStatus = STATUS_INSUFFICIENT_RESOURCES, loop_exit);

		// Copy the key name from the basic information struct
		RtlStringCbCopyNW(pwstrKeyName, kBasicInfo->NameLength + sizeof(WCHAR), kBasicInfo->Name, kBasicInfo->NameLength);

		// Make sure the string is null terminated
		pwstrKeyName[(kBasicInfo->NameLength) / sizeof(WCHAR)] = 0;

		// Copy the key name string to a UNICODE string
		RtlInitUnicodeString(&strKeyName, pwstrKeyName);

		// Initialize attributes to open the currently enumerated source key
		InitializeObjectAttributes(&hCurrentSourceKeyAttributes, &strKeyName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, _hSourceKey, NULL);

		// Open the currently enumerated source key
		ntStatus = ZwOpenKey(&hCurrentSourceKey, KEY_READ, &hCurrentSourceKeyAttributes);
		IF_FAILED_ACTION_JUMP(ntStatus, ZwClose(hCurrentSourceKey), loop_exit);

		// Initialize attributes to create the new destination key
		InitializeObjectAttributes(&hNewDestinationKeyAttributes, &strKeyName, OBJ_KERNEL_HANDLE, _hDestinationKey, NULL);

		// Create the key at the destination
		ntStatus = ZwCreateKey(&hNewDestinationKey, KEY_WRITE, &hNewDestinationKeyAttributes, 0, NULL, REG_OPTION_NON_VOLATILE, &ulDisposition);
		IF_FAILED_ACTION_JUMP(ntStatus, ZwClose(hCurrentSourceKey), loop_exit);

		// Now copy the contents of the currently enumerated key to the destination
		ntStatus = CopyRegistryKey(hCurrentSourceKey, hNewDestinationKey, TRUE);
		IF_FAILED_JUMP(ntStatus, loop_exit);

	loop_exit:
		// Free the key name string
		if (pwstrKeyName)
		{
			ExFreePoolWithTag(pwstrKeyName, MINIADAPTER_POOLTAG);
		}

		// Close the current source key
		if (hCurrentSourceKey)
		{
			ZwClose(hCurrentSourceKey);
		}

		// Close the new destination key
		if (hNewDestinationKey)
		{
			ZwClose(hNewDestinationKey);
		}

		// Bail if anything failed
		IF_FAILED_JUMP(ntStatus, Exit);
	}

copy_values:
	// Copy the values 
	if (_bOverwrite)
	{
		ntStatus = RegistryHelper::CopyRegistryValues(_hSourceKey, _hDestinationKey);
		IF_FAILED_JUMP(ntStatus, Exit);
	}

Exit:
	// Free the basic information structure
	if (kBasicInfo)
	{
		ExFreePoolWithTag(kBasicInfo, MINIADAPTER_POOLTAG);
	}
	return ntStatus;
}

NTSTATUS RegistryHelper::CopyRegistryValues(HANDLE _hSourceKey, HANDLE _hDestinationKey)
/*++

Routine Description:

  This method copies the registry values in _hSourceKey to _hDestinationKey.

Return Value:

  NT status code.

--*/
{
	NTSTATUS                    ntStatus = STATUS_SUCCESS;
	PKEY_VALUE_FULL_INFORMATION kvFullInfo = NULL;
	ULONG                       ulFullInfoLength = 0;
	ULONG                       ulFullInfoResultLength = 0;
	PWSTR                       pwstrKeyValueName = NULL;
	UNICODE_STRING              strKeyValueName;
	PAGED_CODE();
	// Allocate the KEY_VALUE_FULL_INFORMATION structure
	ulFullInfoLength = sizeof(KEY_VALUE_FULL_INFORMATION) + MAX_DEVICE_REG_KEY_LENGTH;
	kvFullInfo = (PKEY_VALUE_FULL_INFORMATION)ExAllocatePoolWithTag(NonPagedPoolNx, ulFullInfoLength, MINIADAPTER_POOLTAG);
	IF_TRUE_ACTION_JUMP(kvFullInfo == NULL, ntStatus = STATUS_INSUFFICIENT_RESOURCES, Exit);

	// Iterate over each value and copy it to the destination
	for (UINT i = 0; NT_SUCCESS(ntStatus); i++)
	{
		// Enumerate the next value
		ntStatus = ZwEnumerateValueKey(_hSourceKey, i, KeyValueFullInformation, kvFullInfo, ulFullInfoLength, &ulFullInfoResultLength);

		// Jump out of this loop if there are no more values
		IF_TRUE_ACTION_JUMP(ntStatus == STATUS_NO_MORE_ENTRIES, ntStatus = STATUS_SUCCESS, Exit);

		// Handle incorrect buffer size
		if (ntStatus == STATUS_BUFFER_TOO_SMALL || ntStatus == STATUS_BUFFER_OVERFLOW)
		{
			// Free and re-allocate the KEY_VALUE_FULL_INFORMATION structure with the correct size
			ExFreePoolWithTag(kvFullInfo, MINIADAPTER_POOLTAG);

			ulFullInfoLength = ulFullInfoResultLength;

			kvFullInfo = (PKEY_VALUE_FULL_INFORMATION)ExAllocatePoolWithTag(NonPagedPoolNx, ulFullInfoLength, MINIADAPTER_POOLTAG);
			IF_TRUE_ACTION_JUMP(kvFullInfo == NULL, ntStatus = STATUS_INSUFFICIENT_RESOURCES, loop_exit);

			// Try to enumerate the current value again
			ntStatus = ZwEnumerateValueKey(_hSourceKey, i, KeyValueFullInformation, kvFullInfo, ulFullInfoLength, &ulFullInfoResultLength);

			// Jump out of this loop if there are no more values
			IF_TRUE_ACTION_JUMP(ntStatus == STATUS_NO_MORE_ENTRIES, ntStatus = STATUS_SUCCESS, Exit);
			IF_FAILED_JUMP(ntStatus, loop_exit);
		}
		else
		{
			IF_FAILED_JUMP(ntStatus, loop_exit);
		}

		// Allocate the key value name string
		pwstrKeyValueName = (PWSTR)ExAllocatePoolWithTag(NonPagedPoolNx, kvFullInfo->NameLength + sizeof(WCHAR) * 2, MINIADAPTER_POOLTAG);
		IF_TRUE_ACTION_JUMP(kvFullInfo == NULL, ntStatus = STATUS_INSUFFICIENT_RESOURCES, loop_exit);

		// Copy the key value name from the full information struct
		RtlStringCbCopyNW(pwstrKeyValueName, kvFullInfo->NameLength + sizeof(WCHAR) * 2, kvFullInfo->Name, kvFullInfo->NameLength);

		// Make sure the string is null terminated
		pwstrKeyValueName[(kvFullInfo->NameLength) / sizeof(WCHAR)] = 0;

		// Copy the key value name string to a UNICODE string
		RtlInitUnicodeString(&strKeyValueName, pwstrKeyValueName);

		// Write the key value from the source into the destination
		ntStatus = ZwSetValueKey(_hDestinationKey, &strKeyValueName, 0, kvFullInfo->Type, (PVOID)((PUCHAR)kvFullInfo + kvFullInfo->DataOffset), kvFullInfo->DataLength);
		IF_FAILED_JUMP(ntStatus, loop_exit);

	loop_exit:
		// Free the key value name string
		if (pwstrKeyValueName)
		{
			ExFreePoolWithTag(pwstrKeyValueName, MINIADAPTER_POOLTAG);
		}

		// Bail if anything failed
		IF_FAILED_JUMP(ntStatus, Exit);
	}

Exit:
	// Free the KEY_VALUE_FULL_INFORMATION structure
	if (kvFullInfo)
	{
		ExFreePoolWithTag(kvFullInfo, MINIADAPTER_POOLTAG);
	}

	return ntStatus;
}
