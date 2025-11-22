/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_DeviceInterfaceMultipleTarget.c

Abstract:

    Creates a stream of asynchronous requests to a dynamic PnP IO Target. Also, there is support
    for sending synchronous requests to the same IO Target. The Module supports multiple instances
    of the same device interface target.

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
#include "Dmf_DeviceInterfaceMultipleTarget.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#if defined(DMF_USER_MODE)
// Specific external includes for this DMF Module.
//
#include <Guiddef.h>
#include <cfgmgr32.h>
#include <ndisguid.h>
#endif // defined(DMF_USER_MODE)

typedef struct
{
    // Underlying Device Target.
    //
    WDFIOTARGET IoTarget;
    // During QueryRemove, IoTarget is closed and set to NULL.
    // This is a copy of IoTarget so that if the driver is removed
    // right after QueryRemove but before RemoveCancel/RemoveComplete,
    // the IoTarget can be deleted.
    //
    WDFIOTARGET IoTargetForDestroyAfterQueryRemove;
    // Support proper rundown per target.
    //
    DMFMODULE DmfModuleRundown;
    // Save Symbolic Link Name to be able to deal with multiple instances of the same
    // device interface.
    //
    WDFMEMORY MemorySymbolicLink;
    UNICODE_STRING SymbolicLinkName;
    DMFMODULE DmfModuleRequestTarget;
    DeviceInterfaceMultipleTarget_Target DmfIoTarget;
    // Surprise removal path does not send a QueryRemove, only a RemoveComplete
    // notification. This flag tracks that so that RemoveComplete path properly
    // stops target and Closes Module during surprise-removal path.
    //
    BOOLEAN QueryRemoveHappened;
    // This flag tells the rest of ensures the target rundown code executes exactly one time.
    // Using this flag allows the IoTarget handle to remain set while rundown is happening,
    // but *after* the target has closed (so that all pending buffers will be canceled).
    //
    BOOLEAN TargetClosedOrClosing;
} DeviceInterfaceMultipleTarget_IoTarget;

typedef struct
{
    // Details of the Target.
    //
    DeviceInterfaceMultipleTarget_IoTarget* Target;
    // This Module's Handle
    //
    DMFMODULE DmfModuleDeviceInterfaceMultipleTarget;
} DeviceInterfaceMultipleTarget_IoTargetContext;

WDF_DECLARE_CONTEXT_TYPE(DeviceInterfaceMultipleTarget_IoTargetContext);

typedef struct
{
    // If set to TRUE, Buffer will be removed from buffer pool if found during enumeration.
    //
    BOOLEAN RemoveBuffer;
    // Data used in the enumeration callback functions.
    //
    VOID* ContextData;
    // Return the target that matches the given criteria.
    //
    VOID* TargetData;
    // BOOLEAN to specify if we should take a reference on the target.
    //
    BOOLEAN AcquireTargetRundownReference;
    // Set to TRUE in enumeration callback if buffer is found.
    //
    BOOLEAN BufferFound;
} DeviceInterfaceMultipleTarget_EnumerationContext;

// These are virtual Methods that are set based on the transport.
// These functions are common to both the Stream and Target transport.
// They are set to the correct version when the Module is created.
// NOTE: The DmfModule that is sent is the DeviceInterfaceMultipleTarget Module.
//

typedef
BOOLEAN
RequestSink_Cancel_Type(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_ RequestTarget_DmfRequestCancel DmfRequestIdCancel
    );

typedef
NTSTATUS
RequestSink_ReuseCreate_Type(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _Out_ RequestTarget_DmfRequestReuse* DmfRequestIdReuse
    );

typedef
BOOLEAN
RequestSink_ReuseDelete_Type(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_ RequestTarget_DmfRequestReuse DmfRequestIdReuse
    );

typedef
_Must_inspect_result_
NTSTATUS
RequestSink_SendSynchronously_Type(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
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
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
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
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
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
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
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
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_ WDFIOTARGET IoTarget
    );

typedef
VOID
RequestSink_IoTargetClear_Type(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target
    );

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_DeviceInterfaceMultipleTarget
{
    // Device Interface arrival/removal notification handle.
    //
#if defined(DMF_USER_MODE)
    HCMNOTIFICATION DeviceInterfaceNotification;
#else
    VOID* DeviceInterfaceNotification;
#endif // defined(DMF_USER_MODE)

    DMFMODULE DmfModuleBufferQueue;
    // Ensures that Module Open/Close are called a single time.
    //
    LONG NumberOfTargetsOpened;

    // Redirect Input buffer callback from ContinuousRequestTarget to this callback.
    //
    EVT_DMF_ContinuousRequestTarget_BufferInput* EvtContinuousRequestTargetBufferInput;
    // Redirect Output buffer callback from ContinuousRequestTarget to this callback.
    //
    EVT_DMF_ContinuousRequestTarget_BufferOutput* EvtContinuousRequestTargetBufferOutput;

    // This Module has two modes:
    // 1. Streaming is enabled and DmfModuleRequestTarget is valid.
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
    RequestSink_SendSynchronously_Type* RequestSink_SendSynchronously;
    RequestSink_Send_Type* RequestSink_Send;
    RequestSink_SendEx_Type* RequestSink_SendEx;
    RequestSink_ReuseSend_Type* RequestSink_ReuseSend;
    RequestSink_Cancel_Type* RequestSink_Cancel;
    RequestSink_ReuseCreate_Type* RequestSink_ReuseCreate;
    RequestSink_ReuseDelete_Type* RequestSink_ReuseDelete;
    RequestSink_IoTargetSet_Type* RequestSink_IoTargetSet;
    RequestSink_IoTargetClear_Type* RequestSink_IoTargetClear;

    // Passive level desired by Client. This is used to instantiate underlying Child Modules.
    //
    BOOLEAN PassiveLevel;

    // Stores callback/callback context for asynchronous sends.
    //
    DMFMODULE DmfModuleBufferPool;
} DMF_CONTEXT_DeviceInterfaceMultipleTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(DeviceInterfaceMultipleTarget)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(DeviceInterfaceMultipleTarget)

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
} DeviceInterfaceMultipleTarget_SingleAsynchronousRequestContext;

_Function_class_(EVT_DMF_RequestTarget_SendCompletion)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
DeviceInterfaceMultipleTarget_SendCompletion(
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_SingleAsynchronousRequestContext* completionCallbackContext;

    dmfModule = DMF_ParentModuleGet(DmfModuleContinuousRequestTarget);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    completionCallbackContext = (DeviceInterfaceMultipleTarget_SingleAsynchronousRequestContext*)ClientRequestContext;

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

VOID
DeviceInterfaceMultipleTarget_SymbolicLinkNameClear(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target
    )
/*++

Routine Description:

    Delete the stored symbolic link from the given context. This is needed to deal with multiple instances of
    the same device interface.

Arguments:

    DmfModule - This Module's DMF Module handle.
    Target - The given context.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    if (Target->MemorySymbolicLink != NULL)
    {
        WdfObjectDelete(Target->MemorySymbolicLink);
        Target->MemorySymbolicLink = NULL;
        Target->SymbolicLinkName.Buffer = NULL;
        Target->SymbolicLinkName.Length = 0;
        Target->SymbolicLinkName.MaximumLength = 0;
    }
}

_Must_inspect_result_
NTSTATUS
DeviceInterfaceMultipleTarget_SymbolicLinkNameStore(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_ UNICODE_STRING* SymbolicLinkName
    )
/*++

Routine Description:

    Create a copy of symbolic link name and store it in the given context. This is needed to deal with
    multiple instances of the same device interface.

Arguments:

    DmfModule - This Module's DMF Module handle.
    Target - The given context.
    SymbolicLinkName - The given symbolic link name.

Return Value:

    NTSTATUS

--*/
{
    USHORT symbolicLinkStringLength;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    NTSTATUS ntStatus;

    symbolicLinkStringLength = SymbolicLinkName->Length;
    if (0 == symbolicLinkStringLength)
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_UNSUCCESSFUL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Symbolic link name length is 0");
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;

    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               symbolicLinkStringLength + sizeof(UNICODE_NULL),
                               &Target->MemorySymbolicLink,
                               (VOID**)&Target->SymbolicLinkName.Buffer);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,  DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }
    DmfAssert(NULL != Target->SymbolicLinkName.Buffer);

    Target->SymbolicLinkName.Length = symbolicLinkStringLength;
    Target->SymbolicLinkName.MaximumLength = symbolicLinkStringLength + sizeof(UNICODE_NULL);

#if defined(DMF_USER_MODE)
    // Overwrite with string.
    //
    RtlZeroMemory(Target->SymbolicLinkName.Buffer,
                  Target->SymbolicLinkName.MaximumLength);
    RtlCopyMemory(Target->SymbolicLinkName.Buffer,
                  SymbolicLinkName->Buffer,
                  symbolicLinkStringLength);
#else
    ntStatus = RtlUnicodeStringCopy(&Target->SymbolicLinkName,
                                    SymbolicLinkName);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlUnicodeStringCopy fails: ntStatus=%!STATUS!", ntStatus);
        DeviceInterfaceMultipleTarget_SymbolicLinkNameClear(DmfModule,
                                                            Target);
        goto Exit;
    }
#endif

Exit:

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DeviceInterfaceMultipleTarget_TargetDestroy(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_opt_ WDFIOTARGET IoTarget
    )
/*++

Routine Description:

    Destroy the underlying IoTarget.
    NOTE: This code executes in two paths:
          1. QueryRemove/RemoveComplete (Underlying target is removed.)
          2. When device is removed normally (Driver disable).
    In the first case this call is not necessary because the Module has already
    been closed, but the call is benign because the IoTarget is already NULL.
    In the second path, however, this call is necessary.
    NOTE: This function is not paged because it can acquire a spinlock.

Arguments:

    DmfModule - The given Module.
    Target - Stores the IoTarget to destroy.
    IoTarget - The WDFIOTARGET to delete in cases where it has been closed, but not deleted.

Return Value:

    None

--*/
{
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // It is important to check the IoTarget because it may have been closed via
    // two asynchronous removal paths: 1. Device is removed. 2. Underlying target is removed.
    //
    BOOLEAN closeTarget;
    DMF_ModuleLock(DmfModule);
    if (!Target->TargetClosedOrClosing)
    {
        // This code path indicates that target close and rundown will start.
        // Target->IoTarget can be NULL if create/open failed.
        //
        Target->TargetClosedOrClosing = TRUE;
        closeTarget = TRUE;
    }
    else
    {
        closeTarget = FALSE;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "IoTarget=0x%p Target=0x%p closeTarget=%d Target->QueryRemoveHappened=%d", Target->IoTarget, Target, closeTarget, Target->QueryRemoveHappened);

    if (Target->QueryRemoveHappened)
    {
        // QueryRemove has happened but this call happens before RemoveCancel or RemoveComplete.
        // Setting IoTarget enforces that the target is deleted.
        //
        IoTarget = Target->IoTargetForDestroyAfterQueryRemove;
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Force WDFIOTARGET=0x%p to be deleted", IoTarget);
    }

    DMF_ModuleUnlock(DmfModule);

    if (closeTarget)
    {
        if (Target->DmfModuleRequestTarget != NULL)
        {
            if (moduleContext->ContinuousRequestTargetMode == ContinuousRequestTarget_Mode_Automatic)
            {
                // By calling this function here, callbacks at the Client will happen only before the Module is closed.
                //
                DMF_ContinuousRequestTarget_StopAndWait(Target->DmfModuleRequestTarget);
            }
        }

        // Destroy the underlying IoTarget. NOTE: This will cancel all pending requests including
        // synchronous requests. This needs to happen before Rundown waits otherwise Rundown waits
        // forever for synchronous requests.
        //
        if (Target->IoTarget != NULL)
        {
            if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange != NULL)
            {
                DmfAssert(moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChangeEx == NULL);
                moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange(DmfModule,
                                                                            Target->DmfIoTarget,
                                                                            DeviceInterfaceMultipleTarget_StateType_Close);
            }
            else if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChangeEx != NULL)
            {
                moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChangeEx(DmfModule,
                                                                              Target->DmfIoTarget,
                                                                              DeviceInterfaceMultipleTarget_StateType_Close);
            }

            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "WdfIoTargetClose(IoTarget=0x%p) Target=0x%p", Target->IoTarget, Target);
            WdfIoTargetClose(Target->IoTarget);

            // Ensure that all Methods running against this Target finish executing
            // and prevent new Methods from starting because the IoTarget will be
            // set to NULL.
            //
            if (Target->DmfModuleRundown != NULL)
            {
                // This Module is only created after the target has been opened. So, if the
                // underlying target cannot open and returns error, this Module is not created.
                // In that case, this clean up function must check to see if the handle is
                // valid otherwise a BSOD will happen.
                //
                DMF_Rundown_EndAndWait(Target->DmfModuleRundown);
            }

            // WDFIOTARGET is closed. Make sure it is deleted below.
            //
            IoTarget = Target->IoTarget;

            // Now, the Target's handle can be cleared because no other thread will use it.
            // (It is not necessary to clear it as it will be deleted just below.)
            //
            Target->IoTarget = NULL;
        }
        else
        {
            // This path means that the WDFIOTARGET appeared but the Client decided not to
            // open it or it cannot be opened.
            //
        }
    }

    // In case WDFIOTARGET was closed but not deleted, delete DmfModuleRundown now.
    //
    if (Target->DmfModuleRundown != NULL)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "WdfObjectDelete(Target->DmfModuleRundown=0x%p) Target=0x%p Target->IoTarget=0x%p", Target->DmfModuleRundown, Target, Target->IoTarget);
        WdfObjectDelete(Target->DmfModuleRundown);
        Target->DmfModuleRundown = NULL;
    }

    // Delete the associated DmfModuleRequestTarget.
    //
    if (Target->DmfModuleRequestTarget != NULL)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "WdfObjectDelete(Target->DmfModuleRequestTarget=0x%p) Target=0x%p Target->IoTarget=0x%p", Target->DmfModuleRequestTarget, Target, Target->IoTarget);
        WdfObjectDelete(Target->DmfModuleRequestTarget);
        Target->DmfModuleRequestTarget = NULL;
    }

    // In case WDFIOTARGET not previously deleted, delete it now.
    //
    if (IoTarget != NULL)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "WdfObjectDelete(IoTarget=0x%p)", IoTarget);
        WdfObjectDelete(IoTarget);
    }

    // Delete stored symbolic link if set. (This will never be set in User-mode.)
    //
    DeviceInterfaceMultipleTarget_SymbolicLinkNameClear(DmfModule,
                                                        Target);

    if (Target->DmfIoTarget)
    {
        WdfObjectDelete(Target->DmfIoTarget);
        Target->DmfIoTarget = NULL;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_BufferQueue_Reuse(Target=0x%p)", Target);
    DMF_BufferQueue_Reuse(moduleContext->DmfModuleBufferQueue,
                          (VOID *)Target);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DeviceInterfaceMultipleTarget_ModuleOpenIfNoOpenTargets(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Open the given Module there are no open Targets.

Arguments:

    DmfModule - The given Module.

Return Value:

    NSTATUS of Module's Open callback.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    LONG numberOfTargetsOpened;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    DMF_ModuleLock(DmfModule);
    DmfAssert(moduleContext->NumberOfTargetsOpened >= 0);
    moduleContext->NumberOfTargetsOpened++;
    numberOfTargetsOpened = moduleContext->NumberOfTargetsOpened;
    DmfAssert(moduleContext->NumberOfTargetsOpened >= 1);
    DMF_ModuleUnlock(DmfModule);

    if (numberOfTargetsOpened == 1)
    {
        // Open the Module.
        //
        ntStatus = DMF_ModuleOpen(DmfModule);
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceInterfaceMultipleTarget_ModuleOpenIfNoOpenTargets(DmfModule=0x%p) OPENED NumberOfTargetsOpened=%d", DmfModule, numberOfTargetsOpened);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceInterfaceMultipleTarget_ModuleOpenIfNoOpenTargets(DmfModule=0x%p) NOT OPENED NumberOfTargetsOpened=%d", DmfModule, numberOfTargetsOpened);
    }

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DeviceInterfaceMultipleTarget_ModuleCloseIfNoOpenTargets(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Close the given Module there are no open Targets.

Arguments:

    DmfModule - The given Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    LONG numberOfTargetsOpened;
    BOOLEAN callModuleClose;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);
    // Only decrement if there are open targets.
    //
    if (moduleContext->NumberOfTargetsOpened > 0)
    {
        moduleContext->NumberOfTargetsOpened--;
        if (moduleContext->NumberOfTargetsOpened == 0)
        {
            // Only Close the Module when there are no open WDFIOTARGETS.
            //
            callModuleClose = TRUE;
        }
        else
        {
            callModuleClose = FALSE;
        }
    }
    else
    {
        // Module was previously closed or never opened.
        //
        callModuleClose = FALSE;
    }
    numberOfTargetsOpened = moduleContext->NumberOfTargetsOpened;
    DMF_ModuleUnlock(DmfModule);

    if (callModuleClose)
    {
        // Close the Module.
        //
        DMF_ModuleClose(DmfModule);
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceInterfaceMultipleTarget_ModuleCloseIfNoOpenTargets(DmfModule=0x%p) CLOSED NumberOfTargetsOpened=%d", DmfModule, numberOfTargetsOpened);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceInterfaceMultipleTarget_ModuleCloseIfNoOpenTargets(DmfModule=0x%p) NOT CLOSED NumberOfTargetsOpened=%d", DmfModule, numberOfTargetsOpened);
    }
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DeviceInterfaceMultipleTarget_TargetDestroyAndCloseModule(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target
    )
/*++

Routine Description:

    Destroy the underlying IoTarget.
    Reuse the target buffer.
    Close the Module if its last target.

Arguments:

    DmfModule - The given Module.
    Target -

Return Value:

    None

--*/
{
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceInterfaceMultipleTarget_TargetDestroyAndCloseModule(Target=0x%p)", Target);

    DeviceInterfaceMultipleTarget_TargetDestroy(DmfModule,
                                                Target,
                                                Target->IoTarget);

    DeviceInterfaceMultipleTarget_ModuleCloseIfNoOpenTargets(DmfModule);
}
#pragma code_seg()

// ContinuousRequestTarget Methods
// -------------------------------
//

BOOLEAN
DeviceInterfaceMultipleTarget_Stream_Cancel(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_ RequestTarget_DmfRequestCancel DmfRequestIdCancel
    )
{
    BOOLEAN returnValue;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    returnValue = DMF_ContinuousRequestTarget_Cancel(Target->DmfModuleRequestTarget,
                                                     DmfRequestIdCancel);

    return returnValue;
}

NTSTATUS
DeviceInterfaceMultipleTarget_Stream_ReuseCreate(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _Out_ RequestTarget_DmfRequestReuse* DmfRequestIdReuse
    )
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    ntStatus = DMF_ContinuousRequestTarget_ReuseCreate(Target->DmfModuleRequestTarget,
                                                       DmfRequestIdReuse);

    return ntStatus;
}

BOOLEAN
DeviceInterfaceMultipleTarget_Stream_ReuseDelete(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_ RequestTarget_DmfRequestReuse DmfRequestIdReuse
    )
{
    BOOLEAN returnValue;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    returnValue = DMF_ContinuousRequestTarget_ReuseDelete(Target->DmfModuleRequestTarget,
                                                          DmfRequestIdReuse);

    return returnValue;
}

_Must_inspect_result_
NTSTATUS
DeviceInterfaceMultipleTarget_Stream_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    return DMF_ContinuousRequestTarget_SendSynchronously(Target->DmfModuleRequestTarget,
                                                         RequestBuffer,
                                                         RequestLength,
                                                         ResponseBuffer,
                                                         ResponseLength,
                                                         RequestType,
                                                         RequestIoctl,
                                                         RequestTimeout,
                                                         BytesWritten);
}

_Must_inspect_result_
NTSTATUS
DeviceInterfaceMultipleTarget_Stream_SendEx(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
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

_Must_inspect_result_
NTSTATUS
DeviceInterfaceMultipleTarget_Stream_Send(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
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
    return DeviceInterfaceMultipleTarget_Stream_SendEx(DmfModule,
                                                       Target,
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
DeviceInterfaceMultipleTarget_Stream_SendEx(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
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
    )
{
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_SingleAsynchronousRequestContext* completionCallbackContext;
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

    ntStatus = DMF_ContinuousRequestTarget_SendEx(Target->DmfModuleRequestTarget,
                                                  RequestBuffer,
                                                  RequestLength,
                                                  ResponseBuffer,
                                                  ResponseLength,
                                                  RequestType,
                                                  RequestIoctl,
                                                  RequestTimeoutMilliseconds,
                                                  DeviceInterfaceMultipleTarget_SendCompletion,
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
DeviceInterfaceMultipleTarget_Stream_ReuseSend(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_SingleAsynchronousRequestContext* completionCallbackContext;
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

    ntStatus = DMF_ContinuousRequestTarget_ReuseSend(Target->DmfModuleRequestTarget,
                                                     DmfRequestIdReuse,
                                                     RequestBuffer,
                                                     RequestLength,
                                                     ResponseBuffer,
                                                     ResponseLength,
                                                     RequestType,
                                                     RequestIoctl,
                                                     RequestTimeoutMilliseconds,
                                                     DeviceInterfaceMultipleTarget_SendCompletion,
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
DeviceInterfaceMultipleTarget_Stream_IoTargetSet(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_ WDFIOTARGET IoTarget
    )
{
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    DMF_ContinuousRequestTarget_IoTargetSet(Target->DmfModuleRequestTarget,
                                            IoTarget);
}

VOID
DeviceInterfaceMultipleTarget_Stream_IoTargetClear(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target
    )
{
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    DMF_ContinuousRequestTarget_IoTargetClear(Target->DmfModuleRequestTarget);
}

// RequestTarget Methods
// ---------------------
//

BOOLEAN
DeviceInterfaceMultipleTarget_Target_Cancel(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_ RequestTarget_DmfRequestCancel DmfRequestIdCancel
    )
{
    BOOLEAN returnValue;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(! moduleContext->OpenedInStreamMode);

    returnValue = DMF_RequestTarget_Cancel(Target->DmfModuleRequestTarget,
                                           DmfRequestIdCancel);

    return returnValue;
}

NTSTATUS
DeviceInterfaceMultipleTarget_Target_ReuseCreate(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _Out_ RequestTarget_DmfRequestReuse* DmfRequestIdReuse
    )
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(! moduleContext->OpenedInStreamMode);

    ntStatus = DMF_RequestTarget_ReuseCreate(Target->DmfModuleRequestTarget,
                                             DmfRequestIdReuse);

    return ntStatus;
}

BOOLEAN
DeviceInterfaceMultipleTarget_Target_ReuseDelete(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_ RequestTarget_DmfRequestReuse DmfRequestIdReuse
    )
{
    BOOLEAN returnValue;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(! moduleContext->OpenedInStreamMode);

    returnValue = DMF_RequestTarget_ReuseDelete(Target->DmfModuleRequestTarget,
                                                DmfRequestIdReuse);

    return returnValue;
}

_Must_inspect_result_
NTSTATUS
DeviceInterfaceMultipleTarget_Target_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(! moduleContext->OpenedInStreamMode);
    ntStatus = DMF_RequestTarget_SendSynchronously(Target->DmfModuleRequestTarget,
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
DeviceInterfaceMultipleTarget_Target_SendEx(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
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

_Must_inspect_result_
NTSTATUS
DeviceInterfaceMultipleTarget_Target_Send(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
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
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DeviceInterfaceMultipleTarget_Target_SendEx(DmfModule,
                                                           Target,
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

    return ntStatus;
}

_Must_inspect_result_
NTSTATUS
DeviceInterfaceMultipleTarget_Target_SendEx(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
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
    )
{
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_SingleAsynchronousRequestContext* completionCallbackContext;
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

    ntStatus = DMF_RequestTarget_SendEx(Target->DmfModuleRequestTarget,
                                        RequestBuffer,
                                        RequestLength,
                                        ResponseBuffer,
                                        ResponseLength,
                                        RequestType,
                                        RequestIoctl,
                                        RequestTimeoutMilliseconds,
                                        DeviceInterfaceMultipleTarget_SendCompletion,
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
DeviceInterfaceMultipleTarget_Target_ReuseSend(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_SingleAsynchronousRequestContext* completionCallbackContext;
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

    ntStatus = DMF_RequestTarget_ReuseSend(Target->DmfModuleRequestTarget,
                                             DmfRequestIdReuse,
                                             RequestBuffer,
                                             RequestLength,
                                             ResponseBuffer,
                                             ResponseLength,
                                             RequestType,
                                             RequestIoctl,
                                             RequestTimeoutMilliseconds,
                                             DeviceInterfaceMultipleTarget_SendCompletion,
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
DeviceInterfaceMultipleTarget_Target_IoTargetSet(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_ WDFIOTARGET IoTarget
    )
{
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(! moduleContext->OpenedInStreamMode);
    DMF_RequestTarget_IoTargetSet(Target->DmfModuleRequestTarget,
                                  IoTarget);
}

VOID
DeviceInterfaceMultipleTarget_Target_IoTargetClear(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target
    )
{
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(! moduleContext->OpenedInStreamMode);
    DMF_RequestTarget_IoTargetClear(Target->DmfModuleRequestTarget);
}

// General Module Support Code
// ---------------------------
//

_Function_class_(EVT_DMF_BufferPool_Enumeration)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BufferPool_EnumerationDispositionType
DeviceInterfaceMultipleTarget_FindTarget(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext,
    _In_opt_ VOID* ClientDriverCallbackContext
    )
/*++

Routine Description:

    Enumeration callback to check if a target is already available in the pool.

Arguments:

    DmfModule - ContinuousRequestTarget DMFMODULE.
    ClientBuffer - The given input buffer.
    ClientBufferContext - Context associated with the given input buffer.
    ClientDriverCallbackContext - Context for this enumeration.

Return Value:

    None

--*/
{
    DeviceInterfaceMultipleTarget_EnumerationContext *callbackContext;
    DeviceInterfaceMultipleTarget_IoTarget *target;
    DeviceInterfaceMultipleTarget_IoTarget *targetToCompare;
    BufferPool_EnumerationDispositionType returnValue;

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ClientBufferContext);

    target = (DeviceInterfaceMultipleTarget_IoTarget *)ClientBuffer;
    DmfAssert(target->SymbolicLinkName.Length != 0);
    DmfAssert(target->SymbolicLinkName.Buffer != NULL);
    // After RemoveComplete IoTarget is NULL because it was cleared in QueryRemove.
    //

    callbackContext = (DeviceInterfaceMultipleTarget_EnumerationContext*)ClientDriverCallbackContext;
    // 'Dereferencing NULL pointer. 'callbackContext'
    //
    #pragma warning(suppress:28182)
    targetToCompare = (DeviceInterfaceMultipleTarget_IoTarget*)callbackContext->ContextData;

    returnValue = BufferPool_EnumerationDisposition_ContinueEnumeration;

    if (target == targetToCompare)
    {
        callbackContext->BufferFound = TRUE;
        if (callbackContext->RemoveBuffer)
        {
            returnValue = BufferPool_EnumerationDisposition_RemoveAndStopEnumeration;
        }
        else
        {
            returnValue = BufferPool_EnumerationDisposition_StopEnumeration;
        }
    }

    FuncExit(DMF_TRACE, "Enumeration Disposition=%d", returnValue);

    return returnValue;
}

_Function_class_(EVT_DMF_BufferPool_Enumeration)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BufferPool_EnumerationDispositionType
DeviceInterfaceMultipleTarget_FindCookieAndVerifyValid(
    _In_ DMFMODULE DmfModuleMultipleTarget,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext,
    _In_opt_ VOID* ClientDriverCallbackContext
    )
/*++

Routine Description:

    Enumeration callback to check if a target is already available in the pool
    by using the cookie and verify that the target is still valid. This is used
    to verify that the target is still valid in case it is being removed.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The enumerated context.
    ClientBufferContext - Context associated with the ClientBuffer (not used).
    ClientDriverCallbackContext - Context for this enumeration.

Return Value:

    BufferPool_EnumerationDispositionType

--*/
{
    DeviceInterfaceMultipleTarget_EnumerationContext *callbackContext;
    DeviceInterfaceMultipleTarget_IoTarget *target;
    DeviceInterfaceMultipleTarget_Target targetToCompare;
    BufferPool_EnumerationDispositionType returnValue;

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfModuleMultipleTarget);
    UNREFERENCED_PARAMETER(ClientBufferContext);

    target = (DeviceInterfaceMultipleTarget_IoTarget *)ClientBuffer;
    DmfAssert(target->SymbolicLinkName.Length != 0);
    DmfAssert(target->SymbolicLinkName.Buffer != NULL);
    // After RemoveComplete IoTarget is NULL because it was cleared in QueryRemove.
    //

    callbackContext = (DeviceInterfaceMultipleTarget_EnumerationContext*)ClientDriverCallbackContext;
    // 'Dereferencing NULL pointer. 'callbackContext'
    //
    #pragma warning(suppress:28182)
    targetToCompare = (DeviceInterfaceMultipleTarget_Target)callbackContext->ContextData;

    returnValue = BufferPool_EnumerationDisposition_ContinueEnumeration;

    // We are only consider target in valid state, target becomes invalid if it is in the processing
    // of closing which is an asynchronous operation that is not lock. So we depend on the buffer
    // module synchronization when checking target state.
    //
    if (target->DmfIoTarget == targetToCompare)
    {
        NTSTATUS ntStatus = STATUS_SUCCESS;

        if (callbackContext->AcquireTargetRundownReference)
        {
            // Ensure Target structure is valid during the duration of this Method.
            //
            ntStatus = DMF_Rundown_Reference(target->DmfModuleRundown);
        }

        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Rundown_Reference fails: ntStatus=%!STATUS!", ntStatus);
        }
        else
        {
            callbackContext->BufferFound = TRUE;
            if (callbackContext->RemoveBuffer)
            {
                returnValue = BufferPool_EnumerationDisposition_RemoveAndStopEnumeration;
            }
            else
            {
                returnValue = BufferPool_EnumerationDisposition_StopEnumeration;
            }
            callbackContext->TargetData = (VOID*)target;
        }
    }

    FuncExit(DMF_TRACE, "Enumeration Disposition=%d", returnValue);

    return returnValue;
}

_Function_class_(EVT_DMF_BufferPool_Enumeration)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BufferPool_EnumerationDispositionType
DeviceInterfaceMultipleTarget_FindSymbolicLink(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext,
    _In_opt_ VOID* ClientDriverCallbackContext
    )
/*++

Routine Description:

    Enumeration callback to check if a target with the same symbolic link is already available in the pool.

Arguments:

    DmfModule - ContinuousRequestTarget DMFMODULE.
    ClientBuffer - The given input buffer.
    ClientBufferContext - Context associated with the given input buffer.
    ClientDriverCallbackContext - Context for this enumeration.

Return Value:

    None

--*/
{
    DeviceInterfaceMultipleTarget_IoTarget *target;
    DeviceInterfaceMultipleTarget_EnumerationContext* callbackContext;
    BufferPool_EnumerationDispositionType returnValue;
    PUNICODE_STRING symbolicLinkName;
    SIZE_T matchLength;

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ClientBufferContext);

    target = (DeviceInterfaceMultipleTarget_IoTarget *)ClientBuffer;
    DmfAssert(target->SymbolicLinkName.Length != 0);
    DmfAssert(target->SymbolicLinkName.Buffer != NULL);
    // NOTE: target->IoTarget = NULL if IoTarget could not be opened again
    //       during "RemoveCancel" path.
    //

    callbackContext = (DeviceInterfaceMultipleTarget_EnumerationContext*)ClientDriverCallbackContext;
    // 'Dereferencing NULL pointer. 'callbackContext'
    //
    #pragma warning(suppress:28182)
    symbolicLinkName = (PUNICODE_STRING)callbackContext->ContextData;

    returnValue = BufferPool_EnumerationDisposition_ContinueEnumeration;

    if (target->SymbolicLinkName.Length == symbolicLinkName->Length)
    {
        matchLength = RtlCompareMemory((VOID*)target->SymbolicLinkName.Buffer,
                                       symbolicLinkName->Buffer,
                                       target->SymbolicLinkName.Length);
        if (target->SymbolicLinkName.Length == matchLength)
        {
            callbackContext->BufferFound = TRUE;
            if (callbackContext->RemoveBuffer)
            {
                returnValue = BufferPool_EnumerationDisposition_RemoveAndStopEnumeration;
            }
            else
            {
                returnValue = BufferPool_EnumerationDisposition_StopEnumeration;
            }
        }
    }

    FuncExit(DMF_TRACE, "Enumeration Disposition=%d", BufferPool_EnumerationDisposition_RemoveAndStopEnumeration);

    return returnValue;
}

DeviceInterfaceMultipleTarget_IoTarget*
DeviceInterfaceMultipleTarget_BufferGet(
    DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Method to get the buffer associated with the given DeviceInterfaceMultipleTarget_Target handle.

Arguments:

    Target - The given DeviceInterfaceMultipleTarget_Target handle.

Return Value:

    Pointer to buffer.

--*/
{
    DeviceInterfaceMultipleTarget_IoTarget* target;
    size_t bufferSize;

    target = (DeviceInterfaceMultipleTarget_IoTarget*)WdfMemoryGetBuffer((WDFMEMORY)Target,
                                                                         &bufferSize);

    DmfAssert(bufferSize == sizeof(DeviceInterfaceMultipleTarget_IoTarget));

    return target;
}

_Function_class_(EVT_DMF_ContinuousRequestTarget_BufferInput)
VOID
DeviceInterfaceMultipleTarget_Stream_BufferInput(
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

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
DeviceInterfaceMultipleTarget_Stream_BufferOutput(
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
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

VOID
DeviceInterfaceMultipleTarget_StopTargetAndCloseModule(
    _In_ WDFIOTARGET IoTarget
    )
/*++

Routine Description:

    Stops the target and Closes the Module. This is called from QueryRemove.
    It is also called from RemoveComplete in the surprise-removal case because
    QueryRemove does not happen in that path.

Arguments:

    IoTarget - A handle to an I/O target object.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;
    DeviceInterfaceMultipleTarget_IoTargetContext* targetContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    // The IoTarget's Module Context area has the DMF Module.
    //
    targetContext = WdfObjectGet_DeviceInterfaceMultipleTarget_IoTargetContext(IoTarget);
    dmfModule = targetContext->DmfModuleDeviceInterfaceMultipleTarget;
    target = targetContext->Target;

    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    // Transparently stop the stream in automatic mode.
    //
    if (moduleContext->ContinuousRequestTargetMode == ContinuousRequestTarget_Mode_Automatic)
    {
        DMF_DeviceInterfaceMultipleTarget_StreamStop(dmfModule,
                                                     target->DmfIoTarget);
    }

    // Don't let Methods call while in QueryRemoved state.
    // This Module is only created after the target has been opened. So, if the
    // underlying target cannot open and returns error, this Module is not created.
    // In that case, this clean up function must check to see if the handle is
    // valid otherwise a BSOD will happen.
    //
    if (target->DmfModuleRundown != NULL)
    {
        DMF_Rundown_EndAndWait(target->DmfModuleRundown);
    }

    // QueryRemove will Close the Module but not remove the Target from the Queue.
    //
    DeviceInterfaceMultipleTarget_ModuleCloseIfNoOpenTargets(dmfModule);

    FuncExitVoid(DMF_TRACE);
}

EVT_WDF_IO_TARGET_QUERY_REMOVE DeviceInterfaceMultipleTarget_EvtIoTargetQueryRemove;

_Function_class_(EVT_WDF_IO_TARGET_QUERY_REMOVE)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DeviceInterfaceMultipleTarget_EvtIoTargetQueryRemove(
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
    DMFMODULE dmfModule;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;
    DeviceInterfaceMultipleTarget_IoTargetContext* targetContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;
    WDF_IO_TARGET_STATE wdfIoTargetState;

    ntStatus = STATUS_SUCCESS;

    FuncEntry(DMF_TRACE);

    wdfIoTargetState = WdfIoTargetGetState(IoTarget);
    if (wdfIoTargetState == WdfIoTargetClosedForQueryRemove)
    {
        // This can happen if PnP tries again due to some error condition.
        // it means something is wrong, but running code in this callback twice
        // results in BSOD. So avoid this path.
        //
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "Duplicate QueryRemove: IoTarget=0x%p",
                    IoTarget);
        DmfAssert(FALSE);
        goto Exit;
    }

    // The IoTarget's Module Context area has the DMF Module.
    //
    targetContext = WdfObjectGet_DeviceInterfaceMultipleTarget_IoTargetContext(IoTarget);
    dmfModule = targetContext->DmfModuleDeviceInterfaceMultipleTarget;
    target = targetContext->Target;

    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                "IoTarget=0x%p target=0x%p  DmfModuleRundown=0x%p wdfIoTargetState=%d target->IoTarget=0x%p ENTER",
                IoTarget, target, target->DmfModuleRundown, wdfIoTargetState, target->IoTarget);

    // Remember QueryRemove happened so that we can adjust for cases where it does not (surprise removal).
    //
    target->QueryRemoveHappened = TRUE;

    // If the WDFIOTARGET was opened, it must equal the WDFIOTARGET in the context.
    //
    DmfAssert((target->IoTarget == NULL) ||
              (IoTarget == target->IoTarget));

    if (target->IoTarget != NULL)
    {
        // If the Client has registered for device interface state changes, call the notification callback.
        //
        if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange != NULL)
        {
            DmfAssert(moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChangeEx == NULL);
            moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange(dmfModule,
                                                                        target->DmfIoTarget,
                                                                        DeviceInterfaceMultipleTarget_StateType_QueryRemove);
        }
        else if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChangeEx != NULL)
        {
            // This version allows Client to veto the remove.
            //
            ntStatus = moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChangeEx(dmfModule,
                                                                                     target->DmfIoTarget,
                                                                                     DeviceInterfaceMultipleTarget_StateType_QueryRemove);
        }
    }
    else
    {
        // Target was not opened so client was not initially informed of Open so do not inform client about removal.
        //
    }

    // Only stop streaming and Close the Module if Client has not vetoed QueryRemove.
    //
    if (NT_SUCCESS(ntStatus))
    {
        // Stop the target and Close the Module.
        //
        DeviceInterfaceMultipleTarget_StopTargetAndCloseModule(IoTarget);

        // Close to prepare for removal. Do this regardless of whether WDFIOTARGET could be opened
        // previously. MSDN implies this must done always.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "WdfIoTargetCloseForQueryRemove(IoTarget=0x%p) target=0x%p", IoTarget, target);
        WdfIoTargetCloseForQueryRemove(IoTarget);

        // Indicate that the target has been closed to differentiate from veto where the
        // target is still open.
        //
        target->IoTarget = NULL;

        // IoTarget will be closed, but not deleted. Save it so that it can be deleted in case
        // the driver is removed right after QueryRemove happens but before RemoveCance/RemoveComplete.
        //
        target->IoTargetForDestroyAfterQueryRemove = IoTarget;
    }

    // MSDN states that STATUS_SUCCESS or STATUS_UNSUCCESSFUL must be returned.
    //
    if (! NT_SUCCESS(ntStatus))
    {
        ntStatus = STATUS_UNSUCCESSFUL;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                "IoTarget=0x%p target=0x%p  DmfModuleRundown=0x%p wdfIoTargetState=%d target->IoTarget=0x%p EXIT",
                IoTarget, target, target->DmfModuleRundown, wdfIoTargetState, target->IoTarget);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

EVT_WDF_IO_TARGET_REMOVE_CANCELED DeviceInterfaceMultipleTarget_EvtIoTargetRemoveCancel;

VOID
DeviceInterfaceMultipleTarget_EvtIoTargetRemoveCancel(
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
    DMFMODULE dmfModule;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;
    DeviceInterfaceMultipleTarget_IoTargetContext* targetContext;
    WDF_IO_TARGET_OPEN_PARAMS openParams;
    DeviceInterfaceMultipleTarget_IoTarget* target;
    BOOLEAN informClient;

    FuncEntry(DMF_TRACE);

    // The IoTarget's Module Context area has the DMF Module.
    //
    targetContext = WdfObjectGet_DeviceInterfaceMultipleTarget_IoTargetContext(IoTarget);
    dmfModule = targetContext->DmfModuleDeviceInterfaceMultipleTarget;
    target = targetContext->Target;

    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);
    informClient = FALSE;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                "IoTarget=0x%p target=0x%p  DmfModuleRundown=0x%p QueryRemoveHappened=%d target->IoTarget=0x%p",
                IoTarget, target, target->DmfModuleRundown, target->QueryRemoveHappened, target->IoTarget);

    // Clear this flag in case it was set during QueryRemove.
    //
    target->QueryRemoveHappened = FALSE;
    target->IoTargetForDestroyAfterQueryRemove = NULL;

    if ((target->IoTarget == NULL) &&
        (target->DmfModuleRundown != NULL))
    {
        // Open has succeeded, inform client.
        //
        informClient = TRUE;

        target->IoTarget = IoTarget;

        WDF_IO_TARGET_OPEN_PARAMS_INIT_REOPEN(&openParams);

        ntStatus = WdfIoTargetOpen(IoTarget,
                                   &openParams);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetOpen fails: ntStatus=%!STATUS!", ntStatus);
            // Clear target so that Close/Delete paths do not happen as they have
            // already happened.
            //
            target->IoTarget = NULL;
            goto Exit;
        }
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "WdfIoTargetOpen(IoTarget=0x%p) STATUS_SUCCESS", IoTarget);

        // Now, the Counters which are not matched will become matched again.
        // The counters became mismatched in QueryRemove.
        //
        ntStatus = DeviceInterfaceMultipleTarget_ModuleOpenIfNoOpenTargets(dmfModule);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DeviceInterfaceMultipleTarget_ModuleOpenIfNoOpenTargets fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Rundown ended in QueryRemove. Restart again. Allow Clients to call Methods.
        //
        DMF_Rundown_Start(target->DmfModuleRundown);

        // Transparently restart the stream in automatic mode.
        // Do this before informing Client of RemoveCancel.
        //
        if (moduleContext->ContinuousRequestTargetMode == ContinuousRequestTarget_Mode_Automatic)
        {
            ntStatus = DMF_DeviceInterfaceMultipleTarget_StreamStart(dmfModule,
                                                                     target->DmfIoTarget);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_DeviceInterfaceMultipleTarget_StreamStart fails: ntStatus=%!STATUS!", ntStatus);
            }
        }
    }
    else
    {
        // QueryRemove was vetoed so target was not closed.
        //
        DmfAssert(target->IoTarget == IoTarget);
    }

    // If the client has registered for device interface state changes, call the notification callback.
    //
    if (informClient)
    {
        if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange != NULL)
        {
            DmfAssert(moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChangeEx == NULL);
            moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange(dmfModule,
                                                                        target->DmfIoTarget,
                                                                        DeviceInterfaceMultipleTarget_StateType_RemoveCancel);
        }
        else if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChangeEx != NULL)
        {
            moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChangeEx(dmfModule,
                                                                          target->DmfIoTarget,
                                                                          DeviceInterfaceMultipleTarget_StateType_RemoveCancel);
        }
    }

Exit:

    FuncExitVoid(DMF_TRACE);
}

EVT_WDF_IO_TARGET_REMOVE_COMPLETE DeviceInterfaceMultipleTarget_EvtIoTargetRemoveComplete;

VOID
DeviceInterfaceMultipleTarget_EvtIoTargetRemoveComplete(
    WDFIOTARGET IoTarget
    )
/*++

Routine Description:

    Called when the Target device is removed ( either the target
    received IRP_MN_REMOVE_DEVICE or IRP_MN_SURPRISE_REMOVAL)

Arguments:

    IoTarget - A handle to an I/O target object.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;
    DeviceInterfaceMultipleTarget_IoTargetContext* targetContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;
    DeviceInterfaceMultipleTarget_EnumerationContext callbackContext;

    FuncEntry(DMF_TRACE);

    // The IoTarget's Module Context area has the DMF Module.
    //
    targetContext = WdfObjectGet_DeviceInterfaceMultipleTarget_IoTargetContext(IoTarget);
    dmfModule = targetContext->DmfModuleDeviceInterfaceMultipleTarget;

    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    callbackContext.ContextData = (VOID*)targetContext->Target;
    callbackContext.RemoveBuffer = TRUE;
    callbackContext.BufferFound = FALSE;
    DMF_BufferQueue_Enumerate(moduleContext->DmfModuleBufferQueue,
                              DeviceInterfaceMultipleTarget_FindTarget,
                              (VOID *)&callbackContext,
                              (VOID **)&target,
                              NULL);
    if (!callbackContext.BufferFound)
    {
        // The target buffer might not be in the consumer pool if the target failed to open.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_BufferQueue_Enumerate() BufferFound=FALSE IoTarget=0x%p", IoTarget);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                "IoTarget=0x%p target=0x%p  DmfModuleRundown=0x%p QueryRemoveHappened=%d target->IoTarget=0x%p",
                IoTarget, target, target->DmfModuleRundown, target->QueryRemoveHappened, target->IoTarget);

    // If QueryRemove did not happen, close the underlying WDFIOTARGET.
    // NOTE: Do this before calling the Client's callback so that the view from the Client is the
    //       same in both QueryRemove-RemoveComplete and Remove-Complete paths.
    //
    if (!target->QueryRemoveHappened)
    {
        // Surprise Remove happened, so QueryRemove did not happen. The Target
        // still needs to be stopped and Module Closed.
        //
        DeviceInterfaceMultipleTarget_StopTargetAndCloseModule(IoTarget);
    }
    else
    {
        // Clear for next time.
        //
        target->QueryRemoveHappened = FALSE;
        target->IoTargetForDestroyAfterQueryRemove = NULL;
    }

    if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange != NULL)
    {
        DmfAssert(moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChangeEx == NULL);
        moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange(dmfModule,
                                                                    target->DmfIoTarget,
                                                                    DeviceInterfaceMultipleTarget_StateType_RemoveComplete);
    }
    else if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChangeEx != NULL)
    {
        moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChangeEx(dmfModule,
                                                                      target->DmfIoTarget,
                                                                      DeviceInterfaceMultipleTarget_StateType_RemoveComplete);
    }

    // The underlying target has been removed and is no longer accessible.
    // Module is already closed in QueryRemove path.
    //
    DeviceInterfaceMultipleTarget_TargetDestroy(dmfModule,
                                                target,
                                                IoTarget);

Exit:

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
static
VOID
DeviceInterfaceMultipleTarget_RemoveBufferFromQueue(
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* ModuleContext,
    DeviceInterfaceMultipleTarget_IoTarget* IoTarget
    )
/*++

Routine Description:

    Helper function to remove the given buffer from the buffer queue.

Arguments:

    IoTarget - A handle to an I/O target object.

Return Value:

    NONE

--*/
{
    NTSTATUS ntStatus;
    DeviceInterfaceMultipleTarget_EnumerationContext callbackContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    callbackContext.ContextData = (VOID*)IoTarget;
    callbackContext.RemoveBuffer = TRUE;
    callbackContext.BufferFound = FALSE;
    DMF_BufferQueue_Enumerate(ModuleContext->DmfModuleBufferQueue,
                              DeviceInterfaceMultipleTarget_FindTarget,
                              (VOID *)&callbackContext,
                              (VOID **)&target,
                              NULL);
    if (!callbackContext.BufferFound)
    {
        // The target buffer might not be in the consumer pool if the target failed to open.
        //
        ntStatus = STATUS_DEVICE_DOES_NOT_EXIST;
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "DMF_BufferQueue_Enumerate() BufferFound=FALSE IoTarget=0x%p", IoTarget);
        goto Exit;
    }

Exit:
    FuncExitVoid(DMF_TRACE);

    return;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DeviceInterfaceMultipleTarget_ContinuousRequestTargetCreate(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target
    )
/*++

Routine Description:

    Configure and add the required Child Modules to the given Parent Module.

Arguments:

    DmfModule - The given Parent Module.
    DmfParentModuleAttributes - Pointer to the parent DMF_MODULE_ATTRIBUTES structure.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    device = DMF_ParentDeviceGet(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;

    // If Client has set ContinousRequestCount > 0, then it means streaming is capable.
    // Otherwise, streaming is not capable.
    //
    if (moduleConfig->ContinuousRequestTargetModuleConfig.ContinuousRequestCount > 0)
    {
        // ContinuousRequestTarget
        // -----------------------
        //
        DMF_CONFIG_ContinuousRequestTarget moduleConfigContinuousRequestTarget;

        // Store ContinuousRequestTarget callbacks from config into DeviceInterfaceMultipleTarget context for redirection.
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
        // Replace ContinuousRequestTarget callbacks in config with DeviceInterfaceMultipleTarget callbacks.
        //
        moduleConfigContinuousRequestTarget.EvtContinuousRequestTargetBufferInput = DeviceInterfaceMultipleTarget_Stream_BufferInput;
        moduleConfigContinuousRequestTarget.EvtContinuousRequestTargetBufferOutput = DeviceInterfaceMultipleTarget_Stream_BufferOutput;

        moduleAttributes.PassiveLevel = moduleContext->PassiveLevel;
        ntStatus = DMF_ContinuousRequestTarget_Create(device,
                                                      &moduleAttributes,
                                                      &objectAttributes,
                                                      &Target->DmfModuleRequestTarget);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_Create fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Set the transport methods.
        //
        moduleContext->RequestSink_IoTargetClear = DeviceInterfaceMultipleTarget_Stream_IoTargetClear;
        moduleContext->RequestSink_IoTargetSet = DeviceInterfaceMultipleTarget_Stream_IoTargetSet;
        moduleContext->RequestSink_Send = DeviceInterfaceMultipleTarget_Stream_Send;
        moduleContext->RequestSink_SendEx = DeviceInterfaceMultipleTarget_Stream_SendEx;
        moduleContext->RequestSink_ReuseSend = DeviceInterfaceMultipleTarget_Stream_ReuseSend;
        moduleContext->RequestSink_Cancel = DeviceInterfaceMultipleTarget_Stream_Cancel;
        moduleContext->RequestSink_ReuseCreate = DeviceInterfaceMultipleTarget_Stream_ReuseCreate;
        moduleContext->RequestSink_ReuseDelete = DeviceInterfaceMultipleTarget_Stream_ReuseDelete;
        moduleContext->RequestSink_SendSynchronously = DeviceInterfaceMultipleTarget_Stream_SendSynchronously;
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
        moduleAttributes.PassiveLevel = moduleContext->PassiveLevel;
        ntStatus = DMF_RequestTarget_Create(device,
                                            &moduleAttributes,
                                            &objectAttributes,
                                            &Target->DmfModuleRequestTarget);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_Create fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Set the transport methods.
        //
        moduleContext->RequestSink_IoTargetClear = DeviceInterfaceMultipleTarget_Target_IoTargetClear;
        moduleContext->RequestSink_IoTargetSet = DeviceInterfaceMultipleTarget_Target_IoTargetSet;
        moduleContext->RequestSink_Send = DeviceInterfaceMultipleTarget_Target_Send;
        moduleContext->RequestSink_SendEx = DeviceInterfaceMultipleTarget_Target_SendEx;
        moduleContext->RequestSink_ReuseSend = DeviceInterfaceMultipleTarget_Target_ReuseSend;
        moduleContext->RequestSink_Cancel = DeviceInterfaceMultipleTarget_Target_Cancel;
        moduleContext->RequestSink_ReuseCreate = DeviceInterfaceMultipleTarget_Target_ReuseCreate;
        moduleContext->RequestSink_ReuseDelete = DeviceInterfaceMultipleTarget_Target_ReuseDelete;
        moduleContext->RequestSink_SendSynchronously = DeviceInterfaceMultipleTarget_Target_SendSynchronously;
        moduleContext->OpenedInStreamMode = FALSE;
    }

    // Manually delete this Module as each target is removed.
    //
    objectAttributes.ParentObject = NULL;
    DMF_Rundown_ATTRIBUTES_INIT(&moduleAttributes);
    ntStatus = DMF_Rundown_Create(device,
                                  &moduleAttributes,
                                  &objectAttributes,
                                  &Target->DmfModuleRundown);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Rundown_Create fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_Rundown_Create(Target=0x%p) DmfModuleRundown=0x%p", Target, Target->DmfModuleRundown);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DeviceInterfaceMultipleTarget_DeviceCreateNewIoTargetByName(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_ PUNICODE_STRING SymbolicLinkName
    )
/*++

Routine Description:

    Open the target device similar to CreateFile().

Arguments:

    Device - This driver's WDFDEVICE.
    Target -
    SymbolicLinkName - The name of the device to open.

Return Value:

    STATUS_SUCCESS or underlying failure code.

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;
    WDF_IO_TARGET_OPEN_PARAMS openParams;
    WDF_OBJECT_ATTRIBUTES targetAttributes;
    DeviceInterfaceMultipleTarget_IoTargetContext *targetContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    device = DMF_ParentDeviceGet(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    DmfAssert(Target->IoTarget == NULL);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams,
                                                SymbolicLinkName,
                                                GENERIC_READ | GENERIC_WRITE);
    openParams.ShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE;
    openParams.EvtIoTargetQueryRemove = DeviceInterfaceMultipleTarget_EvtIoTargetQueryRemove;
    openParams.EvtIoTargetRemoveCanceled = DeviceInterfaceMultipleTarget_EvtIoTargetRemoveCancel;
    openParams.EvtIoTargetRemoveComplete = DeviceInterfaceMultipleTarget_EvtIoTargetRemoveComplete;

    WDF_OBJECT_ATTRIBUTES_INIT(&targetAttributes);
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&targetAttributes,
                                           DeviceInterfaceMultipleTarget_IoTargetContext);
    targetAttributes.ParentObject = DmfModule;

    // Create an I/O target object.
    //
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Attempt to create WDFIOTARGET...");
    DmfAssert(Target->IoTarget == NULL);
    ntStatus = WdfIoTargetCreate(device,
                                 &targetAttributes,
                                 &Target->IoTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "WDFIOTARGET created: Target=0x%p IoTarget=0x%p", Target, Target->IoTarget);

    targetContext = WdfObjectGet_DeviceInterfaceMultipleTarget_IoTargetContext(Target->IoTarget);
    targetContext->DmfModuleDeviceInterfaceMultipleTarget = DmfModule;
    targetContext->Target = Target;

    ntStatus = WdfIoTargetOpen(Target->IoTarget,
                               &openParams);
    if (! NT_SUCCESS(ntStatus))
    {
        // WDFIOTARGET cannot be opened. Fall through to delete so that no state
        // changes happen.
        //
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetOpen fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "WdfIoTargetOpen SUCCESS: Target=0x%p IoTarget=0x%p", Target, Target->IoTarget);

    ntStatus = DeviceInterfaceMultipleTarget_ContinuousRequestTargetCreate(DmfModule,
                                                                           Target);
    if (!NT_SUCCESS(ntStatus))
    {
        // WDFIOTARGET cannot be used so close because it will be deleted and
        // not state changes will happen.
        //
        WdfIoTargetClose(Target->IoTarget);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DeviceInterfaceMultipleTarget_ContinuousRequestTargetCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    moduleContext->RequestSink_IoTargetSet(DmfModule,
                                           Target,
                                           Target->IoTarget);

    // Allow Methods to be called against the Target.
    //
    DMF_Rundown_Start(Target->DmfModuleRundown);

    if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange != NULL)
    {
        DmfAssert(moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChangeEx == NULL);
        moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange(DmfModule,
                                                                    Target->DmfIoTarget,
                                                                    DeviceInterfaceMultipleTarget_StateType_Open);
    }
    else if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChangeEx != NULL)
    {
        // This version allows Client to veto the open.
        //
        ntStatus = moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChangeEx(DmfModule,
                                                                                 Target->DmfIoTarget,
                                                                                 DeviceInterfaceMultipleTarget_StateType_Open);
    }

    // Handle is still created, it must not be set to NULL so devices can still send it requests.
    //
    DmfAssert(Target->IoTarget != NULL);

Exit:

    if (!NT_SUCCESS(ntStatus))
    {
        if (Target->IoTarget != NULL)
        {
            WdfObjectDelete(Target->IoTarget);
            Target->IoTarget = NULL;
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DeviceInterfaceMultipleTarget_InitializeIoTargetIfNeeded(
    _In_ DMFMODULE DmfModule,
    _In_ PUNICODE_STRING SymbolicLinkName
    )
/*++

Routine Description:

    Ask client if target device identified by the given device name should be opened.
    If yes, initialize the target device.

Arguments:

    DmfModule - The given Parent Module.
    SymbolicLinkName - The given device name.

Return Value:

    STATUS_SUCCESS or underlying failure code.

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;
    DeviceInterfaceMultipleTarget_EnumerationContext enumerationCallbackContext;
    BOOLEAN ioTargetOpen;
    DeviceInterfaceMultipleTarget_IoTarget* target;
    WDFIOTARGET ioTarget;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceInterfaceMultipleTarget_InitializeIoTargetIfNeeded(SymbolicLinkName=%S)", SymbolicLinkName->Buffer);

    device = DMF_ParentDeviceGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    // By default, always open the Target.
    //
    ioTargetOpen = TRUE;
    ntStatus = STATUS_SUCCESS;
    target = NULL;
    ioTarget = NULL;

    enumerationCallbackContext.ContextData = (VOID*)SymbolicLinkName;
    enumerationCallbackContext.RemoveBuffer = FALSE;
    enumerationCallbackContext.BufferFound = FALSE;
    DMF_BufferQueue_Enumerate(moduleContext->DmfModuleBufferQueue,
                              DeviceInterfaceMultipleTarget_FindSymbolicLink,
                              &enumerationCallbackContext,
                              NULL,
                              NULL);
    if (enumerationCallbackContext.BufferFound)
    {
        // Interface already part of buffer queue, just return STATUS_SUCCESS.
        //
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Duplicate Arrival Interface Notification. Do Nothing");
        goto Exit;
    }

    if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnPnpNotification != NULL)
    {
        // Ask client if this IoTarget needs to be opened.
        //
        moduleConfig->EvtDeviceInterfaceMultipleTargetOnPnpNotification(DmfModule,
                                                                        SymbolicLinkName,
                                                                        &ioTargetOpen);
    }

    if (ioTargetOpen)
    {
        WDF_OBJECT_ATTRIBUTES objectAttributes;
        WDFMEMORY dmfIoTargetMemory;

        ntStatus = DMF_BufferQueue_Fetch(moduleContext->DmfModuleBufferQueue,
                                         (VOID **)&target,
                                         NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BufferQueue_Fetch() fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Clear memory because it may not have been cleared in case of reuse.
        //
        RtlZeroMemory(target,
                      sizeof(DeviceInterfaceMultipleTarget_IoTarget));

        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        objectAttributes.ParentObject = DmfModule;

        ntStatus = WdfMemoryCreatePreallocated(&objectAttributes,
                                               (VOID *)target,
                                               sizeof(DeviceInterfaceMultipleTarget_IoTarget),
                                               &dmfIoTargetMemory);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreatePreallocated() fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        target->DmfIoTarget = (DeviceInterfaceMultipleTarget_Target)dmfIoTargetMemory;

        // Iotarget will be opened. Save symbolic link name to make sure removal is referenced to correct interface.
        //
        ntStatus = DeviceInterfaceMultipleTarget_SymbolicLinkNameStore(DmfModule,
                                                                       target,
                                                                       SymbolicLinkName);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DeviceInterfaceMultipleTarget_SymbolicLinkNameStore() fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        ntStatus = DeviceInterfaceMultipleTarget_ModuleOpenIfNoOpenTargets(DmfModule);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DeviceInterfaceMultipleTarget_ModuleOpenIfNoOpenTargets() fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // We enqueue the target as the underlying call may need to access it at this point if it is created in passive mode.
        //
        DMF_BufferQueue_Enqueue(moduleContext->DmfModuleBufferQueue,
                                (VOID *)target);

        // Create and open the underlying target.
        //
        ntStatus = DeviceInterfaceMultipleTarget_DeviceCreateNewIoTargetByName(DmfModule,
                                                                               target,
                                                                               SymbolicLinkName);
        if (!NT_SUCCESS(ntStatus))
        {
            // ioTarget is already NULL so no WDFIOTARGET will be deleted at the end of this call.
            //
            DmfAssert(ioTarget == NULL);
            DeviceInterfaceMultipleTarget_ModuleCloseIfNoOpenTargets(DmfModule);
            // Just remove from buffer queue, the DeviceInterfaceMultipleTarget_TargetDestroy call below will
            // reuse the buffer after performing cleanup.
            //
            DeviceInterfaceMultipleTarget_RemoveBufferFromQueue(moduleContext,
                                                                target);
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "DeviceInterfaceMultipleTarget_DeviceCreateNewIoTargetByName() fails: ntStatus=%!STATUS! (SymbolicLinkName=%S)",
                        ntStatus,
                        SymbolicLinkName->Buffer);
            goto Exit;
        }

        // Save so it can be destroyed if rest of code fails.
        //
        ioTarget = target->IoTarget;

        if (moduleContext->ContinuousRequestTargetMode == ContinuousRequestTarget_Mode_Automatic)
        {
            // By calling this function here, callbacks at the Client will happen only after the Module is open.
            //
            DmfAssert(target->DmfModuleRequestTarget != NULL);
            ntStatus = DMF_ContinuousRequestTarget_Start(target->DmfModuleRequestTarget);
            if (!NT_SUCCESS(ntStatus))
            {
                DeviceInterfaceMultipleTarget_ModuleCloseIfNoOpenTargets(DmfModule);
                // Just remove from buffer queue, the DeviceInterfaceMultipleTarget_TargetDestroy call below will
                // reuse the buffer after performing cleanup.
                //
                DeviceInterfaceMultipleTarget_RemoveBufferFromQueue(moduleContext,
                                                                    target);
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_Start fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
        }
    }

Exit:

    if (!NT_SUCCESS(ntStatus))
    {
        if (target != NULL)
        {
            DeviceInterfaceMultipleTarget_TargetDestroy(DmfModule,
                                                        target,
                                                        ioTarget);
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DeviceInterfaceMultipleTarget_UninitializeIoTargetIfNeeded(
    _In_ DMFMODULE DmfModule,
    _In_ PUNICODE_STRING SymbolicLinkName
    )
/*++

Routine Description:

    Check if target device identified by the given device name is opened.
    If yes, unintitialize the target device.

Arguments:

    DmfModule - The given Parent Module.
    SymbolicLinkName - The given device name.

Return Value:

    None.

--*/
{
    WDFDEVICE device;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_EnumerationContext enumerationCallbackContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceInterfaceMultipleTarget_UninitializeIoTargetIfNeeded SymbolicLinkName=%S", SymbolicLinkName->Buffer);

    device = DMF_ParentDeviceGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    enumerationCallbackContext.ContextData = (VOID*)SymbolicLinkName;
    enumerationCallbackContext.RemoveBuffer = TRUE;
    enumerationCallbackContext.BufferFound = FALSE;
    DMF_BufferQueue_Enumerate(moduleContext->DmfModuleBufferQueue,
                              DeviceInterfaceMultipleTarget_FindSymbolicLink,
                              &enumerationCallbackContext,
                              (VOID **)&target,
                              NULL);

    if (enumerationCallbackContext.BufferFound)
    {
        DmfAssert(target != NULL);
        DeviceInterfaceMultipleTarget_TargetDestroyAndCloseModule(DmfModule,
                                                                  target);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DeviceInterfaceMultipleTarget_NotificationUnregisterCleanup(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Upon notification unregister, cleanup all the targets which were not removed and uninitialized.

Arguments:

    DmfModule - The given Parent Module.

Return Value:

    STATUS_SUCCESS or underlying failure code.

--*/
{
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;
    LONG targetCount;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceInterfaceMultipleTarget_NotificationUnregisterCleanup");

    // Already unregistered from PnP notification.
    // Clean the buffer queue here since the notifications callback will no longer be called.
    //
    while ((targetCount = DMF_BufferQueue_Count(moduleContext->DmfModuleBufferQueue)) != 0)
    {
        // NOTE: targetCount may not equal moduleContext->NumberOfTargetsOpened if WDFIOTARGET
        //       failed to reopen during RemoveCancel. Thus, the number of contexts may not
        //       equal the number of targets opened.
        //
        DMF_BufferQueue_Dequeue(moduleContext->DmfModuleBufferQueue,
                                (VOID**)&target,
                                NULL);
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceInterfaceMultipleTarget_NotificationUnregisterCleanup =0x%p", target);
        DeviceInterfaceMultipleTarget_TargetDestroyAndCloseModule(DmfModule,
                                                                  target);
    }
    // NOTE: This number can be less than zero if target failed to reopen during RemoveCancel.
    //       Reset to zero for case where PrepareHardware happens after ReleaseHardware.
    //
    DmfAssert(moduleContext->NumberOfTargetsOpened <= 0);
    moduleContext->NumberOfTargetsOpened = 0;

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#if defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DeviceInterfaceMultipleTarget_InitializeTargets(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Opens a handle to the Target device if available

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;
    DWORD cmListSize;
    WCHAR *bufferPointer;
    UNICODE_STRING symbolicLinkNameString;
    NTSTATUS ntStatus;
    CONFIGRET configRet;
    PWSTR currentInterface;
    ULONG currentInterfaceLength;

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;
    bufferPointer = NULL;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    configRet = CM_Get_Device_Interface_List_Size(&cmListSize,
                                                  &moduleConfig->DeviceInterfaceMultipleTargetGuid,
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
        configRet = CM_Get_Device_Interface_List(&moduleConfig->DeviceInterfaceMultipleTargetGuid,
                                                 NULL,
                                                 bufferPointer,
                                                 cmListSize,
                                                 CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
        if (configRet != CR_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Get_Device_Interface_List fails: configRet=0x%x", configRet);
            ntStatus = ERROR_NOT_FOUND;
            goto Exit;
        }

        // Enumerate devices of this interface class
        //
        currentInterface = bufferPointer;
        currentInterfaceLength = (ULONG)wcsnlen(currentInterface,
                                                cmListSize);
        while ((currentInterfaceLength < cmListSize) && (*currentInterface != UNICODE_NULL))
        {
            RtlInitUnicodeString(&symbolicLinkNameString,
                                 bufferPointer);
            DeviceInterfaceMultipleTarget_InitializeIoTargetIfNeeded(DmfModule,
                                                                     &symbolicLinkNameString);

            currentInterfaceLength = (ULONG)wcsnlen(currentInterface,
                                                    cmListSize);
            currentInterface += ((size_t)currentInterfaceLength) + 1;
        }
    }
    else
    {
        ntStatus = ERROR_NOT_ENOUGH_MEMORY;
    }

Exit:
    if (bufferPointer != NULL)
    {
        free(bufferPointer);
    }

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
DWORD
DeviceInterfaceMultipleTarget_UserNotificationCallback(
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;
    NTSTATUS ntStatus;
    UNICODE_STRING symbolickLink;

    UNREFERENCED_PARAMETER(hNotify);
    UNREFERENCED_PARAMETER(EventData);
    UNREFERENCED_PARAMETER(EventDataSize);

    ntStatus = STATUS_SUCCESS;

    dmfModule = DMFMODULEVOID_TO_MODULE(Context);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    moduleConfig = DMF_CONFIG_GET(dmfModule);

    if (Action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL)
    {
        if (EventData->FilterType == CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE)
        {
            RtlInitUnicodeString(&symbolickLink,
                                 EventData->u.DeviceInterface.SymbolicLink);
            DeviceInterfaceMultipleTarget_InitializeIoTargetIfNeeded(dmfModule,
                                                                     &symbolickLink);
        }
    }
    else if (Action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL)
    {
        if (EventData->FilterType == CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE)
        {
            // This path executes when the device interface is disabled.
            // This is different than when the underlying device is actually removed.
            //
            RtlInitUnicodeString(&symbolickLink,
                                 EventData->u.DeviceInterface.SymbolicLink);
            DeviceInterfaceMultipleTarget_UninitializeIoTargetIfNeeded(dmfModule,
                                                                       &symbolickLink);
        }
    }

    return (DWORD)ntStatus;
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
DeviceInterfaceMultipleTarget_InterfaceArrivalCallback(
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
    DEVICE_INTERFACE_CHANGE_NOTIFICATION* deviceInterfaceChangeNotification;
    DMFMODULE dmfModule;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // warning C6387: 'Context' could be '0'.
    //
    #pragma warning(suppress:6387)
    dmfModule = DMFMODULEVOID_TO_MODULE(Context);
    DmfAssert(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);
    target = NULL;

    deviceInterfaceChangeNotification = (PDEVICE_INTERFACE_CHANGE_NOTIFICATION)NotificationStructure;

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Found device: %S", deviceInterfaceChangeNotification->SymbolicLinkName->Buffer);

    if (DMF_Utility_IsEqualGUID((LPGUID)&(deviceInterfaceChangeNotification->Event),
                                (LPGUID)&GUID_DEVICE_INTERFACE_ARRIVAL))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Arrival Interface Notification.");

        DeviceInterfaceMultipleTarget_InitializeIoTargetIfNeeded(dmfModule,
                                                                 deviceInterfaceChangeNotification->SymbolicLinkName);
    }
    else if (DMF_Utility_IsEqualGUID((LPGUID)&(deviceInterfaceChangeNotification->Event),
                                     (LPGUID)&GUID_DEVICE_INTERFACE_REMOVAL))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Removal Interface Notification.");

        DeviceInterfaceMultipleTarget_UninitializeIoTargetIfNeeded(dmfModule,
                                                                   deviceInterfaceChangeNotification->SymbolicLinkName);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid Notification. GUID=%!GUID!", &deviceInterfaceChangeNotification->Event);
        DmfAssert(FALSE);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);
    return STATUS_SUCCESS;
}
#pragma code_seg()

#endif // !defined(DMF_USER_MODE)

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ BOOLEAN AcquireTargetRundownReference,
    _Out_ DeviceInterfaceMultipleTarget_IoTarget** IoTarget
    )
/*++

Routine Description:

    Method to get the buffer associated with the given DeviceInterfaceMultipleTarget_Target handle.
    In passive mode, we first check if the buffer consumer pool has any target that matches the Target (cookie) and
    if there is one ensure it is valid. If it is valid we acquire a rundown reference on the target and return the
    IoTarget.

Arguments:

    DmfModule - The given Parent Module.
    Target - The given DeviceInterfaceMultipleTarget_Target handle.
    AcquireTargetRundownReference - Indicates if a rundown reference should be acquired.
    IoTarget - Returns a valid IoTarget

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DeviceInterfaceMultipleTarget_EnumerationContext callbackContext;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    *IoTarget = NULL;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Check if the cookie is in the list, this enumeration function checks if the list based on the cookie and only
    // returns if it is valid.
    //
    callbackContext.ContextData = (VOID*)Target;
    callbackContext.RemoveBuffer = FALSE;
    callbackContext.BufferFound = FALSE;
    callbackContext.TargetData = NULL;
    callbackContext.AcquireTargetRundownReference = AcquireTargetRundownReference;
    DMF_BufferQueue_Enumerate(moduleContext->DmfModuleBufferQueue,
                              DeviceInterfaceMultipleTarget_FindCookieAndVerifyValid,
                              (VOID *)&callbackContext,
                              NULL,
                              NULL);
    if (!callbackContext.BufferFound)
    {
        ntStatus = STATUS_DEVICE_DOES_NOT_EXIST;
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "DMF_BufferQueue_Enumerate() BufferFound=FALSE Target=0x%p", Target);
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;
    *IoTarget = (DeviceInterfaceMultipleTarget_IoTarget*)callbackContext.TargetData;

Exit:

    return ntStatus;
}

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
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_NotificationRegisterUser(
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;
    CM_NOTIFY_FILTER cmNotifyFilter;
    CONFIGRET configRet;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // This function should not be not called twice.
    //
    DmfAssert(NULL == moduleContext->DeviceInterfaceNotification);

    cmNotifyFilter.cbSize = sizeof(CM_NOTIFY_FILTER);
    cmNotifyFilter.Flags = 0;
    cmNotifyFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    cmNotifyFilter.u.DeviceInterface.ClassGuid = moduleConfig->DeviceInterfaceMultipleTargetGuid;

    configRet = CM_Register_Notification(&cmNotifyFilter,
                                         (VOID*)DmfModule,
                                         (PCM_NOTIFY_CALLBACK)DeviceInterfaceMultipleTarget_UserNotificationCallback,
                                         &(moduleContext->DeviceInterfaceNotification));

    // Target device might already be there. Try now.
    //
    if (configRet == CR_SUCCESS)
    {
        DeviceInterfaceMultipleTarget_InitializeTargets(DmfModule);

        // Should always return success here since notification might be called back later.
        //
        ntStatus = STATUS_SUCCESS;
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

_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_DeviceInterfaceMultipleTarget_NotificationUnregisterUser(
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    CM_Unregister_Notification(moduleContext->DeviceInterfaceNotification);

    moduleContext->DeviceInterfaceNotification = NULL;

    DeviceInterfaceMultipleTarget_NotificationUnregisterCleanup(DmfModule);
}

#endif // defined(DMF_USER_MODE)

#if !defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_Function_class_(DMF_NotificationRegister)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_NotificationRegister(
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // This function should not be not called twice.
    //
    DmfAssert(NULL == moduleContext->DeviceInterfaceNotification);

    parentDevice = DMF_ParentDeviceGet(DmfModule);
    DmfAssert(parentDevice != NULL);
    deviceObject = WdfDeviceWdmGetDeviceObject(parentDevice);
    DmfAssert(deviceObject != NULL);
    driverObject = deviceObject->DriverObject;

    // 'Leaking memory 'moduleContext->DeviceInterfaceNotification'
    // (It is freed in DMF_DeviceInterfaceMultipleTarget_NotificationUnregister()).
    //
    #pragma warning(suppress: 6014)
    ntStatus = IoRegisterPlugPlayNotification(EventCategoryDeviceInterfaceChange,
                                              PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
                                              (void*)&moduleConfig->DeviceInterfaceMultipleTargetGuid,
                                              driverObject,
                                              (PDRIVER_NOTIFICATION_CALLBACK_ROUTINE)DeviceInterfaceMultipleTarget_InterfaceArrivalCallback,
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
DMF_DeviceInterfaceMultipleTarget_NotificationUnregister(
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

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
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "IoUnregisterPlugPlayNotificationEx fails: ntStatus=%!STATUS!",
                        ntStatus);
            goto Exit;
        }

        moduleContext->DeviceInterfaceNotification = NULL;

        DeviceInterfaceMultipleTarget_NotificationUnregisterCleanup(DmfModule);
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
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DeviceInterfaceMultipleTarget_ChildModulesAdd(
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
    DMF_CONFIG_BufferQueue moduleBufferQueueConfigList;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Save for Dynamic Module instantiation later.
    //
    moduleContext->PassiveLevel = DmfParentModuleAttributes->PassiveLevel;

    // BufferQueue
    // -----------
    //
    DMF_CONFIG_BufferQueue_AND_ATTRIBUTES_INIT(&moduleBufferQueueConfigList,
                                               &moduleAttributes);
    moduleBufferQueueConfigList.SourceSettings.EnableLookAside = TRUE;
    moduleBufferQueueConfigList.SourceSettings.BufferCount = 0;
    moduleBufferQueueConfigList.SourceSettings.BufferSize = sizeof(DeviceInterfaceMultipleTarget_IoTarget);
    moduleBufferQueueConfigList.SourceSettings.PoolType = NonPagedPoolNx;
    moduleAttributes.ClientModuleInstanceName = "DeviceInterfaceMultipleTargetBufferQueue";
    // BufferQueue is accessed in interface arrival callbacks, which needs to execute at PASSIVE_LEVEL
    // because the symbolic link name buffer is allocated by another actor using PagedPool.
    //
    moduleAttributes.PassiveLevel = TRUE;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferQueue);

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
    moduleConfigBufferPool.Mode.SourceSettings.BufferSize = sizeof(DeviceInterfaceMultipleTarget_SingleAsynchronousRequestContext);
    moduleAttributes.ClientModuleInstanceName = "BufferPoolContext";
    moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPool);


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
DMF_DeviceInterfaceMultipleTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type DeviceInterfaceMultipleTarget.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_DeviceInterfaceMultipleTarget;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_DeviceInterfaceMultipleTarget;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;
    DmfModuleOpenOption openOption;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = (DMF_CONFIG_DeviceInterfaceMultipleTarget*)DmfModuleAttributes->ModuleConfigPointer;

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_DeviceInterfaceMultipleTarget);
    dmfCallbacksDmf_DeviceInterfaceMultipleTarget.ChildModulesAdd = DMF_DeviceInterfaceMultipleTarget_ChildModulesAdd;
#if defined(DMF_USER_MODE)
    dmfCallbacksDmf_DeviceInterfaceMultipleTarget.DeviceNotificationRegister = DMF_DeviceInterfaceMultipleTarget_NotificationRegisterUser;
    dmfCallbacksDmf_DeviceInterfaceMultipleTarget.DeviceNotificationUnregister = DMF_DeviceInterfaceMultipleTarget_NotificationUnregisterUser;
#else
    dmfCallbacksDmf_DeviceInterfaceMultipleTarget.DeviceNotificationRegister = DMF_DeviceInterfaceMultipleTarget_NotificationRegister;
    dmfCallbacksDmf_DeviceInterfaceMultipleTarget.DeviceNotificationUnregister = DMF_DeviceInterfaceMultipleTarget_NotificationUnregister;
#endif // defined(DMF_USER_MODE)

    // DeviceInterfaceMultipleTarget supports multiple open option configurations.
    // Choose the open option based on Module configuration.
    //
    switch (moduleConfig->ModuleOpenOption)
    {
    case DeviceInterfaceMultipleTarget_PnpRegisterWhen_PrepareHardware:
        openOption = DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware;
        break;
    case DeviceInterfaceMultipleTarget_PnpRegisterWhen_D0Entry:
        openOption = DMF_MODULE_OPEN_OPTION_NOTIFY_D0Entry;
        break;
    case DeviceInterfaceMultipleTarget_PnpRegisterWhen_Create:
        openOption = DMF_MODULE_OPEN_OPTION_NOTIFY_Create;
        break;
    default:
        DmfAssert(FALSE);
        openOption = DMF_MODULE_OPEN_OPTION_Invalid;
        break;
    }

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_DeviceInterfaceMultipleTarget,
                                            DeviceInterfaceMultipleTarget,
                                            DMF_CONTEXT_DeviceInterfaceMultipleTarget,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            openOption);

    dmfModuleDescriptor_DeviceInterfaceMultipleTarget.CallbacksDmf = &dmfCallbacksDmf_DeviceInterfaceMultipleTarget;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_DeviceInterfaceMultipleTarget,
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
DMF_DeviceInterfaceMultipleTarget_BufferPut(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference(DmfModule,
                                                                                      Target,
                                                                                      TRUE,
                                                                                      &target);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference fails: ntStatus=%!STATUS!",
                    ntStatus);
        DMF_ModuleDereference(DmfModule);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    DMF_ContinuousRequestTarget_BufferPut(target->DmfModuleRequestTarget,
                                          ClientBuffer);

    DMF_Rundown_Dereference(target->DmfModuleRundown);
    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_DeviceInterfaceMultipleTarget_Cancel(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ RequestTarget_DmfRequestCancel DmfRequestIdCancel
    )
/*++

Routine Description:

    Cancels a given WDFREQUEST associated with DmfRequestIdCancel that has been sent to a given Target.

Arguments:

    DmfModule - This Module's handle.
    Target - The given Target.
    DmfRequestIdCancel - The given DmfRequestIdCancel.

Return Value:

    TRUE if the given WDFREQUEST was has been canceled.
    FALSE if the given WDFREQUEST is not canceled because it has already been completed or deleted.

--*/
{
    BOOLEAN returnValue;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    NTSTATUS ntStatus;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        returnValue = FALSE;
        goto Exit;
    }

    ntStatus = DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference(DmfModule,
                                                                                      Target,
                                                                                      TRUE,
                                                                                      &target);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference fails: ntStatus=%!STATUS!",
                    ntStatus);
        returnValue = FALSE;
        DMF_ModuleDereference(DmfModule);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Needs to be checked after rundown check.
    //
    DmfAssert(target->IoTarget != NULL);
    returnValue = moduleContext->RequestSink_Cancel(DmfModule,
                                                    target,
                                                    DmfRequestIdCancel);

    DMF_Rundown_Dereference(target->DmfModuleRundown);
    DMF_ModuleDereference(DmfModule);

Exit:

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_Get(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    DmfAssert(IoTarget != NULL);
    *IoTarget = NULL;

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference(DmfModule,
                                                                                      Target,
                                                                                      TRUE,
                                                                                      &target);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference fails: ntStatus=%!STATUS!",
                    ntStatus);
        DMF_ModuleDereference(DmfModule);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // It will only be NULL if Module is closed or closing due to rundown protection.
    //
    DmfAssert(target->IoTarget != NULL);
    *IoTarget = target->IoTarget;

    DMF_Rundown_Dereference(target->DmfModuleRundown);
    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExitVoid(DMF_TRACE);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_GuidGet(
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
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    DmfAssert(Guid != NULL);

    ntStatus = STATUS_SUCCESS;
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    *Guid = moduleConfig->DeviceInterfaceMultipleTargetGuid;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_ReuseCreate(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _Out_ RequestTarget_DmfRequestReuse* DmfRequestIdReuse
    )
/*++

Routine Description:

    Creates a WDFREQUEST that will be reused one or more times with the "Reuse" Methods.
    The WDFREQUEST is associated with a specific given Target instance.

Arguments:

    DmfModule - This Module's handle.
    Target - The given Target instance.
    DmfRequestIdReuse - Address where the created WDFREQUEST's cookie is returned.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    NTSTATUS ntStatus;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ntStatus = DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference(DmfModule,
                                                                                      Target,
                                                                                      TRUE,
                                                                                      &target);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference fails: ntStatus=%!STATUS!",
                    ntStatus);
        DMF_ModuleDereference(DmfModule);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ntStatus = moduleContext->RequestSink_ReuseCreate(DmfModule,
                                                      target,
                                                      DmfRequestIdReuse);

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_DeviceInterfaceMultipleTarget_ReuseDelete(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ RequestTarget_DmfRequestReuse DmfRequestIdReuse
    )
/*++

Routine Description:

    Deletes a WDFREQUEST that was previously created using "..._ReuseCreate" Method.
    The WDFREQUEST is associated with a specific given Target instance.

Arguments:

    DmfModule - This Module's handle.
    Target - The given Target instance.
    DmfRequestIdReuse - Associated cookie of the WDFREQUEST to delete.

Return Value:

    TRUE if the WDFREQUEST was found and deleted.
    FALSE if the WDFREQUEST was not found.

--*/
{
    BOOLEAN returnValue;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Target exists until all Resuse requests are deleted.
    //
    target = DeviceInterfaceMultipleTarget_BufferGet(Target);

    // We took this reference during the create of this request.
    //
    DMF_Rundown_Dereference(target->DmfModuleRundown);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    returnValue = moduleContext->RequestSink_ReuseDelete(DmfModule,
                                                         target,
                                                         DmfRequestIdReuse);

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

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
    )
/*++

Routine Description:

    Reuses a given WDFREQUEST created by "...Reuse" Method. Attaches buffers, prepares it to be
    sent to WDFIOTARGET and sends it. The request is sent to a specific given Target instance.

Arguments:

    DmfModule - This Module's handle.
    Target - The given Target instance.
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ntStatus = DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference(DmfModule,
                                                                                      Target,
                                                                                      TRUE,
                                                                                      &target);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference fails: ntStatus=%!STATUS!",
                    ntStatus);
        DMF_ModuleDereference(DmfModule);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(target->IoTarget != NULL);
    ntStatus = moduleContext->RequestSink_ReuseSend(DmfModule,
                                                    target,
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

    DMF_Rundown_Dereference(target->DmfModuleRundown);
    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Creates and sends an Asynchronous request to the IoTarget given a buffer, IOCTL and other information
    for a given Target instance.

Arguments:

    DmfModule - This Module's handle.
    Target - The given Target instance.
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference(DmfModule,
                                                                                      Target,
                                                                                      TRUE,
                                                                                      &target);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference fails: ntStatus=%!STATUS!",
                    ntStatus);
        DMF_ModuleDereference(DmfModule);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(target->IoTarget != NULL);
    // This assert will fail if the Target is valid the it is sent to a wrong Module instance
    // that is not properly initialized.
    //
    DmfAssert(moduleContext->RequestSink_Send != NULL);
    ntStatus = moduleContext->RequestSink_Send(DmfModule,
                                               target,
                                               RequestBuffer,
                                               RequestLength,
                                               ResponseBuffer,
                                               ResponseLength,
                                               RequestType,
                                               RequestIoctl,
                                               RequestTimeoutMilliseconds,
                                               EvtContinuousRequestTargetSingleAsynchronousRequest,
                                               SingleAsynchronousRequestClientContext);

    DMF_Rundown_Dereference(target->DmfModuleRundown);
    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Creates and sends an Asynchronous request to the IoTarget given a buffer, IOCTL and other information
    for a given Target instance.

Arguments:

    DmfModule - This Module's handle.
    Target - The given Target instance.
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference(DmfModule,
                                                                                      Target,
                                                                                      TRUE,
                                                                                      &target);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference fails: ntStatus=%!STATUS!",
                    ntStatus);
        DMF_ModuleDereference(DmfModule);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(target->IoTarget != NULL);
    // This assert will fail if the Target is valid the it is sent to a wrong Module instance
    // that is not properly initialized.
    //
    DmfAssert(moduleContext->RequestSink_SendEx != NULL);
    ntStatus = moduleContext->RequestSink_SendEx(DmfModule,
                                                 target,
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

    DMF_Rundown_Dereference(target->DmfModuleRundown);
    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Creates and sends a synchronous request to the IoTarget given a buffer, IOCTL and other information
    for a given Target instance.

Arguments:

    DmfModule - This Module's handle.
    Target - The given Target instance.
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;
    BOOLEAN moduleReferenced = FALSE;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }
    moduleReferenced = TRUE;

    ntStatus = DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference(DmfModule,
                                                                                      Target,
                                                                                      TRUE,
                                                                                      &target);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(target->IoTarget != NULL);

    ntStatus = moduleContext->RequestSink_SendSynchronously(DmfModule,
                                                            target,
                                                            RequestBuffer,
                                                            RequestLength,
                                                            ResponseBuffer,
                                                            ResponseLength,
                                                            RequestType,
                                                            RequestIoctl,
                                                            RequestTimeoutMilliseconds,
                                                            BytesWritten);

    DMF_Rundown_Dereference(target->DmfModuleRundown);

Exit:
    if (moduleReferenced)
    {
        DMF_ModuleDereference(DmfModule);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_StreamStart(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Starts streaming Asynchronous requests to the IoTarget for a given Target instance.


Arguments:

    DmfModule - This Module's handle.
    Target - The given Target instance.

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference(DmfModule,
                                                                                      Target,
                                                                                      TRUE,
                                                                                      &target);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference fails: ntStatus=%!STATUS!",
                    ntStatus);
        DMF_ModuleDereference(DmfModule);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(target->IoTarget != NULL);

    DmfAssert(moduleContext->OpenedInStreamMode);
    ntStatus = DMF_ContinuousRequestTarget_Start(target->DmfModuleRequestTarget);

    DMF_Rundown_Dereference(target->DmfModuleRundown);
    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_DeviceInterfaceMultipleTarget_StreamStop(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Stops streaming Asynchronous requests to the IoTarget and Cancels all the existing requests
    for a given Target instance.

Arguments:

    DmfModule - This Module's handle.
    Target - The given Target instance.

Return Value:

    VOID.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference(DmfModule,
                                                                                      Target,
                                                                                      TRUE,
                                                                                      &target);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference fails: ntStatus=%!STATUS!",
                    ntStatus);
        DMF_ModuleDereference(DmfModule);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(target->IoTarget != NULL);

    DmfAssert(moduleContext->OpenedInStreamMode);
    DMF_ContinuousRequestTarget_StopAndWait(target->DmfModuleRequestTarget);

    DMF_Rundown_Dereference(target->DmfModuleRundown);
    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_DeviceInterfaceMultipleTarget_TargetDereference(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Release a reference to the underlying WFIOTARGET.
    NOTE: Client cannot use DMF_ModuleDereference() because that is per Module, not per WDFIOTARGET.

Arguments:

    DmfModule - This Module's handle.
    Target - Target to release reference for.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    ntStatus = DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference(DmfModule,
                                                                                      Target,
                                                                                      FALSE,
                                                                                      &target);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

Exit:

    // Dereference for the purpose of this Method.
    //
    DMF_Rundown_Dereference(target->DmfModuleRundown);
    DMF_ModuleDereference(DmfModule);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_TargetReference(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Acquires a reference to the underlying WFIOTARGET.
    NOTE: Client cannot use DMF_ModuleReference() because that is per Module, not per WDFIOTARGET.

Arguments:

    DmfModule - This Module's handle.
    Target - Target to acquire reference for.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference(DmfModule,
                                                                                      Target,
                                                                                      TRUE,
                                                                                      &target);
    if (!NT_SUCCESS(ntStatus))
    {
        DMF_ModuleDereference(DmfModule);
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DeviceInterfaceMultipleTarget_BufferGetCheckAndAcquireRundownReference fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// eof: Dmf_DeviceInterfaceMultipleTarget.c
//
