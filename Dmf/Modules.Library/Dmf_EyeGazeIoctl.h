/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_EyeGazeGhost.h

Abstract:

    Companion file to Dmf_EyeGazeGhost.c.

Environment:

    Kernel-mode Driver Framework

--*/

#pragma once

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    USHORT VendorId;
    USHORT ProductId;
    USHORT VersionId;
} DMF_CONFIG_EyeGazeIoctl;

// This macro declares the following functions:
// DMF_EyeGazeIoctl_ATTRIBUTES_INIT()
// DMF_CONFIG_EyeGazeIoctl_AND_ATTRIBUTES_INIT()
// DMF_EyeGazeIoctl_Create()
//
DECLARE_DMF_MODULE(EyeGazeIoctl)

// Module Methods
//

// eof: Dmf_EyeGazeIoctl.h
//
