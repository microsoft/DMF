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
    ConfigurationParametersValidFlags = SerialBaudRateFlag |
                                        SerialLineControlFlag |
                                        SerialCharsFlag |
                                        SerialTimeoutsFlag |
                                        SerialQueueSizeFlag |
                                        SerialHandflowFlag |
                                        SerialWaitMaskFlag |
                                        SerialClearRtsFlag |
                                        SerialClearDtrFlag
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
} SerialTarget_Configuration;

// Client Driver callback function to get Configuration parameters.
//
typedef
_Function_class_(EVT_DMF_SerialTarget_CustomConfiguration)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_SerialTarget_CustomConfiguration(_In_ DMFMODULE DmfModule,
                                         _Out_ SerialTarget_Configuration* ConfigurationParameters);

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
    // Child Request Stream Module.
    //
    DMF_CONFIG_ContinuousRequestTarget ContinuousRequestTargetModuleConfig;
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
VOID
DMF_SerialTarget_IoTargetGet(
    _In_ DMFMODULE DmfModule,
    _Out_ WDFIOTARGET* IoTarget
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_SerialTarget_Send(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SingleAsynchronousBufferOutput* EvtContinuousRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_SerialTarget_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _Out_opt_ size_t* BytesWritten
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_SerialTarget_StreamStart(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_SerialTarget_StreamStop(
    _In_ DMFMODULE DmfModule
    );

#endif // ! defined(DMF_USER_MODE)

// eof: Dmf_SerialTarget.h
//
