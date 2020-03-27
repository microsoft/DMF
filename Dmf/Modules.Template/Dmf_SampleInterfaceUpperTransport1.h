/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_SampleInterfaceUpperTransport1.h

Abstract:

    Companion file to Dmf_SampleInterfaceUpperTransport1.c.

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
} DMF_CONFIG_SampleInterfaceUpperTransport1;

// This macro declares the following functions:
// DMF_SampleInterfaceUpperTransport1_ATTRIBUTES_INIT()
// DMF_CONFIG_SampleInterfaceUpperTransport1_AND_ATTRIBUTES_INIT()
// DMF_SampleInterfaceUpperTransport1_Create()
//
DECLARE_DMF_MODULE(SampleInterfaceUpperTransport1)

// eof: Dmf_SampleInterfaceUpperTransport1.h
//
