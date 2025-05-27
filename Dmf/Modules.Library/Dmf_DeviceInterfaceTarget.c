/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_DeviceInterfaceTarget.c

Abstract:

    Creates a stream of asynchronous requests to a dynamic PnP IO Target. Also, there is support
    for sending synchronous requests to the same IO Target.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_DeviceInterfaceTarget.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// These are virtual Methods that are set based on the transport.
// These functions are common to both the Stream and Target transport.
// They are set to the correct version when the Module is created.
// NOTE: The DmfModule that is sent is the DeviceInterfaceTarget Module.
//

typedef
BOOLEAN
RequestSink_Cancel_Type(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequestCancel DmfRequestIdCancel
    );

typedef
NTSTATUS
RequestSink_ReuseCreate_Type(
    _In_ DMFMODULE DmfModule,
    _Out_ RequestTarget_DmfRequestReuse* DmfRequestIdReuse
    );

typedef
BOOLEAN
RequestSink_ReuseDelete_Type(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequestReuse DmfRequestIdReuse
    );

typedef
_Must_inspect_result_
NTSTATUS
RequestSink_SendSynchronously_Type(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeout,
    _Out_opt_ size_t* BytesWritten
    );

typedef
_Must_inspect_result_
NTSTATUS
RequestSink_Send_Type(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtRequestSinkSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    );

typedef
_Must_inspect_result_
NTSTATUS
RequestSink_SendEx_Type(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtRequestSinkSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext,
    _Out_opt_ RequestTarget_DmfRequestCancel* DmfRequestIdCancel
    );

typedef
_Must_inspect_result_
NTSTATUS
RequestSink_ReuseSend_Type(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequestReuse DmfRequestIdReuse,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtRequestSinkSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext,
    _Out_opt_ RequestTarget_DmfRequestCancel* DmfRequestIdCancel
    );

typedef
VOID
RequestSink_IoTargetSet_Type(
    _In_ DMFMODULE DmfModule,
    _In_ WDFIOTARGET IoTarget
    );

typedef
VOID
RequestSink_IoTargetClear_Type(
    _In_ DMFMODULE DmfModule
    );

// SYNCHRONIZATION NOTE:
//
// This Module must synchronize the following:
//
// 1. NotificationUnregister callback with QueryRemove, RemoveCancel and RemoveComplete backs.
//    It means that there are two possible valid paths:
//    a) NotificationUnregister happens first. In this case, that callback will close the underlying
//       IoTarget and call the Module's Close callback. Once NotificationUnregister has happened,
//       if QueryRemove or RemoveCancel happen, they must do nothing because their code path
//       will execute or is already executing. The Close callback will happen one time, regardless.
//    b) QueryRemove or RemoveComplete happens first (before NotificationUnregister). In this 
//       case, the Module will close and destroy the underlying IoTarget by the time RemoveComplete
//       happens. If during that time, NotificationUnregister happens, it must not
//       try to also close/destroy the target and close the Module as that will already have
//       started happening.
// 2. Module Methods with the IoTarget. 
//    The IoTarget is always cleared at the end of the  Module Close callback. Because DMF 
//    framework automatically performs rundown management between Methods and the Close callback,
//    it means Methods are always synchronized  with the IoTarget. This fact also keeps the 
//    Methods synchronized with QueryRemove, RemoveCancel and RemoveComplete and NotificationUnregister
//    because Methods can only run after the Module is open and will stop running before the Module is 
//    closed.
//

// This setting keeps track of what code path has previously begun to close
// or has closed the Module.
//
typedef enum
{
    ModuleCloseReason_NotSet = 0,
    ModuleCloseReason_NotificationUnregister,
    ModuleCloseReason_QueryRemove,
    ModuleCloseReason_RemoveComplete,
    ModuleCloseReason_NotificationRemoval,
    ModuleCloseReason_Duplicate
} ModuleCloseReason_Type;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_DeviceInterfaceTarget
{
    // Device Interface arrival/removal notification handle.
    // 
#if defined(DMF_USER_MODE)
    HCMNOTIFICATION DeviceInterfaceNotification;
#else
    VOID* DeviceInterfaceNotification;
#endif // defined(DMF_USER_MODE)
    // Underlying Device Target.
    //
    WDFIOTARGET IoTarget;
    // Save Symbolic Link Name to be able to deal with multiple instances of the same
    // device interface.
    //
    WDFMEMORY MemorySymbolicLink;
    UNICODE_STRING SymbolicLinkName;
    // Redirect Input buffer callback from ContinuousRequestTarget to this callback.
    //
    EVT_DMF_ContinuousRequestTarget_BufferInput* EvtContinuousRequestTargetBufferInput;
    // Redirect Output buffer callback from ContinuousRequestTarget to this callback.
    //
    EVT_DMF_ContinuousRequestTarget_BufferOutput* EvtContinuousRequestTargetBufferOutput;

    // This Module has two modes:
    // 1. Streaming is enabled and DmfModuleContinuousRequestTarget is valid.
    // 2. Streaming is not enabled and DmfModuleRequestTarget is used.
    //
    // In order to not check for NULL Handles, this flag is used when a choice must be made.
    // This flag is also used for assertions in case people misuse APIs.
    //
    BOOLEAN OpenedInStreamMode;

    // Indicates the mode of ContinuousRequestTarget.
    //
    ContinuousRequestTarget_ModeType ContinuousRequestTargetMode;

    // Underlying Transport Methods.
    //
    DMFMODULE DmfModuleContinuousRequestTarget;
    DMFMODULE DmfModuleRequestTarget;
    // Stores callback/callback context for asynchronous sends.
    //
    DMFMODULE DmfModuleBufferPool;
    RequestSink_SendSynchronously_Type* RequestSink_SendSynchronously;
    RequestSink_Send_Type* RequestSink_Send;
    RequestSink_SendEx_Type* RequestSink_SendEx;
    RequestSink_ReuseSend_Type* RequestSink_ReuseSend;
    RequestSink_Cancel_Type* RequestSink_Cancel;
    RequestSink_ReuseCreate_Type* RequestSink_ReuseCreate;
    RequestSink_ReuseDelete_Type* RequestSink_ReuseDelete;
    RequestSink_IoTargetSet_Type* RequestSink_IoTargetSet;
    RequestSink_IoTargetClear_Type* RequestSink_IoTargetClear;
    ContinuousRequestTarget_CompletionOptions DefaultCompletionOption;
    
    // Tracks which code path has started to close or has closed the Module.
    //
    ModuleCloseReason_Type ModuleCloseReason;
    // Module has started shutting down while RemoveCancel was ongoing.
    //
    BOOLEAN CloseAfterRemoveCancel;

#if defined(DMF_USER_MODE)
    // Custom device notification handle (using device handle filter).
    //
    HCMNOTIFICATION CustomDeviceNotificationUser;
#endif // defined(DMF_USER_MODE)

} DMF_CONTEXT_DeviceInterfaceTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(DeviceInterfaceTarget)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(DeviceInterfaceTarget)

#define MemoryTag 'MTID'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Context that stores client's callback information so that proper callback chaining happens.
//
typedef struct
{
    // Client's callbck.
    //
    EVT_DMF_RequestTarget_SendCompletion* SendCompletionCallback;
    // Client's callback context.
    //
    VOID* SendCompletionCallbackContext;
} DeviceInterfaceTarget_SingleAsynchronousRequestContext;

#if defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_Function_class_(PCM_NOTIFY_CALLBACK)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
DWORD
CALLBACK
DeviceInterfaceTarget_CustomNotificationCallbackUser(
    _In_ HCMNOTIFICATION hNotify,
    _In_opt_ VOID* Context,
    _In_ CM_NOTIFY_ACTION Action,
    _In_reads_bytes_(EventDataSize) PCM_NOTIFY_EVENT_DATA EventData,
    _In_ DWORD EventDataSize
    )
/*++

Routine Description:

    Internal callback invoked by the system when a custom device notification occurs,
    registered using CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE. This function forwards
    the notification to the client's registered callback.

Arguments:

    hNotify - The notification handle.
    Context - The context provided during registration (DMFMODULE handle).
    Action - The PnP notification action.
    EventData - Pointer to the notification data.
    EventDataSize - Size of the notification data.

Return Value:

    ERROR_SUCCESS or an appropriate error code.

--*/
{
    DMFMODULE dmfModule;
    DMF_CONFIG_DeviceInterfaceTarget* moduleConfig;
    DWORD result = ERROR_SUCCESS;

    UNREFERENCED_PARAMETER(hNotify);

    PAGED_CODE();
    FuncEntry(DMF_TRACE);

    // Context is the DMFMODULE handle.
    //
    dmfModule = DMFMODULEVOID_TO_MODULE(Context);
    if (dmfModule == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid context provided to custom notification callback.");
        result = ERROR_INVALID_DATA;
        goto Exit;
    }

    moduleConfig = DMF_CONFIG_GET(dmfModule);

    // Check if the client provided a callback. It should, otherwise callback won't be registered.
    //
    if (moduleConfig->EvtPnpCustomNotificationCallbackUser != NULL)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Forwarding custom notification Action=%d to client DmfModule=0x%p", Action, dmfModule);

        // Call the client's callback. Does not have callback context.
        //
        result = moduleConfig->EvtPnpCustomNotificationCallbackUser(dmfModule,
                                                                    Action,
                                                                    EventData,
                                                                    EventDataSize);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "No client callback registered for custom notification Action=%d DmfModule=0x%p", Action, dmfModule);
        // No callback registered, which is fine. Just acknowledge the event.
        //
        result = ERROR_SUCCESS;
    }

Exit:
    FuncExit(DMF_TRACE, "result=%u", result);
    return result;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DeviceInterfaceTarget_CustomNotificationRegisterUser(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Registers for custom PnP notifications associated with the underlying device handle
    of the opened I/O target.
    Custom notification/event comes from IoReportTargetDeviceChangeAsynchronous.

Arguments:

    DmfModule - This Module's handle.
   
Return Value:

    NTSTATUS indicating success or failure.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    CM_NOTIFY_FILTER cmNotifyFilter;
    CONFIGRET configRet;
    HANDLE hDeviceTargetFile = INVALID_HANDLE_VALUE;

    PAGED_CODE();
    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference failed: ntStatus=%!STATUS!", ntStatus);
        goto ExitNoReference;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Ensure notification is not already registered for this Module instance.
    //
    if (moduleContext->CustomDeviceNotificationUser != NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Custom notification already registered for DmfModule=0x%p", DmfModule);
        ntStatus = STATUS_DEVICE_ALREADY_ATTACHED; 
        goto Exit;
    }

    // Ensure the underlying IoTarget is open and valid.
    //
    if (moduleContext->IoTarget == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "IoTarget is not open, cannot register custom notification for DmfModule=0x%p", DmfModule);
        ntStatus = STATUS_INVALID_DEVICE_STATE;
        goto Exit;
    }

    // Get the file handle associated with the WDFIOTARGET.
    // This function works for both KMDF and UMDF 2.0+.
    //
    hDeviceTargetFile = WdfIoTargetWdmGetTargetFileHandle(moduleContext->IoTarget);
    if (hDeviceTargetFile == NULL || hDeviceTargetFile == INVALID_HANDLE_VALUE)
    {
        // This might happen if the target wasn't opened in a way that provides a file object handle,
        // or if there's an issue retrieving it.
        //
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to get target file handle for IoTarget=0x%p DmfModule=0x%p", moduleContext->IoTarget, DmfModule);
        ntStatus = STATUS_OBJECT_NAME_NOT_FOUND;
        goto Exit;
    }

    // Initialize the notification filter using the device handle.
    //
    RtlZeroMemory(&cmNotifyFilter, 
                  sizeof(cmNotifyFilter));
    cmNotifyFilter.cbSize = sizeof(CM_NOTIFY_FILTER);
    cmNotifyFilter.Flags = 0;
    cmNotifyFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE;
    cmNotifyFilter.u.DeviceHandle.hTarget = hDeviceTargetFile;

    // Register for the notification.
    // The context passed is the DMFMODULE handle, which our internal callback will use.
    //
    configRet = CM_Register_Notification(&cmNotifyFilter,
                                         (VOID*)DmfModule,
                                         DeviceInterfaceTarget_CustomNotificationCallbackUser,
                                         &(moduleContext->CustomDeviceNotificationUser));

    if (configRet != CR_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Register_Notification (DeviceHandle) failed: cr=0x%x, DmfModule=0x%p", configRet, DmfModule);
        ntStatus = STATUS_UNSUCCESSFUL;
        moduleContext->CustomDeviceNotificationUser = NULL;
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Successfully registered custom notification for DmfModule=0x%p, Handle=0x%p", DmfModule, moduleContext->CustomDeviceNotificationUser);
    ntStatus = STATUS_SUCCESS;

Exit:
    DMF_ModuleDereference(DmfModule);

ExitNoReference:
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);
    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DeviceInterfaceTarget_CustomNotificationUnregisterUser(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Unregisters a previously registered custom PnP notification associated with the
    underlying device handle.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS indicating success or failure. STATUS_SUCCESS if already unregistered.

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    CONFIGRET configRet;

    PAGED_CODE();
    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference failed: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Check if a notification handle exists.
    if (moduleContext->CustomDeviceNotificationUser != NULL)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Unregistering custom notification Handle=0x%p for DmfModule=0x%p",
                    moduleContext->CustomDeviceNotificationUser, DmfModule);

        configRet = CM_Unregister_Notification(moduleContext->CustomDeviceNotificationUser);

        if (configRet != CR_SUCCESS)
        {
            // Log the error, but proceed to clear context fields.
            // It might fail if the underlying device is gone, etc.
            //
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Unregister_Notification (DeviceHandle) failed: cr=0x%x, DmfModule=0x%p", configRet, DmfModule);
            ntStatus = STATUS_UNSUCCESSFUL;
        }
        else
        {
             ntStatus = STATUS_SUCCESS;
        }

        moduleContext->CustomDeviceNotificationUser = NULL;
    }
    else
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Custom notification already unregistered or never registered for DmfModule=0x%p", DmfModule);
        ntStatus = STATUS_SUCCESS;
    }

    DMF_ModuleDereference(DmfModule);

Exit:
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);
    return ntStatus;
}
#pragma code_seg()

#endif // defined(DMF_USER_MODE)

_Function_class_(EVT_DMF_RequestTarget_SendCompletion)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
DeviceInterfaceTarget_SendCompletion(
    _In_ DMFMODULE DmfModuleContinuousRequestTarget,
    _In_ VOID* ClientRequestContext,
    _In_reads_(InputBufferBytesRead) VOID* InputBuffer,
    _In_ size_t InputBufferBytesRead,
    _In_reads_(OutputBufferBytesWritten) VOID* OutputBuffer,
    _In_ size_t OutputBufferBytesWritten,
    _In_ NTSTATUS CompletionStatus
    )
/*++

Routine Description:

    Completion routine for _Send() and _SendEx().

Arguments:

    DmfModuleContinuousRequestTarget - Child Module that calls this callback.
    ClientRequestContext - Context passed by caller of _Send() or _SendEx().
    InputBuffer - Input buffer  passed by caller of _Send() or _SendEx().
    InputBufferBytesRead - Bytes read by WDFIOTARGET.
    OutputBuffer - Output buffer  passed by caller of _Send() or _SendEx().
    OutputBufferBytesRead - Bytes written by WDFIOTARGET.
    CompletionStatus - Completion status returned by WDFIOTARGET.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DeviceInterfaceTarget_SingleAsynchronousRequestContext* completionCallbackContext;

    dmfModule = DMF_ParentModuleGet(DmfModuleContinuousRequestTarget);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    completionCallbackContext = (DeviceInterfaceTarget_SingleAsynchronousRequestContext*)ClientRequestContext;

    if (completionCallbackContext->SendCompletionCallback != NULL)
    {
        completionCallbackContext->SendCompletionCallback(dmfModule,
                                                          completionCallbackContext->SendCompletionCallbackContext,
                                                          InputBuffer,
                                                          InputBufferBytesRead,
                                                          OutputBuffer,
                                                          OutputBufferBytesWritten,
                                                          CompletionStatus);
    }

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPool,
                       completionCallbackContext);
}

ModuleCloseReason_Type
DeviceInterfaceTarget_ModuleCloseReasonSet(
    _In_ DMFMODULE DmfModule,
    _In_ ModuleCloseReason_Type ModuleCloseReason
    )
/*++

Routine Description:

    If possible indicate that an IoTarget removal path has started. If a path has already
    started, then this call indicates that fact and prevents the new path from starting.

Arguments:

    DmfModule - This Module's DMF Module handle.
    ModuleCloseReason - The new path that will start to close the IoTarget.

Return Value:

    If return value == ModuleCloseReason, then this code path may proceed because no
    other path has started. No other close path will be able to start.
    If return value 1= ModuleCloseReason, then this code path not proceed because
    another code paths has already started to close the IoTarget.

--*/
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    ModuleCloseReason_Type moduleCloseReasonType;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    TraceEvents(TRACE_LEVEL_INFORMATION,  DMF_TRACE,
                "DeviceInterfaceTarget_ModuleCloseReasonSet DmfModule=0x%p ModuleCloseReason=%d ENTER",
                DmfModule, ModuleCloseReason);

    moduleCloseReasonType = moduleContext->ModuleCloseReason;
    if (moduleContext->ModuleCloseReason == ModuleCloseReason_NotSet)
    {
        // No code path has started to close IoTarget yet.
        //
        moduleContext->ModuleCloseReason = ModuleCloseReason;
        moduleCloseReasonType = moduleContext->ModuleCloseReason;
    }
    else if (ModuleCloseReason == moduleContext->ModuleCloseReason)
    {
        // This path (duplicate QueryRemove) can happen when multiple
        // drivers are in the stack. this case causes any duplicate messages
        // to be ignored. It is necessary to do this to prevent Module's
        // Close callback from being called twice.
        //
        moduleCloseReasonType = ModuleCloseReason_Duplicate;
    }
    else if ((ModuleCloseReason == ModuleCloseReason_NotificationUnregister) ||
             (ModuleCloseReason == ModuleCloseReason_NotificationRemoval))
    {
        // If this is not the first path to try to close, then always close after
        // RemoveCancel.
        //
        moduleContext->CloseAfterRemoveCancel = TRUE;
    }
    
    TraceEvents(TRACE_LEVEL_INFORMATION,  DMF_TRACE,
                "DeviceInterfaceTarget_ModuleCloseReasonSet DmfModule=0x%p ModuleCloseReason=%d moduleCloseReasonType=%d EXIT",
                DmfModule, ModuleCloseReason, moduleCloseReasonType);

    DMF_ModuleUnlock(DmfModule);

    // Return the current path that has started executing.
    //
    return moduleCloseReasonType;
}

VOID
DeviceInterfaceTarget_SymbolicLinkNameClear(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Delete the stored symbolic link from the context. This is needed to deal with multiple instances of 
    the same device interface.

Arguments:

    DmfModule - This Module's DMF Module handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->MemorySymbolicLink != NULL)
    {
        WdfObjectDelete(moduleContext->MemorySymbolicLink);
        moduleContext->MemorySymbolicLink = NULL;
        moduleContext->SymbolicLinkName.Buffer = NULL;
        moduleContext->SymbolicLinkName.Length = 0;
        moduleContext->SymbolicLinkName.MaximumLength = 0;
    }
}

_Must_inspect_result_
NTSTATUS
DeviceInterfaceTarget_SymbolicLinkNameStore(
    _In_ DMFMODULE DmfModule,
    _In_ UNICODE_STRING* SymbolicLinkName
    )
/*++

Routine Description:

    Create a copy of symbolic link name and store it in the given Module's context. This is needed to deal with
    multiple instances of the same device interface.

Arguments:

    DmfModule - This Module's DMF Module handle.
    SymbolicLinkName - The given symbolic link name.

Return Value:

    None

--*/
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    USHORT symbolicLinkStringLength;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    NTSTATUS ntStatus;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    symbolicLinkStringLength = SymbolicLinkName->Length;
    if (0 == symbolicLinkStringLength)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Symbolic link name length is 0");
        goto Exit;
    }

    DmfAssert(NULL == moduleContext->SymbolicLinkName.Buffer);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DMF_ParentDeviceGet(DmfModule);

    ntStatus = WdfMemoryCreate(&objectAttributes,
                                NonPagedPoolNx,
                                MemoryTag,
                                symbolicLinkStringLength + sizeof(UNICODE_NULL),
                                &moduleContext->MemorySymbolicLink,
                                (VOID**)&moduleContext->SymbolicLinkName.Buffer);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,  DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }
    DmfAssert(NULL != moduleContext->SymbolicLinkName.Buffer);

    moduleContext->SymbolicLinkName.Length = symbolicLinkStringLength;
    moduleContext->SymbolicLinkName.MaximumLength = symbolicLinkStringLength + sizeof(UNICODE_NULL);

#if defined(DMF_USER_MODE)
    // Overwrite with string.
    //
    RtlZeroMemory(moduleContext->SymbolicLinkName.Buffer,
                  moduleContext->SymbolicLinkName.MaximumLength);
    RtlCopyMemory(moduleContext->SymbolicLinkName.Buffer,
                  SymbolicLinkName->Buffer,
                  symbolicLinkStringLength);
#else
    ntStatus = RtlUnicodeStringCopy(&(moduleContext->SymbolicLinkName),
                                    SymbolicLinkName);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,  DMF_TRACE, "RtlUnicodeStringCopy fails: ntStatus=%!STATUS!", ntStatus);
        DeviceInterfaceTarget_SymbolicLinkNameClear(DmfModule);
        goto Exit;
    }
#endif

Exit:
    
    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DeviceInterfaceTarget_StreamStopAndModuleClose(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stop streaming if automatic streaming is enabled and close the Module.

Arguments:

    DmfModule - The given Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->ContinuousRequestTargetMode == ContinuousRequestTarget_Mode_Automatic)
    {
        // By calling this function here, callbacks at the Client will happen only before the Module is closed.
        //
        DmfAssert(moduleContext->DmfModuleContinuousRequestTarget != NULL);
        DMF_ContinuousRequestTarget_StopAndWait(moduleContext->DmfModuleContinuousRequestTarget);
    }

    // Close the Module. After this, no Methods will run.
    //
    DMF_ModuleClose(DmfModule);
}
#pragma code_seg()

// ContinuousRequestTarget Methods
// -------------------------------
//

BOOLEAN
DeviceInterfaceTarget_Stream_Cancel(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequestCancel DmfRequestIdCancel
    )
{
    BOOLEAN returnValue;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    returnValue = DMF_ContinuousRequestTarget_Cancel(moduleContext->DmfModuleContinuousRequestTarget,
                                                     DmfRequestIdCancel);

    return returnValue;
}

NTSTATUS
DeviceInterfaceTarget_Stream_ReuseCreate(
    _In_ DMFMODULE DmfModule,
    _Out_ RequestTarget_DmfRequestReuse* DmfRequestIdReuse
    )
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    ntStatus = DMF_ContinuousRequestTarget_ReuseCreate(moduleContext->DmfModuleContinuousRequestTarget,
                                                       DmfRequestIdReuse);

    return ntStatus;
}

BOOLEAN
DeviceInterfaceTarget_Stream_ReuseDelete(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequestReuse DmfRequestIdReuse
    )
{
    BOOLEAN returnValue;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    returnValue = DMF_ContinuousRequestTarget_ReuseDelete(moduleContext->DmfModuleContinuousRequestTarget,
                                                          DmfRequestIdReuse);

    return returnValue;
}

_Must_inspect_result_
NTSTATUS
DeviceInterfaceTarget_Stream_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeout,
    _Out_opt_ size_t* BytesWritten
    )
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    return DMF_ContinuousRequestTarget_SendSynchronously(moduleContext->DmfModuleContinuousRequestTarget,
                                                         RequestBuffer,
                                                         RequestLength,
                                                         ResponseBuffer,
                                                         ResponseLength,
                                                         RequestType,
                                                         RequestIoctl,
                                                         RequestTimeout,
                                                         BytesWritten);
}

// TEMPORARY: This is here to make code review eaiser.
//            Remove after code review and move this function under the next function.
//
_Must_inspect_result_
NTSTATUS
DeviceInterfaceTarget_Stream_SendEx(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestSinkSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext,
    _Out_opt_ RequestTarget_DmfRequestCancel* DmfRequestIdCancel
    );

_Must_inspect_result_
NTSTATUS
DeviceInterfaceTarget_Stream_Send(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtRequestSinkSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    )
{
    return DeviceInterfaceTarget_Stream_SendEx(DmfModule,
                                               RequestBuffer,
                                               RequestLength,
                                               ResponseBuffer,
                                               ResponseLength,
                                               RequestType,
                                               RequestIoctl,
                                               RequestTimeoutMilliseconds,
                                               EvtRequestSinkSingleAsynchronousRequest,
                                               SingleAsynchronousRequestClientContext,
                                               NULL);
}

_Must_inspect_result_
NTSTATUS
DeviceInterfaceTarget_Stream_SendEx(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestSinkSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext,
    _Out_opt_ RequestTarget_DmfRequestCancel* DmfRequestIdCancel
    )
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DeviceInterfaceTarget_SingleAsynchronousRequestContext* completionCallbackContext;
    NTSTATUS ntStatus;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&completionCallbackContext,
                                  NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    completionCallbackContext->SendCompletionCallback = EvtRequestSinkSingleAsynchronousRequest;
    completionCallbackContext->SendCompletionCallbackContext = SingleAsynchronousRequestClientContext;

    ntStatus = DMF_ContinuousRequestTarget_SendEx(moduleContext->DmfModuleContinuousRequestTarget,
                                                  RequestBuffer,
                                                  RequestLength,
                                                  ResponseBuffer,
                                                  ResponseLength,
                                                  RequestType,
                                                  RequestIoctl,
                                                  RequestTimeoutMilliseconds,
                                                  DeviceInterfaceTarget_SendCompletion,
                                                  completionCallbackContext,
                                                  DmfRequestIdCancel);
    if (!NT_SUCCESS(ntStatus))
    {
        DMF_BufferPool_Put(moduleContext->DmfModuleBufferPool,
                           completionCallbackContext);
    }

Exit:

    return ntStatus;
}

_Must_inspect_result_
NTSTATUS
DeviceInterfaceTarget_Stream_ReuseSend(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequestReuse DmfRequestIdReuse,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestSinkSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext,
    _Out_opt_ RequestTarget_DmfRequestCancel* DmfRequestIdCancel
    )
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DeviceInterfaceTarget_SingleAsynchronousRequestContext* completionCallbackContext;
    NTSTATUS ntStatus;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&completionCallbackContext,
                                  NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    completionCallbackContext->SendCompletionCallback = EvtRequestSinkSingleAsynchronousRequest;
    completionCallbackContext->SendCompletionCallbackContext = SingleAsynchronousRequestClientContext;

    ntStatus = DMF_ContinuousRequestTarget_ReuseSend(moduleContext->DmfModuleContinuousRequestTarget,
                                                     DmfRequestIdReuse,
                                                     RequestBuffer,
                                                     RequestLength,
                                                     ResponseBuffer,
                                                     ResponseLength,
                                                     RequestType,
                                                     RequestIoctl,
                                                     RequestTimeoutMilliseconds,
                                                     DeviceInterfaceTarget_SendCompletion,
                                                     completionCallbackContext,
                                                     DmfRequestIdCancel);
    if (!NT_SUCCESS(ntStatus))
    {
        DMF_BufferPool_Put(moduleContext->DmfModuleBufferPool,
                           completionCallbackContext);
    }

Exit:

    return ntStatus;
}

VOID
DeviceInterfaceTarget_Stream_IoTargetSet(
    _In_ DMFMODULE DmfModule,
    _In_ WDFIOTARGET IoTarget
    )
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    DMF_ContinuousRequestTarget_IoTargetSet(moduleContext->DmfModuleContinuousRequestTarget,
                                            IoTarget);
}

VOID
DeviceInterfaceTarget_Stream_IoTargetClear(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    DMF_ContinuousRequestTarget_IoTargetClear(moduleContext->DmfModuleContinuousRequestTarget);
}

// RequestTarget Methods
// ---------------------
//

BOOLEAN
DeviceInterfaceTarget_Target_Cancel(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequestCancel DmfRequestIdCancel
    )
{
    BOOLEAN returnValue;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(! moduleContext->OpenedInStreamMode);

    returnValue = DMF_RequestTarget_Cancel(moduleContext->DmfModuleRequestTarget,
                                           DmfRequestIdCancel);

    return returnValue;
}

NTSTATUS
DeviceInterfaceTarget_Target_ReuseCreate(
    _In_ DMFMODULE DmfModule,
    _Out_ RequestTarget_DmfRequestReuse* DmfRequestIdReuse
    )
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(! moduleContext->OpenedInStreamMode);

    ntStatus = DMF_RequestTarget_ReuseCreate(moduleContext->DmfModuleRequestTarget,
                                             DmfRequestIdReuse);

    return ntStatus;
}

BOOLEAN
DeviceInterfaceTarget_Target_ReuseDelete(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequestReuse DmfRequestIdReuse
    )
{
    BOOLEAN returnValue;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(! moduleContext->OpenedInStreamMode);

    returnValue = DMF_RequestTarget_ReuseDelete(moduleContext->DmfModuleRequestTarget,
                                                DmfRequestIdReuse);

    return returnValue;
}

_Must_inspect_result_
NTSTATUS
DeviceInterfaceTarget_Target_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _Out_opt_ size_t* BytesWritten
    )
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(! moduleContext->OpenedInStreamMode);
    ntStatus = DMF_RequestTarget_SendSynchronously(moduleContext->DmfModuleRequestTarget,
                                                   RequestBuffer,
                                                   RequestLength,
                                                   ResponseBuffer,
                                                   ResponseLength,
                                                   RequestType,
                                                   RequestIoctl,
                                                   RequestTimeoutMilliseconds,
                                                   BytesWritten);

    return ntStatus;
}

_Must_inspect_result_
NTSTATUS
DeviceInterfaceTarget_Target_SendEx(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestSinkSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext,
    _Out_opt_ RequestTarget_DmfRequestCancel* DmfRequestIdCancel
    );

_Must_inspect_result_
NTSTATUS
DeviceInterfaceTarget_Target_Send(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestSinkSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    )
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(! moduleContext->OpenedInStreamMode);

    return DeviceInterfaceTarget_Target_SendEx(DmfModule,
                                               RequestBuffer,
                                               RequestLength,
                                               ResponseBuffer,
                                               ResponseLength,
                                               RequestType,
                                               RequestIoctl,
                                               RequestTimeoutMilliseconds,
                                               EvtRequestSinkSingleAsynchronousRequest,
                                               SingleAsynchronousRequestClientContext,
                                               NULL);
}

_Must_inspect_result_
NTSTATUS
DeviceInterfaceTarget_Target_SendEx(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestSinkSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext,
    _Out_opt_ RequestTarget_DmfRequestCancel* DmfRequestIdCancel
    )
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DeviceInterfaceTarget_SingleAsynchronousRequestContext* completionCallbackContext;
    NTSTATUS ntStatus;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(! moduleContext->OpenedInStreamMode);

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&completionCallbackContext,
                                  NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    completionCallbackContext->SendCompletionCallback = EvtRequestSinkSingleAsynchronousRequest;
    completionCallbackContext->SendCompletionCallbackContext = SingleAsynchronousRequestClientContext;

    ntStatus = DMF_RequestTarget_SendEx(moduleContext->DmfModuleRequestTarget,
                                        RequestBuffer,
                                        RequestLength,
                                        ResponseBuffer,
                                        ResponseLength,
                                        RequestType,
                                        RequestIoctl,
                                        RequestTimeoutMilliseconds,
                                        DeviceInterfaceTarget_SendCompletion,
                                        completionCallbackContext,
                                        DmfRequestIdCancel);
    if (!NT_SUCCESS(ntStatus))
    {
        DMF_BufferPool_Put(moduleContext->DmfModuleBufferPool,
                           completionCallbackContext);
    }

Exit:

    return ntStatus;
}

_Must_inspect_result_
NTSTATUS
DeviceInterfaceTarget_Target_ReuseSend(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequestReuse DmfRequestIdReuse,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_RequestTarget_SendCompletion* EvtRequestSinkSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext,
    _Out_opt_ RequestTarget_DmfRequestCancel* DmfRequestIdCancel
    )
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DeviceInterfaceTarget_SingleAsynchronousRequestContext* completionCallbackContext;
    NTSTATUS ntStatus;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(! moduleContext->OpenedInStreamMode);

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&completionCallbackContext,
                                  NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    completionCallbackContext->SendCompletionCallback = EvtRequestSinkSingleAsynchronousRequest;
    completionCallbackContext->SendCompletionCallbackContext = SingleAsynchronousRequestClientContext;

    ntStatus = DMF_RequestTarget_ReuseSend(moduleContext->DmfModuleRequestTarget,
                                           DmfRequestIdReuse,
                                           RequestBuffer,
                                           RequestLength,
                                           ResponseBuffer,
                                           ResponseLength,
                                           RequestType,
                                           RequestIoctl,
                                           RequestTimeoutMilliseconds,
                                           DeviceInterfaceTarget_SendCompletion,
                                           completionCallbackContext,
                                           DmfRequestIdCancel);
    if (!NT_SUCCESS(ntStatus))
    {
        DMF_BufferPool_Put(moduleContext->DmfModuleBufferPool,
                           completionCallbackContext);
    }

Exit:

    return ntStatus;
}

VOID
DeviceInterfaceTarget_Target_IoTargetSet(
    _In_ DMFMODULE DmfModule,
    _In_ WDFIOTARGET IoTarget
    )
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(! moduleContext->OpenedInStreamMode);
    DMF_RequestTarget_IoTargetSet(moduleContext->DmfModuleRequestTarget,
                                  IoTarget);
}

VOID
DeviceInterfaceTarget_Target_IoTargetClear(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(! moduleContext->OpenedInStreamMode);
    DMF_RequestTarget_IoTargetClear(moduleContext->DmfModuleRequestTarget);
}

// General Module Support Code
// ---------------------------
//

_Function_class_(EVT_DMF_ContinuousRequestTarget_BufferInput)
VOID
DeviceInterfaceTarget_Stream_BufferInput(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(*InputBufferSize) VOID* InputBuffer,
    _Out_ size_t* InputBufferSize,
    _In_ VOID* ClientBuferContextInput
    )
/*++

Routine Description:

    Redirect input buffer callback from Request Stream to Parent Module/Device. 

Arguments:

    DmfModule - ContinuousRequestTarget DMFMODULE.
    InputBuffer - The given input buffer.
    InputBufferSize - Size of the given input buffer.
    ClientBuferContextInput - Context associated with the given input buffer.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    dmfModule = DMF_ParentModuleGet(DmfModule);
    DmfAssert(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    if (moduleContext->EvtContinuousRequestTargetBufferInput != NULL)
    {
        // 'Using uninitialized memory '*InputBufferSize''
        //
        #pragma warning(disable:6001)
        moduleContext->EvtContinuousRequestTargetBufferInput(dmfModule,
                                                             InputBuffer,
                                                             InputBufferSize,
                                                             ClientBuferContextInput);
    }
    else
    {
        // For SAL.
        //
        *InputBufferSize = 0;
    }

    FuncExitVoid(DMF_TRACE);
}

_Function_class_(EVT_DMF_ContinuousRequestTarget_BufferOutput)
ContinuousRequestTarget_BufferDisposition
DeviceInterfaceTarget_Stream_BufferOutput(
    _In_ DMFMODULE DmfModule,
    _In_reads_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _In_ VOID* ClientBufferContextOutput,
    _In_ NTSTATUS CompletionStatus
    )
/*++

Routine Description:

    Redirect output buffer callback from Request Stream to Parent Module/Device. 

Arguments:

    DmfModule - ContinuousRequestTarget DMFMODULE.
    OutputBuffer - The given output buffer.
    OutputBufferSize - Size of the given output buffer.
    ClientBufferContextOutput - Context associated with the given output buffer.
    CompletionStatus - Request completion status.

Return Value:

    ContinuousRequestTarget_BufferDisposition.

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    ContinuousRequestTarget_BufferDisposition bufferDisposition;

    FuncEntry(DMF_TRACE);

    dmfModule = DMF_ParentModuleGet(DmfModule);
    DmfAssert(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    if (moduleContext->EvtContinuousRequestTargetBufferOutput != NULL)
    {
        bufferDisposition = moduleContext->EvtContinuousRequestTargetBufferOutput(dmfModule,
                                                                                  OutputBuffer,
                                                                                  OutputBufferSize,
                                                                                  ClientBufferContextOutput,
                                                                                  CompletionStatus);
    }
    else
    {
        bufferDisposition = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming;
    }

    FuncExit(DMF_TRACE, "bufferDisposition=%d", bufferDisposition);

    return bufferDisposition;
}

EVT_WDF_IO_TARGET_QUERY_REMOVE DeviceInterfaceTarget_EvtIoTargetQueryRemove;

_Function_class_(EVT_WDF_IO_TARGET_QUERY_REMOVE)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DeviceInterfaceTarget_EvtIoTargetQueryRemove(
    _In_ WDFIOTARGET IoTarget
    )
/*++

Routine Description:

    Indicates whether the framework can safely remove a specified remote I/O target's device.

Arguments:

    IoTarget - A handle to an I/O target object.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceTarget* moduleConfig;
    DMFMODULE* dmfModuleAddress;

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceInterfaceTarget_EvtIoTargetQueryRemove IoTarget=0x%p", IoTarget);

    ntStatus = STATUS_SUCCESS;

    // The IoTarget's Module Context area has the DMF Module.
    //
    dmfModuleAddress = WdfObjectGet_DMFMODULE(IoTarget);

    // If NotificationUnregister has not yet started, prevent it from starting and begin
    // removing the IoTarget.
    // If NotificationUnregister has already started, do nothing because the target will
    // is already being removed.
    //
    if (DeviceInterfaceTarget_ModuleCloseReasonSet(*dmfModuleAddress,
                                                   ModuleCloseReason_QueryRemove) == ModuleCloseReason_QueryRemove)
    {
        moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
        moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

        // If the Client has registered for device interface state changes, call the notification callback.
        //
        if (moduleConfig->EvtDeviceInterfaceTargetOnStateChange != NULL)
        {
            DmfAssert(moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx == NULL);
            moduleConfig->EvtDeviceInterfaceTargetOnStateChange(*dmfModuleAddress,
                                                                DeviceInterfaceTarget_StateType_QueryRemove);
        }
        else if (moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx != NULL)
        {
            // This version allows Client to veto the remove.
            //
            ntStatus = moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx(*dmfModuleAddress,
                                                                             DeviceInterfaceTarget_StateType_QueryRemove);
        }

        // Only stop streaming and Close the Module if Client has not vetoed QueryRemove.
        //
        if (NT_SUCCESS(ntStatus))
        {
            // Stop streaming and Close the Module.
            //
            DeviceInterfaceTarget_StreamStopAndModuleClose(*dmfModuleAddress);
        }

#if defined(DMF_USER_MODE)
        // Case where unregister happens during QueryRemove. In User-mode, it appears to
        // be necessary to delete the WDFIOTARGET now so that RemoveCancel does not happen
        // otherwise BSOD happens.
        //
        BOOLEAN closeAfterRemoveCancel;
        DMF_ModuleLock(*dmfModuleAddress);
        closeAfterRemoveCancel = moduleContext->CloseAfterRemoveCancel;
        DMF_ModuleUnlock(*dmfModuleAddress);

        if (closeAfterRemoveCancel)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceInterfaceTarget_EvtIoTargetQueryRemove Delete due to Unregistration: IoTarget=0x%p", IoTarget);
            WdfObjectDelete(IoTarget);
            DeviceInterfaceTarget_SymbolicLinkNameClear(*dmfModuleAddress);
        }
#endif
    }

    // MSDN states that STATUS_SUCCESS or STATUS_UNSUCCESSFUL must be returned.
    //
    if (! NT_SUCCESS(ntStatus))
    {
        ntStatus = STATUS_UNSUCCESSFUL;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

EVT_WDF_IO_TARGET_REMOVE_CANCELED DeviceInterfaceTarget_EvtIoTargetRemoveCancel;

VOID
DeviceInterfaceTarget_EvtIoTargetRemoveCancel(
    _In_ WDFIOTARGET IoTarget
    )
/*++

Routine Description:

    Performs operations when the removal of a specified remote I/O target is canceled.

Arguments:

    IoTarget - A handle to an I/O target object.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE* dmfModuleAddress;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceTarget* moduleConfig;
    WDF_IO_TARGET_OPEN_PARAMS openParams;

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceInterfaceTarget_EvtIoTargetRemoveCancel IoTarget=0x%p", IoTarget);

    // The IoTarget's Module Context area has the DMF Module.
    //
    dmfModuleAddress = WdfObjectGet_DMFMODULE(IoTarget);

    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

    if (moduleContext->IoTarget == NULL)
    {
        // Client did not veto QueryRemove. Reopen the WDFIOTARGET because it
        // was closed to prepare for removal.
        //

        moduleContext->IoTarget = IoTarget;

        WDF_IO_TARGET_OPEN_PARAMS_INIT_REOPEN(&openParams);
        ntStatus = WdfIoTargetOpen(moduleContext->IoTarget,
                                   &openParams);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetOpen fails: ntStatus=%!STATUS!", ntStatus);
            WdfObjectDelete(moduleContext->IoTarget);
            moduleContext->IoTarget = NULL;
            // Clear symbolic link to reset to original state before remote target appeared.
            //
            DeviceInterfaceTarget_SymbolicLinkNameClear(*dmfModuleAddress);
            // In this case, ModuleCloseReason remains set so that Close will not happen,
            // because Module is actually closed.
            //
            goto Exit;
        }

        // Reopen the Module.
        //
        ntStatus = DMF_ModuleOpen(*dmfModuleAddress);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleOpen fails: ntStatus=%!STATUS!", ntStatus);
            WdfIoTargetClose(moduleContext->IoTarget);
            WdfObjectDelete(moduleContext->IoTarget);
            moduleContext->IoTarget = NULL;
            // Clear symbolic link to reset to original state before remote target appeared.
            //
            DeviceInterfaceTarget_SymbolicLinkNameClear(*dmfModuleAddress);
            // In this case, ModuleCloseReason remains set so that Close will not happen,
            // because Module is actually closed.
            //
            goto Exit;
        }

        // Transparently restart the stream in automatic mode. This must be done before notifying the
        // Client of the state change.
        //
        if (moduleContext->ContinuousRequestTargetMode == ContinuousRequestTarget_Mode_Automatic)
        {
            ntStatus = DMF_DeviceInterfaceTarget_StreamStart(*dmfModuleAddress);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_DeviceInterfaceTarget_StreamStart fails: ntStatus=%!STATUS!", ntStatus);
                // Fall-through. (Client will detect error and deal with it.)
                //
            }
        }
    }
    else
    {
        // Client vetoed QueryRemove so WDFIOTARGET was not closed and streaming was not stopped.
        //
        DmfAssert(moduleContext->IoTarget == IoTarget);
        ntStatus = STATUS_SUCCESS;
    }

    // If the client has registered for device interface state changes, call the notification callback.
    //
    if (moduleConfig->EvtDeviceInterfaceTargetOnStateChange != NULL)
    {
        DmfAssert(moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx == NULL);
        moduleConfig->EvtDeviceInterfaceTargetOnStateChange(*dmfModuleAddress,
                                                            DeviceInterfaceTarget_StateType_RemoveCancel);
    }
    else if (moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx != NULL)
    {
        moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx(*dmfModuleAddress,
                                                              DeviceInterfaceTarget_StateType_RemoveCancel);
    }

Exit:

    // End of sequence. Allow another close to happen. Now NotificationUnregister or
    // QueryRemove can happen.
    //
    BOOLEAN closeAfterRemoveCancel;
    DMF_ModuleLock(*dmfModuleAddress);
    closeAfterRemoveCancel = moduleContext->CloseAfterRemoveCancel;
    if (moduleContext->CloseAfterRemoveCancel)
    {
        // NotificationUnregsiter happened while removing target. Now, execute that path so
        // driver can unload.
        //
        moduleContext->ModuleCloseReason = ModuleCloseReason_NotificationUnregister;
    }
    else
    {
        // Back to original state where target is running.
        // NotificationUnregister can now happen.
        //
        moduleContext->ModuleCloseReason = ModuleCloseReason_NotSet;
    }
    DMF_ModuleUnlock(*dmfModuleAddress);

    if (closeAfterRemoveCancel)
    {
        // NotificationUnregister happened during RemoveCancel. So, act as if it
        // if happened just afterward.
        //
        if (moduleConfig->EvtDeviceInterfaceTargetOnStateChange != NULL)
        {
            DmfAssert(moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx == NULL);
            moduleConfig->EvtDeviceInterfaceTargetOnStateChange(*dmfModuleAddress,
                                                                DeviceInterfaceTarget_StateType_Close);
        }
        else if (moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx != NULL)
        {
            moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx(*dmfModuleAddress,
                                                                  DeviceInterfaceTarget_StateType_Close);
        }
        // Stop streaming and close the Module only if it was successfully reopened.
        //
        if (NT_SUCCESS(ntStatus))
        {
            DeviceInterfaceTarget_StreamStopAndModuleClose(*dmfModuleAddress);
        }
    }

    FuncExitVoid(DMF_TRACE);
}

EVT_WDF_IO_TARGET_REMOVE_COMPLETE DeviceInterfaceTarget_EvtIoTargetRemoveComplete;

VOID
DeviceInterfaceTarget_EvtIoTargetRemoveComplete(
    WDFIOTARGET IoTarget
    )
/*++

Routine Description:

    Called when the Target device is removed (either the target
    received IRP_MN_REMOVE_DEVICE or IRP_MN_SURPRISE_REMOVAL)

Arguments:

    IoTarget - A handle to an I/O target object.

Return Value:

    None

--*/
{
    DMFMODULE* dmfModuleAddress;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceTarget* moduleConfig;

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceInterfaceTarget_EvtIoTargetRemoveComplete IoTarget=0x%p", IoTarget);

    // The IoTarget's Module Context area has the DMF Module.
    //
    dmfModuleAddress = WdfObjectGet_DMFMODULE(IoTarget);

    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

    // Transition from QueryRemove to RemoveRemoveComplete or
    // start IoTarget removal due to surprise removal by starting with RemoveComplete.
    // Keep preventing NotificationUnregister from closing the Module because this code
    // path will open it.
    //
    ModuleCloseReason_Type moduleCloseReason;
    moduleCloseReason = DeviceInterfaceTarget_ModuleCloseReasonSet(*dmfModuleAddress,
                                                                   ModuleCloseReason_RemoveComplete);
    if ((moduleCloseReason == ModuleCloseReason_QueryRemove) ||
        (moduleCloseReason == ModuleCloseReason_RemoveComplete))
    {
        if (moduleConfig->EvtDeviceInterfaceTargetOnStateChange != NULL)
        {
            DmfAssert(moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx == NULL);
            moduleConfig->EvtDeviceInterfaceTargetOnStateChange(*dmfModuleAddress,
                                                                DeviceInterfaceTarget_StateType_RemoveComplete);
        }
        else if (moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx != NULL)
        {
            moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx(*dmfModuleAddress,
                                                                  DeviceInterfaceTarget_StateType_RemoveComplete);
        }

        if (moduleCloseReason == ModuleCloseReason_RemoveComplete)
        {
            // QueryRemove did not happen so make sure streaming is stopped and Module is closed.
            // IoTarget will be closed and deleted during Module Close callback.
            //
            DmfAssert(IoTarget == moduleContext->IoTarget);
            DeviceInterfaceTarget_StreamStopAndModuleClose(*dmfModuleAddress);
        }
        else
        {
            // QueryRemove already closed the target. Just need to delete and clear it.
            // (This was the previously opened target that was closed during 
            // QueryRemove.)
            //
            WdfObjectDelete(IoTarget);
            // Module Close has already happened during QueryRemove. The symbolic link was
            // not cleared, so it must be cleared here.
            //
            DeviceInterfaceTarget_SymbolicLinkNameClear(*dmfModuleAddress);
        }

        // Do not allow another close to begin until after a new IoTarget has opened.
        // The Module Close Reason is reset when the Target is opened. This prevents
        // a close from happening after the target has been removed.
        //
    }

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
static
_Must_inspect_result_
NTSTATUS
DeviceInterfaceTarget_DeviceCreateNewIoTargetByName(
    _In_ DMFMODULE DmfModule,
    _In_ PUNICODE_STRING SymbolicLinkName
    )
/*++

Routine Description:

    Open the target device similar to CreateFile().

Arguments:

    Device - This driver's WDFDEVICE.
    SymbolicLinkName - The name of the device to open.
    IoTarget - The result handle.

Return Value:

    STATUS_SUCCESS or underlying failure code.

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceTarget* moduleConfig;
    WDF_IO_TARGET_OPEN_PARAMS openParams;
    WDF_OBJECT_ATTRIBUTES targetAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    device = DMF_ParentDeviceGet(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    DmfAssert(moduleContext->IoTarget == NULL);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams,
                                                SymbolicLinkName,
                                                GENERIC_READ | GENERIC_WRITE);
    openParams.ShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE;
    openParams.EvtIoTargetQueryRemove = DeviceInterfaceTarget_EvtIoTargetQueryRemove;
    openParams.EvtIoTargetRemoveCanceled = DeviceInterfaceTarget_EvtIoTargetRemoveCancel;
    openParams.EvtIoTargetRemoveComplete = DeviceInterfaceTarget_EvtIoTargetRemoveComplete;

    WDF_OBJECT_ATTRIBUTES_INIT(&targetAttributes);
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&targetAttributes,
                                           DMFMODULE);

    // Use WDF device as the parent instead of DMF Module so that the I/O target is not prematurely
    // disposed when this Module is deleted as part of a Dynamic Module tree.
    //
    targetAttributes.ParentObject = device;

    // Create an I/O target object.
    //
    ntStatus = WdfIoTargetCreate(device,
                                 &targetAttributes,
                                 &moduleContext->IoTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // NOTE: It is not possible to get the parent of a WDFIOTARGET.
    // Therefore, it is necessary to save the DmfModule in its context area.
    //
    DMF_ModuleInContextSave(moduleContext->IoTarget,
                            DmfModule);

    ntStatus = WdfIoTargetOpen(moduleContext->IoTarget,
                               &openParams);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetOpen fails: ntStatus=%!STATUS!", ntStatus);
        WdfObjectDelete(moduleContext->IoTarget);
        moduleContext->IoTarget = NULL;
        goto Exit;
    }

    if (moduleConfig->EvtDeviceInterfaceTargetOnStateChange != NULL)
    {
        DmfAssert(moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx == NULL);
        moduleConfig->EvtDeviceInterfaceTargetOnStateChange(DmfModule,
                                                            DeviceInterfaceTarget_StateType_Open);
    }
    else if (moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx != NULL)
    {
        // This version allows Client to veto the open.
        //
        ntStatus = moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx(DmfModule,
                                                                         DeviceInterfaceTarget_StateType_Open);
    }

    // Handle is still created, it must not be set to NULL so devices can still send it requests.
    //
    DmfAssert(moduleContext->IoTarget != NULL);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#if defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DeviceInterfaceTarget_TargetGet(
    _In_ VOID* Context
    )
/*++

Routine Description:

    Opens a handle to the Target device if available

Arguments:

    Context - This Module's handle.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceTarget* moduleConfig;
    DWORD cmListSize;
    WCHAR *bufferPointer;
    UNICODE_STRING targetName;
    NTSTATUS ntStatus;
    CONFIGRET configRet;

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;
    dmfModule = DMFMODULEVOID_TO_MODULE(Context);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    // Check for possible duplicate arrival message.
    //
    if (moduleContext->IoTarget != NULL)
    {
        // Already have the IoTarget. Nothing to do. Don't overwrite the target.
        // This can happen during stress on clean up.
        //
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Duplicate Arrival Interface Notification. Do Nothing");
        goto Exit;
    }

    moduleConfig = DMF_CONFIG_GET(dmfModule);

    configRet = CM_Get_Device_Interface_List_Size(&cmListSize,
                                                  &moduleConfig->DeviceInterfaceTargetGuid,
                                                  NULL,
                                                  CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (configRet != CR_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Get_Device_Interface_List_Size fails: configRet=0x%x", configRet);
        ntStatus = ERROR_NOT_FOUND;
        goto Exit;
    }

    bufferPointer = (WCHAR*)malloc(cmListSize * sizeof(WCHAR));
    if (bufferPointer != NULL)
    {
        configRet = CM_Get_Device_Interface_List(&moduleConfig->DeviceInterfaceTargetGuid,
                                                 NULL,
                                                 bufferPointer,
                                                 cmListSize,
                                                 CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
        if (configRet != CR_SUCCESS)
        {
            free(bufferPointer);
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Get_Device_Interface_List fails: configRet=0x%x", configRet);
            ntStatus = ERROR_NOT_FOUND;
            goto Exit;
        }

        RtlInitUnicodeString(&targetName,
                             bufferPointer);

        if ( moduleContext->SymbolicLinkName.Buffer == NULL)
        {
            // This is necessary for the first call after user-mode registration of the PnP callback because
            // the caller has not yet set the symbolic link.
            //
            ntStatus = DeviceInterfaceTarget_SymbolicLinkNameStore(dmfModule,
                                                                   &targetName);
            if (! NT_SUCCESS(ntStatus))
            {
                free(bufferPointer);
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceInterfaceTarget_SymbolicLinkNameStore fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
        }

        // Ask Client if this IoTarget needs to be opened if the Client 
        // requested notification.
        //
        BOOLEAN ioTargetOpen = TRUE;
        if (moduleConfig->EvtDeviceInterfaceTargetOnPnpNotification != NULL)
        {
            moduleConfig->EvtDeviceInterfaceTargetOnPnpNotification(dmfModule,
                                                                    &targetName,
                                                                    &ioTargetOpen);
        }
        if (ioTargetOpen)
        {
            ntStatus = DeviceInterfaceTarget_DeviceCreateNewIoTargetByName(dmfModule,
                                                                           &targetName);
            if (NT_SUCCESS(ntStatus))
            {
                // New open will happen. Reset this flag in case Module was previously closed.
                // Don't set it in Open() because it needs to be not cleared until Cancel logic 
                // has finished executing. Also, note that this is the INITIAL open as opposed to
                // a re-open.
                //
                moduleContext->ModuleCloseReason = ModuleCloseReason_NotSet;
                moduleContext->CloseAfterRemoveCancel = FALSE;

                ntStatus = DMF_ModuleOpen(dmfModule);
            }
        }

        free(bufferPointer);

        if (NT_SUCCESS(ntStatus))
        {
            if (moduleContext->ContinuousRequestTargetMode == ContinuousRequestTarget_Mode_Automatic)
            {
                // By calling this function here, callbacks at the Client will happen only after the Module is open.
                //
                DmfAssert(moduleContext->DmfModuleContinuousRequestTarget != NULL);
                ntStatus = DMF_ContinuousRequestTarget_Start(moduleContext->DmfModuleContinuousRequestTarget);
                if (!NT_SUCCESS(ntStatus))
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_Start fails: ntStatus=%!STATUS!", ntStatus);
                }
            }
        }
    }
    else
    {
        ntStatus = ERROR_NOT_ENOUGH_MEMORY;
    }

Exit:

    if (!NT_SUCCESS(ntStatus))
    {
        DeviceInterfaceTarget_SymbolicLinkNameClear(dmfModule);
    }

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
DWORD
DeviceInterfaceTarget_UserNotificationCallback(
    _In_ HCMNOTIFICATION hNotify,
    _In_opt_ VOID* Context,
    _In_ CM_NOTIFY_ACTION Action,
    _In_reads_bytes_(EventDataSize) PCM_NOTIFY_EVENT_DATA EventData,
    _In_ DWORD EventDataSize
    )
/*++

Routine Description:

    Callback called when the notification that is registered detects an arrival or
    removal of an instance of a registered device. This function determines if the instance
    of the device is the proper device to open, and if so, opens it.

Arguments:

    Context - This Module's handle.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceTarget* moduleConfig;
    NTSTATUS ntStatus;
    UNICODE_STRING symbolicLinkName;

    UNREFERENCED_PARAMETER(hNotify);
    UNREFERENCED_PARAMETER(EventDataSize);

    ntStatus = STATUS_SUCCESS;
    dmfModule = DMFMODULEVOID_TO_MODULE(Context);
    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    RtlInitUnicodeString(&symbolicLinkName,
                         EventData->u.DeviceInterface.SymbolicLink);

    if (Action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Interface Notification: ARRIVAL symbolicLinkName=%S", symbolicLinkName.Buffer);

        // NOTE: WDFIOTARGET may be NULL if this callback happens after QueryRemove.
        //
        if (moduleContext->SymbolicLinkName.Buffer != NULL)
        {
            TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Duplicate Arrival Interface Notification. Do Nothing");
            goto Exit;
        }

        ntStatus = DeviceInterfaceTarget_SymbolicLinkNameStore(dmfModule,
                                                               &symbolicLinkName);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceInterfaceTarget_SymbolicLinkNameStore fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // NOTE: This function does everything that Kernel-mode arrival does. The organization of
        //       the code is different because in User-mode the arrival callback is not called
        //       if the interface already exists.
        //
        ntStatus = DeviceInterfaceTarget_TargetGet(Context);
    }
    else if (Action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL)
    {
        // Verify that the device to be removed is the same as the device that was opened. 
        //
        LONG comparisonResult;

        comparisonResult = RtlCompareUnicodeString(&symbolicLinkName,
                                                   &moduleContext->SymbolicLinkName,
                                                   FALSE);
        if (comparisonResult != 0)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlCompareUnicodeStrings comparisonResult=%d", comparisonResult);
            goto Exit;
        }

        // NOTE: Targets are not closed when underlying driver disables device interface.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Interface Notification: REMOVAL symbolicLinkName=%S", symbolicLinkName.Buffer);

        // This path executes when the device interface is disabled.
        // This is different than when the underlying device is actually removed.
        //
        // If any arrival/remove path code is executing the fact that the driver is closing is remembered. 
        // After the target arrival/removal operation finishes, the Module is closed gracefully.
        //
        if (DeviceInterfaceTarget_ModuleCloseReasonSet(dmfModule,
                                                       ModuleCloseReason_NotificationRemoval) == ModuleCloseReason_NotificationRemoval)
        {
            // Module has not started closing yet. If the Module is Open, Close it.
            // It is safe to check this handle because no other path can modify it.
            //
            if (moduleContext->IoTarget != NULL)
            {
                DeviceInterfaceTarget_StreamStopAndModuleClose(dmfModule);
            }
        }
    }

Exit:

    return STATUS_SUCCESS;
}
#pragma code_seg()

#endif // defined(DMF_USER_MODE)

#if !defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_Function_class_(DRIVER_NOTIFICATION_CALLBACK_ROUTINE)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DeviceInterfaceTarget_InterfaceArrivalRemovalCallback(
    _In_ VOID* NotificationStructure,
    _Inout_opt_ VOID* Context
    )
/*++

Routine Description:

    Callback called when the notification that is registered detects an arrival or
    removal of an instance of a registered device. This function determines if the instance
    of the device is the proper device to open, and if so, opens it.

Arguments:

    Context - This Module's handle.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DEVICE_INTERFACE_CHANGE_NOTIFICATION* deviceInterfaceChangeNotification;
    DMFMODULE dmfModule;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceTarget* moduleConfig;
    BOOLEAN ioTargetOpen;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // warning C6387: 'Context' could be '0'.
    //
    #pragma warning(suppress:6387)
    dmfModule = DMFMODULEVOID_TO_MODULE(Context);
    DmfAssert(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    // Open the IoTarget by default.
    //
    ioTargetOpen = TRUE;
    deviceInterfaceChangeNotification = (PDEVICE_INTERFACE_CHANGE_NOTIFICATION)NotificationStructure;

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Found device: %S", deviceInterfaceChangeNotification->SymbolicLinkName->Buffer);

    if (DMF_Utility_IsEqualGUID((LPGUID)&(deviceInterfaceChangeNotification->Event),
                                (LPGUID)&GUID_DEVICE_INTERFACE_ARRIVAL))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Interface Notification: ARRIVAL");

        // WARNING: If the caller specifies PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
        // the operating system might call the PnP notification callback routine twice for a single
        // EventCategoryDeviceInterfaceChange event for an existing interface.
        // Can safely ignore the second call to the callback.
        // The operating system will not call the callback more than twice for a single event
        // So, if the IoTarget is already created, do nothing.
        // NOTE: WDFIOTARGET may be NULL if this callback happens after QueryRemove.
        //
        if (moduleContext->SymbolicLinkName.Buffer != NULL)
        {
            TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Duplicate Arrival Interface Notification. Do Nothing");
            goto Exit;
        }

        if (moduleConfig->EvtDeviceInterfaceTargetOnPnpNotification != NULL)
        {
            // Ask client if this IoTarget needs to be opened.
            //
            moduleConfig->EvtDeviceInterfaceTargetOnPnpNotification(dmfModule,
                                                                    deviceInterfaceChangeNotification->SymbolicLinkName,
                                                                    &ioTargetOpen);
        }

        if (ioTargetOpen)
        {
            // IoTarget will be opened. Save symbolic link name to make sure removal is referenced to correct interface.
            //
            ntStatus = DeviceInterfaceTarget_SymbolicLinkNameStore(dmfModule,
                                                                    deviceInterfaceChangeNotification->SymbolicLinkName);
            if (! NT_SUCCESS(ntStatus))
            {
                goto Exit;
            }

            // Create and open the underlying target.
            //
            ntStatus = DeviceInterfaceTarget_DeviceCreateNewIoTargetByName(dmfModule,
                                                                           deviceInterfaceChangeNotification->SymbolicLinkName);
            if (! NT_SUCCESS(ntStatus))
            {
                DeviceInterfaceTarget_SymbolicLinkNameClear(dmfModule);
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DeviceInterfaceTarget_DeviceCreateNewIoTargetByName() fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }

            // New open will happen. Reset this flag in case Module was previously closed.
            // Don't set it in Open() because it needs to be not cleared until Cancel logic 
            // has finished executing.
            //
            moduleContext->ModuleCloseReason = ModuleCloseReason_NotSet;
            moduleContext->CloseAfterRemoveCancel = FALSE;

            // The target has been opened. Perform any other operation that must be done.
            // NOTE: That this causes any children to open.
            //
            ntStatus = DMF_ModuleOpen(dmfModule);
            if (! NT_SUCCESS(ntStatus))
            {
                DeviceInterfaceTarget_SymbolicLinkNameClear(dmfModule);
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleOpen() fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }

            if (moduleContext->ContinuousRequestTargetMode == ContinuousRequestTarget_Mode_Automatic)
            {
                // By calling this function here, callbacks at the Client will happen only after the Module is open.
                //
                DmfAssert(moduleContext->DmfModuleContinuousRequestTarget != NULL);
                ntStatus = DMF_ContinuousRequestTarget_Start(moduleContext->DmfModuleContinuousRequestTarget);
                if (!NT_SUCCESS(ntStatus))
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_Start fails: ntStatus=%!STATUS!", ntStatus);
                }
            }
        }
    }
    else if (DMF_Utility_IsEqualGUID((LPGUID)&(deviceInterfaceChangeNotification->Event),
                                     (LPGUID)&GUID_DEVICE_INTERFACE_REMOVAL))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Interface Notification: REMOVAL");

        // Verify that the device to be removed is the same as the device that was opened. 
        //
        LONG comparisonResult;

        comparisonResult = RtlCompareUnicodeString(deviceInterfaceChangeNotification->SymbolicLinkName,
                                                   &moduleContext->SymbolicLinkName,
                                                   FALSE);
        if (comparisonResult != 0)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlCompareUnicodeString comparisonResult=%d", comparisonResult);
            goto Exit;
        }

        // If any arrival/remove path code is executing the fact that the driver is closing is remembered. 
        // After the target arrival/removal operation finishes, the Module is closed gracefully.
        //
        if (DeviceInterfaceTarget_ModuleCloseReasonSet(dmfModule,
                                                       ModuleCloseReason_NotificationRemoval) == ModuleCloseReason_NotificationRemoval)
        {
            // Module has not started closing yet. If the Module is Open, Close it.
            // It is safe to check this handle because no other path can modify it.
            //
            if (moduleContext->IoTarget != NULL)
            {
                DeviceInterfaceTarget_StreamStopAndModuleClose(dmfModule);
            }
        }
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid Notification. GUID=%!GUID!", &deviceInterfaceChangeNotification->Event);
        DmfAssert(FALSE);
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);
    return STATUS_SUCCESS;
}
#pragma code_seg()

#endif // !defined(DMF_USER_MODE)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#if defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_Function_class_(DMF_NotificationRegister)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_DeviceInterfaceTarget_NotificationRegisterUser(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This callback is called when the Module Open Flags indicate that the this Module
    is opened after an asynchronous notification has happened.
    (DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware or DMF_MODULE_OPEN_OPTION_NOTIFY_D0Entry)
    This callback registers the notification.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceTarget* moduleConfig;
    CM_NOTIFY_FILTER cmNotifyFilter;
    CONFIGRET configRet;

    PAGED_CODE();
    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_DeviceInterfaceTarget_NotificationRegisterUser: DmfModule=0x%p", DmfModule);

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    moduleContext->CloseAfterRemoveCancel = FALSE;

    // This function should not be not called twice.
    //
    DmfAssert(NULL == moduleContext->DeviceInterfaceNotification);

    cmNotifyFilter.cbSize = sizeof(CM_NOTIFY_FILTER);
    cmNotifyFilter.Flags = 0;
    cmNotifyFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    cmNotifyFilter.u.DeviceInterface.ClassGuid = moduleConfig->DeviceInterfaceTargetGuid;

    configRet = CM_Register_Notification(&cmNotifyFilter,
                                         (VOID*)DmfModule,
                                         (PCM_NOTIFY_CALLBACK)DeviceInterfaceTarget_UserNotificationCallback,
                                         &(moduleContext->DeviceInterfaceNotification));

    // Target device might already be there. Try now.
    // 
    if (configRet == CR_SUCCESS)
    {
        // User-mode version must call this function for interfaces that already exist when the callback 
        // above is registered (unlike Kernel-mode).
        // NOTE: Ignore return value as it this path always returns STATUS_SUCCESS.
        //
        DeviceInterfaceTarget_TargetGet(DmfModule);

        // Should always return success here since notification might be called back later.
        //
        ntStatus = STATUS_SUCCESS;

        // Register for custom notifications if the client has registered for them.
        // Ignore return value.
        //
        if (moduleConfig->EvtPnpCustomNotificationCallbackUser != NULL)
        {
            DeviceInterfaceTarget_CustomNotificationRegisterUser(DmfModule);
        }
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Register_Notification fails: configRet=0x%x", configRet);

        // Just a catchall error. Trace event configret should point to what went wrong.
        //
        ntStatus = STATUS_NOT_FOUND;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

_Function_class_(DMF_NotificationUnregister)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_DeviceInterfaceTarget_NotificationUnregisterUser(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This function is called when the TargetDevice is removed. This closes the handle to the
    target device.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NONE

--*/
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceTarget* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_DeviceInterfaceTarget_NotificationUnregisterUser: DmfModule=0x%p", DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (moduleConfig->EvtPnpCustomNotificationCallbackUser != NULL)
    {
        DeviceInterfaceTarget_CustomNotificationUnregisterUser(DmfModule);
    }

    CM_Unregister_Notification(moduleContext->DeviceInterfaceNotification);

    moduleContext->DeviceInterfaceNotification = NULL;

    // If any arrival/remove path code is executing the fact that the driver is closing is remembered. 
    // After the target arrival/removal operation finishes, the Module is closed gracefully.
    //
    if (DeviceInterfaceTarget_ModuleCloseReasonSet(DmfModule,
                                                   ModuleCloseReason_NotificationUnregister) == ModuleCloseReason_NotificationUnregister)
    {
        // Module has not started closing yet. If the Module is Open, Close it.
        // It is safe to check this handle because no other path can modify it.
        // Arrival cannot happen because notification handler is unregistered.
        //
        if (moduleContext->IoTarget != NULL)
        {
            // Inform Client that Module will close so that Client will stop communicating with Module.
            // This needs to be done before Module is closed otherwise there might be outstanding references.
            //

            if (moduleConfig->EvtDeviceInterfaceTargetOnStateChange != NULL)
            {
                DmfAssert(moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx == NULL);
                moduleConfig->EvtDeviceInterfaceTargetOnStateChange(DmfModule,
                                                                    DeviceInterfaceTarget_StateType_Close);
            }
            else if (moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx != NULL)
            {
                moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx(DmfModule,
                                                                      DeviceInterfaceTarget_StateType_Close);
            }

            DeviceInterfaceTarget_StreamStopAndModuleClose(DmfModule);
        }
    }
}

#endif // defined(DMF_USER_MODE)

#if !defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_Function_class_(DMF_NotificationRegister)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_DeviceInterfaceTarget_NotificationRegister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This callback is called when the Module Open Flags indicate that the this Module
    is opened after an asynchronous notification has happened.
    (DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware or DMF_MODULE_OPEN_OPTION_NOTIFY_D0Entry)
    This callback registers the notification.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE parentDevice;
    PDEVICE_OBJECT deviceObject;
    PDRIVER_OBJECT driverObject;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceTarget* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    moduleContext->CloseAfterRemoveCancel = FALSE;

    // This function should not be not called twice.
    //
    DmfAssert(NULL == moduleContext->DeviceInterfaceNotification);

    parentDevice = DMF_ParentDeviceGet(DmfModule);
    DmfAssert(parentDevice != NULL);
    deviceObject = WdfDeviceWdmGetDeviceObject(parentDevice);
    DmfAssert(deviceObject != NULL);
    driverObject = deviceObject->DriverObject;

    // 'Leaking memory 'moduleContext->DeviceInterfaceNotification'
    // (It is freed in DMF_DeviceInterfaceTarget_NotificationUnregister()).
    //
    #pragma warning(suppress: 6014)
    ntStatus = IoRegisterPlugPlayNotification(EventCategoryDeviceInterfaceChange,
                                              PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
                                              (void*)&moduleConfig->DeviceInterfaceTargetGuid,
                                              driverObject,
                                              (PDRIVER_NOTIFICATION_CALLBACK_ROUTINE)DeviceInterfaceTarget_InterfaceArrivalRemovalCallback,
                                              (VOID*)DmfModule,
                                              &(moduleContext->DeviceInterfaceNotification));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()
#endif // !defined(DMF_USER_MODE)

#if !defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_Function_class_(DMF_NotificationUnregister)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_DeviceInterfaceTarget_NotificationUnregister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This callback is called when the Module Open Flags indicate that the this Module
    is opened after an asynchronous notification has happened.
    (DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware or DMF_MODULE_OPEN_OPTION_NOTIFY_D0Entry)
    This callback unregisters the notification that was previously registered.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    
    // The notification routine could be called after the IoUnregisterPlugPlayNotification method 
    // has returned which was undesirable. IoUnregisterPlugPlayNotificationEx prevents the 
    // notification routine from being called after IoUnregisterPlugPlayNotificationEx returns.
    //
    if (moduleContext->DeviceInterfaceNotification != NULL)
    {
        ntStatus = IoUnregisterPlugPlayNotificationEx(moduleContext->DeviceInterfaceNotification);
        if (! NT_SUCCESS(ntStatus))
        {
            DmfAssert(FALSE);
            TraceEvents(TRACE_LEVEL_VERBOSE,
                        DMF_TRACE,
                        "IoUnregisterPlugPlayNotificationEx fails: ntStatus=%!STATUS!",
                        ntStatus);
            goto Exit;
        }

        moduleContext->DeviceInterfaceNotification = NULL;

        // If any arrival/remove path code is executing the fact that the driver is closing is remembered. 
        // After the target arrival/removal operation finishes, the Module is closed gracefully.
        //
        if (DeviceInterfaceTarget_ModuleCloseReasonSet(DmfModule,
                                                       ModuleCloseReason_NotificationUnregister) == ModuleCloseReason_NotificationUnregister)
        {
            // Module has not started closing yet. If the Module is Open, Close it.
            // It is safe to check this handle because no other path can modify it.
            // Arrival cannot happen because notification handler is unregistered.
            //
            if (moduleContext->IoTarget != NULL)
            {
                // Inform Client that Module will close so that Client will stop communicating with Module.
                // This needs to be done before Module is closed otherwise there might be outstanding references.
                //
                DMF_CONFIG_DeviceInterfaceTarget* moduleConfig = DMF_CONFIG_GET(DmfModule);

                if (moduleConfig->EvtDeviceInterfaceTargetOnStateChange != NULL)
                {
                    DmfAssert(moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx == NULL);
                    moduleConfig->EvtDeviceInterfaceTargetOnStateChange(DmfModule,
                                                                        DeviceInterfaceTarget_StateType_Close);
                }
                else if (moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx != NULL)
                {
                    moduleConfig->EvtDeviceInterfaceTargetOnStateChangeEx(DmfModule,
                                                                          DeviceInterfaceTarget_StateType_Close);
                }

                DeviceInterfaceTarget_StreamStopAndModuleClose(DmfModule);
            }
        }
    }
    else
    {
        // Allow caller to unregister notification even if it has not been registered.
        //
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);
}
#pragma code_seg()

#endif // !defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_DeviceInterfaceTarget_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type DeviceInterfaceTarget.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (DMF_IsModulePassiveLevel(DmfModule))
    {
        moduleContext->DefaultCompletionOption = ContinuousRequestTarget_CompletionOptions_Passive;
    }
    else
    {
        moduleContext->DefaultCompletionOption = ContinuousRequestTarget_CompletionOptions_Dispatch;
    }

    moduleContext->RequestSink_IoTargetSet(DmfModule,
                                           moduleContext->IoTarget);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_DeviceInterfaceTarget_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type DeviceInterfaceTarget.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceTarget* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    moduleContext->RequestSink_IoTargetClear(DmfModule);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_DeviceInterfaceTarget_Close: DmfModule=0x%p, ModuleCloseReason=%d", DmfModule, moduleContext->ModuleCloseReason);

    switch (moduleContext->ModuleCloseReason)
    {
        case ModuleCloseReason_NotificationUnregister:
        {
            // Normal close that happens without QueryRemove.
            //
            WdfIoTargetClose(moduleContext->IoTarget);
            WdfObjectDelete(moduleContext->IoTarget);
            // Delete stored symbolic link if set. (This will never be set in User-mode.)
            //
            DeviceInterfaceTarget_SymbolicLinkNameClear(DmfModule);
            break;
        }
        case ModuleCloseReason_QueryRemove:
        {
            // Close that happens after QueryRemove.
            //
            WdfIoTargetCloseForQueryRemove(moduleContext->IoTarget);
            // Do not delete the target. It may be re-opened.
            // NOTE: Module Close will not happen again. Either the
            //       IoTarget will be deleted (RemoveComplete) or the
            //       Module and underlying IoTarget will Open
            //       again (RemoveCancel).
            //
            break;
        }
        case ModuleCloseReason_RemoveComplete:
        case ModuleCloseReason_NotificationRemoval:
        {
            // This is the case where RemoveComplete happens without QueryRemove.
            // Module has been closed. Still need to Close and delete
            // the IoTarget.
            // This is also the case where the underlying target has disabled the
            // device interface.
            //
            WdfIoTargetClose(moduleContext->IoTarget);
            WdfObjectDelete(moduleContext->IoTarget);
            // Delete stored symbolic link if set. (This will never be set in User-mode.)
            //
            DeviceInterfaceTarget_SymbolicLinkNameClear(DmfModule);
            break;
        }
        case ModuleCloseReason_NotSet:
        {
            // It means PnP notification was registered and then unregistered without
            // the WDFIOTARGET opening.
            //
            break;
        }
        default:
        {
            DmfAssert(FALSE);
            break;
        }
    }

    // No other close will happen and all Methods have run down.
    // It is safe to clear now.
    //
    moduleContext->IoTarget = NULL;

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DeviceInterfaceTarget_ChildModulesAdd(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Configure and add the required Child Modules to the given Parent Module.

Arguments:

    DmfModule - The given Parent Module.
    DmfParentModuleAttributes - Pointer to the parent DMF_MODULE_ATTRIBUTES structure.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    None

--*/
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_DeviceInterfaceTarget* moduleConfig;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // BufferPoolContext
    // -----------------
    //
    DMF_CONFIG_BufferPool moduleConfigBufferPool;
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                              &moduleAttributes);
    moduleConfigBufferPool.BufferPoolMode = BufferPool_Mode_Source;
    moduleConfigBufferPool.Mode.SourceSettings.EnableLookAside = TRUE;
    moduleConfigBufferPool.Mode.SourceSettings.BufferCount = 1;
    // NOTE: BufferPool context must always be NonPagedPool because it is accessed in the
    //       completion routine running at DISPATCH_LEVEL.
    //
    moduleConfigBufferPool.Mode.SourceSettings.PoolType = NonPagedPoolNx;
    moduleConfigBufferPool.Mode.SourceSettings.BufferSize = sizeof(DeviceInterfaceTarget_SingleAsynchronousRequestContext);
    moduleAttributes.ClientModuleInstanceName = "BufferPoolContext";
    moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPool);

    // If Client has set ContinousRequestCount > 0, then it means streaming is capable.
    // Otherwise, streaming is not capable.
    //
    if (moduleConfig->ContinuousRequestTargetModuleConfig.ContinuousRequestCount > 0)
    {
        // ContinuousRequestTarget
        // -----------------------
        //
        DMF_CONFIG_ContinuousRequestTarget moduleConfigContinuousRequestTarget;

        // Store ContinuousRequestTarget callbacks from config into DeviceInterfaceTarget context for redirection.
        //
        moduleContext->EvtContinuousRequestTargetBufferInput = moduleConfig->ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput;
        moduleContext->EvtContinuousRequestTargetBufferOutput = moduleConfig->ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput;

        DMF_CONFIG_ContinuousRequestTarget_AND_ATTRIBUTES_INIT(&moduleConfigContinuousRequestTarget,
                                                               &moduleAttributes);
        // Copy ContinuousRequestTarget Config from Client's Module Config.
        //
        RtlCopyMemory(&moduleConfigContinuousRequestTarget,
                      &moduleConfig->ContinuousRequestTargetModuleConfig,
                      sizeof(moduleConfig->ContinuousRequestTargetModuleConfig));
        // Replace ContinuousRequestTarget callbacks in config with DeviceInterfaceTarget callbacks.
        //
        moduleConfigContinuousRequestTarget.EvtContinuousRequestTargetBufferInput = DeviceInterfaceTarget_Stream_BufferInput;
        moduleConfigContinuousRequestTarget.EvtContinuousRequestTargetBufferOutput = DeviceInterfaceTarget_Stream_BufferOutput;

        moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleContinuousRequestTarget);

        // Set the transport methods.
        //
        moduleContext->RequestSink_IoTargetClear = DeviceInterfaceTarget_Stream_IoTargetClear;
        moduleContext->RequestSink_IoTargetSet = DeviceInterfaceTarget_Stream_IoTargetSet;
        moduleContext->RequestSink_Send = DeviceInterfaceTarget_Stream_Send;
        moduleContext->RequestSink_SendEx = DeviceInterfaceTarget_Stream_SendEx;
        moduleContext->RequestSink_ReuseSend = DeviceInterfaceTarget_Stream_ReuseSend;
        moduleContext->RequestSink_Cancel = DeviceInterfaceTarget_Stream_Cancel;
        moduleContext->RequestSink_ReuseCreate = DeviceInterfaceTarget_Stream_ReuseCreate;
        moduleContext->RequestSink_ReuseDelete = DeviceInterfaceTarget_Stream_ReuseDelete;
        moduleContext->RequestSink_SendSynchronously = DeviceInterfaceTarget_Stream_SendSynchronously;
        moduleContext->OpenedInStreamMode = TRUE;
        // Remember Client's choice so this Module can start/stop streaming appropriately.
        //
        moduleContext->ContinuousRequestTargetMode = moduleConfig->ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode;
    }
    else
    {
        // RequestTarget
        // -------------
        //

        // Streaming functionality is not required. 
        // Create DMF_RequestTarget instead of DMF_ContinuousRequestTarget.
        //

        DMF_RequestTarget_ATTRIBUTES_INIT(&moduleAttributes);
        moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleRequestTarget);

        // Set the transport methods.
        //
        moduleContext->RequestSink_IoTargetClear = DeviceInterfaceTarget_Target_IoTargetClear;
        moduleContext->RequestSink_IoTargetSet = DeviceInterfaceTarget_Target_IoTargetSet;
        moduleContext->RequestSink_Send = DeviceInterfaceTarget_Target_Send;
        moduleContext->RequestSink_SendEx = DeviceInterfaceTarget_Target_SendEx;
        moduleContext->RequestSink_ReuseSend = DeviceInterfaceTarget_Target_ReuseSend;
        moduleContext->RequestSink_Cancel = DeviceInterfaceTarget_Target_Cancel;
        moduleContext->RequestSink_ReuseCreate = DeviceInterfaceTarget_Target_ReuseCreate;
        moduleContext->RequestSink_ReuseDelete = DeviceInterfaceTarget_Target_ReuseDelete;
        moduleContext->RequestSink_SendSynchronously = DeviceInterfaceTarget_Target_SendSynchronously;
        moduleContext->OpenedInStreamMode = FALSE;
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type DeviceInterfaceTarget.

Arguments:

    Device - Client driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_DeviceInterfaceTarget;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_DeviceInterfaceTarget;
    DmfModuleOpenOption openOption;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);
        
    // For dynamic instances, this Module will register for 
    // PnP notifications upon create. 
    //
    if (DmfModuleAttributes->DynamicModule)
    {
        openOption = DMF_MODULE_OPEN_OPTION_NOTIFY_Create;
    }
    else
    {
        openOption = DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware;
    }

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_DeviceInterfaceTarget,
                                            DeviceInterfaceTarget,
                                            DMF_CONTEXT_DeviceInterfaceTarget,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            openOption);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_DeviceInterfaceTarget);
    dmfCallbacksDmf_DeviceInterfaceTarget.DeviceOpen = DMF_DeviceInterfaceTarget_Open;
    dmfCallbacksDmf_DeviceInterfaceTarget.DeviceClose = DMF_DeviceInterfaceTarget_Close;
    dmfCallbacksDmf_DeviceInterfaceTarget.ChildModulesAdd = DMF_DeviceInterfaceTarget_ChildModulesAdd;
#if defined(DMF_USER_MODE)
    dmfCallbacksDmf_DeviceInterfaceTarget.DeviceNotificationRegister = DMF_DeviceInterfaceTarget_NotificationRegisterUser;
    dmfCallbacksDmf_DeviceInterfaceTarget.DeviceNotificationUnregister = DMF_DeviceInterfaceTarget_NotificationUnregisterUser;
#else
    dmfCallbacksDmf_DeviceInterfaceTarget.DeviceNotificationRegister = DMF_DeviceInterfaceTarget_NotificationRegister;
    dmfCallbacksDmf_DeviceInterfaceTarget.DeviceNotificationUnregister = DMF_DeviceInterfaceTarget_NotificationUnregister;
#endif // defined(DMF_USER_MODE)

    dmfModuleDescriptor_DeviceInterfaceTarget.CallbacksDmf = &dmfCallbacksDmf_DeviceInterfaceTarget;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_DeviceInterfaceTarget,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceTarget_BufferPut(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    )
/*++

Routine Description:

    Add the output buffer back to OutputBufferPool.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The buffer to add to the list.
    NOTE: This must be a properly formed buffer that was created by this Module.

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference");
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    DMF_ContinuousRequestTarget_BufferPut(moduleContext->DmfModuleContinuousRequestTarget,
                                          ClientBuffer);

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_DeviceInterfaceTarget_Cancel(
    _In_ DMFMODULE DmfModule,
    _In_ RequestTarget_DmfRequestCancel DmfRequestIdCancel
    )
/*++

Routine Description:

    Cancels a given WDFREQUEST associated with DmfRequestIdCancel.

Arguments:

    DmfModule - This Module's handle.
    DmfRequestIdCancel - The given DmfRequestIdCancel.

Return Value:

    TRUE if the given WDFREQUEST was has been canceled.
    FALSE if the given WDFREQUEST is not canceled because it has already been completed or deleted.

--*/
{
    BOOLEAN returnValue;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference");
        returnValue = FALSE;
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    returnValue = moduleContext->RequestSink_Cancel(DmfModule,
                                                    DmfRequestIdCancel);

    DMF_ModuleDereference(DmfModule);

Exit:

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceTarget_Get(
    _In_ DMFMODULE DmfModule,
    _Out_ WDFIOTARGET* IoTarget
    )
/*++

Routine Description:

    Get the IoTarget to Send Requests to.

Arguments:

    DmfModule - This Module's handle.
    IoTarget - IO Target.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceTarget);

    DmfAssert(IoTarget != NULL);
    *IoTarget = NULL;

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference");
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    DmfAssert(moduleContext->IoTarget != NULL);

    *IoTarget = moduleContext->IoTarget;

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExitVoid(DMF_TRACE);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceTarget_GuidGet(
    _In_ DMFMODULE DmfModule,
    _Out_ GUID* Guid
    )
/*++

Routine Description:

    The device interface GUID associated with this Module's WDFIOTARGET.

Arguments:

    DmfModule - This Module's handle.
    Guid - The device interface GUID associated with this Module's WDFIOTARGET.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_DeviceInterfaceTarget* moduleConfig;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceTarget);

    DmfAssert(Guid != NULL);

    ntStatus = STATUS_SUCCESS;
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    *Guid = moduleConfig->DeviceInterfaceTargetGuid;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceTarget_ReuseCreate(
    _In_ DMFMODULE DmfModule,
    _Out_ RequestTarget_DmfRequestReuse* DmfRequestIdReuse
    )
/*++

Routine Description:

    Creates a WDFREQUEST that will be reused one or more times with the "Reuse" Methods.

Arguments:

    DmfModule - This Module's handle.
    DmfRequestIdReuse - Address where the created WDFREQUEST's cookie is returned.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ntStatus = moduleContext->RequestSink_ReuseCreate(DmfModule,
                                                      DmfRequestIdReuse);

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_DeviceInterfaceTarget_ReuseDelete(
    _In_ DMFMODULE DmfModule, 
    _In_ RequestTarget_DmfRequestReuse DmfRequestIdReuse
    )
/*++

Routine Description:

    Deletes a WDFREQUEST that was previously created using "..._ReuseCreate" Method.

Arguments:

    DmfModule - This Module's handle.
    DmfRequestIdReuse - Associated cookie of the WDFREQUEST to delete.

Return Value:

    TRUE if the WDFREQUEST was found and deleted.
    FALSE if the WDFREQUEST was not found.

--*/
{
    BOOLEAN returnValue;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    returnValue = moduleContext->RequestSink_ReuseDelete(DmfModule,
                                                         DmfRequestIdReuse);

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceTarget_ReuseSend(
    _In_ DMFMODULE DmfModule,
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
    )
/*++

Routine Description:

    Reuses a given WDFREQUEST created by "...Reuse" Method. Attaches buffers, prepares it to be
    sent to WDFIOTARGET and sends it.

Arguments:

    DmfModule - This Module's handle.
    DmfRequestIdReuse - Associated cookie of the given WDFREQUEST.
    RequestBuffer - Buffer of data to attach to request to be sent.
    RequestLength - Number of bytes in RequestBuffer to send.
    ResponseBuffer - Buffer of data that is returned by the request.
    ResponseLength - Size of Response Buffer in bytes.
    RequestType - Read or Write or Ioctl
    RequestIoctl - The given IOCTL.
    RequestTimeoutMilliseconds - Timeout value in milliseconds of the transfer or zero for no timeout.
    EvtContinuousRequestTargetSingleAsynchronousRequest - Callback to be called in completion routine.
    SingleAsynchronousRequestClientContext - Client context sent in callback
    DmfRequestIdCancel - Contains a unique request Id that is sent back by the Client to cancel the asynchronous transaction.

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->IoTarget != NULL);
    ntStatus = moduleContext->RequestSink_ReuseSend(DmfModule,
                                                    DmfRequestIdReuse,
                                                    RequestBuffer,
                                                    RequestLength,
                                                    ResponseBuffer,
                                                    ResponseLength,
                                                    RequestType,
                                                    RequestIoctl,
                                                    RequestTimeoutMilliseconds,
                                                    EvtContinuousRequestTargetSingleAsynchronousRequest,
                                                    SingleAsynchronousRequestClientContext,
                                                    DmfRequestIdCancel);

    DMF_ModuleDereference(DmfModule);
     
Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceTarget_Send(
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
    )
/*++

Routine Description:

    Creates and sends an Asynchronous request to the IoTarget given a buffer, IOCTL and other information.

Arguments:

    DmfModule - This Module's handle.
    RequestBuffer - Buffer of data to attach to request to be sent.
    RequestLength - Number of bytes to in RequestBuffer to send.
    ResponseBuffer - Buffer of data that is returned by the request.
    ResponseLength - Size of Response Buffer in bytes.
    RequestType - Read or Write or Ioctl
    RequestIoctl - The given IOCTL.
    RequestTimeoutMilliseconds - Timeout value in milliseconds of the transfer or zero for no timeout.
    EvtContinuousRequestTargetSingleAsynchronousRequest - Callback to be called in completion routine.
    SingleAsynchronousRequestClientContext - Client context sent in callback

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    // This Module Method can be called when SSH is removed or being removed. The code in this function is 
    // protected due to call to ModuleAcquire.
    //
    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference");
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->IoTarget != NULL);
    ntStatus = moduleContext->RequestSink_Send(DmfModule,
                                               RequestBuffer,
                                               RequestLength,
                                               ResponseBuffer,
                                               ResponseLength,
                                               RequestType,
                                               RequestIoctl,
                                               RequestTimeoutMilliseconds,
                                               EvtContinuousRequestTargetSingleAsynchronousRequest,
                                               SingleAsynchronousRequestClientContext);

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceTarget_SendEx(
    _In_ DMFMODULE DmfModule,
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
    )
/*++

Routine Description:

    Creates and sends an Asynchronous request to the IoTarget given a buffer, IOCTL and other information.
    Once the request completes, EvtContinuousRequestTargetSingleAsynchronousRequest will be called at passive level.

Arguments:

    DmfModule - This Module's handle.
    RequestBuffer - Buffer of data to attach to request to be sent.
    RequestLength - Number of bytes to in RequestBuffer to send.
    ResponseBuffer - Buffer of data that is returned by the request.
    ResponseLength - Size of Response Buffer in bytes.
    RequestType - Read or Write or Ioctl
    RequestIoctl - The given IOCTL.
    RequestTimeoutMilliseconds - Timeout value in milliseconds of the transfer or zero for no timeout.
    EvtContinuousRequestTargetSingleAsynchronousRequest - Callback to be called in completion routine.
    SingleAsynchronousRequestClientContext - Client context sent in callback.
    DmfRequestIdCancel - Unique id associated with the underlying WDFREQUEST.

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    // This Module Method can be called when SSH is removed or being removed. The code in this function is 
    // protected due to call to ModuleAcquire.
    //
    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference");
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->IoTarget != NULL);
    ntStatus = moduleContext->RequestSink_SendEx(DmfModule,
                                                 RequestBuffer,
                                                 RequestLength,
                                                 ResponseBuffer,
                                                 ResponseLength,
                                                 RequestType,
                                                 RequestIoctl,
                                                 RequestTimeoutMilliseconds,
                                                 EvtContinuousRequestTargetSingleAsynchronousRequest,
                                                 SingleAsynchronousRequestClientContext,
                                                 DmfRequestIdCancel);

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceTarget_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _Out_opt_ size_t* BytesWritten
    )
/*++

Routine Description:

    Creates and sends a synchronous request to the IoTarget given a buffer, IOCTL and other information.

Arguments:

    DmfModule - This Module's handle.
    RequestBuffer - Buffer of data to attach to request to be sent.
    RequestLength - Number of bytes to in RequestBuffer to send.
    ResponseBuffer - Buffer of data that is returned by the request.
    ResponseLength - Size of Response Buffer in bytes.
    RequestType - Read or Write or Ioctl
    RequestIoctl - The given IOCTL.
    RequestTimeoutMilliseconds - Timeout value in milliseconds of the transfer or zero for no timeout.
    BytesWritten - Bytes returned by the transaction.

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    // This Module Method can be called when SSH is removed or being removed. The code in this function is 
    // protected due to call to ModuleAcquire.
    //
    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference");
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->IoTarget != NULL);

    ntStatus = moduleContext->RequestSink_SendSynchronously(DmfModule,
                                                            RequestBuffer,
                                                            RequestLength,
                                                            ResponseBuffer,
                                                            ResponseLength,
                                                            RequestType,
                                                            RequestIoctl,
                                                            RequestTimeoutMilliseconds,
                                                            BytesWritten);

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceTarget_StreamStart(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Starts streaming Asynchronous requests to the IoTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference");
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->IoTarget != NULL);

    DmfAssert(moduleContext->OpenedInStreamMode);
    ntStatus = DMF_ContinuousRequestTarget_Start(moduleContext->DmfModuleContinuousRequestTarget);

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DeviceInterfaceTarget_StreamStop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stops streaming Asynchronous requests to the IoTarget and Cancels all the existing requests.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference");
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->IoTarget != NULL);

    DmfAssert(moduleContext->OpenedInStreamMode);
    DMF_ContinuousRequestTarget_StopAndWait(moduleContext->DmfModuleContinuousRequestTarget);

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExitVoid(DMF_TRACE);
}

// eof: Dmf_DeviceInterfaceTarget.c
//
