#include "SubdeviceCache.h"

typedef struct _MINIPAIR_UNKNOWN
{
	LIST_ENTRY              ListEntry;
	WCHAR                   Name[MAX_PATH];
	PUNKNOWN                PortInterface;
	PUNKNOWN                MiniportInterface;
} MINIPAIR_UNKNOWN;

SubdeviceCache::SubdeviceCache()
{
	InitializeListHead(&m_DeviceList);
}


SubdeviceCache::~SubdeviceCache()
{
	
}

NTSTATUS __stdcall SubdeviceCache::Put
(
	_In_ PWSTR Name,
	_In_ PUNKNOWN UnknownPort,
	_In_ PUNKNOWN UnknownMiniport
)
{
	PAGED_CODE();
	DPF_ENTER(("[CAdapterCommon::CacheSubdevice]"));

	// add the item with this name/interface to the list
	NTSTATUS         ntStatus = STATUS_SUCCESS;
	MINIPAIR_UNKNOWN* pNewSubdevice = NULL;

	pNewSubdevice = new(NonPagedPoolNx, MINIADAPTER_POOLTAG) MINIPAIR_UNKNOWN;

	if (!pNewSubdevice)
	{
		DPF(D_TERSE, ("Insufficient memory to cache subdevice"));
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
	}

	if (NT_SUCCESS(ntStatus))
	{
		memset(pNewSubdevice, 0, sizeof(MINIPAIR_UNKNOWN));

		ntStatus = RtlStringCchCopyW(pNewSubdevice->Name, SIZEOF_ARRAY(pNewSubdevice->Name), Name);
	}

	if (NT_SUCCESS(ntStatus))
	{
		pNewSubdevice->PortInterface = UnknownPort;
		pNewSubdevice->PortInterface->AddRef();

		pNewSubdevice->MiniportInterface = UnknownMiniport;
		pNewSubdevice->MiniportInterface->AddRef();

		InsertTailList(&m_DeviceList, &pNewSubdevice->ListEntry);
	}

	if (!NT_SUCCESS(ntStatus))
	{
		if (pNewSubdevice)
		{
			delete pNewSubdevice;
		}
	}

	return ntStatus;
}

NTSTATUS __stdcall SubdeviceCache::Get
(
	_In_ PWSTR Name,
	_Out_opt_ PUNKNOWN *OutUnknownPort,
	_Out_opt_ PUNKNOWN *OutUnknownMiniport
)
{
	PAGED_CODE();
	DPF_ENTER(("[SubdeviceCache::Get]"));

	// search list, return interface to device if found, fail if not found
	PLIST_ENTRY le = NULL;
	BOOL bFound = FALSE;

	for (le = m_DeviceList.Flink; le != &m_DeviceList && !bFound; le = le->Flink)
	{
		MINIPAIR_UNKNOWN *pRecord = CONTAINING_RECORD(le, MINIPAIR_UNKNOWN, ListEntry);

		if (0 == wcscmp(Name, pRecord->Name))
		{
			if (OutUnknownPort)
			{
				*OutUnknownPort = pRecord->PortInterface;
				(*OutUnknownPort)->AddRef();
			}

			if (OutUnknownMiniport)
			{
				*OutUnknownMiniport = pRecord->MiniportInterface;
				(*OutUnknownMiniport)->AddRef();
			}

			bFound = TRUE;
		}
	}

	return bFound ? STATUS_SUCCESS : STATUS_OBJECT_NAME_NOT_FOUND;
}

NTSTATUS __stdcall SubdeviceCache::Remove
(
	_In_ PWSTR Name
)
{
	PAGED_CODE();
	DPF_ENTER(("[CAdapterCommon::RemoveCachedSubdevice]"));

	// search list, remove the entry from the list

	PLIST_ENTRY le = NULL;
	BOOL bRemoved = FALSE;

	for (le = m_DeviceList.Flink; le != &m_DeviceList && !bRemoved; le = le->Flink)
	{
		MINIPAIR_UNKNOWN *pRecord = CONTAINING_RECORD(le, MINIPAIR_UNKNOWN, ListEntry);

		if (0 == wcscmp(Name, pRecord->Name))
		{
			SAFE_RELEASE(pRecord->PortInterface);
			SAFE_RELEASE(pRecord->MiniportInterface);
			memset(pRecord->Name, 0, sizeof(pRecord->Name));
			RemoveEntryList(le);
			bRemoved = TRUE;
			delete pRecord;
			break;
		}
	}

	return bRemoved ? STATUS_SUCCESS : STATUS_OBJECT_NAME_NOT_FOUND;
}

void __stdcall SubdeviceCache::Clear(void)
{
	PAGED_CODE();
	DPF_ENTER(("[CAdapterCommon::EmptySubdeviceCache]"));

	while (!IsListEmpty(&m_DeviceList))
	{
		PLIST_ENTRY le = RemoveHeadList(&m_DeviceList);
		MINIPAIR_UNKNOWN *pRecord = CONTAINING_RECORD(le, MINIPAIR_UNKNOWN, ListEntry);

		SAFE_RELEASE(pRecord->PortInterface);
		SAFE_RELEASE(pRecord->MiniportInterface);
		memset(pRecord->Name, 0, sizeof(pRecord->Name));

		delete pRecord;
	}
}
