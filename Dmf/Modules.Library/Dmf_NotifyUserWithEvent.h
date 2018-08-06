/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_NotifyUserWithEvent.h

Abstract:

    Companion file to Dmf_NotifyUserWithEvent.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// The maximum number of events that the Client Driver can create.
// Increase this value if necessary.
// Or, create multiple instances of this object, each with
// this maximum number of events. 
//
#define NotifyUserWithEvent_MAXIMUM_EVENTS          4

// For Client Driver that just have a single event, allow them to use a simpler API.
//
#define NotifyUserWithEvent_DefaultIndex            0

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Used for index validation.
    //
    ULONG MaximumEventIndex;
    // The names of all events that are to be opened.
    //
    UNICODE_STRING EventNames[NotifyUserWithEvent_MAXIMUM_EVENTS];
} DMF_CONFIG_NotifyUserWithEvent;

// This macro declares the following functions:
// DMF_NotifyUserWithEvent_ATTRIBUTES_INIT()
// DMF_CONFIG_NotifyUserWithEvent_AND_ATTRIBUTES_INIT()
// DMF_NotifyUserWithEvent_Create()
//
DECLARE_DMF_MODULE(NotifyUserWithEvent)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Dmf_NotifyUserWithEvent_Notify(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Dmf_NotifyUserWithEvent_NotifyByIndex(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG EventIndex
    );

// eof: Dmf_NotifyUserWithEvent.h
//
