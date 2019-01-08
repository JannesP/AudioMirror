#pragma once
#include "Globals.h"
#include "MiniportTopology.h"
#include "MicrophoneWaveProperties.h"

static KSDATARANGE MicInTopoPinDataRangesBridge[] =
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

static PKSDATARANGE MicInTopoPinDataRangePointersBridge[] =
{
	&MicInTopoPinDataRangesBridge[0]
};

static PCPIN_DESCRIPTOR MicInTopoMiniportPins[] =
{
	// KSPIN - topology filter in-pin
	{
	  0,
	  0,
	  0,                                                  // InstanceCount
	  NULL,                                               // AutomationTable
	  {                                                   // KsPinDescriptor
		0,                                                // InterfacesCount
		NULL,                                             // Interfaces
		0,                                                // MediumsCount
		NULL,                                             // Mediums
		SIZEOF_ARRAY(MicInTopoPinDataRangePointersBridge),// DataRangesCount
		MicInTopoPinDataRangePointersBridge,              // DataRanges
		KSPIN_DATAFLOW_IN,                                // DataFlow
		KSPIN_COMMUNICATION_NONE,                         // Communication
		&KSNODETYPE_MICROPHONE,                           // Category
		NULL,											  // Name
		0                                                 // Reserved
	  }
	},

	// KSPIN - topology filter out-in
	{
	  0,
	  0,
	  0,                                                  // InstanceCount
	  NULL,                                               // AutomationTable
	  {                                                   // KsPinDescriptor
		0,                                                // InterfacesCount
		NULL,                                             // Interfaces
		0,                                                // MediumsCount
		NULL,                                             // Mediums
		SIZEOF_ARRAY(MicInTopoPinDataRangePointersBridge),// DataRangesCount
		MicInTopoPinDataRangePointersBridge,              // DataRanges
		KSPIN_DATAFLOW_OUT,                               // DataFlow
		KSPIN_COMMUNICATION_NONE,                         // Communication
		&KSCATEGORY_AUDIO,                                // Category
		NULL,                                             // Name
		0                                                 // Reserved
	  }
	}
};

static PCCONNECTION_DESCRIPTOR MicInMiniportConnections[] =
{
	//  FromNode,			FromPin,						ToNode,				ToPin
	{   PCFILTER_NODE,		(int)TopologyPin::Microphone,	PCFILTER_NODE,		(int)TopologyPin::Bridge }
};

static PHYSICALCONNECTIONTABLE MicInTopologyPhysicalConnections[] =
{
	{
		(int)TopologyPin::Bridge,          // TopologyOut
		(int)WaveCapturePins::KSPIN_WAVE_BRIDGE,          // WaveIn
		PhysicalConnectionOrientation::TopologyToWave
	}
};

static PCFILTER_DESCRIPTOR MicInTopoMiniportFilterDescriptor =
{
  0,                                        // Version
  NULL,               // AutomationTable
  sizeof(PCPIN_DESCRIPTOR),                 // PinSize
  SIZEOF_ARRAY(MicInTopoMiniportPins),      // PinCount
  MicInTopoMiniportPins,                    // Pins
  sizeof(PCNODE_DESCRIPTOR),                // NodeSize
  0,										// NodeCount
  NULL,										// Nodes
  SIZEOF_ARRAY(MicInMiniportConnections),   // ConnectionCount
  MicInMiniportConnections,                 // Connections
  0,                                        // CategoryCount
  NULL                                      // Categories
};