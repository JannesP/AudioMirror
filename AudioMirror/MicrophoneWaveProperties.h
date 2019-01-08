#pragma once

#include "Globals.h"

#include "EndpointMinipair.h"
#include "KsAudioProcessingAttribute.h"
#include "MiniportWaveRT.h"

//
// Mic in (external: headphone) range.
//
#define MICIN_DEVICE_MAX_CHANNELS           2       // Max Channels.
#define MICIN_MIN_BITS_PER_SAMPLE_PCM       16      // Min Bits Per Sample
#define MICIN_MAX_BITS_PER_SAMPLE_PCM       16      // Max Bits Per Sample
#define MICIN_MIN_SAMPLE_RATE               44100    // Min Sample Rate
#define MICIN_MAX_SAMPLE_RATE               44100   // Max Sample Rate

//
// Max # of pin instances.
//
#define MICIN_MAX_INPUT_STREAMS 1

//=============================================================================
static
KSDATAFORMAT_WAVEFORMATEXTENSIBLE MicInPinSupportedDeviceFormats[] =
{
	{ // 1
		{
			sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),
			0,
			0,
			0,
			STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
			STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
			STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
		},
		{
			{
				WAVE_FORMAT_EXTENSIBLE,
				2,
				44100,
				176400,
				4,
				16,
				sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
			},
			16,
			KSAUDIO_SPEAKER_STEREO,
			STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
		}
	}
};

//
// Supported modes (only on streaming pins).
//
static
MODE_AND_DEFAULT_FORMAT MicInPinSupportedDeviceModes[] =
{
	{
		STATIC_AUDIO_SIGNALPROCESSINGMODE_RAW,
		&MicInPinSupportedDeviceFormats[SIZEOF_ARRAY(MicInPinSupportedDeviceFormats) - 1].DataFormat,
	},
	{
		STATIC_AUDIO_SIGNALPROCESSINGMODE_DEFAULT,
		&MicInPinSupportedDeviceFormats[SIZEOF_ARRAY(MicInPinSupportedDeviceFormats) - 1].DataFormat,
	},
	{
		STATIC_AUDIO_SIGNALPROCESSINGMODE_SPEECH,
		&MicInPinSupportedDeviceFormats[SIZEOF_ARRAY(MicInPinSupportedDeviceFormats) - 1].DataFormat,
	},
	{
		STATIC_AUDIO_SIGNALPROCESSINGMODE_COMMUNICATIONS,
		&MicInPinSupportedDeviceFormats[SIZEOF_ARRAY(MicInPinSupportedDeviceFormats) - 1].DataFormat,
	}
};

//
// The entries here must follow the same order as the filter's pin
// descriptor array.
static PIN_DEVICE_FORMATS_AND_MODES MicInPinDeviceFormatsAndModes[] =
{
	{
		PinType::BridgePin,
		NULL,
		0,
		NULL,
		0
	},
	{
		PinType::SystemCapturePin,
		MicInPinSupportedDeviceFormats,
		SIZEOF_ARRAY(MicInPinSupportedDeviceFormats),
		MicInPinSupportedDeviceModes,
		SIZEOF_ARRAY(MicInPinSupportedDeviceModes)
	}
};

//=============================================================================
static
KSDATARANGE MicInPinDataRangesBridge[] =
{
	{
		sizeof(KSDATARANGE),
		0,
		0,
		0,
		STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
		STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
		STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
	}
};

static
PKSDATARANGE MicInPinDataRangePointersBridge[] =
{
	&MicInPinDataRangesBridge[0]
};

//=============================================================================
static
KSDATARANGE_AUDIO MicInPinDataRangesStream[] =
{
	{
		{
			sizeof(KSDATARANGE_AUDIO),
			KSDATARANGE_ATTRIBUTES,         // An attributes list follows this data range
			0,
			0,
			STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
			STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
			STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
		},
		MICIN_DEVICE_MAX_CHANNELS,
		MICIN_MIN_BITS_PER_SAMPLE_PCM,
		MICIN_MAX_BITS_PER_SAMPLE_PCM,
		MICIN_MIN_SAMPLE_RATE,
		MICIN_MAX_SAMPLE_RATE
	},
};

static
PKSDATARANGE MicInPinDataRangePointersStream[] =
{
	PKSDATARANGE(&MicInPinDataRangesStream[0]),
	PKSDATARANGE(&PinDataRangeAttributeList),
};

//=============================================================================
static
PCPIN_DESCRIPTOR MicInWaveMiniportPins[] =
{
	// Wave In Bridge Pin (Capture - From Topology) KSPIN_WAVE_BRIDGE
	{
		0,
		0,
		0,
		NULL,
		{
			0,
			NULL,
			0,
			NULL,
			SIZEOF_ARRAY(MicInPinDataRangePointersBridge),
			MicInPinDataRangePointersBridge,
			KSPIN_DATAFLOW_IN,
			KSPIN_COMMUNICATION_NONE,
			&KSNODETYPE_MICROPHONE,
			NULL,
			0
		}
	},

	// Wave In Streaming Pin (Capture) KSPIN_WAVEIN_HOST
	{
		MICIN_MAX_INPUT_STREAMS,
		MICIN_MAX_INPUT_STREAMS,
		0,
		NULL,
		{
			0,
			NULL,
			0,
			NULL,
			SIZEOF_ARRAY(MicInPinDataRangePointersStream),
			MicInPinDataRangePointersStream,
			KSPIN_DATAFLOW_OUT,
			KSPIN_COMMUNICATION_SINK,
			&KSNODETYPE_MICROPHONE,
			NULL,
			0
		}
	}
};

//=============================================================================
static
PCNODE_DESCRIPTOR MicInWaveMiniportNodes[] =
{
	// KSNODE_WAVE_ADC
	{
		0,                      // Flags
		NULL,                   // AutomationTable
		&KSNODETYPE_ADC,        // Type
		NULL                    // Name
	}
};

//=============================================================================
static
PCCONNECTION_DESCRIPTOR MicInWaveMiniportConnections[] =
{
	{ PCFILTER_NODE, (int)WaveCapturePins::KSPIN_WAVE_BRIDGE, PCFILTER_NODE, (int)WaveCapturePins::KSPIN_WAVEIN_HOST }
	/*{ PCFILTER_NODE,    (int)WaveCapturePins::KSPIN_WAVE_BRIDGE,    KSNODE_WAVE_ADC,     1 },
	{ KSNODE_WAVE_ADC,  0,											PCFILTER_NODE,       (int)WaveCapturePins::KSPIN_WAVEIN_HOST },*/
};

//=============================================================================
static
PCPROPERTY_ITEM PropertiesMicInWaveFilter[] =
{
	{
		&KSPROPSETID_Pin,
		KSPROPERTY_PIN_PROPOSEDATAFORMAT,
		KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
		MiniportWaveRT::PropertyHandler_WaveFilter
	},
	{
		&KSPROPSETID_Pin,
		KSPROPERTY_PIN_PROPOSEDATAFORMAT2,
		KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
		MiniportWaveRT::PropertyHandler_WaveFilter
	}
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationMicInWaveFilter, PropertiesMicInWaveFilter);

//=============================================================================
static
PCFILTER_DESCRIPTOR MicInWaveMiniportFilterDescriptor =
{
	0,                                          // Version
	&AutomationMicInWaveFilter,                 // AutomationTable
	sizeof(PCPIN_DESCRIPTOR),                   // PinSize
	SIZEOF_ARRAY(MicInWaveMiniportPins),        // PinCount
	MicInWaveMiniportPins,                      // Pins
	sizeof(PCNODE_DESCRIPTOR),                  // NodeSize
	0,       // NodeCount
	NULL,                     // Nodes
	SIZEOF_ARRAY(MicInWaveMiniportConnections), // ConnectionCount
	MicInWaveMiniportConnections,               // Connections
	0,                                          // CategoryCount
	NULL                                        // Categories  - use defaults (audio, render, capture)
};