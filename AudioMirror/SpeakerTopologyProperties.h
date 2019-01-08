#pragma once

#include "Globals.h"
#include "MiniportTopology.h"

enum class TopologyRenderPins
{
	WAVEOUT_SINK = 0,
	LINEOUT_DEST,
};

static
PCCONNECTION_DESCRIPTOR SpeakerTopoMiniportConnections[] =
{
	//  FromNode,       FromPin,									ToNode,         ToPin
	{   PCFILTER_NODE,	(int)TopologyRenderPins::WAVEOUT_SINK,    PCFILTER_NODE,  (int)TopologyRenderPins::LINEOUT_DEST}
};

static
KSDATARANGE SpeakerTopoPinDataRangesBridge[] =
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
PKSDATARANGE SpeakerTopoPinDataRangePointersBridge[] =
{
  &SpeakerTopoPinDataRangesBridge[0]
};

static
PCPIN_DESCRIPTOR SpeakerTopoMiniportPins[] =
{
	// WAVEOUT_SINK
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
		SIZEOF_ARRAY(SpeakerTopoPinDataRangePointersBridge),// DataRangesCount
		SpeakerTopoPinDataRangePointersBridge,            // DataRanges
		KSPIN_DATAFLOW_IN,                                // DataFlow
		KSPIN_COMMUNICATION_NONE,                         // Communication
		&KSCATEGORY_AUDIO,                                // Category
		NULL,                                             // Name
		0                                                 // Reserved
	  }
	},
	// KSPIN_TOPO_LINEOUT_DEST
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
		SIZEOF_ARRAY(SpeakerTopoPinDataRangePointersBridge),// DataRangesCount
		SpeakerTopoPinDataRangePointersBridge,            // DataRanges
		KSPIN_DATAFLOW_OUT,                               // DataFlow
		KSPIN_COMMUNICATION_NONE,                         // Communication
		&KSNODETYPE_SPEAKER,                              // Category
		NULL,                                             // Name
		0                                                 // Reserved
	  }
	}
};

static PHYSICALCONNECTIONTABLE SpeakerTopologyPhysicalConnections[] =
{
	{
		(int)TopologyPin::Microphone,  // TopologyIn
		(int)WaveRenderPins::SOURCE,   // WaveOut
		PhysicalConnectionOrientation::WaveToTopology
	}
};

static PCFILTER_DESCRIPTOR SpeakerTopoMiniportFilterDescriptor =
{
  0,                                            // Version
  NULL,                 // AutomationTable
  sizeof(PCPIN_DESCRIPTOR),                     // PinSize
  SIZEOF_ARRAY(SpeakerTopoMiniportPins),        // PinCount
  SpeakerTopoMiniportPins,                      // Pins
  sizeof(PCNODE_DESCRIPTOR),                    // NodeSize
  0,                                            // NodeCount
  NULL,                                         // Nodes
  SIZEOF_ARRAY(SpeakerTopoMiniportConnections), // ConnectionCount
  SpeakerTopoMiniportConnections,               // Connections
  0,                                            // CategoryCount
  NULL                                          // Categories
};