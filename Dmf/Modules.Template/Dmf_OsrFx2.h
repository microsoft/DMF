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

// These messages allow the Client to perform logging when specific events happen 
// inside the Module. The logging mechanism is Client specific. It may just be tracing or
// it may write to event log.
//
typedef enum
{
    OsrFx2_EventWriteMessage_Invalid,
    OsrFx2_EventWriteMessage_ReadStart,
    OsrFx2_EventWriteMessage_ReadFail,
    OsrFx2_EventWriteMessage_ReadStop,
    OsrFx2_EventWriteMessage_WriteStart,
    OsrFx2_EventWriteMessage_WriteFail,
    OsrFx2_EventWriteMessage_WriteStop,
    OsrFx2_EventWriteMessage_SelectConfigFailure,
    OsrFx2_EventWriteMessage_DeviceReenumerated,
} OsrFx2_EventWriteMessage;

// This callback is called when the Module determines a code path has occurrec that the Client
// may want to write to a logging output device.
//
typedef
_Function_class_(EVT_DMF_OsrFx2_EventWriteCallback)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_OsrFx2_EventWriteCallback(_In_ DMFMODULE DmfModule,
                                  _In_ OsrFx2_EventWriteMessage EventWriteMessage,
                                  _In_ ULONG_PTR Parameter1,
                                  _In_ ULONG_PTR Parameter2,
                                  _In_ ULONG_PTR Parameter3,
                                  _In_ ULONG_PTR Parameter4,
                                  _In_ ULONG_PTR Parameter5);

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
    // When interrupt pipe returns data, this callback is called at PASSIVE_LEVEL.
    //
    EVT_DMF_OsrFx2_InterruptPipeCallback* InterruptPipeCallbackPassive;
    // Allows Client to turn off default settings related to how the device will function.
    //
    OsrFx2_Settings Settings;
    // Allows a Client to write events to event long using Client specific constructs.
    //
    EVT_DMF_OsrFx2_EventWriteCallback* EventWriteCallback;
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
