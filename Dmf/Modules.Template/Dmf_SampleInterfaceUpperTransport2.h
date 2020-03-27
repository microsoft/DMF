/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_SampleInterfaceUpperTransport2.h

Abstract:

    Companion file to Dmf_SampleInterfaceUpperTransport2.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Transport uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // This Module's ID.
    //
    ULONG ModuleId;
    // This Module's Name.
    //
    PSTR ModuleName;
    // Binds an a transport at lower edge.
    //
    SampleInterfaceUpperTransport_Binding TransportBindingCallback;
} DMF_CONFIG_SampleInterfaceUpperTransport2;

// This macro declares the following functions:
// DMF_SampleInterfaceUpperTransport2_ATTRIBUTES_INIT()
// DMF_CONFIG_SampleInterfaceUpperTransport2_AND_ATTRIBUTES_INIT()
// DMF_SampleInterfaceUpperTransport2_Create()
//
DECLARE_DMF_MODULE(SampleInterfaceUpperTransport2)

// eof: Dmf_SampleInterfaceUpperTransport2.h
//
