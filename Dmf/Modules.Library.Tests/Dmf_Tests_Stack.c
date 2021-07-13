/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_Stack.c

Abstract:

    Functional tests for Dmf_Stack Module.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_Tests_Stack.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Size of each buffer
//
#define BUFFER_SIZE                 (32)
// Number of preallocated buffers in Source
//
#if defined(DMF_KERNEL_MODE)
#define BUFFER_COUNT_PREALLOCATED   (16)
#else
#define BUFFER_COUNT_PREALLOCATED   (64)
#endif
// Max number of buffers we get from Source, preallocated + dynamic
//
#define BUFFER_COUNT_MAX            (24)
// Number of working threads
//
#define THREAD_COUNT                (1)

#define CLIENT_CONTEXT_SIGNATURE    'GISB'

typedef struct
{
    ULONG BufferCount;
    UINT32 Signature;
    UINT16 CheckSum;
} CLIENT_BUFFER_CONTEXT;

typedef
VOID
_IRQL_requires_max_(PASSIVE_LEVEL)
(*Tests_Stack_TestAction)(_In_ DMFMODULE DmfModule);

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_Tests_Stack
{
    // Stack Module to test
    //
    DMFMODULE DmfModuleStack;
    // Work threads
    //
    DMFMODULE DmfModuleThread[THREAD_COUNT];
    // Stack buffer size
    //
    ULONG StackBufferSize;
    // Keep track of buffers pushed.
    //
    ULONG BuffersInStack;
} DMF_CONTEXT_Tests_Stack;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_Stack)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_Stack)

// Memory Pool Tag.
//
#define MemoryTag 'STMD' // DMTS

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static
VOID
Tests_Stack_Validate(
    _In_ DMFMODULE DmfModule,
    _In_ UINT8* ClientBuffer,
    _In_ CLIENT_BUFFER_CONTEXT* ClientBufferContext
    )
{
    DMF_CONTEXT_Tests_Stack* moduleContext;
    UINT16 checkSum;
    ULONG currentDepth;

    UNREFERENCED_PARAMETER(ClientBufferContext);

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(ClientBuffer != NULL);
    DmfAssert(ClientBufferContext != NULL);

    checkSum = TestsUtility_CrcCompute(ClientBuffer,
                                       BUFFER_SIZE);
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Pop checkSum=0x%02X ClientBufferContext->CheckSum=0x%02X", checkSum, ClientBufferContext->CheckSum);

    // Get the current number of buffers in the stack
    //
    currentDepth = DMF_Stack_Depth(moduleContext->DmfModuleStack);

    // Depth should be one less than the buffer count.
    //
    DmfAssert(currentDepth == (ClientBufferContext->BufferCount - 1));

    DmfAssert(CLIENT_CONTEXT_SIGNATURE == ClientBufferContext->Signature);
    DmfAssert(checkSum == ClientBufferContext->CheckSum);

    DmfAssert(moduleContext->BuffersInStack == ClientBufferContext->BufferCount);
}

static
NTSTATUS
Tests_Stack_CreateBuffer(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** Buffer,
    _Out_ WDFMEMORY* ClientBufferMemory
    )
{
    DMF_CONTEXT_Tests_Stack* moduleContext;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    NTSTATUS ntStatus;
    CLIENT_BUFFER_CONTEXT* clientBufferContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               moduleContext->StackBufferSize,
                               ClientBufferMemory,
                               (VOID**)Buffer);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfMemoryCreate fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    clientBufferContext = (CLIENT_BUFFER_CONTEXT*)(*Buffer);

Exit:
    return ntStatus;
}

#pragma code_seg("PAGE")
static
void
Tests_Stack_ThreadAction_Push(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_Stack* moduleContext;
    CLIENT_BUFFER_CONTEXT* clientBufferContext;
    NTSTATUS ntStatus;
    WDFMEMORY clientBufferMemory;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    clientBufferContext = NULL;

    // Don't enqueue more then BUFFER_COUNT_MAX buffers.
    //
    if (DMF_Stack_Depth(moduleContext->DmfModuleStack) >= BUFFER_COUNT_MAX)
    {
        goto Exit;
    }

    // Allocate a buffer and context.
    //
    ntStatus = Tests_Stack_CreateBuffer(DmfModule,
                                        (VOID**)&clientBufferContext,
                                        &clientBufferMemory);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(clientBufferContext != NULL);

    // Populate the buffer with test data.
    //
    UINT8* clientBuffer = (UINT8*)(clientBufferContext + 1);
    TestsUtility_FillWithSequentialData(clientBuffer,
                                        BUFFER_SIZE);

    // Acquire lock to keep index in sync.
    //
    DMF_ModuleLock(DmfModule);

    moduleContext->BuffersInStack++;

    clientBufferContext->BufferCount = moduleContext->BuffersInStack;
    clientBufferContext->Signature = CLIENT_CONTEXT_SIGNATURE;
    clientBufferContext->CheckSum = TestsUtility_CrcCompute(clientBuffer,
                                                            BUFFER_SIZE);
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Push CheckSum=0x%02X", clientBufferContext->CheckSum);

    // Add this buffer to the stack.
    //
    ntStatus = DMF_Stack_Push(moduleContext->DmfModuleStack,
                              clientBufferContext);
    if (!NT_SUCCESS(ntStatus))
    {
        moduleContext->BuffersInStack--;
    }

    DMF_ModuleUnlock(DmfModule);

    // Free the memory.
    //
    WdfObjectDelete(clientBufferMemory);

Exit:

    return;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_Stack_ThreadAction_Push_NoLimit(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_Stack* moduleContext;
    CLIENT_BUFFER_CONTEXT* clientBufferContext;
    NTSTATUS ntStatus;
    WDFMEMORY clientBufferMemory;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    clientBufferContext = NULL;

    // Allocate a buffer and context.
    //
    ntStatus = Tests_Stack_CreateBuffer(DmfModule,
                                        (VOID**)&clientBufferContext,
                                        &clientBufferMemory);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(clientBufferContext != NULL);

    // Populate the buffer with test data.
    //
    UINT8* clientBuffer = (UINT8*)(clientBufferContext + 1);
    TestsUtility_FillWithSequentialData(clientBuffer,
                                        BUFFER_SIZE);

    // Acquire lock to keep index in sync.
    //
    DMF_ModuleLock(DmfModule);

    moduleContext->BuffersInStack++;

    clientBufferContext->BufferCount = moduleContext->BuffersInStack;
    clientBufferContext->Signature = CLIENT_CONTEXT_SIGNATURE;
    clientBufferContext->CheckSum = TestsUtility_CrcCompute(clientBuffer,
                                                            BUFFER_SIZE);
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Push CheckSum=0x%02X", clientBufferContext->CheckSum);

    // Add this buffer to the stack.
    //
    ntStatus = DMF_Stack_Push(moduleContext->DmfModuleStack,
                              clientBufferContext);
    DmfAssert(NT_SUCCESS(ntStatus));

    DMF_ModuleUnlock(DmfModule);

    // Free the memory.
    //
    WdfObjectDelete(clientBufferMemory);

    return;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_Stack_ThreadAction_Pop(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_Stack* moduleContext;
    CLIENT_BUFFER_CONTEXT* clientBufferContext;
    NTSTATUS ntStatus;
    WDFMEMORY clientBufferMemory;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Allocate a buffer and context.
    //
    ntStatus = Tests_Stack_CreateBuffer(DmfModule,
                                        (VOID**)&clientBufferContext,
                                        &clientBufferMemory);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(clientBufferContext != NULL);

    // Acquire lock to keep index in sync.
    //
    DMF_ModuleLock(DmfModule);

    // Pop the buffer.
    //
    ntStatus = DMF_Stack_Pop(moduleContext->DmfModuleStack,
                             (VOID*)clientBufferContext,
                             moduleContext->StackBufferSize);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Validate this buffer.
    // The payload is located at the end of the context.
    //
    UINT8* clientBuffer = (UINT8*)(clientBufferContext + 1);
    Tests_Stack_Validate(DmfModule,
                         clientBuffer,
                         clientBufferContext);

    // Update the last index.
    //
    moduleContext->BuffersInStack--;

Exit:

    DMF_ModuleUnlock(DmfModule);

    // Free the memory.
    //
    WdfObjectDelete(clientBufferMemory);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_Stack_ThreadAction_Depth(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_Stack* moduleContext;
    ULONG currentCount;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Get the current number of buffers in the stack
    //
    currentCount = DMF_Stack_Depth(moduleContext->DmfModuleStack);

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
Tests_Stack_ThreadAction_Flush(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_Stack* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    
    DMF_ModuleLock(DmfModule);

    DMF_Stack_Flush(moduleContext->DmfModuleStack);
    moduleContext->BuffersInStack = 0;

    DMF_ModuleUnlock(DmfModule);
}
#pragma code_seg()

// Test actions executed by work threads.
//
static
Tests_Stack_TestAction
TestActionArray[] =
{
    Tests_Stack_ThreadAction_Push,
    Tests_Stack_ThreadAction_Push_NoLimit,
    Tests_Stack_ThreadAction_Pop,
    Tests_Stack_ThreadAction_Depth,
    Tests_Stack_ThreadAction_Flush
};

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_Stack_WorkThread(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_Stack* moduleContext;
    ULONG testActionIndex;
    Tests_Stack_TestAction testAction;

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

    // Slow down a bit to reduce traffic.
    //
    DMF_Utility_DelayMilliseconds(100);
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
Tests_Stack_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Test_Stack.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_CONTEXT_Tests_Stack* moduleContext;
    NTSTATUS ntStatus;
    LONG index;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Initialize the index.
    //
    moduleContext->BuffersInStack = 0;

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
Tests_Stack_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Tests_Stack.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_Stack* moduleContext;
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
DMF_Tests_Stack_ChildModulesAdd(
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
    DMF_CONTEXT_Tests_Stack* moduleContext;
    DMF_CONFIG_Stack moduleConfigStack;
    DMF_CONFIG_Thread moduleConfigThread;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleContext->StackBufferSize = sizeof(CLIENT_BUFFER_CONTEXT) + BUFFER_SIZE;

    // Stack
    // -----------
    //
    DMF_CONFIG_Stack_AND_ATTRIBUTES_INIT(&moduleConfigStack,
                                         &moduleAttributes);
    moduleConfigStack.StackElementSize = moduleContext->StackBufferSize;
    moduleConfigStack.StackDepth = BUFFER_COUNT_PREALLOCATED;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleStack);

    // Thread
    // ------
    //
    for (ULONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_Stack_WorkThread;
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
DMF_Tests_Stack_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Test_Stack.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Tests_Stack;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Tests_Stack;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Tests_Stack);
    dmfCallbacksDmf_Tests_Stack.ChildModulesAdd = DMF_Tests_Stack_ChildModulesAdd;
    dmfCallbacksDmf_Tests_Stack.DeviceOpen = Tests_Stack_Open;
    dmfCallbacksDmf_Tests_Stack.DeviceClose = Tests_Stack_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Tests_Stack,
                                            Tests_Stack,
                                            DMF_CONTEXT_Tests_Stack,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Tests_Stack.CallbacksDmf = &dmfCallbacksDmf_Tests_Stack;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Tests_Stack,
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

// eof: Dmf_Tests_Stack.c
//
