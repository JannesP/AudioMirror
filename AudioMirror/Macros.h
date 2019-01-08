#pragma once
#include "Globals.h"

#pragma warning (disable : 4127)

#define STR_MODULENAME              "AudioMirror: "

// Debug utility macros
#define D_FUNC                      4
#define D_BLAB                      DEBUGLVL_BLAB
#define D_VERBOSE                   DEBUGLVL_VERBOSE        
#define D_TERSE                     DEBUGLVL_TERSE          
#define D_ERROR                     DEBUGLVL_TERSE          
#define DPF                         _DbgPrintF
#define DPF_ENTER(x)                DPF(D_FUNC, x)

//-------------------------------------------------------------------------
// Description:
//
// If the ntStatus is not NT_SUCCESS, perform the given statement then jump to
// the given label.
//
// Parameters:
//
//      ntStatus - [in] Value to check
//      action - [in] action to perform in body of if statement
//
#define IF_FAILED_ACTION_RETURN(ntStatus, action)        \
        if (!NT_SUCCESS(ntStatus))                              \
        {                                                       \
            action;                                             \
            return ntStatus;                                    \
        }

//-------------------------------------------------------------------------
// Description:
//
// If the ntStatus is not NT_SUCCESS, perform the given statement then jump to
// the given label.
//
// Parameters:
//
//      ntStatus - [in] Value to check
//      action - [in] action to perform in body of if statement
//
#define IF_FAILED_LOG_RETURN(ntStatus, message, arg)			\
        if (!NT_SUCCESS(ntStatus))                              \
        {                                                       \
            _DbgPrintF(DEBUGLVL_ERROR, (message, arg));         \
            return ntStatus;                                    \
        }

//-------------------------------------------------------------------------
// Description:
//
// If the ntStatus passed is not NT_SUCCESS, jump to the given label.
//
// Parameters:
//
//      ntStatus - [in] Value to check
//      label - [in] label to jump if condition is met
//
#define IF_FAILED_JUMP(ntStatus, label)                         \
    if (!NT_SUCCESS(ntStatus))                                   \
    {                                                           \
        goto label;                                             \
    }

//-------------------------------------------------------------------------
// Description:
//
// If the condition evaluates to TRUE, perform the given statement
// then jump to the given label.
//
// Parameters:
//
//      condition - [in] Code that fits in if statement
//      action - [in] action to perform in body of if statement
//      label - [in] label to jump if condition is met
//
#define IF_TRUE_ACTION_JUMP(condition, action, label)           \
    if (condition)                                               \
    {                                                           \
        action;                                                 \
        goto label;                                             \
    }

//-------------------------------------------------------------------------
// Description:
//
// If the ntStatus is not NT_SUCCESS, perform the given statement then jump to
// the given label.
//
// Parameters:
//
//      ntStatus - [in] Value to check
//      action - [in] action to perform in body of if statement
//      label - [in] label to jump if condition is met
//
#define IF_FAILED_ACTION_JUMP(ntStatus, action, label)          \
        if (!NT_SUCCESS(ntStatus))                              \
        {                                                       \
            action;                                             \
            goto label;                                         \
        }

#define SAFE_RELEASE(p) {if (p) { (p)->Release(); (p) = nullptr; } }

#define VALUE_NORMALIZE_P(v, step) \
    ((((v) + (step)/2) / (step)) * (step))

#define VALUE_NORMALIZE(v, step) \
    ((v) > 0 ? VALUE_NORMALIZE_P((v), (step)) : -(VALUE_NORMALIZE_P(-(v), (step))))

#define VALUE_NORMALIZE_IN_RANGE_EX(v, min, max, step) \
    ((v) > (max) ? (max) : \
     (v) < (min) ? (min) : \
     VALUE_NORMALIZE((v), (step)))

// to normalize volume values.
#define VOLUME_NORMALIZE_IN_RANGE(v) \
    VALUE_NORMALIZE_IN_RANGE_EX((v), VOLUME_SIGNED_MINIMUM, VOLUME_SIGNED_MAXIMUM, VOLUME_STEPPING_DELTA)

// to normalize sample peak meter.
#define PEAKMETER_NORMALIZE_IN_RANGE(v) \
    VALUE_NORMALIZE_IN_RANGE_EX((v), PEAKMETER_SIGNED_MINIMUM, PEAKMETER_SIGNED_MAXIMUM, PEAKMETER_STEPPING_DELTA)