/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_SampleInterfaceLowerTransport2.h

Abstract:

    Companion file to Dmf_SampleInterfaceLowerTransport2.c.

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
} DMF_CONFIG_SampleInterfaceLowerTransport2;

// This macro declares the following functions:
// DMF_SampleInterfaceLowerTransport2_ATTRIBUTES_INIT()
// DMF_CONFIG_SampleInterfaceLowerTransport2_AND_ATTRIBUTES_INIT()
// DMF_SampleInterfaceLowerTransport2_Create()
//
DECLARE_DMF_MODULE(SampleInterfaceLowerTransport2)

// eof: Dmf_SampleInterfaceLowerTransport2.h
//
