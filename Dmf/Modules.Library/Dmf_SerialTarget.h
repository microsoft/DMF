/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_SerialTarget.h

Abstract:

    Companion file to Dmf_SerialTarget.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Due to User-mode include conflict with winioctl.h
// this file cannot compile in User-mode.
//
#if !defined(DMF_USER_MODE)

typedef enum 
{
    SerialBaudRateFlag                = 0x0001,
    SerialLineControlFlag             = 0x0002,
    SerialCharsFlag                   = 0x0004,
    SerialTimeoutsFlag                = 0x0008,
    SerialQueueSizeFlag               = 0x0010,
    SerialHandflowFlag                = 0x0020,
    SerialWaitMaskFlag                = 0x0040,
    SerialClearRtsFlag                = 0x0080,
    SerialClearDtrFlag                = 0x0100,
    SerialHighResolutionTimerFlag     = 0x0200,
    ConfigurationParametersValidFlags = SerialBaudRateFlag |
                                        SerialLineControlFlag |
                                        SerialCharsFlag |
                                        SerialTimeoutsFlag |
                                        SerialQueueSizeFlag |
                                        SerialHandflowFlag |
                                        SerialWaitMaskFlag |
                                        SerialClearRtsFlag |
                                        SerialClearDtrFlag |
                                        SerialHighResolutionTimerFlag
} SerialStream_ConfigurationParameters_Flags;

typedef struct
{
    ULONG Flags;
    SERIAL_BAUD_RATE BaudRate;
    SERIAL_LINE_CONTROL SerialLineControl;
    SERIAL_CHARS SerialChars;
    SERIAL_TIMEOUTS SerialTimeouts;
    SERIAL_QUEUE_SIZE QueueSize;
    SERIAL_HANDFLOW SerialHandflow;
    ULONG SerialWaitMask;
    BOOLEAN EnableHighResolutionTimer;
} SerialTarget_Configuration;

typedef enum
{
    // Module is opened in PrepareHardware and closed in ReleaseHardware.
    //
    SerialTarget_OpenOption_PrepareHardware = 0,
    // Module is opened in D0Entry when system transitions from SX to S0.
    //
    SerialTarget_OpenOption_D0EntrySystemPowerUp,
    // Module is opened in D0Entry and closed in D0Exit.
    //
    SerialTarget_OpenOption_D0Entry
} SerialTarget_OpenOption;

// Client Driver callback function to get Configuration parameters.
//
typedef
_Function_class_(EVT_DMF_SerialTarget_CustomConfiguration)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_SerialTarget_CustomConfiguration(_In_ DMFMODULE DmfModule,
                                         _Out_ SerialTarget_Configuration* ConfigurationParameters);

// Client Driver callback function for QueryRemove.
//
typedef
_Function_class_(EVT_DMF_SerialTarget_QueryRemove)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_SerialTarget_QueryRemove(_In_ DMFMODULE DmfModule);

// Client Driver callback function for RemoveCanceled.
//
typedef
_Function_class_(EVT_DMF_SerialTarget_RemoveCanceled)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_SerialTarget_RemoveCanceled(_In_ DMFMODULE DmfModule);

// Client Driver callback function for RemoveComplete.
//
typedef
_Function_class_(EVT_DMF_SerialTarget_RemoveComplete)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_SerialTarget_RemoveComplete(_In_ DMFMODULE DmfModule);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Serial Io device configuration parameters.
    //
    EVT_DMF_SerialTarget_CustomConfiguration* EvtSerialTargetCustomConfiguration;
    // Open in Read or Write mode.
    //
    ULONG OpenMode;
    // Share Access.
    //
    ULONG ShareAccess;
    // Module's Open option.
    //
    SerialTarget_OpenOption ModuleOpenOption;
    // Child Request Stream Module.
    //
    DMF_CONFIG_ContinuousRequestTarget ContinuousRequestTargetModuleConfig;
    // Client's QueryRemove callback.
    //
    EVT_DMF_SerialTarget_QueryRemove* EvtSerialTargetQueryRemove;
    // Client's RemoveCancel callback.
    //
    EVT_DMF_SerialTarget_RemoveCanceled* EvtSerialTargetRemoveCanceled;
    // Client's RemoveComplete callback.
    //
    EVT_DMF_SerialTarget_RemoveComplete* EvtSerialTargetRemoveComplete;
} DMF_CONFIG_SerialTarget;

// This macro declares the following functions:
// DMF_SerialTarget_ATTRIBUTES_INIT()
// DMF_CONFIG_SerialTarget_AND_ATTRIBUTES_INIT()
// DMF_SerialTarget_Create()
//
DECLARE_DMF_MODULE(SerialTarget)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_SerialTarget_BufferPut(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SerialTarget_IoTargetGet(
    _In_ DMFMODULE DmfModule,
    _Out_ WDFIOTARGET* IoTarget
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SerialTarget_Send(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtContinuousRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SerialTarget_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _Out_opt_ size_t* BytesWritten
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SerialTarget_StreamStart(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_SerialTarget_StreamStop(
    _In_ DMFMODULE DmfModule
    );

#endif // ! defined(DMF_USER_MODE)

// eof: Dmf_SerialTarget.h
//
