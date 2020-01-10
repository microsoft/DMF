/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Interrupt.c

Abstract:

    This modules has routines configure a continuous reader on an
    interrupt pipe to asynchronously read toggle switch states.

Environment:

    Kernel mode

--*/

#include <osrusbfx2.h>

#include "interrupt.tmh"

_Use_decl_annotations_
VOID
OsrFx2InterruptPipeCallback(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR SwitchState,
    _In_ NTSTATUS NtStatus
    )
{
    WDFDEVICE device;

    // NOTE: We could pass the SwitchState to the function below, but keep the code
    //       as in the original sample so that this sample can illustrate a
    //       Module Method (that gets data from the Module).
    //
    UNREFERENCED_PARAMETER(SwitchState);

    device = DMF_ParentDeviceGet(DmfModule);

    OsrUsbIoctlGetInterruptMessage(device, 
                                   NtStatus);
}

