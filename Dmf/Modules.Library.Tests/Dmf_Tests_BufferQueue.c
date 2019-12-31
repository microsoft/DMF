/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_BufferQueue.c

Abstract:

    Functional tests for Dmf_BufferQueue Module.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"

#include "Dmf_Tests_BufferQueue.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Size of each buffer
//
#define BUFFER_SIZE                 (32)
// Number of preallocated buffers in Source
//
#define BUFFER_COUNT_PREALLOCATED   (16)
// Max number of buffers we get from Source, preallocated + dynamic
//
#define BUFFER_COUNT_MAX            (24)
// Number of working threads
//
#define THREAD_COUNT                (2)

#define CLIENT_CONTEXT_SIGNATURE    'GISB'

typedef struct
{
    UINT32 Signature;
    UINT16 CheckSum;
} CLIENT_BUFFER_CONTEXT, *PCLIENT_BUFFER_CONTEXT;

typedef struct
{
    BufferPool_EnumerationDispositionType Disposition;
    BOOLEAN ClientOwnsBuffer;
} ENUM_CONTEXT_Tests_BufferQueue, *PENUM_CONTEXT_Tests_BufferQueue;

typedef
VOID
_IRQL_requires_max_(PASSIVE_LEVEL)
(*Tests_BufferQueue_TestAction)(_In_ DMFMODULE DmfModule);

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // BufferQueue Module to test
    //
    DMFMODULE DmfModuleBufferQueue;
    // Work threads
    //
    DMFMODULE DmfModuleThread[THREAD_COUNT];
} DMF_CONTEXT_Tests_BufferQueue, *PDMF_CONTEXT_Tests_BufferQueue;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_BufferQueue)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_BufferQueue)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static
VOID
Tests_BufferQueue_Validate(
    _In_ DMFMODULE DmfModuleBufferQueue,
    _In_ UINT8* ClientBuffer,
    _In_ PCLIENT_BUFFER_CONTEXT ClientBufferContext
    )
{
    UINT16 checkSum;

    UNREFERENCED_PARAMETER(DmfModuleBufferQueue);
    UNREFERENCED_PARAMETER(ClientBufferContext);

    DmfAssert(ClientBuffer != NULL);
    DmfAssert(ClientBufferContext != NULL);

    checkSum = TestsUtility_CrcCompute(ClientBuffer,
                                       BUFFER_SIZE);

    DmfAssert(CLIENT_CONTEXT_SIGNATURE == ClientBufferContext->Signature);
    DmfAssert(checkSum == ClientBufferContext->CheckSum);
}

_Function_class_(EVT_DMF_BufferPool_Enumeration)
static
BufferPool_EnumerationDispositionType
Tests_BufferQueue_EnumerationCallback(
    _In_ DMFMODULE DmfModuleBufferPool,
    _In_ PVOID ClientBuffer,
    _In_ PVOID ClientBufferContext,
    _In_opt_ PVOID ClientDriverCallbackContext
    )
{
    DMFMODULE dmfModule;
    PDMF_CONTEXT_Tests_BufferQueue moduleContext;
    PENUM_CONTEXT_Tests_BufferQueue enumContext;

    dmfModule = DMF_ParentModuleGet(DmfModuleBufferPool);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    enumContext = (PENUM_CONTEXT_Tests_BufferQueue)ClientDriverCallbackContext;
    DmfAssert(enumContext != NULL);

    Tests_BufferQueue_Validate(DmfModuleBufferPool,
                              (UINT8*)ClientBuffer,
                              (PCLIENT_BUFFER_CONTEXT)ClientBufferContext);

    // 'Dereferencing NULL pointer. 'enumContext' contains the same NULL value as 'ClientDriverCallbackContext' did.'
    //
    #pragma warning(suppress:28182)
    enumContext->ClientOwnsBuffer = (enumContext->Disposition == BufferPool_EnumerationDisposition_RemoveAndStopEnumeration);

    return enumContext->Disposition;
}

#pragma code_seg("PAGE")
static
void
Tests_BufferQueue_ThreadAction_Enqueue(
    _In_ DMFMODULE DmfModule
    )
{
    PDMF_CONTEXT_Tests_BufferQueue moduleContext;
    PUINT8 clientBuffer;
    PCLIENT_BUFFER_CONTEXT clientBufferContext;
    NTSTATUS ntStatus;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    clientBuffer = NULL;
    clientBufferContext = NULL;

    // Don't enqueue more then BUFFER_COUNT_MAX buffers
    //
    if (DMF_BufferQueue_Count(moduleContext->DmfModuleBufferQueue) >= BUFFER_COUNT_MAX)
    {
        goto Exit;
    }

    // Fetch a new buffer from producer list.
    //
    ntStatus = DMF_BufferQueue_Fetch(moduleContext->DmfModuleBufferQueue,
                                     (PVOID*)&clientBuffer,
                                     (PVOID*)&clientBufferContext);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(clientBuffer != NULL);
    DmfAssert(clientBufferContext != NULL);

    // Populate the buffer with test data
    //
    TestsUtility_FillWithSequentialData(clientBuffer,
                                        BUFFER_SIZE);

    clientBufferContext->Signature = CLIENT_CONTEXT_SIGNATURE;
    clientBufferContext->CheckSum = TestsUtility_CrcCompute(clientBuffer,
                                                            BUFFER_SIZE);

    // Add this buffer to the queue
    //
    DMF_BufferQueue_Enqueue(moduleContext->DmfModuleBufferQueue,
                            clientBuffer);

Exit:

    return;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_BufferQueue_ThreadAction_Dequeue(
    _In_ DMFMODULE DmfModule
)
{
    PDMF_CONTEXT_Tests_BufferQueue moduleContext;
    PUINT8 clientBuffer;
    PCLIENT_BUFFER_CONTEXT clientBufferContext;
    NTSTATUS ntStatus;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Dequeue the buffer.
    //
    ntStatus = DMF_BufferQueue_Dequeue(moduleContext->DmfModuleBufferQueue,
                                       (PVOID*)&clientBuffer,
                                       (PVOID*)&clientBufferContext);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Validate this buffer
    //
    Tests_BufferQueue_Validate(moduleContext->DmfModuleBufferQueue,
                               clientBuffer,
                               clientBufferContext);

    // Return it to the queue's producer list for reuse.
    //
    DMF_BufferQueue_Reuse(moduleContext->DmfModuleBufferQueue, 
                          clientBuffer);

Exit:

    return;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_BufferQueue_ThreadAction_Enumerate(
    _In_ DMFMODULE DmfModule
)
{
    PDMF_CONTEXT_Tests_BufferQueue moduleContext;
    ENUM_CONTEXT_Tests_BufferQueue enumContext;
    PUINT8 clientBuffer;
    ULONG randomNumber;
    PCLIENT_BUFFER_CONTEXT clientBufferContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    clientBuffer = NULL;
    clientBufferContext = NULL;

    // Generate a random value for EnumerationDisposition
    //
    randomNumber = TestsUtility_GenerateRandomNumber(BufferPool_EnumerationDisposition_ContinueEnumeration,
                                                     BufferPool_EnumerationDisposition_ResetTimerAndContinueEnumeration);
    enumContext.Disposition = (BufferPool_EnumerationDispositionType)randomNumber;
    enumContext.ClientOwnsBuffer = FALSE;

    // Enumerate buffers in the queue
    //
    DMF_BufferQueue_Enumerate(moduleContext->DmfModuleBufferQueue,
                              Tests_BufferQueue_EnumerationCallback,
                              &enumContext,
                              (PVOID*)&clientBuffer,
                              (PVOID*)&clientBufferContext);

    // Return the resulting buffer to the source, if needed.
    //
    if (enumContext.ClientOwnsBuffer)
    {
        DmfAssert(clientBuffer != NULL);
        DmfAssert(clientBufferContext != NULL);

        Tests_BufferQueue_Validate(moduleContext->DmfModuleBufferQueue,
                                  clientBuffer,
                                  clientBufferContext);

        DMF_BufferQueue_Reuse(moduleContext->DmfModuleBufferQueue,
                              clientBuffer);
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_BufferQueue_ThreadAction_Count(
    _In_ DMFMODULE DmfModule
    )
{
    PDMF_CONTEXT_Tests_BufferQueue moduleContext;
    ULONG currentCount;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Get the current number of buffers in the queue
    //
    currentCount = DMF_BufferQueue_Count(moduleContext->DmfModuleBufferQueue);

    // Adding thread count, since when acquiring we don't synchronize checking 
    // for the currently acquired number and putting buffers to sync.
    // So a number of acquired buffers may be up to THREAD_COUNT more, in case of a race conditions.
    //
    DmfAssert(currentCount <= BUFFER_COUNT_MAX + THREAD_COUNT);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_BufferQueue_ThreadAction_Flush(
    _In_ DMFMODULE DmfModule
)
{
    PDMF_CONTEXT_Tests_BufferQueue moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_BufferQueue_Flush(moduleContext->DmfModuleBufferQueue);
}
#pragma code_seg()

// Test actions executed by work threads.
//
static
Tests_BufferQueue_TestAction
TestActionArray[] = {
    Tests_BufferQueue_ThreadAction_Enqueue,
    Tests_BufferQueue_ThreadAction_Dequeue,
    Tests_BufferQueue_ThreadAction_Enumerate,
    Tests_BufferQueue_ThreadAction_Count,
    Tests_BufferQueue_ThreadAction_Flush
};

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_BufferQueue_WorkThread(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    PDMF_CONTEXT_Tests_BufferQueue moduleContext;
    ULONG testActionIndex;
    Tests_BufferQueue_TestAction testAction;

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    // Pick a random test action for a current iteration.
    //
    testActionIndex = TestsUtility_GenerateRandomNumber(0, 
                                                        ARRAYSIZE(TestActionArray) - 1);
    testAction = TestActionArray[testActionIndex];

    // Execute the test action.
    //
    testAction(dmfModule);
    
    // Repeat the test, until stop is signaled.
    //
    if (!DMF_Thread_IsStopPending(DmfModuleThread))
    {
        DMF_Thread_WorkReady(DmfModuleThread);
    }

    TestsUtility_YieldExecution();
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
Tests_BufferQueue_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Test_BufferQueue.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    PDMF_CONTEXT_Tests_BufferQueue moduleContext;
    NTSTATUS ntStatus;
    LONG index;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    for (index = 0; index < THREAD_COUNT; index++)
    {
        ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThread[index]);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Start fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    for (index = 0; index < THREAD_COUNT; index++)
    {
        DMF_Thread_WorkReady(moduleContext->DmfModuleThread[index]);
    }

Exit:
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_BufferQueue_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Tests_BufferQueue.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    PDMF_CONTEXT_Tests_BufferQueue moduleContext;
    LONG index;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (index = 0; index < THREAD_COUNT; index++)
    {
        DMF_Thread_Stop(moduleContext->DmfModuleThread[index]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_BufferQueue_ChildModulesAdd(
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
    DMF_CONTEXT_Tests_BufferQueue* moduleContext;
    DMF_CONFIG_BufferQueue moduleConfigBufferQueue;
    DMF_CONFIG_Thread moduleConfigThread;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // BufferQueue
    // -----------
    //
    DMF_CONFIG_BufferQueue_AND_ATTRIBUTES_INIT(&moduleConfigBufferQueue,
                                               &moduleAttributes);
    moduleConfigBufferQueue.SourceSettings.BufferContextSize = sizeof(CLIENT_BUFFER_CONTEXT);
    moduleConfigBufferQueue.SourceSettings.BufferSize = BUFFER_SIZE;
    moduleConfigBufferQueue.SourceSettings.BufferCount = BUFFER_COUNT_PREALLOCATED;
    moduleConfigBufferQueue.SourceSettings.CreateWithTimer = FALSE;
    moduleConfigBufferQueue.SourceSettings.EnableLookAside = TRUE;
    moduleConfigBufferQueue.SourceSettings.PoolType = NonPagedPoolNx;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferQueue);

    // Thread
    // ------
    //
    for (ULONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_BufferQueue_WorkThread;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleThread[threadIndex]);
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
DMF_Tests_BufferQueue_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Test_BufferQueue.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Tests_BufferQueue;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Tests_BufferQueue;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Tests_BufferQueue);
    dmfCallbacksDmf_Tests_BufferQueue.ChildModulesAdd = DMF_Tests_BufferQueue_ChildModulesAdd;
    dmfCallbacksDmf_Tests_BufferQueue.DeviceOpen = Tests_BufferQueue_Open;
    dmfCallbacksDmf_Tests_BufferQueue.DeviceClose = Tests_BufferQueue_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Tests_BufferQueue,
                                            Tests_BufferQueue,
                                            DMF_CONTEXT_Tests_BufferQueue,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Tests_BufferQueue.CallbacksDmf = &dmfCallbacksDmf_Tests_BufferQueue;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Tests_BufferQueue,
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

// eof: Dmf_Tests_BufferQueue.c
//
