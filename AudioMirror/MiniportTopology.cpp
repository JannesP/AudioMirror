#include "MiniportTopology.h"

#include "KsHelper.h"
#include "KsAudioProcessingAttribute.h"

#pragma code_seg("PAGE")
NTSTATUS MiniportTopology::Create(
	_Out_           PUNKNOWN *                              Unknown,
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
	PAGED_CODE();

	UNREFERENCED_PARAMETER(UnknownAdapter);

	ASSERT(Unknown);
	ASSERT(MiniportPair);

	MiniportTopology *obj = new (PoolType, TOPOLOGY_POOLTAG) MiniportTopology(
		UnknownOuter,
		MiniportPair->TopoDescriptor,
		MiniportPair->DeviceMaxChannels,
		MiniportPair->DeviceType,
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

MiniportTopology::MiniportTopology
(
	_In_opt_    PUNKNOWN                UnknownOuter,
	_In_        PCFILTER_DESCRIPTOR*	FilterDesc,
	_In_        USHORT                  DeviceMaxChannels,
	_In_        DeviceType				DeviceType,
	_In_opt_    PVOID                   DeviceContext
) :
	CUnknown(UnknownOuter),
	m_FilterDescriptor(FilterDesc),
	m_DeviceType(DeviceType),
	m_DeviceContext(DeviceContext),
	m_DeviceMaxChannels(DeviceMaxChannels)
{
	
}


MiniportTopology::~MiniportTopology()
{
	SAFE_RELEASE(m_Adapter);
}

NTSTATUS MiniportTopology::Init(PUNKNOWN UnknownAdapter, PRESOURCELIST ResourceList, PPORTTOPOLOGY Port)
{
	UNREFERENCED_PARAMETER(ResourceList);
	UNREFERENCED_PARAMETER(Port);

	NTSTATUS ntStatus = UnknownAdapter->QueryInterface(IID_IAdapterCommon, (PVOID*)&m_Adapter);
		
	return ntStatus;
}

#pragma code_seg("PAGE")
NTSTATUS MiniportTopology::DataRangeIntersection
(
	_In_  ULONG                 PinId,
	_In_  PKSDATARANGE          ClientDataRange,
	_In_  PKSDATARANGE          MyDataRange,
	_In_  ULONG                 OutputBufferLength,
	_Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength)
	PVOID                 ResultantFormat     OPTIONAL,
	_Out_ PULONG                ResultantFormatLength
)
/*++

Routine Description:

  The DataRangeIntersection function determines the highest
  quality intersection of two data ranges. Topology miniport does nothing.

Arguments:

  PinId - Pin for which data intersection is being determined.

  ClientDataRange - Pointer to KSDATARANGE structure which contains the data range
					submitted by client in the data range intersection property
					request

  MyDataRange - Pin's data range to be compared with client's data range

  OutputBufferLength - Size of the buffer pointed to by the resultant format
					   parameter

  ResultantFormat - Pointer to value where the resultant format should be
					returned

  ResultantFormatLength - Actual length of the resultant format that is placed
						  at ResultantFormat. This should be less than or equal
						  to OutputBufferLength

Return Value:

  NT status code.

--*/
{
	UNREFERENCED_PARAMETER(PinId);
	UNREFERENCED_PARAMETER(ClientDataRange);
	UNREFERENCED_PARAMETER(MyDataRange);
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(ResultantFormat);
	UNREFERENCED_PARAMETER(ResultantFormatLength);

	PAGED_CODE();

	DPF_ENTER(("[%s]", __FUNCTION__));

	return (STATUS_NOT_IMPLEMENTED);
} // DataRangeIntersection

#pragma code_seg("PAGE")
NTSTATUS MiniportTopology::GetDescription
(
	_Out_ PPCFILTER_DESCRIPTOR *  OutFilterDescriptor
)
/*++

Routine Description:

  The GetDescription function gets a pointer to a filter description.
  It provides a location to deposit a pointer in miniport's description
  structure. This is the placeholder for the FromNode or ToNode fields in
  connections which describe connections to the filter's pins

Arguments:

  OutFilterDescriptor - Pointer to the filter description.

Return Value:

  NT status code.

--*/
{
	PAGED_CODE();

	ASSERT(OutFilterDescriptor);

	DPF_ENTER(("[%s]", __FUNCTION__));

	*OutFilterDescriptor = m_FilterDescriptor;

	return (STATUS_SUCCESS);
} // GetDescription

#pragma code_seg("PAGE")
STDMETHODIMP MiniportTopology::NonDelegatingQueryInterface
(
	_In_ REFIID                  Interface,
	_COM_Outptr_ PVOID      * Object
)
/*++

Routine Description:

  QueryInterface for MiniportTopology

Arguments:

  Interface - GUID of the interface

  Object - interface object to be returned.

Return Value:

  NT status code.

--*/
{
	PAGED_CODE();

	ASSERT(Object);

	if (IsEqualGUIDAligned(Interface, IID_IUnknown))
	{
		*Object = PVOID(PUNKNOWN(this));
	}
	else if (IsEqualGUIDAligned(Interface, IID_IMiniport))
	{
		*Object = PVOID(PMINIPORT(this));
	}
	else if (IsEqualGUIDAligned(Interface, IID_IMiniportTopology))
	{
		*Object = PVOID(PMINIPORTTOPOLOGY(this));
	}
	else
	{
		*Object = NULL;
	}

	if (*Object)
	{
		// We reference the interface for the caller.
		PUNKNOWN(*Object)->AddRef();
		return(STATUS_SUCCESS);
	}

	return(STATUS_INVALID_PARAMETER);
} // NonDelegatingQueryInterface