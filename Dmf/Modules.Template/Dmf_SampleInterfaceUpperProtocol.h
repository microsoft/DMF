/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_SampleInterfaceUpperProtocol.h

Abstract:

    Companion file to Dmf_SampleInterfaceUpperProtocol.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

typedef
NTSTATUS
(*SampleInterfaceUpperTransport_Binding)(_In_ DMFMODULE ProtocolModule,
                                         _Out_ DMFMODULE* TransportModule);

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
} DMF_CONFIG_SampleInterfaceUpperProtocol;

// This macro declares the following functions:
// DMF_SampleInterfaceUpperProtocol_ATTRIBUTES_INIT()
// DMF_CONFIG_SampleInterfaceUpperProtocol_AND_ATTRIBUTES_INIT()
// DMF_SampleInterfaceUpperProtocol_Create()
//
DECLARE_DMF_MODULE(SampleInterfaceUpperProtocol)

// Protocol Methods - Called by Client Driver.
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_SampleInterfaceUpperProtocol_TestMethod(
    _In_ DMFMODULE DmfModule
    );

// eof: Dmf_SampleInterfaceUpperProtocol.h
//
