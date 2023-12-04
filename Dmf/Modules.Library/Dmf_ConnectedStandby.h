/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_ConnectedStandby.h

Abstract:

    Companion file to Dmf_ConnectedStandby.c.

Environment:

    Kernel-mode Driver Framework

--*/

#pragma once

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_ConnectedStandbyStateChangedCallback(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN SystemInConnectedStandby
    );

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Client's callback when Connected Standby state changes.
    //
    EVT_DMF_ConnectedStandbyStateChangedCallback* ConnectedStandbyStateChangedCallback;
} DMF_CONFIG_ConnectedStandby;


// This macro declares the following functions:
// DMF_ConnectedStandby_ATTRIBUTES_INIT()
// DMF_CONFIG_ConnectedStandby_AND_ATTRIBUTES_INIT()
// DMF_ConnectedStandby_Create()
//
DECLARE_DMF_MODULE(ConnectedStandby)

// Module Methods
//

// eof: Dmf_ConnectedStandby.h
//

