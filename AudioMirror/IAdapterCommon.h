#pragma once
#include "Globals.h"

#include "EndpointMinipair.h"

DEFINE_GUID(IID_IAdapterCommon,
	0x7eda2950, 0xbf9f, 0x11d0, 0x87, 0x1f, 0x0, 0xa0, 0xc9, 0x11, 0xb5, 0x44);

//
// This is the structure of the portclass FDO device extension Nt has created
// for us.  We keep the adapter common object here.
//
DECLARE_INTERFACE_(IAdapterCommon, IUnknown)
{
	STDMETHOD_(NTSTATUS, Init)
		(
			THIS_
			_In_ IRP* StartupIrp,
			_In_  PDEVICE_OBJECT      DeviceObject
			) PURE;

	STDMETHOD_(PDEVICE_OBJECT, GetDeviceObject)
		(
			THIS
			)PURE;

	STDMETHOD_(PDEVICE_OBJECT, GetPhysicalDeviceObject)
		(
			THIS
			)PURE;

	STDMETHOD_(NTSTATUS, WriteEtwEvent)
		(
			THIS_
			_In_ EPcMiniportEngineEvent    miniportEventType,
			_In_ ULONGLONG  ullData1,
			_In_ ULONGLONG  ullData2,
			_In_ ULONGLONG  ullData3,
			_In_ ULONGLONG  ullData4
			) PURE;

	STDMETHOD_(void, SetEtwHelper)
		(
			THIS_
			PPORTCLSETWHELPER _pPortClsEtwHelper
			) PURE;

	STDMETHOD_(void, Cleanup)();
};