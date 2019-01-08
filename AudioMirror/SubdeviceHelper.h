#pragma once
#include "Globals.h"

#include "EndpointMinipair.h"
#include "IAdapterCommon.h"
#include "SubdeviceCache.h"

class SubdeviceHelper
{
public:
	SubdeviceHelper(IAdapterCommon* adapter);
	~SubdeviceHelper();

private:
	SubdeviceCache m_DeviceCache;
	IAdapterCommon* m_Adapter;

	NTSTATUS MigrateDeviceInterfaceTemplateParameters(
		PUNICODE_STRING SymbolicLinkName, 
		PDEVICE_OBJECT pPhysicalDeviceObject, 
		PCWSTR TemplateReferenceString
	);
	NTSTATUS SysvadIoSetDeviceInterfacePropertyDataMultiple(
		PUNICODE_STRING SymbolicLinkName, 
		ULONG cPropertyCount, 
		const SYSVAD_DEVPROPERTY * 
		pProperties
	);
	NTSTATUS CreateAudioInterfaceWithProperties(
		PCWSTR ReferenceString,
		PCWSTR TemplateReferenceString,
		ULONG cPropertyCount,
		const SYSVAD_DEVPROPERTY * pProperties,
		PDEVICE_OBJECT PhysicalDeviceObject,
		UNICODE_STRING * AudioSymbolicLinkName
	);
	NTSTATUS __stdcall InstallSubdevice(
		_In_opt_        PIRP                                    Irp,
		_In_            PWSTR                                   Name,
		_In_opt_        PWSTR                                   TemplateName,
		_In_            REFGUID                                 PortClassId,
		_In_            REFGUID                                 MiniportClassId,
		_In_opt_        PFNCREATEMINIPORT                       MiniportCreate,
		_In_            ULONG                                   cPropertyCount,
		_In_reads_opt_(cPropertyCount) const SYSVAD_DEVPROPERTY* pProperties,
		_In_opt_        PVOID                                   DeviceContext,
		_In_            PENDPOINT_MINIPAIR                      MiniportPair,
		_In_opt_        PRESOURCELIST                           ResourceList,
		_In_            REFGUID                                 PortInterfaceId,
		_Out_opt_       PUNKNOWN*								OutPortInterface,
		_Out_opt_       PUNKNOWN*								OutPortUnknown,
		_Out_opt_       PUNKNOWN*								OutMiniportUnknown
	);
	NTSTATUS __stdcall ConnectTopologies(
		_In_ PUNKNOWN                   UnknownTopology,
		_In_ PUNKNOWN                   UnknownWave,
		_In_ PHYSICALCONNECTIONTABLE*   PhysicalConnections,
		_In_ ULONG                      PhysicalConnectionCount
	);
	NTSTATUS __stdcall DisconnectTopologies(
		_In_ PUNKNOWN                   UnknownTopology,
		_In_ PUNKNOWN                   UnknownWave,
		_In_ PHYSICALCONNECTIONTABLE*   PhysicalConnections,
		_In_ ULONG                      PhysicalConnectionCount
	);
public:
	NTSTATUS __stdcall InstallMinipair(
		_In_opt_    PIRP                Irp,
		_In_        PENDPOINT_MINIPAIR  MiniportPair,
		_In_opt_    PVOID               DeviceContext,
		_Out_opt_   PUNKNOWN *          UnknownTopology,
		_Out_opt_   PUNKNOWN *          UnknownWave,
		_Out_opt_   PUNKNOWN *          UnknownMiniportTopology,
		_Out_opt_   PUNKNOWN *          UnknownMiniportWave
	);
	void __stdcall Clean();
	
};

