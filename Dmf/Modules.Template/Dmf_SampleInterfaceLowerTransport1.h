/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_SampleInterfaceLowerTransport1.h

Abstract:

    Companion file to Dmf_SampleInterfaceLowerTransport1.c.

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
} DMF_CONFIG_SampleInterfaceLowerTransport1;

// This macro declares the following functions:
// DMF_SampleInterfaceLowerTransport1_ATTRIBUTES_INIT()
// DMF_CONFIG_SampleInterfaceLowerTransport1_AND_ATTRIBUTES_INIT()
// DMF_SampleInterfaceLowerTransport1_Create()
//
DECLARE_DMF_MODULE(SampleInterfaceLowerTransport1)

// eof: Dmf_SampleInterfaceLowerTransport1.h
//
