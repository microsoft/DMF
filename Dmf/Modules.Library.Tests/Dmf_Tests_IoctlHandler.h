/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_IoctlHandler.h

Abstract:

    Companion file to Dmf_Tests_IoctlHandler.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

typedef struct
{
    // TRUE means that a device interface is created so that remote targets
    // can access this Module's IOCTL handler.
    //
    BOOLEAN CreateDeviceInterface;
} DMF_CONFIG_Tests_IoctlHandler;

// This macro declares the following functions:
// DMF_Tests_IoctlHandler_ATTRIBUTES_INIT()
// DMF_CONFIG_Tests_IoctlHandler_AND_ATTRIBUTES_INIT()
// DMF_Tests_IoctlHandler_Create()
//
DECLARE_DMF_MODULE(Tests_IoctlHandler)

// Module Methods
//

// eof: Dmf_Tests_IoctlHandler.h
//
