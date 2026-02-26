/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_VirtualHidDeviceVhfSample.h

Abstract:

    Companion file to Dmf_VirtualHidDeviceVhfSample.c.

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
} DMF_CONFIG_VirtualHidDeviceVhfSample;

// This macro declares the following functions:
// DMF_VirtualHidDeviceVhfSample_ATTRIBUTES_INIT()
// DMF_CONFIG_VirtualHidDeviceVhfSample_AND_ATTRIBUTES_INIT()
// DMF_VirtualHidDeviceVhfSample_Create()
//
DECLARE_DMF_MODULE(VirtualHidDeviceVhfSample)

// Module Methods
//

// eof: Dmf_VirtualHidDeviceVhfSample.h
//
