/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_DeviceInterfaceMultipleTarget.h

Abstract:

    Companion file to Dmf_DeviceInterfaceMultipleTarget.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// WDFOBJECT that abstracts a WDFIOTARGET instance.
//
DECLARE_HANDLE(DeviceInterfaceMultipleTarget_Target);

// Enum to specify IO Target State.
//
typedef enum
{
    DeviceInterfaceMultipleTarget_StateType_Invalid,
    DeviceInterfaceMultipleTarget_StateType_Open,
    DeviceInterfaceMultipleTarget_StateType_QueryRemove,
    // NOTE: This name is not correct. The correct name is on next line, but old name is kept for backward compatibility.
    //
    DeviceInterfaceMultipleTarget_StateType_QueryRemoveCancelled,
    DeviceInterfaceMultipleTarget_StateType_RemoveCancel = DeviceInterfaceMultipleTarget_StateType_QueryRemoveCancelled,
    // NOTE: This name is not correct. The correct name is on next line, but old name is kept for backward compatibility.
    //
    DeviceInterfaceMultipleTarget_StateType_QueryRemoveComplete,
    DeviceInterfaceMultipleTarget_StateType_RemoveComplete = DeviceInterfaceMultipleTarget_StateType_QueryRemoveComplete,
    DeviceInterfaceMultipleTarget_StateType_Close,
    DeviceInterfaceMultipleTarget_StateType_Maximum
} DeviceInterfaceMultipleTarget_StateType;

// Enum to determine when the Module should register for PnpNotifications for the device interface GUID specified in the Module configuration.
// Module will register for existing interfaces, so arrival callbacks can happen immediately after registration.
//
typedef enum
{
    // Module is opened in PrepareHardware and closed in ReleaseHardware.
    //
    DeviceInterfaceMultipleTarget_PnpRegisterWhen_PrepareHardware = 0,
    // Module is opened in D0Entry and closed in D0Exit.
    //
    DeviceInterfaceMultipleTarget_PnpRegisterWhen_D0Entry,
    // Module is opened in when Module is created.
    //
    DeviceInterfaceMultipleTarget_PnpRegisterWhen_Create
} DeviceInterfaceMultipleTarget_PnpRegisterWhen_Type;

// Client Driver callback to notify IoTarget State.
//
typedef
_Function_class_(EVT_DMF_DeviceInterfaceMultipleTarget_OnStateChange)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_DeviceInterfaceMultipleTarget_OnStateChange(_In_ DMFMODULE DmfModule,
                                                    _In_ DeviceInterfaceMultipleTarget_Target Target,
                                                    _In_ DeviceInterfaceMultipleTarget_StateType IoTargetState);

// Client Driver callback to notify IoTarget State.
// This version allows Client to veto the open and remove.
//
typedef
_Function_class_(EVT_DMF_DeviceInterfaceMultipleTarget_OnStateChangeEx)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_DeviceInterfaceMultipleTarget_OnStateChangeEx(_In_ DMFMODULE DmfModule,
                                                      _In_ DeviceInterfaceMultipleTarget_Target Target,
                                                      _In_ DeviceInterfaceMultipleTarget_StateType IoTargetState);

// Client Driver callback to notify Interface arrival.
//
typedef
_Function_class_(EVT_DMF_DeviceInterfaceMultipleTarget_OnPnpNotification)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_DeviceInterfaceMultipleTarget_OnPnpNotification(_In_ DMFMODULE DmfModule,
                                                        _In_ PUNICODE_STRING SymbolicLinkName,
                                                        _Out_ BOOLEAN* IoTargetOpen);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Module's Open option.
    //
    DeviceInterfaceMultipleTarget_PnpRegisterWhen_Type ModuleOpenOption;
    // Target Device Interface GUID.
    //
    GUID DeviceInterfaceMultipleTargetGuid;
    // Open in Read or Write mode.
    //
    ULONG OpenMode;
    // Share Access.
    //
    ULONG ShareAccess;
    // Module Config for Child Module.
    //
    DMF_CONFIG_ContinuousRequestTarget ContinuousRequestTargetModuleConfig;
    // Callback to specify IoTarget State.
    // Use Ex version instead. This version is included for legacy Clients only.
    //
    EVT_DMF_DeviceInterfaceMultipleTarget_OnStateChange* EvtDeviceInterfaceMultipleTargetOnStateChange;
    // Callback to specify IoTarget State.
    // This version allows Client to veto the open and remove.
    //
    EVT_DMF_DeviceInterfaceMultipleTarget_OnStateChangeEx* EvtDeviceInterfaceMultipleTargetOnStateChangeEx;
    // Callback to notify Interface arrival.
    //
    EVT_DMF_DeviceInterfaceMultipleTarget_OnPnpNotification* EvtDeviceInterfaceMultipleTargetOnPnpNotification;
} DMF_CONFIG_DeviceInterfaceMultipleTarget;

// This macro declares the following functions:
// DMF_DeviceInterfaceMultipleTarget_ATTRIBUTES_INIT()
// DMF_CONFIG_DeviceInterfaceMultipleTarget_AND_ATTRIBUTES_INIT()
// DMF_DeviceInterfaceMultipleTarget_Create()
//
DECLARE_DMF_MODULE(DeviceInterfaceMultipleTarget)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_BufferPut(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ VOID* ClientBuffer
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_DeviceInterfaceMultipleTarget_Cancel(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ RequestTarget_DmfRequestCancel DmfRequestIdCancel
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_Get(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _Out_ WDFIOTARGET* IoTarget
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_GuidGet(
    _In_ DMFMODULE DmfModule,
    _Out_ GUID* Guid
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_ReuseCreate(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _Out_ RequestTarget_DmfRequestReuse* DmfRequestIdReuse
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_DeviceInterfaceMultipleTarget_ReuseDelete(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ RequestTarget_DmfRequestReuse DmfRequestIdReuse
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_ReuseSend(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ RequestTarget_DmfRequestReuse DmfRequestIdReuse,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtContinuousRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext,
    _Out_opt_ RequestTarget_DmfRequestCancel* DmfRequestIdCancel
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_Send(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
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
DMF_DeviceInterfaceMultipleTarget_SendEx(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtContinuousRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext,
    _Out_opt_ RequestTarget_DmfRequestCancel* DmfRequestIdCancel
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
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
DMF_DeviceInterfaceMultipleTarget_StreamStart(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_DeviceInterfaceMultipleTarget_StreamStop(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_DeviceInterfaceMultipleTarget_TargetDereference(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_TargetReference(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    );

// eof: Dmf_DeviceInterfaceMultipleTarget.h
//
