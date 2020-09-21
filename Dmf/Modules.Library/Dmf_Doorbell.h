/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Doorbell.h

Abstract:

    Companion file to Dmf_Doorbell.c

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

typedef
_Function_class_(EVT_DMF_Doorbell_ClientCallback)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_Doorbell_ClientCallback(
    _In_ DMFMODULE DmfModule
    );

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Function to call from Work Item once the Doorbell is ringed.
    //
    EVT_DMF_Doorbell_ClientCallback* WorkItemCallback;
} DMF_CONFIG_Doorbell;

DECLARE_DMF_MODULE(Doorbell)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Doorbell_Flush(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Doorbell_Ring(
    _In_ DMFMODULE DmfModule
    );

// eof: Dmf_Doorbell.h
//