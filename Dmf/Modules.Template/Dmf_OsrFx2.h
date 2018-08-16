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

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // When interrupt pipe returns data, this callback is called.
    //
    EVT_DMF_OsrFx2_InterruptPipeCallback* InterruptPipeCallback;
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
