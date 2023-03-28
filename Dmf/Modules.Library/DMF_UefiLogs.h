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

// This macro declares the following functions:
// DMF_UefiLogs_ATTRIBUTES_INIT()
// DMF_CONFIG_UefiLogs_AND_ATTRIBUTES_INIT()
// DMF_UefiLogs_Create()
//
DECLARE_DMF_MODULE_NO_CONFIG(UefiLogs)

// Module Methods
//

// eof: Dmf_UefiLogs.h
//
