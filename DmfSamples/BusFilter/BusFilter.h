/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Device.h

Abstract:

    Companion file to Device.c.

Environment:

    Kernel mode only

--*/

#pragma once

// Child device context
// 
typedef struct _CHILD_DEVICE_CONTEXT
{
    WDFDEVICE Parent;
    Tests_IoctlHandler_INTERFACE_STANDARD OriginalInterface;
} CHILD_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CHILD_DEVICE_CONTEXT, ChildDeviceGetContext)

// eof: Device.h
//

