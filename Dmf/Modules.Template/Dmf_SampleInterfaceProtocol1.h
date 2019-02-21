/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_SampleInterfaceProtocol1.h

Abstract:

    Companion file to Dmf_SampleInterfaceProtocol1.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Protocol uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // NOTE: These elements used for sample purposes only. They are not required
    //       in all Protocols.
    //
    
    // This Module's Id.
    //
    ULONG ModuleId;
    // This Module's Name.
    //
    PSTR ModuleName;
} DMF_CONFIG_SampleInterfaceProtocol1;

// This macro declares the following functions:
// DMF_SampleInterfaceProtocol1_ATTRIBUTES_INIT()
// DMF_CONFIG_SampleInterfaceProtocol1_AND_ATTRIBUTES_INIT()
// DMF_SampleInterfaceProtocol1_Create()
//
DECLARE_DMF_MODULE(SampleInterfaceProtocol1)

// Protocol Methods - Called by Client Driver.
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_SampleInterfaceProtocol1_TestMethod(
    _In_ DMFMODULE DmfModule
    );

// eof: Dmf_SampleInterfaceProtocol1.h
//
