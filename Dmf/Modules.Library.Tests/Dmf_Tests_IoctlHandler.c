/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_IoctlHandler.c

Abstract:

    Functional tests for Dmf_IoctlHandler Module.
    NOTE: This Module simply instantiates an instance of DMF_IoctlHandler. It provides a target
          for other Test Modules to send and receive data via an IOCTL interface.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"

#include "Dmf_Tests_IoctlHandler.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    WDFREQUEST Request;
    Tests_IoctlHandler_Sleep SleepRequest;
} SleepContext;

typedef struct
{
    DMFMODULE DmfModuleTestIoctlHandler;
} REQUEST_CONTEXT, *PREQUEST_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE(REQUEST_CONTEXT);

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Allocates buffers to be inserted into pending pool.
    //
    DMFMODULE DmfModuleBufferPoolFree;
    // Module that stores all pending sleep contexts.
    //
    DMFMODULE DmfModuleBufferPoolPending;
} DMF_CONTEXT_Tests_IoctlHandler;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_IoctlHandler)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(Tests_IoctlHandler)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_Function_class_(EVT_DMF_BufferPool_TimerCallback)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
Test_IoctlHandler_BufferPool_TimerCallback(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext,
    _In_opt_ VOID* ClientDriverCallbackContext
    )
{
    DMFMODULE dmfModuleParent;
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;
    SleepContext* sleepContext;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(ClientBufferContext);
    UNREFERENCED_PARAMETER(ClientDriverCallbackContext);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);

    sleepContext = (SleepContext*)ClientBuffer;
    
    ntStatus = WdfRequestUnmarkCancelable(sleepContext->Request);
    if (ntStatus != STATUS_CANCELLED)
    {
        WdfRequestComplete(sleepContext->Request,
                           STATUS_SUCCESS);
    }

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolFree,
                       ClientBuffer);
}

_Function_class_(EVT_DMF_BufferPool_Enumeration)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BufferPool_EnumerationDispositionType
Test_IoctlHandler_BufferPool_Enumeration(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext,
    _In_opt_ VOID* ClientDriverCallbackContext
    )
{
    DMFMODULE dmfModuleParent;
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;
    SleepContext* sleepContext;
    BufferPool_EnumerationDispositionType enumerationDispositionType;

    UNREFERENCED_PARAMETER(ClientBufferContext);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    sleepContext = (SleepContext*)ClientBuffer;

    if (sleepContext->Request == (WDFREQUEST)ClientDriverCallbackContext)
    {
        // Since this is called from Cancel Callback, it is not necessary to
        // "unmark" cancelable.
        //
        WdfRequestComplete(sleepContext->Request,
                            STATUS_CANCELLED);
        enumerationDispositionType = BufferPool_EnumerationDisposition_RemoveAndStopEnumeration;
    }
    else
    {
        enumerationDispositionType = BufferPool_EnumerationDisposition_ResetTimerAndContinueEnumeration;
    }

    return enumerationDispositionType;
}

_Function_class_(EVT_WDF_REQUEST_CANCEL)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
Tests_IoctlHandler_RequestCancel(
    _In_ WDFREQUEST Request
    )
{
    REQUEST_CONTEXT* requestContext;
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;
    SleepContext* sleepContext;

    requestContext = (REQUEST_CONTEXT*)WdfObjectGetTypedContext(Request,
                                                                REQUEST_CONTEXT);

    ASSERT(requestContext->DmfModuleTestIoctlHandler != NULL);
    moduleContext = DMF_CONTEXT_GET(requestContext->DmfModuleTestIoctlHandler);

    // sleepContext contains the buffer that is removed and canceled. It is not necessary
    // for this call to use it, but it must be passed to enumerator. When it is not NULL
    // it means the request was canceled (and should not be accessed).
    //
    DMF_BufferPool_Enumerate(moduleContext->DmfModuleBufferPoolPending,
                             Test_IoctlHandler_BufferPool_Enumeration,
                             Request,
                             (VOID**)&sleepContext,
                             NULL);
}

NTSTATUS
Tests_IoctlHandler_Callback(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoControlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    This event is called when the framework receives IRP_MJ_DEVICE_CONTROL
    requests from the system.

Arguments:

    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.

    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.

    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

    VOID

--*/
{
    DMFMODULE dmfModuleParent;
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;
    NTSTATUS ntStatus;
    VOID* clientBuffer;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(InputBufferSize);

    PAGED_CODE();

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);
    ntStatus = STATUS_NOT_SUPPORTED;

    switch(IoControlCode) 
    {
        case IOCTL_Tests_IoctlHandler_SLEEP:
        {
            Tests_IoctlHandler_Sleep* sleepRequestBuffer;
            SleepContext* sleepContext;
            WDF_OBJECT_ATTRIBUTES objectAttributes;
            REQUEST_CONTEXT* requestContext;

            WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes,
                                                    REQUEST_CONTEXT);
            ntStatus = WdfObjectAllocateContext(Request,
                                                &objectAttributes,
                                                (VOID**)&requestContext);
            if (!NT_SUCCESS(ntStatus))
            {
                goto Exit;
            }

            // Save the Module in private context for cancel routine.
            // It is necessary so that it can be removed from lists.
            //
            requestContext->DmfModuleTestIoctlHandler = dmfModuleParent;

            WdfRequestMarkCancelable(Request,
                                     Tests_IoctlHandler_RequestCancel);

            ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPoolFree,
                                          &clientBuffer,
                                          NULL);
            ASSERT(NT_SUCCESS(ntStatus));

            sleepContext = (SleepContext*)clientBuffer;
            sleepContext->Request = Request;

            sleepRequestBuffer = (Tests_IoctlHandler_Sleep*)InputBuffer;
            RtlCopyMemory(&sleepContext->SleepRequest,
                          sleepRequestBuffer,
                          sizeof(Tests_IoctlHandler_Sleep));

            DMF_BufferPool_PutInSinkWithTimer(moduleContext->DmfModuleBufferPoolPending,
                                              clientBuffer,
                                              sleepRequestBuffer->TimeToSleepMilliSeconds,
                                              Test_IoctlHandler_BufferPool_TimerCallback,
                                              NULL);
            ntStatus = STATUS_PENDING;
            break;
        }
        case IOCTL_Tests_IoctlHandler_ZEROBUFFER:
        {
            RtlZeroMemory(OutputBuffer,
                          OutputBufferSize);
            ntStatus = STATUS_SUCCESS;
            *BytesReturned = OutputBufferSize;
            break;
        }
    }

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

IoctlHandler_IoctlRecord Tests_IoctlHandlerTable[] =
{
    { (LONG)IOCTL_Tests_IoctlHandler_SLEEP,         sizeof(Tests_IoctlHandler_Sleep), 0, Tests_IoctlHandler_Callback, FALSE },
    { (LONG)IOCTL_Tests_IoctlHandler_ZEROBUFFER,    0,                                0, Tests_IoctlHandler_Callback, FALSE },
};

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_IoctlHandler_ChildModulesAdd(
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
    DMF_CONTEXT_Tests_IoctlHandler* moduleContext;
    DMF_CONFIG_IoctlHandler moduleConfigIoctlHandler;
    DMF_CONFIG_BufferPool moduleConfigBufferPool;
    DMF_CONFIG_Tests_IoctlHandler* moduleConfig;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // IoctlHandler
    // ------------
    //
    DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(&moduleConfigIoctlHandler,
                                                &moduleAttributes);
    moduleConfigIoctlHandler.IoctlRecords = Tests_IoctlHandlerTable;
    moduleConfigIoctlHandler.IoctlRecordCount = _countof(Tests_IoctlHandlerTable);
    if (moduleConfig->CreateDeviceInterface)
    {
        moduleConfigIoctlHandler.DeviceInterfaceGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
    }
    moduleConfigIoctlHandler.AccessModeFilter = IoctlHandler_AccessModeDefault;
    DMF_DmfModuleAdd(DmfModuleInit, 
                     &moduleAttributes, 
                     WDF_NO_OBJECT_ATTRIBUTES, 
                     NULL);

    // TODO: Add second instance for Internal IOCTL.
    //

    // BufferPool Source
    // -----------------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                              &moduleAttributes);
    moduleConfigBufferPool.BufferPoolMode = BufferPool_Mode_Source;
    moduleConfigBufferPool.Mode.SourceSettings.BufferSize = sizeof(SleepContext);
    moduleConfigBufferPool.Mode.SourceSettings.BufferCount = 32;
    moduleConfigBufferPool.Mode.SourceSettings.CreateWithTimer = TRUE;
    moduleConfigBufferPool.Mode.SourceSettings.EnableLookAside = TRUE;
    moduleConfigBufferPool.Mode.SourceSettings.PoolType = NonPagedPoolNx;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPoolFree);

    // BufferPool Sink
    // ---------------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                              &moduleAttributes);
    moduleConfigBufferPool.BufferPoolMode = BufferPool_Mode_Sink;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPoolPending);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_Tests_IoctlHandler;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_Tests_IoctlHandler;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Tests_IoctlHandler_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Test_IoctlHandler.

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

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_Tests_IoctlHandler);
    DmfCallbacksDmf_Tests_IoctlHandler.ChildModulesAdd = DMF_Tests_IoctlHandler_ChildModulesAdd;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_Tests_IoctlHandler,
                                            Tests_IoctlHandler,
                                            DMF_CONTEXT_Tests_IoctlHandler,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    DmfModuleDescriptor_Tests_IoctlHandler.CallbacksDmf = &DmfCallbacksDmf_Tests_IoctlHandler;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_Tests_IoctlHandler,
                                DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_Tests_IoctlHandler.c
//
