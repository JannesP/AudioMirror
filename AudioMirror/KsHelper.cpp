#include "KsHelper.h"

#include "KsAudioProcessingAttribute.h"


KsHelper::KsHelper()
{
}


KsHelper::~KsHelper()
{
}

#pragma code_seg("PAGE")
NTSTATUS KsHelper::PropertyHandler_BasicSupport
(
	_In_ PPCPROPERTY_REQUEST         PropertyRequest,
	_In_ ULONG                       Flags,
	_In_ DWORD                       PropTypeSetId
)
/*++

Routine Description:

  Default basic support handler. Basic processing depends on the size of data.
  For ULONG it only returns Flags. For KSPROPERTY_DESCRIPTION, the structure
  is filled.

Arguments:

  PropertyRequest -

  Flags - Support flags.

  PropTypeSetId - PropTypeSetId

Return Value:

	NT status code.

--*/
{
	PAGED_CODE();

	ASSERT(Flags & KSPROPERTY_TYPE_BASICSUPPORT);

	NTSTATUS                    ntStatus = STATUS_INVALID_PARAMETER;

	if (PropertyRequest->ValueSize >= sizeof(KSPROPERTY_DESCRIPTION))
	{
		// if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
		//
		PKSPROPERTY_DESCRIPTION PropDesc =
			PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);

		PropDesc->AccessFlags = Flags;
		PropDesc->DescriptionSize = sizeof(KSPROPERTY_DESCRIPTION);
		if (VT_ILLEGAL != PropTypeSetId)
		{
			PropDesc->PropTypeSet.Set = KSPROPTYPESETID_General;
			PropDesc->PropTypeSet.Id = PropTypeSetId;
		}
		else
		{
			PropDesc->PropTypeSet.Set = GUID_NULL;
			PropDesc->PropTypeSet.Id = 0;
		}
		PropDesc->PropTypeSet.Flags = 0;
		PropDesc->MembersListCount = 0;
		PropDesc->Reserved = 0;

		PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
		ntStatus = STATUS_SUCCESS;
	}
	else if (PropertyRequest->ValueSize >= sizeof(ULONG))
	{
		// if return buffer can hold a ULONG, return the access flags
		//
		*(PULONG(PropertyRequest->Value)) = Flags;

		PropertyRequest->ValueSize = sizeof(ULONG);
		ntStatus = STATUS_SUCCESS;
	}
	else
	{
		PropertyRequest->ValueSize = 0;
		ntStatus = STATUS_BUFFER_TOO_SMALL;
	}

	return ntStatus;
} // PropertyHandler_BasicSupport

#pragma code_seg("PAGE")
NTSTATUS KsHelper::PropertyHandler_BasicSupportPeakMeter2(PPCPROPERTY_REQUEST PropertyRequest, ULONG MaxChannels)
{
	PAGED_CODE();

	NTSTATUS                    ntStatus = STATUS_SUCCESS;
	ULONG                       cbFullProperty =
		sizeof(KSPROPERTY_DESCRIPTION) +
		sizeof(KSPROPERTY_MEMBERSHEADER) +
		sizeof(KSPROPERTY_STEPPING_LONG) * MaxChannels;

	ASSERT(MaxChannels > 0);

	if (PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
	{
		PKSPROPERTY_DESCRIPTION PropDesc =
			PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);

		PropDesc->AccessFlags = KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT;
		PropDesc->DescriptionSize = cbFullProperty;
		PropDesc->PropTypeSet.Set = KSPROPTYPESETID_General;
		PropDesc->PropTypeSet.Id = VT_I4;
		PropDesc->PropTypeSet.Flags = 0;
		PropDesc->MembersListCount = 1;
		PropDesc->Reserved = 0;

		// if return buffer can also hold a range description, return it too
		if (PropertyRequest->ValueSize >= cbFullProperty)
		{
			// fill in the members header
			PKSPROPERTY_MEMBERSHEADER Members =
				PKSPROPERTY_MEMBERSHEADER(PropDesc + 1);

			Members->MembersFlags = KSPROPERTY_MEMBER_STEPPEDRANGES;
			Members->MembersSize = sizeof(KSPROPERTY_STEPPING_LONG);
			Members->MembersCount = MaxChannels;
			Members->Flags = KSPROPERTY_MEMBER_FLAG_BASICSUPPORT_MULTICHANNEL;

			// fill in the stepped range
			PKSPROPERTY_STEPPING_LONG Range =
				PKSPROPERTY_STEPPING_LONG(Members + 1);

			for (ULONG i = 0; i < MaxChannels; ++i)
			{
				Range[i].Bounds.SignedMaximum = PEAKMETER_SIGNED_MAXIMUM;
				Range[i].Bounds.SignedMinimum = PEAKMETER_SIGNED_MINIMUM;
				Range[i].SteppingDelta = PEAKMETER_STEPPING_DELTA;
				Range[i].Reserved = 0;
			}

			// set the return value size
			PropertyRequest->ValueSize = cbFullProperty;
		}
		else
		{
			PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
		}
	}
	else if (PropertyRequest->ValueSize >= sizeof(ULONG))
	{
		// if return buffer can hold a ULONG, return the access flags
		PULONG AccessFlags = PULONG(PropertyRequest->Value);

		PropertyRequest->ValueSize = sizeof(ULONG);
		*AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET;
	}
	else
	{
		PropertyRequest->ValueSize = 0;
		ntStatus = STATUS_BUFFER_TOO_SMALL;
	}

	return ntStatus;
}

#pragma code_seg("PAGE")
NTSTATUS KsHelper::GetAttributesFromAttributeList
(
	_In_ const KSMULTIPLE_ITEM *_pAttributes,
	_In_ size_t _Size,
	_Out_ GUID* _pSignalProcessingMode
)
/*++

Routine Description:

  Processes attributes list and return known attributes.

Arguments:

  _pAttributes - pointer to KSMULTIPLE_ITEM at head of an attributes list.

  _Size - count of bytes in the buffer pointed to by _pAttributes. The routine
	verifies sufficient buffer size while processing the attributes.

  _pSignalProcessingMode - returns the signal processing mode extracted from
	the attribute list, or AUDIO_SIGNALPROCESSINGMODE_DEFAULT if the attribute
	is not present in the list.

Return Value:

  NT status code.

Remarks

	This function is currently written for a single supported attribute
	(KSATTRIBUTEID_AUDIOSIGNALPROCESSING_MODE). As additional attributes are defined in the future,
	this function should be rewritten to be data driven through tables, etc.

--*/
{
	PAGED_CODE();

	//DPF_ENTER(("[CMiniportWaveRT::GetAttributesFromAttributeList]"));

	size_t cbRemaining = _Size;

	*_pSignalProcessingMode = AUDIO_SIGNALPROCESSINGMODE_DEFAULT;

	if (cbRemaining < sizeof(KSMULTIPLE_ITEM))
	{
		return STATUS_INVALID_PARAMETER;
	}
	cbRemaining -= sizeof(KSMULTIPLE_ITEM);

	//
	// Extract attributes.
	//
	PKSATTRIBUTE attributeHeader = (PKSATTRIBUTE)(_pAttributes + 1);

	for (ULONG i = 0; i < _pAttributes->Count; i++)
	{
		if (cbRemaining < sizeof(KSATTRIBUTE))
		{
			return STATUS_INVALID_PARAMETER;
		}

		if (attributeHeader->Attribute == KSATTRIBUTEID_AUDIOSIGNALPROCESSING_MODE)
		{
			KSATTRIBUTE_AUDIOSIGNALPROCESSING_MODE* signalProcessingModeAttribute;

			if (cbRemaining < sizeof(KSATTRIBUTE_AUDIOSIGNALPROCESSING_MODE))
			{
				return STATUS_INVALID_PARAMETER;
			}

			if (attributeHeader->Size != sizeof(KSATTRIBUTE_AUDIOSIGNALPROCESSING_MODE))
			{
				return STATUS_INVALID_PARAMETER;
			}

			signalProcessingModeAttribute = (KSATTRIBUTE_AUDIOSIGNALPROCESSING_MODE*)attributeHeader;

			// Return mode to caller.
			*_pSignalProcessingMode = signalProcessingModeAttribute->SignalProcessingMode;
		}
		else
		{
			return STATUS_NOT_SUPPORTED;
		}

		// Adjust pointer and buffer size to next attribute (QWORD aligned)
		ULONG cbAttribute = ((attributeHeader->Size + FILE_QUAD_ALIGNMENT) & ~FILE_QUAD_ALIGNMENT);

		attributeHeader = (PKSATTRIBUTE)(((PBYTE)attributeHeader) + cbAttribute);
		cbRemaining -= cbAttribute;
	}

	return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
NTSTATUS KsHelper::ValidatePropertyParams(PPCPROPERTY_REQUEST PropertyRequest, ULONG cbValueSize, ULONG cbInstanceSize)
{
	PAGED_CODE();

	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

	if (PropertyRequest && cbValueSize)
	{
		// If the caller is asking for ValueSize.
		//
		if (0 == PropertyRequest->ValueSize)
		{
			PropertyRequest->ValueSize = cbValueSize;
			ntStatus = STATUS_BUFFER_OVERFLOW;
		}
		// If the caller passed an invalid ValueSize.
		//
		else if (PropertyRequest->ValueSize < cbValueSize)
		{
			ntStatus = STATUS_BUFFER_TOO_SMALL;
		}
		else if (PropertyRequest->InstanceSize < cbInstanceSize)
		{
			ntStatus = STATUS_BUFFER_TOO_SMALL;
		}
		// If all parameters are OK.
		// 
		else if (PropertyRequest->ValueSize >= cbValueSize)
		{
			if (PropertyRequest->Value)
			{
				ntStatus = STATUS_SUCCESS;
				//
				// Caller should set ValueSize, if the property 
				// call is successful.
				//
			}
		}
	}
	else
	{
		ntStatus = STATUS_INVALID_PARAMETER;
	}

	// Clear the ValueSize if unsuccessful.
	//
	if (PropertyRequest &&
		STATUS_SUCCESS != ntStatus &&
		STATUS_BUFFER_OVERFLOW != ntStatus)
	{
		PropertyRequest->ValueSize = 0;
	}

	return ntStatus;
}

PWAVEFORMATEX KsHelper::GetWaveFormatEx
(
	_In_  PKSDATAFORMAT           pDataFormat
)
/*++

Routine Description:

  Returns the waveformatex for known formats.

Arguments:

  pDataFormat - data format.

Return Value:

	waveformatex in DataFormat.
	NULL for unknown data formats.

--*/
{
	PAGED_CODE();

	PWAVEFORMATEX           pWfx = NULL;

	// If this is a known dataformat extract the waveformat info.
	//
	if
		(
			pDataFormat &&
			(IsEqualGUIDAligned(pDataFormat->MajorFormat,
				KSDATAFORMAT_TYPE_AUDIO) &&
				(IsEqualGUIDAligned(pDataFormat->Specifier,
					KSDATAFORMAT_SPECIFIER_WAVEFORMATEX) ||
					IsEqualGUIDAligned(pDataFormat->Specifier,
						KSDATAFORMAT_SPECIFIER_DSOUND)))
			)
	{
		pWfx = PWAVEFORMATEX(pDataFormat + 1);

		if (IsEqualGUIDAligned(pDataFormat->Specifier,
			KSDATAFORMAT_SPECIFIER_DSOUND))
		{
			PKSDSOUND_BUFFERDESC    pwfxds;

			pwfxds = PKSDSOUND_BUFFERDESC(pDataFormat + 1);
			pWfx = &pwfxds->WaveFormatEx;
		}
	}

	return pWfx;
} // GetWaveFormatEx

#pragma code_seg("PAGE")
NTSTATUS KsHelper::PropertyHandler_BasicSupportMute
(
	_In_  PPCPROPERTY_REQUEST   PropertyRequest,
	_In_  ULONG                 MaxChannels
)
/*++

Routine Description:

  Handles BasicSupport for Mute nodes.

Arguments:

  PropertyRequest - property request structure.

  MaxChannels - # of supported channels.

Return Value:

  NT status code.

--*/
{
	PAGED_CODE();

	NTSTATUS                    ntStatus = STATUS_SUCCESS;
	ULONG                       cbFullProperty =
		sizeof(KSPROPERTY_DESCRIPTION) +
		sizeof(KSPROPERTY_MEMBERSHEADER) +
		sizeof(KSPROPERTY_STEPPING_LONG) * MaxChannels;

	ASSERT(MaxChannels > 0);

	if (PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
	{
		PKSPROPERTY_DESCRIPTION PropDesc =
			PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);

		PropDesc->AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET;
		PropDesc->DescriptionSize = cbFullProperty;
		PropDesc->PropTypeSet.Set = KSPROPTYPESETID_General;
		PropDesc->PropTypeSet.Id = VT_BOOL;
		PropDesc->PropTypeSet.Flags = 0;
		PropDesc->MembersListCount = 1;
		PropDesc->Reserved = 0;

		// if return buffer can also hold a range description, return it too
		if (PropertyRequest->ValueSize >= cbFullProperty)
		{
			// fill in the members header
			PKSPROPERTY_MEMBERSHEADER Members =
				PKSPROPERTY_MEMBERSHEADER(PropDesc + 1);

			Members->MembersFlags = KSPROPERTY_MEMBER_STEPPEDRANGES;
			Members->MembersSize = sizeof(KSPROPERTY_STEPPING_LONG);
			Members->MembersCount = MaxChannels;
			Members->Flags = KSPROPERTY_MEMBER_FLAG_BASICSUPPORT_MULTICHANNEL;

			// fill in the stepped range
			PKSPROPERTY_STEPPING_LONG Range =
				PKSPROPERTY_STEPPING_LONG(Members + 1);

			for (ULONG i = 0; i < MaxChannels; ++i)
			{
				Range[i].Bounds.SignedMaximum = 1;        // true
				Range[i].Bounds.SignedMinimum = 0;        // false
				Range[i].SteppingDelta = 1;        // false <- -> true
				Range[i].Reserved = 0;
			}

			// set the return value size
			PropertyRequest->ValueSize = cbFullProperty;
		}
		else
		{
			PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
		}
	}
	else if (PropertyRequest->ValueSize >= sizeof(ULONG))
	{
		// if return buffer can hold a ULONG, return the access flags
		PULONG AccessFlags = PULONG(PropertyRequest->Value);

		PropertyRequest->ValueSize = sizeof(ULONG);
		*AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET;
	}
	else
	{
		PropertyRequest->ValueSize = 0;
		ntStatus = STATUS_BUFFER_TOO_SMALL;
	}

	return ntStatus;
} // PropertyHandlerBasicSupportVolume

#pragma code_seg("PAGE")
NTSTATUS KsHelper::PropertyHandler_BasicSupportVolume
(
	_In_  PPCPROPERTY_REQUEST   PropertyRequest,
	_In_  ULONG                 MaxChannels
)
/*++

Routine Description:

  Handles BasicSupport for Volume nodes.

Arguments:

  PropertyRequest - property request structure.

  MaxChannels - # of supported channels.

Return Value:

  NT status code.

--*/
{
	PAGED_CODE();

	NTSTATUS                    ntStatus = STATUS_SUCCESS;
	ULONG                       cbFullProperty =
		sizeof(KSPROPERTY_DESCRIPTION) +
		sizeof(KSPROPERTY_MEMBERSHEADER) +
		sizeof(KSPROPERTY_STEPPING_LONG) * MaxChannels;

	ASSERT(MaxChannels > 0);

	if (PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
	{
		PKSPROPERTY_DESCRIPTION PropDesc =
			PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);

		PropDesc->AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET;
		PropDesc->DescriptionSize = cbFullProperty;
		PropDesc->PropTypeSet.Set = KSPROPTYPESETID_General;
		PropDesc->PropTypeSet.Id = VT_I4;
		PropDesc->PropTypeSet.Flags = 0;
		PropDesc->MembersListCount = 1;
		PropDesc->Reserved = 0;

		// if return buffer can also hold a range description, return it too
		if (PropertyRequest->ValueSize >= cbFullProperty)
		{
			// fill in the members header
			PKSPROPERTY_MEMBERSHEADER Members =
				PKSPROPERTY_MEMBERSHEADER(PropDesc + 1);

			Members->MembersFlags = KSPROPERTY_MEMBER_STEPPEDRANGES;
			Members->MembersSize = sizeof(KSPROPERTY_STEPPING_LONG);
			Members->MembersCount = MaxChannels;
			Members->Flags = KSPROPERTY_MEMBER_FLAG_BASICSUPPORT_MULTICHANNEL;

			// fill in the stepped range
			PKSPROPERTY_STEPPING_LONG Range =
				PKSPROPERTY_STEPPING_LONG(Members + 1);

			for (ULONG i = 0; i < MaxChannels; ++i)
			{
				Range[i].Bounds.SignedMaximum = VOLUME_SIGNED_MAXIMUM;   //   0 dB
				Range[i].Bounds.SignedMinimum = VOLUME_SIGNED_MINIMUM;   // -96 dB
				Range[i].SteppingDelta = VOLUME_STEPPING_DELTA;   //  .5 dB
				Range[i].Reserved = 0;
			}

			// set the return value size
			PropertyRequest->ValueSize = cbFullProperty;
		}
		else
		{
			PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
		}
	}
	else if (PropertyRequest->ValueSize >= sizeof(ULONG))
	{
		// if return buffer can hold a ULONG, return the access flags
		PULONG AccessFlags = PULONG(PropertyRequest->Value);

		PropertyRequest->ValueSize = sizeof(ULONG);
		*AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET;
	}
	else
	{
		PropertyRequest->ValueSize = 0;
		ntStatus = STATUS_BUFFER_TOO_SMALL;
	}

	return ntStatus;
} // PropertyHandlerBasicSupportVolume