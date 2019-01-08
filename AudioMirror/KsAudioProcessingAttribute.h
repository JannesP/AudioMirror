#pragma once

#include "Globals.h"

// Default volume settings.
#define VOLUME_STEPPING_DELTA       0x8000
#define VOLUME_SIGNED_MAXIMUM       0x00000000
#define VOLUME_SIGNED_MINIMUM       (-96 * 0x10000)

// Default peak meter settings
#define PEAKMETER_STEPPING_DELTA    0x1000
#define PEAKMETER_SIGNED_MAXIMUM    LONG_MAX
#define PEAKMETER_SIGNED_MINIMUM    LONG_MIN

#define MIN_BUFFER_DURATION_MS 10
#define MAX_BUFFER_DURATION_MS 20
#define MS_PER_SEC 1000

#define MAX_OUTPUT_LOOPBACK_STREAMS 1

// Wave pins.
enum class WaveRenderPins
{
	SINK_SYSTEM = 0,
	SOURCE
	
};

// Wave pins
enum class WaveCapturePins
{
	KSPIN_WAVE_BRIDGE = 0,
	KSPIN_WAVEIN_HOST
};

// Wave Topology nodes.
enum
{
	KSNODE_WAVE_ADC = 0
};

enum class TopologyPin
{
	Microphone = 0,
	Bridge
};

// Wave Topology nodes - offloading supported.
enum class WaveSpeakerNodes
{
	DAC = 0
};

static
KSATTRIBUTE PinDataRangeSignalProcessingModeAttribute =
{
	sizeof(KSATTRIBUTE),
	0,
	STATICGUIDOF(KSATTRIBUTEID_AUDIOSIGNALPROCESSING_MODE),
};

static
PKSATTRIBUTE PinDataRangeAttributes[] =
{
	&PinDataRangeSignalProcessingModeAttribute,
};

static
KSATTRIBUTE_LIST PinDataRangeAttributeList =
{
	ARRAYSIZE(PinDataRangeAttributes),
	PinDataRangeAttributes,
};