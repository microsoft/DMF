/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_VirtualHidDeviceMiniSample.h

Abstract:

    Companion file to Dmf_VirtualHidDeviceMiniSample.c.

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
} DMF_CONFIG_VirtualHidDeviceMiniSample;

// This macro declares the following functions:
// DMF_VirtualHidDeviceMiniSample_ATTRIBUTES_INIT()
// DMF_CONFIG_VirtualHidDeviceMiniSample_AND_ATTRIBUTES_INIT()
// DMF_VirtualHidDeviceMiniSample_Create()
//
DECLARE_DMF_MODULE(VirtualHidDeviceMiniSample)

// Module Methods
//

// eof: Dmf_VirtualHidDeviceMiniSample.h
//
