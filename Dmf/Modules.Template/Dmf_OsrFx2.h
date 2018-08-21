/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_OsrFx2.h

Abstract:

    Companion file to Dmf_OsrFx2.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// This callback is called when data is available from the OSR FX2 Interrupt Pipe.
//
typedef
_Function_class_(EVT_DMF_OsrFx2_InterruptPipeCallback)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_OsrFx2_InterruptPipeCallback(_In_ DMFMODULE DmfModule,
                                     _In_ UCHAR SwitchState,
                                     _In_ NTSTATUS NtStatus);

// Bit-mask that allows Client to determine how the device operates.
//
typedef enum
{
    OsrFx2_Settings_NoDeviceInterface = 0x01,
    OsrFx2_Settings_NoEnterIdle = 0x02,
    OsrFx2_Settings_IdleIndication = 0x04,
} OsrFx2_Settings;

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // When interrupt pipe returns data, this callback is called.
    //
    EVT_DMF_OsrFx2_InterruptPipeCallback* InterruptPipeCallback;
    // Allows Client to turn off default settings related to how the device will function.
    //
    OsrFx2_Settings Settings;
} DMF_CONFIG_OsrFx2;

// This macro declares the following functions:
// DMF_OsrFx2_ATTRIBUTES_INIT()
// DMF_CONFIG_OsrFx2_AND_ATTRIBUTES_INIT()
// DMF_OsrFx2_Create()
//
DECLARE_DMF_MODULE(OsrFx2)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_OsrFx2_SwitchStateGet(
    _In_ DMFMODULE DmfModule,
    _Out_ UCHAR* SwitchState
    );

// eof: Dmf_OsrFx2.h
//
