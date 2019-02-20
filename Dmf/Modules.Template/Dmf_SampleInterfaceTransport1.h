/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_SampleInterfaceTransport1.h

Abstract:

    Companion file to Dmf_SampleInterfaceTransport1.c.

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
} DMF_CONFIG_SampleInterfaceTransport1;

// This macro declares the following functions:
// DMF_SampleInterfaceTransport1_ATTRIBUTES_INIT()
// DMF_CONFIG_SampleInterfaceTransport1_AND_ATTRIBUTES_INIT()
// DMF_SampleInterfaceTransport1_Create()
//
DECLARE_DMF_MODULE(SampleInterfaceTransport1)

// eof: Dmf_SampleInterfaceTransport1.h
//
