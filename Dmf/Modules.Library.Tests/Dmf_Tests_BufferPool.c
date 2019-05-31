/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_BufferPool.c

Abstract:

    Functional tests for Dmf_BufferPool Module

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"

#include "Dmf_Tests_BufferPool.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define BUFFER_SIZE                 (32)
#define BUFFER_COUNT_PREALLOCATED   (16)
#define BUFFER_COUNT_MAX            (24)
#define THREAD_COUNT                (2)

#define CLIENT_CONTEXT_SIGNATURE    'GISB'

typedef struct
{
    UINT32 Signature;
    UINT16 CheckSum;
} CLIENT_BUFFER_CONTEXT;

typedef struct
{
    BufferPool_EnumerationDispositionType Disposition;
    BOOLEAN ClientOwnsBuffer;
} ENUM_CONTEXT;

typedef enum _TEST_ACTION {
    TEST_ACTION_AQUIRE,
    TEST_ACTION_RETURN,
    TEST_ACTION_ENUMERATE,
    TEST_ACTION_COUNT,
    TEST_ACTION_MINIUM      = TEST_ACTION_AQUIRE,
    TEST_ACTION_MAXIMUM     = TEST_ACTION_COUNT
} TEST_ACTION;

typedef enum _GET_ACTION {
    GET_ACTION_PLAIN,
    GET_ACTION_WITH_MEMORY,
    GET_ACTION_WITH_MEMORY_DESCRIPTOR,
    GET_ACTION_MIN      = GET_ACTION_PLAIN,
    GET_ACTION_MAX      = GET_ACTION_WITH_MEMORY_DESCRIPTOR
} GET_ACTION;

typedef enum _PUT_ACTION {
    PUT_ACTION_PLAIN,
    PUT_ACTION_WITH_TIMEOUT,
    PUT_ACTION_MIN      = PUT_ACTION_PLAIN,
    PUT_ACTION_MAX      = PUT_ACTION_WITH_TIMEOUT
} PUT_ACTION;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // BufferPool source Module to test
    //
    DMFMODULE DmfModuleBufferPoolSource;
    // BufferPool sink Module to test
    //
    DMFMODULE DmfModuleBufferPoolSink;
    // Work threads
    //
    DMFMODULE DmfModuleThread[THREAD_COUNT];
} DMF_CONTEXT_Tests_BufferPool;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_BufferPool)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_BufferPool)

// Memory Pool Tag.
//
#define MemoryTag 'lPBT'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static
VOID
Tests_BufferPool_Validate(
    _In_ DMFMODULE DmfModuleBufferPool,
    _In_ UINT8* ClientBuffer,
    _In_ CLIENT_BUFFER_CONTEXT* ClientBufferContext,
    _In_opt_ WDFMEMORY ClientBufferMemory,
    _In_opt_ PWDF_MEMORY_DESCRIPTOR MemoryDescriptor
    )
{
    UINT16 checkSum;
    WDF_MEMORY_DESCRIPTOR paramMemoryDescriptor;
    WDFMEMORY paramClientBufferMemory;
    ULONG paramClientBufferSize;
    VOID* paramClientBufferContext;
    ULONG paramClientBufferContextSize;

    UNREFERENCED_PARAMETER(ClientBufferContext);

    ASSERT(ClientBuffer != NULL);
    ASSERT(ClientBufferContext != NULL);

    DMF_BufferPool_ParametersGet(DmfModuleBufferPool,
                                 ClientBuffer,
                                 &paramMemoryDescriptor,
                                 &paramClientBufferMemory,
                                 &paramClientBufferSize,
                                 &paramClientBufferContext,
                                 &paramClientBufferContextSize);

    ASSERT(BUFFER_SIZE == paramClientBufferSize);
    ASSERT(sizeof(CLIENT_BUFFER_CONTEXT) == paramClientBufferContextSize);

    checkSum = TestsUtility_CrcCompute(ClientBuffer,
                                       BUFFER_SIZE);

    ASSERT(CLIENT_CONTEXT_SIGNATURE == ClientBufferContext->Signature);
    ASSERT(checkSum == ClientBufferContext->CheckSum);

    if (ClientBufferMemory != NULL)
    {
        ASSERT(ClientBufferMemory == paramClientBufferMemory);
    }

    if (MemoryDescriptor != NULL)
    {
        ASSERT(RtlCompareMemory(MemoryDescriptor, 
                                &paramMemoryDescriptor, 
                                sizeof(paramMemoryDescriptor) == sizeof(paramMemoryDescriptor)));
    }
}

static
NTSTATUS
Tests_BufferPool_GetFromPool(
    _In_ DMFMODULE DmfModuleBufferPool,
    _Out_ UINT8** ClientBuffer,
    _Out_ CLIENT_BUFFER_CONTEXT** ClientBufferContext
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_BufferPool* moduleContext;
    GET_ACTION getAction;
    PUINT8 clientBuffer;
    CLIENT_BUFFER_CONTEXT* clientBufferContext;
    WDFMEMORY clientBufferMemory;
    WDF_MEMORY_DESCRIPTOR memoryDescriptor;
    PWDF_MEMORY_DESCRIPTOR memoryDescriptorPointer;
    NTSTATUS ntStatus;

    ASSERT(ClientBuffer != NULL);
    ASSERT(ClientBufferContext != NULL);

    dmfModule = DMF_ParentModuleGet(DmfModuleBufferPool);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    clientBuffer = NULL;
    clientBufferContext = NULL;
    clientBufferMemory = NULL;
    memoryDescriptorPointer = NULL;

    // Generate a random action Id to get a buffer from the pool
    //
    getAction = (GET_ACTION)TestsUtility_GenerateRandomNumber(GET_ACTION_MIN,
                                                              GET_ACTION_MAX);
    // Get the buffer from the pool.
    //
    switch (getAction)
    {
    case GET_ACTION_PLAIN:
        ntStatus = DMF_BufferPool_Get(DmfModuleBufferPool,
                                      (VOID**)&clientBuffer,
                                      (VOID**)&clientBufferContext);
        if (NT_SUCCESS(ntStatus))
        {
            ASSERT(clientBuffer != NULL);
            ASSERT(clientBufferContext != NULL);
        }
        break;

    case GET_ACTION_WITH_MEMORY:
        ntStatus = DMF_BufferPool_GetWithMemory(DmfModuleBufferPool,
                                               (VOID**)&clientBuffer,
                                               (VOID**)&clientBufferContext,
                                               &clientBufferMemory);
        if (NT_SUCCESS(ntStatus))
        {
            ASSERT(clientBuffer != NULL);
            ASSERT(clientBufferContext != NULL);
            ASSERT(clientBufferMemory != NULL);
        }
        break;

    case GET_ACTION_WITH_MEMORY_DESCRIPTOR:
        ntStatus = DMF_BufferPool_GetWithMemoryDescriptor(DmfModuleBufferPool,
                                                         (VOID**)&clientBuffer,
                                                         &memoryDescriptor,
                                                         (VOID**)&clientBufferContext);
        if (NT_SUCCESS(ntStatus))
        {
            ASSERT(clientBuffer != NULL);
            ASSERT(clientBufferContext != NULL);
            memoryDescriptorPointer = &memoryDescriptor;
        }
        break;

    default:
        ntStatus = STATUS_INTERNAL_ERROR;
        ASSERT(FALSE);
        break;
    }

    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // If retrieving from source - populate the buffer with test data
    //
    if (moduleContext->DmfModuleBufferPoolSource == DmfModuleBufferPool)
    {
        TestsUtility_FillWithSequentialData(clientBuffer,
                                            BUFFER_SIZE);

        clientBufferContext->Signature = CLIENT_CONTEXT_SIGNATURE;
        clientBufferContext->CheckSum = TestsUtility_CrcCompute(clientBuffer,
                                                                BUFFER_SIZE);
    }

    // Make sure it passes validation tests
    //
    Tests_BufferPool_Validate(moduleContext->DmfModuleBufferPoolSource,
                              clientBuffer,
                              clientBufferContext,
                              clientBufferMemory,
                              memoryDescriptorPointer);

    *ClientBuffer = clientBuffer;
    *ClientBufferContext = clientBufferContext;

Exit:

    return ntStatus;
}

static
VOID
_IRQL_requires_max_(DISPATCH_LEVEL)
Tests_BufferPool_TimerCallback(
    _In_ DMFMODULE DmfModuleBufferPool,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext,
    _In_opt_ VOID* ClientDriverCallbackContext
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_BufferPool* moduleContext;

    UNREFERENCED_PARAMETER(ClientDriverCallbackContext);

    dmfModule = DMF_ParentModuleGet(DmfModuleBufferPool);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    Tests_BufferPool_Validate(moduleContext->DmfModuleBufferPoolSource,
                              (UINT8*)ClientBuffer,
                              (CLIENT_BUFFER_CONTEXT*)ClientBufferContext,
                              NULL,
                              NULL);

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolSource,
                       ClientBuffer);
}

static
BufferPool_EnumerationDispositionType
BufferPool_Enumeration_Callback(
    _In_ DMFMODULE DmfModuleBufferPool,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext,
    _In_opt_ VOID* ClientDriverCallbackContext
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_BufferPool* moduleContext;
    ENUM_CONTEXT* enumContext;

    dmfModule = DMF_ParentModuleGet(DmfModuleBufferPool);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    enumContext = (ENUM_CONTEXT*)ClientDriverCallbackContext;
    ASSERT(enumContext != NULL);

    Tests_BufferPool_Validate(moduleContext->DmfModuleBufferPoolSource,
                              (UINT8*)ClientBuffer,
                              (CLIENT_BUFFER_CONTEXT*)ClientBufferContext,
                              NULL,
                              NULL);

    // 'Dereferencing NULL pointer. 'enumContext' contains the same NULL value as 'ClientDriverCallbackContext' did.'
    //
    #pragma warning(suppress:28182)
    enumContext->ClientOwnsBuffer = (enumContext->Disposition == BufferPool_EnumerationDisposition_RemoveAndStopEnumeration);

    return enumContext->Disposition;
}

#pragma code_seg("PAGE")
static
NTSTATUS
Tests_BufferPool_ThreadAction_BufferAquire(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_BufferPool* moduleContext;
    PUT_ACTION putAction;
    PUINT8 clientBuffer;
    CLIENT_BUFFER_CONTEXT* clientBufferContext;
    NTSTATUS ntStatus;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;
    clientBuffer = NULL;
    clientBufferContext = NULL;

    // Don't acquire more then BUFFER_COUNT_MAX buffers
    //
    if (DMF_BufferPool_Count(moduleContext->DmfModuleBufferPoolSink) >= BUFFER_COUNT_MAX)
    {
        goto Exit;
    }

    // Get a buffer from the source.
    //
    ntStatus = Tests_BufferPool_GetFromPool(moduleContext->DmfModuleBufferPoolSource,
                                            &clientBuffer,
                                            &clientBufferContext);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }
    ASSERT(clientBuffer != NULL);
    ASSERT(clientBufferContext != NULL);

    // Generate a random action Id to put the buffer into the sink
    //
    putAction = (PUT_ACTION)TestsUtility_GenerateRandomNumber(PUT_ACTION_MIN,
                                                              PUT_ACTION_MAX);
    // Put the buffer into the sink
    //
    switch (putAction)
    {
    case PUT_ACTION_PLAIN:
        DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolSink, 
                           clientBuffer);
        break;

    case PUT_ACTION_WITH_TIMEOUT:
    {
        ULONGLONG timeout;

        // Generate a random timeout within 1 to 100 ms.
        //
        timeout = TestsUtility_GenerateRandomNumber(1,
                                                    100);

        DMF_BufferPool_PutInSinkWithTimer(moduleContext->DmfModuleBufferPoolSink, 
                                          clientBuffer,
                                          timeout,
                                          Tests_BufferPool_TimerCallback,
                                          NULL);
        break;
    }
    default:
        ASSERT(FALSE);
        break;
    }

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
NTSTATUS
Tests_BufferPool_ThreadAction_BufferReturn(
    _In_ DMFMODULE DmfModule
)
{
    DMF_CONTEXT_Tests_BufferPool* moduleContext;
    PUINT8 clientBuffer;
    CLIENT_BUFFER_CONTEXT* clientBufferContext;
    NTSTATUS ntStatus;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Get a buffer from the sink.
    //
    ntStatus = Tests_BufferPool_GetFromPool(moduleContext->DmfModuleBufferPoolSink,
                                            &clientBuffer,
                                            &clientBufferContext);
    if (!NT_SUCCESS(ntStatus))
    {
        // There is no buffer in sink pool because it has been called before acquire. 
        //
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    // Put it back to the source.
    //
    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolSource, 
                       clientBuffer);

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
NTSTATUS
Tests_BufferPool_ThreadAction_BufferEnumerate(
    _In_ DMFMODULE DmfModule
)
{
    DMF_CONTEXT_Tests_BufferPool* moduleContext;
    ENUM_CONTEXT enumContext;
    PUINT8 clientBuffer;
    ULONG randomNumber;
    CLIENT_BUFFER_CONTEXT* clientBufferContext;
    NTSTATUS ntStatus;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;
    clientBuffer = NULL;
    clientBufferContext = NULL;

    // Generate a random value for EnumerationDisposition
    //
    randomNumber = TestsUtility_GenerateRandomNumber(BufferPool_EnumerationDisposition_ContinueEnumeration,
                                                     BufferPool_EnumerationDisposition_ResetTimerAndContinueEnumeration);
    enumContext.Disposition = (BufferPool_EnumerationDispositionType)randomNumber;
    enumContext.ClientOwnsBuffer = FALSE;

    // Enumerate buffers in the sink
    //
    DMF_BufferPool_Enumerate(moduleContext->DmfModuleBufferPoolSink,
                             BufferPool_Enumeration_Callback,
                             &enumContext,
                             (VOID**)&clientBuffer,
                             (VOID**)&clientBufferContext);

    // Return the resulting buffer to the source, if needed.
    //
    if (enumContext.ClientOwnsBuffer)
    {
        ASSERT(clientBuffer != NULL);
        ASSERT(clientBufferContext != NULL);

        Tests_BufferPool_Validate(moduleContext->DmfModuleBufferPoolSource,
                                  clientBuffer,
                                  clientBufferContext,
                                  NULL,
                                  NULL);

        DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolSource,
                           clientBuffer);
    }

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
NTSTATUS
Tests_BufferPool_ThreadAction_BufferCount(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_BufferPool* moduleContext;
    ULONG currentCount;
    NTSTATUS ntStatus;

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Get the current number of buffers in sink
    //
    currentCount = DMF_BufferPool_Count(moduleContext->DmfModuleBufferPoolSink);

    // Adding thread count, since when acquiring we don't synchronize checking 
    // for the currently acquired number and putting buffers to sync.
    // So a number of acquired buffers may be up to THREAD_COUNT more, in case of a race conditions.
    //
    ASSERT(currentCount <= BUFFER_COUNT_MAX + THREAD_COUNT);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_BufferPool_WorkThread(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_BufferPool* moduleContext;
    TEST_ACTION testAction;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    // Generate a random test action Id for a current iteration.
    //
    testAction = (TEST_ACTION)TestsUtility_GenerateRandomNumber(TEST_ACTION_MINIUM,
                                                                TEST_ACTION_MAXIMUM);
    // Execute the test action.
    //
    switch (testAction)
    {
    case TEST_ACTION_AQUIRE:
        ntStatus = Tests_BufferPool_ThreadAction_BufferAquire(dmfModule);
        break;
    case TEST_ACTION_RETURN:
        ntStatus = Tests_BufferPool_ThreadAction_BufferReturn(dmfModule);
        break;
    case TEST_ACTION_ENUMERATE:
        ntStatus = Tests_BufferPool_ThreadAction_BufferEnumerate(dmfModule);
        break;
    case TEST_ACTION_COUNT:
        ntStatus = Tests_BufferPool_ThreadAction_BufferCount(dmfModule);
        break;
    default:
        ntStatus = STATUS_UNSUCCESSFUL;
        ASSERT(FALSE);
        break;
    }

    ASSERT(NT_SUCCESS(ntStatus) ||
           DMF_Thread_IsStopPending(DmfModuleThread));

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
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
Tests_BufferPool_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Test_BufferPool.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_CONTEXT_Tests_BufferPool* moduleContext;
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
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_BufferPool_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Tests_BufferPool.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_BufferPool* moduleContext;
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
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_BufferPool_ChildModulesAdd(
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
    DMF_CONTEXT_Tests_BufferPool* moduleContext;
    DMF_CONFIG_BufferPool moduleConfigBufferPool;
    DMF_CONFIG_Thread moduleConfigThread;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // BufferPool Source
    // -----------------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                              &moduleAttributes);
    moduleConfigBufferPool.BufferPoolMode = BufferPool_Mode_Source;
    moduleConfigBufferPool.Mode.SourceSettings.BufferContextSize = sizeof(CLIENT_BUFFER_CONTEXT);
    moduleConfigBufferPool.Mode.SourceSettings.BufferSize = BUFFER_SIZE;
    moduleConfigBufferPool.Mode.SourceSettings.BufferCount = BUFFER_COUNT_PREALLOCATED;
    moduleConfigBufferPool.Mode.SourceSettings.CreateWithTimer = TRUE;
    moduleConfigBufferPool.Mode.SourceSettings.EnableLookAside = TRUE;
    moduleConfigBufferPool.Mode.SourceSettings.PoolType = NonPagedPoolNx;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPoolSource);

    // BufferPool Sink
    // ---------------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                              &moduleAttributes);
    moduleConfigBufferPool.BufferPoolMode = BufferPool_Mode_Sink;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPoolSink);

    // Thread
    // ------
    //
    for (ULONG threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_BufferPool_WorkThread;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleThread[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_Tests_BufferPool;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_Tests_BufferPool;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Tests_BufferPool_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Test_BufferPool.

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

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_Tests_BufferPool);
    DmfCallbacksDmf_Tests_BufferPool.ChildModulesAdd = DMF_Tests_BufferPool_ChildModulesAdd;
    DmfCallbacksDmf_Tests_BufferPool.DeviceOpen = Tests_BufferPool_Open;
    DmfCallbacksDmf_Tests_BufferPool.DeviceClose = Tests_BufferPool_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_Tests_BufferPool,
                                            Tests_BufferPool,
                                            DMF_CONTEXT_Tests_BufferPool,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    DmfModuleDescriptor_Tests_BufferPool.CallbacksDmf = &DmfCallbacksDmf_Tests_BufferPool;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_Tests_BufferPool,
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

// eof: Dmf_Tests_BufferPool.c
//
