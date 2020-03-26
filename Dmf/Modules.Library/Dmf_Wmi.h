/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Wmi.h

Abstract:

    Companion file to Dmf_Wmi.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Resource Name.
    //
    PWCHAR ResourceName;
    // Wmi Data Guid;.
    //
    GUID WmiDataGuid;
    // WMI data size.
    //
    size_t WmiDataSize;
    // Callback when caller wants to query the entire data item's buffer.
    //
    PFN_WDF_WMI_INSTANCE_QUERY_INSTANCE EvtWmiInstanceQueryInstance;
    // Callback when caller wants to set the entire data item's buffer.
    //
    PFN_WDF_WMI_INSTANCE_SET_INSTANCE EvtWmiInstanceSetInstance;
    // Callback when caller wants to set a single field in the data item's buffer.
    //
    PFN_WDF_WMI_INSTANCE_SET_ITEM EvtWmiInstanceSetItem;
    // Callback when caller wants to execute a method on the data item.
    //
    PFN_WDF_WMI_INSTANCE_EXECUTE_METHOD EvtWmiInstanceExecuteMethod;
} DMF_CONFIG_Wmi;

// This macro declares the following functions:
// DMF_Wmi_ATTRIBUTES_INIT()
// DMF_CONFIG_Wmi_AND_ATTRIBUTES_INIT()
// DMF_Wmi_Create()
//
DECLARE_DMF_MODULE(Wmi)

// Module Methods
//

// eof: Dmf_Wmi.h
//
