/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_SampleInterfaceTransport2.h

Abstract:

    Companion file to Dmf_SampleInterfaceTransport2.c.

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
} DMF_CONFIG_SampleInterfaceTransport2;

// This macro declares the following functions:
// DMF_SampleInterfaceTransport2_ATTRIBUTES_INIT()
// DMF_CONFIG_SampleInterfaceTransport2_AND_ATTRIBUTES_INIT()
// DMF_SampleInterfaceTransport2_Create()
//
DECLARE_DMF_MODULE(SampleInterfaceTransport2)

// eof: Dmf_SampleInterfaceTransport2.h
//
