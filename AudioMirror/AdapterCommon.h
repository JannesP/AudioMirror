#pragma once
#include "Globals.h"

#include "Macros.h"
#include "IAdapterCommon.h"
#include "SubdeviceHelper.h"

class AdapterCommon : public IAdapterCommon, public CUnknown
{
	private:
		static LONG m_Instances;
		PDEVICE_OBJECT m_pDeviceObject;
		PDEVICE_OBJECT m_pPhysicalDeviceObject;
		SubdeviceHelper* m_pDeviceHelper;
		PPORTCLSETWHELPER m_pPortClsEtwHelper;

		NTSTATUS InstallVirtualMic(IRP* Irp, IUnknown** unknownMiniport);
		NTSTATUS InstallVirtualSpeaker(IRP* Irp, IUnknown** unknownMiniport);
		NTSTATUS InstallVirtualCable(IRP* irp);
	public:
		DECLARE_STD_UNKNOWN()
		AdapterCommon(PUNKNOWN unknown) : CUnknown(unknown) {};
		//DEFINE_STD_CONSTRUCTOR(AdapterCommon)
		~AdapterCommon();

		//DISABLED CAUSE I PROBABLY DONT NEED IT
		// Default IAdapterPowerManagement
		//IMP_IAdapterPowerManagement;

		static NTSTATUS Create(
			_Out_       PUNKNOWN *              Unknown,
			_In_        REFCLSID,
			_In_opt_    PUNKNOWN                UnknownOuter,
			_When_((PoolType & NonPagedPoolMustSucceed) != 0,
				__drv_reportError("Must succeed pool allocations are forbidden. "
					"Allocation failures cause a system crash"))
			_In_        POOL_TYPE               PoolType,
			_In_		PDEVICE_OBJECT			DeviceObject,
			_In_		PIRP StartupIrp
		);

		// Inherited via IAdapterCommon
		NTSTATUS __stdcall Init
		(
			_In_ IRP* StartupIrp, 
			_In_ PDEVICE_OBJECT DeviceObject
		);

		PDEVICE_OBJECT __stdcall GetDeviceObject(void);

		PDEVICE_OBJECT __stdcall AdapterCommon::GetPhysicalDeviceObject(void);

		NTSTATUS __stdcall WriteEtwEvent
		(
			_In_ EPcMiniportEngineEvent    miniportEventType,
			_In_ ULONGLONG      ullData1,
			_In_ ULONGLONG      ullData2,
			_In_ ULONGLONG      ullData3,
			_In_ ULONGLONG      ullData4
		);

		void __stdcall    SetEtwHelper
		(
			PPORTCLSETWHELPER _pPortClsEtwHelper
		);

		void __stdcall Cleanup();
};

