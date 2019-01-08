#pragma once

#include "Globals.h"

#define SPEAKER_DEVICE_MAX_CHANNELS               2       // Max Channels.

#define SPEAKER_MAX_INPUT_SYSTEM_STREAMS            1
#define SPEAKER_MAX_INPUT_OFFLOAD_STREAMS           0
#define SPEAKER_MAX_OUTPUT_LOOPBACK_STREAMS         MAX_OUTPUT_LOOPBACK_STREAMS

#define SPEAKER_HOST_MAX_CHANNELS                   2       // Max Channels.
#define SPEAKER_HOST_MIN_BITS_PER_SAMPLE            16      // Min Bits Per Sample
#define SPEAKER_HOST_MAX_BITS_PER_SAMPLE            16      // Max Bits Per Sample
#define SPEAKER_HOST_MIN_SAMPLE_RATE                44100   // Min Sample Rate
#define SPEAKER_HOST_MAX_SAMPLE_RATE                44100   // Max Sample Rate

#define SPEAKER_OFFLOAD_MAX_CHANNELS                2       // Max Channels.
#define SPEAKER_OFFLOAD_MIN_BITS_PER_SAMPLE         16      // Min Bits Per Sample
#define SPEAKER_OFFLOAD_MAX_BITS_PER_SAMPLE         16      // Max Bits Per Sample
#define SPEAKER_OFFLOAD_MIN_SAMPLE_RATE             44100   // Min Sample Rate
#define SPEAKER_OFFLOAD_MAX_SAMPLE_RATE             44100   // Max Sample Rate

static
KSDATAFORMAT_WAVEFORMATEXTENSIBLE SpeakerHostPinSupportedDeviceFormats[] =
{
	{
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
	},
};

static
MODE_AND_DEFAULT_FORMAT SpeakerHostPinSupportedDeviceModes[] =
{
	{
		STATIC_AUDIO_SIGNALPROCESSINGMODE_DEFAULT,
		&SpeakerHostPinSupportedDeviceFormats[0].DataFormat // 48KHz
	},
};

static PIN_DEVICE_FORMATS_AND_MODES SpeakerPinDeviceFormatsAndModes[] =
{
	{
		PinType::SystemRenderPin,
		SpeakerHostPinSupportedDeviceFormats,
		SIZEOF_ARRAY(SpeakerHostPinSupportedDeviceFormats),
		SpeakerHostPinSupportedDeviceModes,
		SIZEOF_ARRAY(SpeakerHostPinSupportedDeviceModes)
	},
	{
		PinType::BridgePin,
		NULL,
		0,
		NULL,
		0
	},
};

static KSDATARANGE_AUDIO SpeakerPinDataRangesStream[] =
{
	{ // 0
		{
			sizeof(KSDATARANGE_AUDIO),
			KSDATARANGE_ATTRIBUTES,         // An attributes list follows this data range
			0,
			0,
			STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
			STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
			STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
		},
		SPEAKER_HOST_MAX_CHANNELS,
		SPEAKER_HOST_MIN_BITS_PER_SAMPLE,
		SPEAKER_HOST_MAX_BITS_PER_SAMPLE,
		SPEAKER_HOST_MIN_SAMPLE_RATE,
		SPEAKER_HOST_MAX_SAMPLE_RATE
	},
};

static
PKSDATARANGE SpeakerPinDataRangePointersStream[] =
{
	PKSDATARANGE(&SpeakerPinDataRangesStream[0]),
	PKSDATARANGE(&PinDataRangeAttributeList),
};

static
KSDATARANGE SpeakerPinDataRangesBridge[] =
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
PKSDATARANGE SpeakerPinDataRangePointersBridge[] =
{
	&SpeakerPinDataRangesBridge[0]
};

static PCPIN_DESCRIPTOR SpeakerWaveMiniportPins[] =
{
	// Wave Out Streaming Pin (Renderer) KSPIN_WAVE_RENDER_SINK_SYSTEM
	{
		SPEAKER_MAX_INPUT_SYSTEM_STREAMS,
		SPEAKER_MAX_INPUT_SYSTEM_STREAMS,
		0,
		NULL,        // AutomationTable
		{
			0,
			NULL,
			0,
			NULL,
			SIZEOF_ARRAY(SpeakerPinDataRangePointersStream),
			SpeakerPinDataRangePointersStream,
			KSPIN_DATAFLOW_IN,
			KSPIN_COMMUNICATION_SINK,
			&KSNODETYPE_SPEAKER,
			NULL,
			0
		}
	},
	// Wave Out Bridge Pin (Renderer) KSPIN_WAVE_RENDER_SOURCE
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
			SIZEOF_ARRAY(SpeakerPinDataRangePointersBridge),
			SpeakerPinDataRangePointersBridge,
			KSPIN_DATAFLOW_OUT,
			KSPIN_COMMUNICATION_NONE,
			&KSNODETYPE_SPEAKER,
			NULL,
			0
		}
	},
};

static PCPROPERTY_ITEM PropertiesSpeakerWaveFilter[] =
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
	},
};
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationSpeakerWaveFilter, PropertiesSpeakerWaveFilter);

//=============================================================================
//
//                   ----------------------------      
//                   |                          |      
//  System Pin   0-->|                          |
//                   |      				    |      
//  |											|--> 1 KSPIN_WAVE_RENDER_SOURCE
//                   |                          |      
//                   ----------------------------       
static PCCONNECTION_DESCRIPTOR SpeakerWaveMiniportConnections[] =
{
	{ PCFILTER_NODE,						(int)WaveRenderPins::SINK_SYSTEM,   PCFILTER_NODE,						(int)WaveRenderPins::SOURCE },
};

static PCFILTER_DESCRIPTOR SpeakerWaveMiniportFilterDescriptor =
{
	0,                                              // Version
	&AutomationSpeakerWaveFilter,                   // AutomationTable
	sizeof(PCPIN_DESCRIPTOR),                       // PinSize
	SIZEOF_ARRAY(SpeakerWaveMiniportPins),          // PinCount
	SpeakerWaveMiniportPins,                        // Pins
	sizeof(PCNODE_DESCRIPTOR),                      // NodeSize
	0,												// NodeCount
	NULL,											// Nodes
	SIZEOF_ARRAY(SpeakerWaveMiniportConnections),   // ConnectionCount
	SpeakerWaveMiniportConnections,                 // Connections
	0,                                              // CategoryCount
	NULL                                            // Categories  - use defaults (audio, render, capture)
};