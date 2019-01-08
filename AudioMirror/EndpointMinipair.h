#pragma once
#include "Globals.h"

#define ENDPOINT_FLAG_NONE							0x00000000
#define ENDPOINT_FLAG_OFFLOAD_SUPPORTED             0x00000001
#define ENDPOINT_FLAG_LOOPBACK_SUPPORTED            0x00000002

// forward declaration.
typedef struct _ENDPOINT_MINIPAIR *PENDPOINT_MINIPAIR;
struct AUDIOMODULE_DESCRIPTOR;

// both wave & topology miniport create function prototypes have this form:
typedef HRESULT(*PFNCREATEMINIPORT)(
	_Out_           PUNKNOWN                              * Unknown,
	_In_            REFCLSID,
	_In_opt_        PUNKNOWN                                UnknownOuter,
	_When_((PoolType & NonPagedPoolMustSucceed) != 0,
		__drv_reportError("Must succeed pool allocations are forbidden. "
			"Allocation failures cause a system crash"))
	_In_            POOL_TYPE                               PoolType,
	_In_            PUNKNOWN                                UnknownAdapter,
	_In_opt_        PVOID                                   DeviceContext,
	_In_            PENDPOINT_MINIPAIR                      MiniportPair
	);

typedef struct _SYSVAD_DEVPROPERTY {
	const DEVPROPKEY   *PropertyKey;
	DEVPROPTYPE Type;
	ULONG BufferSize;
	__field_bcount_opt(BufferSize) PVOID Buffer;
} SYSVAD_DEVPROPERTY, PSYSVAD_DEVPROPERTY;

//
// Signal processing modes and default formats structs.
//
typedef struct _MODE_AND_DEFAULT_FORMAT {
	GUID            Mode;
	KSDATAFORMAT*   DefaultFormat;
} MODE_AND_DEFAULT_FORMAT, *PMODE_AND_DEFAULT_FORMAT;

//
// Enumeration of the various types of pins implemented in this driver.
//
enum class PinType
{
	NoPin,
	BridgePin,
	SystemRenderPin,
	OffloadRenderPin,
	RenderLoopbackPin,
	SystemCapturePin,
};

//
// PIN_DEVICE_FORMATS_AND_MODES
//
//  Used to specify a pin's type (e.g. system, offload, etc.), formats, and
//  modes. Conceptually serves similar purpose as the PCPIN_DESCRIPTOR to
//  define a pin, but is more specific to driver implementation.
//
//  Arrays of these structures follow the same order as the filter's
//  pin descriptor array so that KS pin IDs can serve as an index.
//
typedef struct _PIN_DEVICE_FORMATS_AND_MODES
{
	PinType                             PinType;

	KSDATAFORMAT_WAVEFORMATEXTENSIBLE * WaveFormats;
	ULONG                               WaveFormatsCount;

	MODE_AND_DEFAULT_FORMAT *           ModeAndDefaultFormat;
	ULONG                               ModeAndDefaultFormatCount;

} PIN_DEVICE_FORMATS_AND_MODES, *PPIN_DEVICE_FORMATS_AND_MODES;

enum class DeviceType
{
	CaptureDevice = 0,
	RenderDevice,
};

// Flags to identify stream processing mode
enum class PhysicalConnectionOrientation {
	TopologyToWave = 0,
	WaveToTopology = 1
};

// Connection table for registering topology/wave bridge connection
typedef struct _PHYSICALCONNECTIONTABLE
{
	ULONG            ulTopology;
	ULONG            ulWave;
	PhysicalConnectionOrientation   eType;
} PHYSICALCONNECTIONTABLE, *PPHYSICALCONNECTIONTABLE;

//
// Module callbacks.
//
typedef
NTSTATUS
(*FN_AUDIOMODULE_INIT_CLASS)
(
	_In_		AUDIOMODULE_DESCRIPTOR* Module,
	_Inout_opt_ PVOID           Context,
	_In_        size_t          Size,
	_In_        KSAUDIOMODULE_NOTIFICATION* NotificationHeader,
	_In_opt_    PPORTCLSNOTIFICATIONS PortNotifications
	);

typedef
NTSTATUS
(*FN_AUDIOMODULE_INIT_INSTANCE)
(
	_In_  const AUDIOMODULE_DESCRIPTOR * Module,
	_In_opt_    PVOID           TemplateContext,
	_Inout_opt_ PVOID           Context,
	_In_        size_t          Size,
	_In_        ULONG           InstanceId
	);

typedef VOID(*FN_AUDIOMODULE_CLEANUP)(
	_In_        PVOID           Context
	);

typedef NTSTATUS(*FN_AUDIOMODULE_HANDLER)(
	_Inout_opt_                          PVOID   Context,
	_In_reads_bytes_(InBufferCb)         PVOID   InBuffer,
	_In_                                 ULONG   InBufferCb,
	_Out_writes_bytes_opt_(*OutBufferCb) PVOID   OutBuffer,
	_Inout_                              ULONG * OutBufferCb
	);

//
// Module description.
//
struct AUDIOMODULE_DESCRIPTOR
{
	const GUID *                    ClassId;
	const GUID *                    ProcessingMode;
	WCHAR                           Name[AUDIOMODULE_MAX_NAME_CCH_SIZE];
	ULONG                           InstanceId;
	ULONG                           VersionMajor;
	ULONG                           VersionMinor;
	ULONG                           Flags;
	size_t                          ContextSize;
	FN_AUDIOMODULE_INIT_CLASS       InitClass;
	FN_AUDIOMODULE_INIT_INSTANCE    InitInstance;
	FN_AUDIOMODULE_CLEANUP          Cleanup;
	FN_AUDIOMODULE_HANDLER          Handler;
};

//
// Used to track run-time audio module changes.
//
struct AUDIOMODULE
{
	const AUDIOMODULE_DESCRIPTOR *  Descriptor;
	PVOID                           Context;
	ULONG                           InstanceId;
	ULONG                           NextCfgInstanceId;  // used by filter modules
	BOOL                            Enabled;
};

//
// Endpoint miniport pair (wave/topology) descriptor.
//
typedef struct _ENDPOINT_MINIPAIR
{
	DeviceType                     DeviceType;

	// Topology miniport.
	PWSTR                           TopoName;               // make sure this or the template name matches with SYSVAD.<TopoName>.szPname in the inf's [Strings] section
	PWSTR                           TemplateTopoName;       // optional template name
	PFNCREATEMINIPORT               TopoCreateCallback;
	PCFILTER_DESCRIPTOR*            TopoDescriptor;
	ULONG                           TopoInterfacePropertyCount;
	const SYSVAD_DEVPROPERTY*       TopoInterfaceProperties;

	// Wave RT miniport.
	PWSTR                           WaveName;               // make sure this or the template name matches with SYSVAD.<WaveName>.szPname in the inf's [Strings] section
	PWSTR                           TemplateWaveName;       // optional template name
	PFNCREATEMINIPORT               WaveCreateCallback;
	PCFILTER_DESCRIPTOR*            WaveDescriptor;
	ULONG                           WaveInterfacePropertyCount;
	const SYSVAD_DEVPROPERTY*       WaveInterfaceProperties;

	USHORT                          DeviceMaxChannels;
	PIN_DEVICE_FORMATS_AND_MODES*   PinDeviceFormatsAndModes;
	ULONG                           PinDeviceFormatsAndModesCount;

	// Miniport physical connections.
	PHYSICALCONNECTIONTABLE*        PhysicalConnections;
	ULONG                           PhysicalConnectionCount;

	// General endpoint flags (one of more ENDPOINT_<flag-type>, see above)
	ULONG                           DeviceFlags;

	// Static module list description.
	AUDIOMODULE_DESCRIPTOR *        ModuleList;
	ULONG                           ModuleListCount;
	const GUID *                    ModuleNotificationDeviceId;
} ENDPOINT_MINIPAIR;