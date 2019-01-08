#pragma once
#include "Globals.h"

#include "IAdapterCommon.h"
#include "EndpointMinipair.h"

class MiniportTopology :
	public IMiniportTopology,
	public CUnknown
{
private:
	PCFILTER_DESCRIPTOR*	m_FilterDescriptor;
	DeviceType				m_DeviceType;
	PVOID                   m_DeviceContext;
	USHORT                  m_DeviceMaxChannels;
	IAdapterCommon*			m_Adapter;
public:
	DECLARE_STD_UNKNOWN();
	static NTSTATUS Create(PUNKNOWN * Unknown, REFCLSID, PUNKNOWN UnknownOuter, POOL_TYPE PoolType, PUNKNOWN UnknownAdapter, PVOID DeviceContext, PENDPOINT_MINIPAIR MiniportPair);
	MiniportTopology
	(
		_In_opt_    PUNKNOWN                UnknownOuter,
		_In_        PCFILTER_DESCRIPTOR*	FilterDesc,
		_In_        USHORT                  DeviceMaxChannels,
		_In_        DeviceType				DeviceType,
		_In_opt_    PVOID                   DeviceContext
	);
	~MiniportTopology();

	IMP_IMiniportTopology;
};

