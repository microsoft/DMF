#pragma once
/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_UefiLogs.h

Abstract:

    Companion file to Dmf_UefiLogs.c.

Environment:

    Kernel-mode Driver Framework

--*/


// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Name of the Registry entry (string type) in Driver's registry path.
    // If set, this Module will collect logs and store it in path specified
    // in the value of this Registry entry.
    //
    const UNICODE_STRING* RegistryEntryName;
} DMF_CONFIG_UefiLogs;

// This macro declares the following functions:
// DMF_UefiLogs_ATTRIBUTES_INIT()
// DMF_CONFIG_UefiLogs_AND_ATTRIBUTES_INIT()
// DMF_UefiLogs_Create()
//
DECLARE_DMF_MODULE(UefiLogs)

// Module Methods
//

// eof: Dmf_UefiLogs.h
//
