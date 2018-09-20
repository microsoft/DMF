/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_PingPongBuffer.c

Abstract:

    Functional tests for DMF_PingPongBuffer Module

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"

#include "Dmf_Tests_PingPongBuffer.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define SAMPLE_BUFFER_SIZE          (64)
#define PINGPONG_BUFFER_SIZE        (64)

typedef enum _TEST_ACTION {
    TEST_ACTION_RESET    = 0,
    TEST_ACTION_SHIFT    = 1,
    TEST_ACTION_CONSUME  = 2,
    TEST_ACTION_MIN      = TEST_ACTION_RESET,
    TEST_ACTION_MAX      = TEST_ACTION_CONSUME
} TEST_ACTION;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Buffer for test sample data
    //
    BYTE SampleBuffer[SAMPLE_BUFFER_SIZE];
    // Current read offset in the test sample data buffer
    //
    ULONG SampleReadOffset;
    // Current read offset in the test sample data buffer
    //
    ULONG SampleWriteOffset;
    // PingPongBuffer Module to test
    //
    DMFMODULE DmfModulePingPongBuffer;
    // Read thread
    //
    DMFMODULE DmfModuleReadThread;
    // Write thread
    //
    DMFMODULE DmfModuleWriteThread;
} DMF_CONTEXT_Tests_PingPongBuffer;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_PingPongBuffer)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_PingPongBuffer)

// Memory Pool Tag.
//
#define MemoryTag 'bpPT'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
static
void
Tests_PingPongBuffer_CheckIntegrity(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_PingPongBuffer* moduleContext;
    ULONG bytesToCheck;
    UCHAR* buffer;
    ULONG size;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    buffer = DMF_PingPongBuffer_Get(moduleContext->DmfModulePingPongBuffer,
                                    &size);

    // Max size of a fragment to check is from current sample data read offset 
    // till the end of sample buffer.
    //
    bytesToCheck = min((size), 
                       (SAMPLE_BUFFER_SIZE - moduleContext->SampleReadOffset));

    // Make sure ping-pong buffer content matches the corresponding sample data fragment.
    //
    ASSERT(bytesToCheck == RtlCompareMemory(moduleContext->SampleBuffer + moduleContext->SampleReadOffset,
                                            buffer,
                                            bytesToCheck));

    DMF_ModuleUnlock(DmfModule);

    return;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_PingPongBuffer_ActionReset(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_PingPongBuffer* moduleContext;
    UCHAR* buffer;
    ULONG size;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);
    
    // Reset the ping-pong buffer.
    //
    DMF_PingPongBuffer_Reset(moduleContext->DmfModulePingPongBuffer);

    moduleContext->SampleReadOffset = 0;
    moduleContext->SampleWriteOffset = 0;

    // Check if it was reset properly.
    //
    buffer = DMF_PingPongBuffer_Get(moduleContext->DmfModulePingPongBuffer,
                                    &size);

    ASSERT(NULL != buffer);
    ASSERT(0 == size);

    DMF_ModuleUnlock(DmfModule);

    Tests_PingPongBuffer_CheckIntegrity(DmfModule);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_PingPongBuffer_ActionShift(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_Tests_PingPongBuffer* moduleContext;
    ULONG currentSize;
    ULONG bytesToShift;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_PingPongBuffer_Get(moduleContext->DmfModulePingPongBuffer,
                           &currentSize);

    // Get a random offset to which we will shift.
    //
    bytesToShift = TestsUtility_GenerateRandomNumber(0,
                                                     currentSize);

    // Shift the ping-pong buffer
    //
    DMF_PingPongBuffer_Shift(moduleContext->DmfModulePingPongBuffer, 
                             bytesToShift);

    // Adjust sample data read pointer
    //
    moduleContext->SampleReadOffset += bytesToShift;
    if (moduleContext->SampleReadOffset >= SAMPLE_BUFFER_SIZE)
    {
        moduleContext->SampleReadOffset -= SAMPLE_BUFFER_SIZE;
    }

    // Make sure remaining ping-pong data is not corrupted.
    //
    Tests_PingPongBuffer_CheckIntegrity(DmfModule);

    return;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_PingPongBuffer_ActionConsume(
    _In_ DMFMODULE DmfModule
)
{
    DMF_CONTEXT_Tests_PingPongBuffer* moduleContext;
    ULONG currentSize;
    ULONG bytesToConsumeMax;
    ULONG offsetToConsume;
    ULONG bytesToConsume;
    UCHAR* bufferConsumed;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_PingPongBuffer_Get(moduleContext->DmfModulePingPongBuffer,
                           &currentSize);

    bytesToConsumeMax = min((currentSize),
                            (SAMPLE_BUFFER_SIZE - moduleContext->SampleReadOffset));

    // Get a random offset and size of data we will consume.
    //
    offsetToConsume = TestsUtility_GenerateRandomNumber(0,
                                                        bytesToConsumeMax);

    bytesToConsume = TestsUtility_GenerateRandomNumber(0,
                                                       bytesToConsumeMax - offsetToConsume);

    // Consume the data from ping-pong buffer.
    //
    bufferConsumed = DMF_PingPongBuffer_Consume(moduleContext->DmfModulePingPongBuffer,
                                                offsetToConsume,
                                                bytesToConsume);

    // Check if the consumed data matches the corresponding sample data fragment.
    //
    ASSERT(bufferConsumed != NULL);
    ASSERT(bytesToConsume == RtlCompareMemory(moduleContext->SampleBuffer + moduleContext->SampleReadOffset + offsetToConsume,
                                              bufferConsumed,
                                              bytesToConsume));

    // Adjust the sample data read offset.
    //
    moduleContext->SampleReadOffset += (offsetToConsume + bytesToConsume);
    if (moduleContext->SampleReadOffset >= SAMPLE_BUFFER_SIZE)
    {
        moduleContext->SampleReadOffset -= SAMPLE_BUFFER_SIZE;
    }

    // Make sure remaining ping-pong data is not corrupted.
    //
    Tests_PingPongBuffer_CheckIntegrity(DmfModule);

    return;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_PingPongBuffer_ReadThreadWork(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_PingPongBuffer* moduleContext;
    TEST_ACTION testAction;

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    // Generate a random test action Id for a current iteration.
    //
    testAction = (TEST_ACTION)TestsUtility_GenerateRandomNumber(TEST_ACTION_MIN,
                                                                TEST_ACTION_MAX);
    // Execute the test action.
    //
    switch (testAction)
    {
    case TEST_ACTION_RESET:
        Tests_PingPongBuffer_ActionReset(dmfModule);
        break;

    case TEST_ACTION_SHIFT:
        Tests_PingPongBuffer_ActionShift(dmfModule);
        break;

    case TEST_ACTION_CONSUME:
        Tests_PingPongBuffer_ActionConsume(dmfModule);
        break;

    default:
        ASSERT(FALSE);
        break;
    }

    // Repeat the test, until stop is signaled.
    //
    if (!DMF_Thread_IsStopPending(DmfModuleThread))
    {
        DMF_Thread_WorkReady(DmfModuleThread);
    }

    TestsUtility_YieldExecution();
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_PingPongBuffer_WriteThreadWork(
    _In_ DMFMODULE DmfModuleThread
)
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_PingPongBuffer* moduleContext;
    ULONG chunkSizeMax;
    ULONG chunkSize;
    ULONG currentSize;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    DMF_ModuleLock(dmfModule);

    DMF_PingPongBuffer_Get(moduleContext->DmfModulePingPongBuffer,
                           &currentSize);

    chunkSizeMax = min((PINGPONG_BUFFER_SIZE - currentSize),
                       (SAMPLE_BUFFER_SIZE - moduleContext->SampleWriteOffset));

    // Get a random number of bytes we will write.
    //
    chunkSize = TestsUtility_GenerateRandomNumber(0,
                                                  chunkSizeMax);

    // Write a fragment of sample data into a ping-pong buffer.
    //
    ntStatus = DMF_PingPongBuffer_Write(moduleContext->DmfModulePingPongBuffer,
                                        moduleContext->SampleBuffer + moduleContext->SampleWriteOffset,
                                        chunkSize,
                                        &currentSize);
    if (!NT_SUCCESS(ntStatus))
    {
        ASSERT(FALSE);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_PingPongBuffer_PingWrite fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Adjust sample data write pointer.
    //
    moduleContext->SampleWriteOffset += chunkSize;
    if (moduleContext->SampleWriteOffset >= SAMPLE_BUFFER_SIZE)
    {
        moduleContext->SampleWriteOffset = 0;
    }

Exit:

    DMF_ModuleUnlock(dmfModule);

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
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
Tests_PingPongBuffer_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Tests_PingPongBuffer.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_CONTEXT_Tests_PingPongBuffer* moduleContext;
    ULONG byteIndex;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Initialize sample data.
    //
    for (byteIndex = 0; byteIndex < SAMPLE_BUFFER_SIZE; ++byteIndex)
    {
        moduleContext->SampleBuffer[byteIndex] = byteIndex % 0xFF;
    }

    moduleContext->SampleReadOffset = 0;
    moduleContext->SampleWriteOffset = 0;

    ntStatus = DMF_Thread_Start(moduleContext->DmfModuleReadThread);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Start fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = DMF_Thread_Start(moduleContext->DmfModuleWriteThread);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Start fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    DMF_Thread_WorkReady(moduleContext->DmfModuleReadThread);
    DMF_Thread_WorkReady(moduleContext->DmfModuleWriteThread);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_PingPongBuffer_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Tests_PingPongBuffer.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_PingPongBuffer* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_Thread_Stop(moduleContext->DmfModuleReadThread);
    DMF_Thread_Stop(moduleContext->DmfModuleWriteThread);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_Tests_PingPongBuffer;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_Tests_PingPongBuffer;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Tests_PingPongBuffer_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Tests_PingPongBuffer.

Arguments:

    Device - Client driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_PingPongBuffer* moduleContext;
    DMF_CONFIG_Thread moduleConfigThread;
    DMF_CONFIG_PingPongBuffer moduleConfigPingPongBuffer;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfModule = NULL;

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_Tests_PingPongBuffer);
    DmfCallbacksDmf_Tests_PingPongBuffer.DeviceOpen = Tests_PingPongBuffer_Open;
    DmfCallbacksDmf_Tests_PingPongBuffer.DeviceClose = Tests_PingPongBuffer_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_Tests_PingPongBuffer,
                                            Tests_PingPongBuffer,
                                            DMF_CONTEXT_Tests_PingPongBuffer,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    DmfModuleDescriptor_Tests_PingPongBuffer.CallbacksDmf = &DmfCallbacksDmf_Tests_PingPongBuffer;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_Tests_PingPongBuffer,
                                &dmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    // Create child modules
    //

    // DmfModule will be set as ParentObject for all child modules.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = dmfModule;

    // PingPongBuffer
    // --------------
    //
    DMF_CONFIG_PingPongBuffer_AND_ATTRIBUTES_INIT(&moduleConfigPingPongBuffer,
                                                  &moduleAttributes);
    moduleConfigPingPongBuffer.BufferSize = PINGPONG_BUFFER_SIZE;
    moduleConfigPingPongBuffer.PoolType = PagedPool;
    moduleAttributes.PassiveLevel = TRUE;
    ntStatus = DMF_PingPongBuffer_Create(Device,
                                         &moduleAttributes,
                                         &objectAttributes,
                                         &moduleContext->DmfModulePingPongBuffer);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_PingPongBuffer_Create fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // ReadThread
    // ----------
    //
    DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                          &moduleAttributes);
    moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
    moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_PingPongBuffer_ReadThreadWork;
    ntStatus = DMF_Thread_Create(Device,
                                 &moduleAttributes,
                                 &objectAttributes,
                                 &moduleContext->DmfModuleReadThread);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Create fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // WriteThread
    // -----------
    //
    DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                          &moduleAttributes);
    moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
    moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_PingPongBuffer_WriteThreadWork;
    ntStatus = DMF_Thread_Create(Device,
                                 &moduleAttributes,
                                 &objectAttributes,
                                 &moduleContext->DmfModuleWriteThread);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Create fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    
    *DmfModule = dmfModule;

Exit:

    if (!NT_SUCCESS(ntStatus))
    {
        if (NULL != dmfModule)
        {
            DMF_Module_Destroy(dmfModule);
            dmfModule = NULL;
        }
    }    

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_Tests_PingPongBuffer.c
//
