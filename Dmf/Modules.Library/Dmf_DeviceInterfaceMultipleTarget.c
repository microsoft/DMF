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

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_DeviceInterfaceMultipleTarget.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#if defined(DMF_USER_MODE)
// Specific external includes for this DMF Module.
//
#include <Guiddef.h>
#include <wdmguid.h>
#include <cfgmgr32.h>
#include <ndisguid.h>
#endif // defined(DMF_USER_MODE)

typedef struct
{
    // Underlying Device Target.
    //
    WDFIOTARGET IoTarget;
    // Save Symbolic Link Name to be able to deal with multiple instances of the same
    // device interface.
    //
    WDFMEMORY MemorySymbolicLink;
    UNICODE_STRING SymbolicLinkName;
    DMFMODULE DmfModuleRequestTarget;
    DeviceInterfaceMultipleTarget_Target DmfIoTarget;
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
NTSTATUS
RequestSink_SendSynchronously_Type(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeout,
    _Out_opt_ size_t* BytesWritten
    );

typedef
NTSTATUS
RequestSink_Send_Type(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtRequestSinkSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
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

typedef struct
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
    LONG NumberOfTargetsCreated;

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
    RequestSink_IoTargetSet_Type* RequestSink_IoTargetSet;
    RequestSink_IoTargetClear_Type* RequestSink_IoTargetClear;

    // Passive level desired by Client. This is used to instantiate underlying Child Modules.
    //
    BOOLEAN PassiveLevel;
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

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DeviceInterfaceMultipleTarget_IoTargetDestroy(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target
    )
/*++

Routine Description:

    Destroy the Io Target opened by this Module.

Arguments:

    ModuleContext - This Module's Module Context.

Return Value:

    None

--*/
{
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Depending on what happened before, the IoTarget may or may not be
    // valid. So, check here.
    //
    if (Target->IoTarget != NULL)
    {
        WdfIoTargetClose(Target->IoTarget);
        if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange)
        {
            moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange(DmfModule,
                                                                        Target->DmfIoTarget,
                                                                        DeviceInterfaceMultipleTarget_StateType_Close);
        }
        moduleContext->RequestSink_IoTargetClear(DmfModule,
                                                 Target);
        WdfObjectDelete(Target->IoTarget);
        Target->IoTarget = NULL;
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

VOID
DeviceInterfaceMultipleTarget_SymbolicLinkNameClear(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target
    )
/*++

Routine Description:

    Delete the stored symbolic link from the context. This is needed to deal with multiple instances of 
    the same device interface.

Arguments:

    DmfModule - This Module's DMF Module handle.
    Target - 

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

NTSTATUS
DeviceInterfaceMultipleTarget_SymbolicLinkNameStore(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_ UNICODE_STRING* SymbolicLinkName
    )
/*++

Routine Description:

    Create a copy of symbolic link name and store it in the given Module's context. This is needed to deal with
    multiple instances of the same device interface.

Arguments:

    DmfModule - This Module's DMF Module handle.
    Target - 
    SymbolicLinkName - The given symbolic link name.

Return Value:

    None

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

    ntStatus = RtlUnicodeStringCopy(&Target->SymbolicLinkName,
                                    SymbolicLinkName);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlUnicodeStringCopy fails: ntStatus=%!STATUS!", ntStatus);
        DeviceInterfaceMultipleTarget_SymbolicLinkNameClear(DmfModule,
                                                            Target);
        goto Exit;
    }

Exit:
    
    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DeviceInterfaceMultipleTarget_TargetDestroy(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target
    )
/*++

Routine Description:

    Destroy the underlying IoTarget. 
    NOTE: IoTarget close is different in QueryRemove.

Arguments:

    DmfModule - The given Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // It is important to check the IoTarget because it may have been closed via 
    // two asynchronous removal paths: 1. Device is removed. 2. Underlying target is removed.
    //
    if (Target->IoTarget != NULL)
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

        // Destroy the underlying IoTarget.
        //
        DeviceInterfaceMultipleTarget_IoTargetDestroy(DmfModule,
                                                      Target);
        DmfAssert(Target->IoTarget == NULL);
    }

    // Delete stored symbolic link if set. (This will never be set in User-mode.)
    //
    DeviceInterfaceMultipleTarget_SymbolicLinkNameClear(DmfModule,
                                                        Target);

    if (Target->DmfIoTarget)
    {
        WdfObjectDelete(Target->DmfIoTarget);
    }
}
#pragma code_seg()

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

    DeviceInterfaceMultipleTarget_TargetDestroy(DmfModule,
                                                Target);
    DMF_BufferQueue_Reuse(moduleContext->DmfModuleBufferQueue,
                          (VOID *)Target);

    // No lock is used here, since the pnp callback is synchronous.
    //
    if (InterlockedDecrement(&moduleContext->NumberOfTargetsCreated) == 0)
    {
        // Close the Module.
        //
        DMF_ModuleClose(DmfModule);
    }
}
#pragma code_seg()

// ContinuousRequestTarget Methods
// -------------------------------
//

NTSTATUS
DeviceInterfaceMultipleTarget_Stream_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
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

NTSTATUS
DeviceInterfaceMultipleTarget_Stream_Send(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtRequestSinkSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    )
{
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    return DMF_ContinuousRequestTarget_Send(Target->DmfModuleRequestTarget,
                                            RequestBuffer,
                                            RequestLength,
                                            ResponseBuffer,
                                            ResponseLength,
                                            RequestType,
                                            RequestIoctl,
                                            RequestTimeoutMilliseconds,
                                            EvtRequestSinkSingleAsynchronousRequest,
                                            SingleAsynchronousRequestClientContext);
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

NTSTATUS
DeviceInterfaceMultipleTarget_Target_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
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

NTSTATUS
DeviceInterfaceMultipleTarget_Target_Send(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_IoTarget* Target,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
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

    DmfAssert(! moduleContext->OpenedInStreamMode);
    ntStatus = DMF_RequestTarget_Send(Target->DmfModuleRequestTarget,
                                      RequestBuffer,
                                      RequestLength,
                                      ResponseBuffer,
                                      ResponseLength,
                                      RequestType,
                                      RequestIoctl,
                                      RequestTimeoutMilliseconds,
                                      EvtRequestSinkSingleAsynchronousRequest,
                                      SingleAsynchronousRequestClientContext);

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
    DmfAssert(target->IoTarget != NULL);

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
    DmfAssert(target->IoTarget != NULL);

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

EVT_WDF_IO_TARGET_QUERY_REMOVE DeviceInterfaceMultipleTarget_EvtIoTargetQueryRemove;

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

    NT_SUCCESS.

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModule;
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceMultipleTarget* moduleConfig;
    DeviceInterfaceMultipleTarget_IoTargetContext* targetContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    ntStatus = STATUS_SUCCESS;

    FuncEntry(DMF_TRACE);

    // The IoTarget's Module Context area has the DMF Module.
    //
    targetContext = WdfObjectGet_DeviceInterfaceMultipleTarget_IoTargetContext(IoTarget);
    dmfModule = targetContext->DmfModuleDeviceInterfaceMultipleTarget;
    target = targetContext->Target;

    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange)
    {
        moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange(dmfModule,
                                                                    target->DmfIoTarget,
                                                                    DeviceInterfaceMultipleTarget_StateType_QueryRemove);
    }

    if (moduleContext->OpenedInStreamMode)
    {
        DMF_DeviceInterfaceMultipleTarget_StreamStop(dmfModule,
                                                     target->DmfIoTarget);
    }

    WdfIoTargetCloseForQueryRemove(IoTarget);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

EVT_WDF_IO_TARGET_REMOVE_CANCELED DeviceInterfaceMultipleTarget_EvtIoTargetRemoveCanceled;

VOID
DeviceInterfaceMultipleTarget_EvtIoTargetRemoveCanceled(
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

    FuncEntry(DMF_TRACE);

    // The IoTarget's Module Context area has the DMF Module.
    //
    targetContext = WdfObjectGet_DeviceInterfaceMultipleTarget_IoTargetContext(IoTarget);
    dmfModule = targetContext->DmfModuleDeviceInterfaceMultipleTarget;
    target = targetContext->Target;

    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange)
    {
        moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange(dmfModule,
                                                                    target->DmfIoTarget,
                                                                    DeviceInterfaceMultipleTarget_StateType_QueryRemoveCancelled);
    }

    WDF_IO_TARGET_OPEN_PARAMS_INIT_REOPEN(&openParams);

    ntStatus = WdfIoTargetOpen(IoTarget,
                               &openParams);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to re-open serial target - %!STATUS!", ntStatus);
        WdfObjectDelete(IoTarget);
        goto Exit;
    }

    if (moduleContext->OpenedInStreamMode)
    {
        ntStatus = DMF_DeviceInterfaceMultipleTarget_StreamStart(dmfModule,
                                                                 target->DmfIoTarget);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_DeviceInterfaceMultipleTarget_StreamStart fails: ntStatus=%!STATUS!", ntStatus);
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
    if (! callbackContext.BufferFound)
    {
        // The target buffer should be in the consumer pool.
        // 
        DmfAssert(FALSE);
        goto Exit;
    }

    if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange)
    {
        moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange(dmfModule,
                                                                    target->DmfIoTarget,
                                                                    DeviceInterfaceMultipleTarget_StateType_QueryRemoveComplete);
    }

    // The underlying target has been removed and is no longer accessible.
    // Close the Module and destroy the IoTarget.
    //
    DeviceInterfaceMultipleTarget_TargetDestroyAndCloseModule(dmfModule,
                                                              target);
Exit:

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
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

        // Store ContinuousRequestTarget callbacks from config into DeviceInterfaceMultipleTarget context for redirection.
        //
        moduleContext->EvtContinuousRequestTargetBufferInput = moduleConfig->ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput;
        moduleContext->EvtContinuousRequestTargetBufferOutput = moduleConfig->ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput;

        // Replace ContinuousRequestTarget callbacks in config with DeviceInterfaceMultipleTarget callbacks.
        //
        moduleConfig->ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput = DeviceInterfaceMultipleTarget_Stream_BufferInput;
        moduleConfig->ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput = DeviceInterfaceMultipleTarget_Stream_BufferOutput;

        DMF_ContinuousRequestTarget_ATTRIBUTES_INIT(&moduleAttributes);
        moduleAttributes.ModuleConfigPointer = &moduleConfig->ContinuousRequestTargetModuleConfig;
        moduleAttributes.SizeOfModuleSpecificConfig = sizeof(moduleConfig->ContinuousRequestTargetModuleConfig);
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
        moduleContext->RequestSink_SendSynchronously = DeviceInterfaceMultipleTarget_Target_SendSynchronously;
        moduleContext->OpenedInStreamMode = FALSE;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
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
    openParams.EvtIoTargetRemoveCanceled = DeviceInterfaceMultipleTarget_EvtIoTargetRemoveCanceled;
    openParams.EvtIoTargetRemoveComplete = DeviceInterfaceMultipleTarget_EvtIoTargetRemoveComplete;

    WDF_OBJECT_ATTRIBUTES_INIT(&targetAttributes);
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&targetAttributes,
                                           DeviceInterfaceMultipleTarget_IoTargetContext);
    targetAttributes.ParentObject = DmfModule;

    // Create an I/O target object.
    //
    ntStatus = WdfIoTargetCreate(device,
                                 &targetAttributes,
                                 &Target->IoTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    targetContext = WdfObjectGet_DeviceInterfaceMultipleTarget_IoTargetContext(Target->IoTarget);
    targetContext->DmfModuleDeviceInterfaceMultipleTarget = DmfModule;
    targetContext->Target = Target;

    ntStatus = WdfIoTargetOpen(Target->IoTarget,
                               &openParams);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetOpen fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = DeviceInterfaceMultipleTarget_ContinuousRequestTargetCreate(DmfModule,
                                                                           Target);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DeviceInterfaceMultipleTarget_ContinuousRequestTargetCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    moduleContext->RequestSink_IoTargetSet(DmfModule,
                                           Target,
                                           Target->IoTarget);

    if (moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange)
    {
        moduleConfig->EvtDeviceInterfaceMultipleTargetOnStateChange(DmfModule,
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

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    device = DMF_ParentDeviceGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    // By default, always open the Target.
    //
    ioTargetOpen = TRUE;
    ntStatus = STATUS_SUCCESS;
    target = NULL;

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
        // Interface already part of buffer queue.
        // TODO: Can we return STATUS_SUCCESS?
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

        // Open the Module if its the first target.
        // No lock is used here, since the PnP callback is synchronous.
        // TODO: Optimize the callback, by queuing a workitem to do all the work. 
        //
        if (InterlockedIncrement(&moduleContext->NumberOfTargetsCreated) == 1)
        {
            ntStatus = DMF_ModuleOpen(DmfModule);
            if (!NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleOpen() fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
        }

        // Create and open the underlying target.
        //
        ntStatus = DeviceInterfaceMultipleTarget_DeviceCreateNewIoTargetByName(DmfModule,
                                                                               target,
                                                                               SymbolicLinkName);
        if (! NT_SUCCESS(ntStatus))
        {
            // TODO: Display SymbolicLinkName.
            //
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DeviceInterfaceMultipleTarget_DeviceCreateNewIoTargetByName() fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        if (moduleContext->ContinuousRequestTargetMode == ContinuousRequestTarget_Mode_Automatic)
        {
            // By calling this function here, callbacks at the Client will happen only after the Module is open.
            //
            DmfAssert(target->DmfModuleRequestTarget != NULL);
            ntStatus = DMF_ContinuousRequestTarget_Start(target->DmfModuleRequestTarget);
            if (!NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_Start fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
        }

        // Target was successfully created.
        // Enqueue target buffer into consumer pool. 
        // 
        DMF_BufferQueue_Enqueue(moduleContext->DmfModuleBufferQueue,
                                (VOID *)target);
    }

Exit:

    if (! NT_SUCCESS(ntStatus))
    {
        if (target != NULL)
        {
            DeviceInterfaceMultipleTarget_TargetDestroyAndCloseModule(DmfModule,
                                                                      target);
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

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Already unregistered from PnP notification.
    // Clean the buffer queue here since the notifications callback will no longer be called.
    //
    while (DMF_BufferQueue_Count(moduleContext->DmfModuleBufferQueue) != 0)
    {
        DMF_BufferQueue_Dequeue(moduleContext->DmfModuleBufferQueue,
                                (VOID**)&target,
                                NULL);
        DeviceInterfaceMultipleTarget_TargetDestroyAndCloseModule(DmfModule,
                                                                  target);
    }

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
    UNICODE_STRING unitargetName;
    NTSTATUS ntStatus;
    CONFIGRET configRet;
    PWSTR currentInterface;
    ULONG currentInterfaceLength;

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

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
        currentInterfaceLength = (ULONG)wcsnlen(currentInterface, cmListSize);
        while ((currentInterfaceLength < cmListSize) && (*currentInterface != UNICODE_NULL))
        {
            RtlInitUnicodeString(&unitargetName,
                                 bufferPointer);
            DeviceInterfaceMultipleTarget_InitializeIoTargetIfNeeded(DmfModule,
                                                                     &unitargetName);

            currentInterfaceLength = (ULONG)wcsnlen(currentInterface, cmListSize);
            currentInterface += currentInterfaceLength + 1;
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
    DeviceInterfaceMultipleTarget_IoTarget* target;
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
    DeviceInterfaceMultipleTarget_IoTarget* target;

    PAGED_CODE();
    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    Target = 0;

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
            TraceEvents(TRACE_LEVEL_VERBOSE,
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference");
        goto Exit;
    }

    target = DeviceInterfaceMultipleTarget_BufferGet(Target);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->OpenedInStreamMode);
    DMF_ContinuousRequestTarget_BufferPut(target->DmfModuleRequestTarget,
                                          ClientBuffer);

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
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

    VOID

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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference");
        goto Exit;
    }

    target = DeviceInterfaceMultipleTarget_BufferGet(Target);
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    DmfAssert(target->IoTarget != NULL);

    *IoTarget = target->IoTarget;

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_Send(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    // This Module Method can be called when SSH is removed or being removed. The code in this function is 
    // protected due to call to ModuleAcquire.
    //
    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference");
        goto Exit;
    }

    target = DeviceInterfaceMultipleTarget_BufferGet(Target);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(target->IoTarget != NULL);
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

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    // This Module Method can be called when SSH is removed or being removed. The code in this function is 
    // protected due to call to ModuleAcquire.
    //
    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference");
        goto Exit;
    }

    target = DeviceInterfaceMultipleTarget_BufferGet(Target);
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

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_DeviceInterfaceMultipleTarget_StreamStart(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target
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
    DMF_CONTEXT_DeviceInterfaceMultipleTarget* moduleContext;
    DeviceInterfaceMultipleTarget_IoTarget* target;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 DeviceInterfaceMultipleTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference");
        goto Exit;
    }

    target = DeviceInterfaceMultipleTarget_BufferGet(Target);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(target->IoTarget != NULL);

    DmfAssert(moduleContext->OpenedInStreamMode);
    ntStatus = DMF_ContinuousRequestTarget_Start(target->DmfModuleRequestTarget);

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

    Stops streaming Asynchronous requests to the IoTarget and Cancels all the existing requests.

Arguments:

    DmfModule - This Module's handle.

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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference");
        goto Exit;
    }

    target = DeviceInterfaceMultipleTarget_BufferGet(Target);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(target->IoTarget != NULL);

    DmfAssert(moduleContext->OpenedInStreamMode);
    DMF_ContinuousRequestTarget_StopAndWait(target->DmfModuleRequestTarget);

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExitVoid(DMF_TRACE);
}

// eof: Dmf_DeviceInterfaceMultipleTarget.c
//
