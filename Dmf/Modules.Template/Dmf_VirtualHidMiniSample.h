/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_VirtualHidMiniSample.h

Abstract:

    Companion file to Dmf_VirtualHidMiniSample.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    ULONG ReadFromRegistry;
} DMF_CONFIG_VirtualHidMiniSample;

// This macro declares the following functions:
// DMF_VirtualHidMiniSample_ATTRIBUTES_INIT()
// DMF_CONFIG_VirtualHidMiniSample_AND_ATTRIBUTES_INIT()
// DMF_VirtualHidMiniSample_Create()
//
DECLARE_DMF_MODULE(VirtualHidMiniSample)

// Module Methods
//

// eof: Dmf_VirtualHidMiniSample.h
//
