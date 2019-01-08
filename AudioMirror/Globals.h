#pragma once

#include <initguid.h>

#include <portcls.h>
#include <stdunk.h>
#include <ksdebug.h>
#include <ntintsafe.h>
#include <wdf.h>
#include <wdfminiport.h>
#include <MsApoFxProxy.h>
#include <Ntstrsafe.h>

#include "Macros.h"
#include "NewDelete.h"

#define MINIADAPTER_POOLTAG         'aMmA'
#define DRIVER_POOLTAG				'pDmA'
#define WAVERT_STREAM_POOLTAG		'sWtR'
#define TOPOLOGY_POOLTAG			'iMoT'

#define MAX_DEVICE_REG_KEY_LENGTH	0x100
#define ALL_CHANNELS_ID				UINT32_MAX