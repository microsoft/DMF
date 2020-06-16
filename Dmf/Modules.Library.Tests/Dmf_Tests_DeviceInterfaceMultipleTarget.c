/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_DeviceInterfaceMultipleTarget.c

Abstract:

    Functional tests for Dmf_DeviceInterfaceMultipleTarget Module.

Environment:

    Kernel-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_Tests_DeviceInterfaceMultipleTarget.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define THREAD_COUNT                            (1)
#define MAXIMUM_SLEEP_TIME_MS                   (15000)
// Keep synchronous maximum time short to make driver disable faster.
//
#define MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS       (1000)
// Asynchronous minimum sleep time to make sure request can be cancelled.
//
#define MINIMUM_SLEEP_TIME_MS                   (4000)

// Random timeouts for IOCTLs sent.
//
#define TIMEOUT_FAST_MS             100
#define TIMEOUT_SLOW_MS             5000
#define TIMEOUT_TRAFFIC_DELAY_MS    1000

typedef enum _TEST_ACTION
{
    TEST_ACTION_SYNCHRONOUS,
    TEST_ACTION_ASYNCHRONOUS,
    TEST_ACTION_ASYNCHRONOUSCANCEL,
    TEST_ACTION_COUNT,
    TEST_ACTION_MINIUM = TEST_ACTION_SYNCHRONOUS,
    TEST_ACTION_MAXIMUM = TEST_ACTION_ASYNCHRONOUSCANCEL
} TEST_ACTION;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Modules under test.
    //
    DMFMODULE DmfModuleDeviceInterfaceMultipleTargetDispatchInput;
    DMFMODULE DmfModuleDeviceInterfaceMultipleTargetPassiveInput;
    DMFMODULE DmfModuleDeviceInterfaceMultipleTargetDispatchInputNonContinuous;
    DMFMODULE DmfModuleDeviceInterfaceMultipleTargetPassiveInputNonContinuous;
    // Source of buffers sent asynchronously.
    //
    DMFMODULE DmfModuleBufferPool;
} DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_DeviceInterfaceMultipleTarget)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_DeviceInterfaceMultipleTarget)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Stores the Module threadIndex so that the corresponding alterable sleep
// can be retrieved inside the thread's callback.
//
typedef struct
{
    DeviceInterfaceMultipleTarget_Target Target;
    DMFMODULE DmfModuleTestsDeviceInterfaceMultipleTarget;
    DMFMODULE DmfModuleAlertableSleep;
} THREAD_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE(THREAD_CONTEXT);

typedef struct
{
    DeviceInterfaceMultipleTarget_Target Target;
    // Work threads that perform actions on the DeviceInterfaceMultipleTarget Module.
    // +1 makes it easy to set THREAD_COUNT = 0 for test purposes.
    //
    DMFMODULE DmfModuleThread[THREAD_COUNT + 1];
    // Use alertable sleep to allow driver to unload faster.
    //
    DMFMODULE DmfModuleAlertableSleep;
    // Need to keep track of this because there is no pre-close per target.
    //
    BOOLEAN Closed;
} TARGET_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(TARGET_CONTEXT, DeviceInterfaceMultipleTarget_TargetContextGet);

_Function_class_(EVT_DMF_ContinuousRequestTarget_BufferInput)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
Tests_DeviceInterfaceMultipleTarget_BufferInput(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(*InputBufferSize) VOID* InputBuffer,
    _Out_ size_t* InputBufferSize,
    _In_ VOID* ClientBuferContextInput
    )
/*++

Routine Description:

    Populate an input buffer before it is sent.

Arguments:

    DmfModule - DMF_DeviceInterfaceMultipleTarget.
    InputBuffer - Input buffer to populate.
    InputBufferSize - Size of InputBuffer.
    ClientBuferContextInput - Context corresponding to the InputBuffer.
    
Return Value:

    None

--*/
{
    Tests_IoctlHandler_Sleep sleepIoctlBuffer;
    GUID guid;

    UNREFERENCED_PARAMETER(InputBufferSize);
    UNREFERENCED_PARAMETER(ClientBuferContextInput);

    // This is just for test purposes.
    //
    DMF_DeviceInterfaceMultipleTarget_GuidGet(DmfModule,
                                              &guid);

    sleepIoctlBuffer.TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0,
                                                                                 MAXIMUM_SLEEP_TIME_MS);
    RtlCopyMemory(InputBuffer,
                  &sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));
    *InputBufferSize = sizeof(sleepIoctlBuffer);
}

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceMultipleTarget_ThreadAction_Synchronous(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ DMFMODULE InstanceToSendTo
    )
/*++

Routine Description:

    Sends synchronous requests to a Target on a given Instance. 

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    DmfModuleAlertableSleep - Used to wait.
    Target - The given Target.
    InstanceToSendTo - Which of the instantiated Modules to send to.
    
Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;
    NTSTATUS ntStatus;
    Tests_IoctlHandler_Sleep sleepIoctlBuffer;
    size_t bytesWritten;

    UNREFERENCED_PARAMETER(DmfModuleAlertableSleep);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // This is just for test purposes to test the API.
    //
    WDFIOTARGET ioTarget;
    ntStatus = DMF_DeviceInterfaceMultipleTarget_Get(InstanceToSendTo,
                                                     Target,
                                                     &ioTarget);

    RtlZeroMemory(&sleepIoctlBuffer,
                  sizeof(sleepIoctlBuffer));

    sleepIoctlBuffer.TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                 MAXIMUM_SLEEP_TIME_SYNCHRONOUS_MS);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceMultipleTarget_SendSynchronously(InstanceToSendTo,
                                                                   Target,
                                                                   &sleepIoctlBuffer,
                                                                   sizeof(Tests_IoctlHandler_Sleep),
                                                                   NULL,
                                                                   NULL,
                                                                   ContinuousRequestTarget_RequestType_Ioctl,
                                                                   IOCTL_Tests_IoctlHandler_SLEEP,
                                                                   0,
                                                                   &bytesWritten);
    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    // TODO: Get time and compare with send time.
    //
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceMultipleTarget_ThreadAction_SynchronousDispatchInput(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Sends synchronous requests to a given Target on the DispatchInput instance.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    DmfModuleAlertableSleep - Used to wait.
    Target - The given Target.
    
Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_DeviceInterfaceMultipleTarget_ThreadAction_Synchronous(DmfModule,
                                                                 DmfModuleAlertableSleep,
                                                                 Target,
                                                                 moduleContext->DmfModuleDeviceInterfaceMultipleTargetDispatchInput);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceMultipleTarget_ThreadAction_SynchronousDispatchInputNonContinuous(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Sends synchronous requests to a given Target on the DispatchInputNonContinuous instance.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    DmfModuleAlertableSleep - Used to wait.
    Target - The given Target.
    
Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_DeviceInterfaceMultipleTarget_ThreadAction_Synchronous(DmfModule,
                                                                 DmfModuleAlertableSleep,
                                                                 Target,
                                                                 moduleContext->DmfModuleDeviceInterfaceMultipleTargetDispatchInputNonContinuous);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceMultipleTarget_ThreadAction_SynchronousPassiveInput(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Sends synchronous requests to a given Target on the PassiveInput instance.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    DmfModuleAlertableSleep - Used to wait.
    Target - The given Target.
    
Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_DeviceInterfaceMultipleTarget_ThreadAction_Synchronous(DmfModule,
                                                                 DmfModuleAlertableSleep,
                                                                 Target,
                                                                 moduleContext->DmfModuleDeviceInterfaceMultipleTargetPassiveInput);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceMultipleTarget_ThreadAction_SynchronousPassiveInputNonContinuous(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Sends synchronous requests to a given Target on the PassiveInputNonContinuous instance.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    DmfModuleAlertableSleep - Used to wait.
    Target - The given Target.
    
Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_DeviceInterfaceMultipleTarget_ThreadAction_Synchronous(DmfModule,
                                                                 DmfModuleAlertableSleep,
                                                                 Target,
                                                                 moduleContext->DmfModuleDeviceInterfaceMultipleTargetPassiveInputNonContinuous);
}
#pragma code_seg()

_Function_class_(EVT_DMF_ContinuousRequestTarget_SendCompletion)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
Tests_DeviceInterfaceMultipleTarget_SendCompletion(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientRequestContext,
    _In_reads_(InputBufferBytesWritten) VOID* InputBuffer,
    _In_ size_t InputBufferBytesWritten,
    _In_reads_(OutputBufferBytesRead) VOID* OutputBuffer,
    _In_ size_t OutputBufferBytesRead,
    _In_ NTSTATUS CompletionStatus
    )
/*++

Routine Description:

    Completion routine called by underlying stack when a given buffer is completed.

Arguments:

    DmfModule - DMF_DeviceInterfaceMultipleTarget.
    ClientRequestContext - Context associated with the buffer's Request.
    InputBuffer - The buffer associated with the request that is completed.
    InputBufferBytesWritten - How much data was written to InputBuffer.
    OutputBuffer - The buffer associated with the request that is completed.
    OutputBufferBytesRead - How much data was read from OutputBuffer.
    CompletionStatus - NTSTATUS returned by underlying target.
    
Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;
    Tests_IoctlHandler_Sleep* sleepIoctlBuffer;

    // TODO: Get time and compare with send time.
    //
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferBytesWritten);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferBytesRead);
    UNREFERENCED_PARAMETER(CompletionStatus);

    moduleContext = (DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget*)ClientRequestContext;
    sleepIoctlBuffer = (Tests_IoctlHandler_Sleep*)InputBuffer;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "MDI: RECEIVE sleepIoctlBuffer->TimeToSleepMilliseconds=%d InputBuffer=0x%p", 
                sleepIoctlBuffer->TimeToSleepMilliseconds,
                InputBuffer);

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPool,
                       (VOID*)sleepIoctlBuffer);
}

_Function_class_(EVT_DMF_ContinuousRequestTarget_SendCompletion)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
Tests_DeviceInterfaceMultipleTarget_SendCompletionMustBeCancelled(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientRequestContext,
    _In_reads_(InputBufferBytesWritten) VOID* InputBuffer,
    _In_ size_t InputBufferBytesWritten,
    _In_reads_(OutputBufferBytesRead) VOID* OutputBuffer,
    _In_ size_t OutputBufferBytesRead,
    _In_ NTSTATUS CompletionStatus
    )
/*++

Routine Description:

    Completion routine called by underlying stack when a given buffer is completed.

Arguments:

    DmfModule - DMF_DeviceInterfaceMultipleTarget.
    ClientRequestContext - Context associated with the buffer's Request.
    InputBuffer - The buffer associated with the request that is completed.
    InputBufferBytesWritten - How much data was written to InputBuffer.
    OutputBuffer - The buffer associated with the request that is completed.
    OutputBufferBytesRead - How much data was read from OutputBuffer.
    CompletionStatus - NTSTATUS returned by underlying target.
    
Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;
    Tests_IoctlHandler_Sleep* sleepIoctlBuffer;

    // TODO: Get time and compare with send time.
    //
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(InputBufferBytesWritten);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferBytesRead);
    UNREFERENCED_PARAMETER(CompletionStatus);

    moduleContext = (DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget*)ClientRequestContext;
    sleepIoctlBuffer = (Tests_IoctlHandler_Sleep*)InputBuffer;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "MDI: CANCELED sleepIoctlBuffer->TimeToSleepMilliseconds=%d InputBuffer=0x%p", 
                sleepIoctlBuffer->TimeToSleepMilliseconds,
                InputBuffer);

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPool,
                       (VOID*)sleepIoctlBuffer);

#if !defined(DMF_WIN32_MODE)
    DmfAssert(STATUS_CANCELLED == CompletionStatus);
#endif
}

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceMultipleTarget_ThreadAction_Asynchronous(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ DMFMODULE InstanceToSendTo
    )
/*++

Routine Description:

    Sends asynchronous requests to a Target on a given Instance. 

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    DmfModuleAlertableSleep - Used to wait.
    Target - The given Target.
    InstanceToSendTo - Which of the instantiated Modules to send to.
    
Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;
    NTSTATUS ntStatus;
    size_t bytesWritten;
    ULONG timeoutMs;
    Tests_IoctlHandler_Sleep* sleepIoctlBuffer;

    UNREFERENCED_PARAMETER(DmfModuleAlertableSleep);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    DmfAssert(NT_SUCCESS(ntStatus));

    RtlZeroMemory(sleepIoctlBuffer,
                  sizeof(Tests_IoctlHandler_Sleep));

    if (TestsUtility_GenerateRandomNumber(0,
                                          1))
    {
        timeoutMs = TestsUtility_GenerateRandomNumber(TIMEOUT_FAST_MS,
                                                      TIMEOUT_SLOW_MS);
    }
    else
    {
        timeoutMs = 0;
    }

    sleepIoctlBuffer->TimeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                                  MAXIMUM_SLEEP_TIME_MS);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceMultipleTarget_Send(InstanceToSendTo,
                                                      Target,
                                                      sleepIoctlBuffer,
                                                      sizeof(Tests_IoctlHandler_Sleep),
                                                      NULL,
                                                      NULL,
                                                      ContinuousRequestTarget_RequestType_Ioctl,
                                                      IOCTL_Tests_IoctlHandler_SLEEP,
                                                      timeoutMs,
                                                      Tests_DeviceInterfaceMultipleTarget_SendCompletion,
                                                      moduleContext);
    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousDispatchInput(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Sends asynchronous requests to a Target of the DispatchInput instance.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    DmfModuleAlertableSleep - Used to wait.
    Target - The given Target.
    InstanceToSendTo - Which of the instantiated Modules to send to.
    
Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_DeviceInterfaceMultipleTarget_ThreadAction_Asynchronous(DmfModule,
                                                                  DmfModuleAlertableSleep,
                                                                  Target,
                                                                  moduleContext->DmfModuleDeviceInterfaceMultipleTargetDispatchInput);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousDispatchInputNonContinuous(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Sends asynchronous requests to a Target of the DispatchInputNonContinuous instance.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    DmfModuleAlertableSleep - Used to wait.
    Target - The given Target.
    InstanceToSendTo - Which of the instantiated Modules to send to.
    
Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_DeviceInterfaceMultipleTarget_ThreadAction_Asynchronous(DmfModule,
                                                                  DmfModuleAlertableSleep,
                                                                  Target,
                                                                  moduleContext->DmfModuleDeviceInterfaceMultipleTargetDispatchInputNonContinuous);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousPassiveInput(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Sends asynchronous requests to a given Target of the PassiveInput instance.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    DmfModuleAlertableSleep - Used to wait.
    Target - The given Target.
    InstanceToSendTo - Which of the instantiated Modules to send to.
    
Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_DeviceInterfaceMultipleTarget_ThreadAction_Asynchronous(DmfModule,
                                                                  DmfModuleAlertableSleep,
                                                                  Target,
                                                                  moduleContext->DmfModuleDeviceInterfaceMultipleTargetPassiveInput);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousPassiveInputNonContinuous(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Sends asynchronous requests to a given Target of the PassiveInputNonContinuous instance.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    DmfModuleAlertableSleep - Used to wait.
    Target - The given Target.
    InstanceToSendTo - Which of the instantiated Modules to send to.
    
Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_DeviceInterfaceMultipleTarget_ThreadAction_Asynchronous(DmfModule,
                                                                  DmfModuleAlertableSleep,
                                                                  Target,
                                                                  moduleContext->DmfModuleDeviceInterfaceMultipleTargetPassiveInputNonContinuous);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousCancel(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ DMFMODULE InstanceToSendTo
    )
/*++

Routine Description:

    Sends requests to a Target of a DispatchInput instance and cancels them.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    DmfModuleAlertableSleep - Used to wait.
    Target - The given Target.
    InstanceToSendTo - Which of the instantiated Modules to send to.
    
Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;
    NTSTATUS ntStatus;
    size_t bytesWritten;
    RequestTarget_DmfRequest DmfRequestId;
    BOOLEAN requestCanceled;
    LONG timeToSleepMilliseconds;
    Tests_IoctlHandler_Sleep* sleepIoctlBuffer;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    /////////////////////////////////////////////////////////////////////////////////////
    // Cancel the request after waiting for a while. It may or may not be canceled.
    //

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    DmfAssert(NT_SUCCESS(ntStatus));

    timeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                MAXIMUM_SLEEP_TIME_MS);

    sleepIoctlBuffer->TimeToSleepMilliseconds = timeToSleepMilliseconds;
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceMultipleTarget_SendEx(InstanceToSendTo,
                                                        Target,
                                                        sleepIoctlBuffer,
                                                        sizeof(Tests_IoctlHandler_Sleep),
                                                        NULL,
                                                        NULL,
                                                        ContinuousRequestTarget_RequestType_Ioctl,
                                                        IOCTL_Tests_IoctlHandler_SLEEP,
                                                        0,
                                                        Tests_DeviceInterfaceMultipleTarget_SendCompletion,
                                                        moduleContext,
                                                        &DmfRequestId);

    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    timeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                MAXIMUM_SLEEP_TIME_MS);
    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        timeToSleepMilliseconds);

    // Cancel the request if possible.
    //
    requestCanceled = DMF_DeviceInterfaceMultipleTarget_Cancel(InstanceToSendTo,
                                                               Target,
                                                               DmfRequestId);

    if (!NT_SUCCESS(ntStatus))
    {
        // Driver is shutting down...get out (after canceling the request).
        //
        goto Exit;
    }

    timeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                MAXIMUM_SLEEP_TIME_MS);

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    DmfAssert(NT_SUCCESS(ntStatus));

    /////////////////////////////////////////////////////////////////////////////////////
    // Cancel the request after waiting the same time sent in timeout. 
    // It may or may not be canceled.
    //

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    DmfAssert(NT_SUCCESS(ntStatus));

    timeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                MAXIMUM_SLEEP_TIME_MS);

    sleepIoctlBuffer->TimeToSleepMilliseconds = timeToSleepMilliseconds;
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceMultipleTarget_SendEx(InstanceToSendTo,
                                                        Target,
                                                        sleepIoctlBuffer,
                                                        sizeof(Tests_IoctlHandler_Sleep),
                                                        NULL,
                                                        NULL,
                                                        ContinuousRequestTarget_RequestType_Ioctl,
                                                        IOCTL_Tests_IoctlHandler_SLEEP,
                                                        0,
                                                        Tests_DeviceInterfaceMultipleTarget_SendCompletion,
                                                        moduleContext,
                                                        &DmfRequestId);

    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        timeToSleepMilliseconds);

    // Cancel the request if possible.
    //
    requestCanceled = DMF_DeviceInterfaceMultipleTarget_Cancel(InstanceToSendTo,
                                                               Target,
                                                               DmfRequestId);

    if (!NT_SUCCESS(ntStatus))
    {
        // Driver is shutting down...get out (after canceling the request).
        //
        goto Exit;
    }

    timeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                MAXIMUM_SLEEP_TIME_MS);

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    DmfAssert(NT_SUCCESS(ntStatus));

    /////////////////////////////////////////////////////////////////////////////////////
    // Cancel the request immediately after sending it. It may or may not be canceled.
    //

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    DmfAssert(NT_SUCCESS(ntStatus));

    timeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(0, 
                                                                MAXIMUM_SLEEP_TIME_MS);

    sleepIoctlBuffer->TimeToSleepMilliseconds = timeToSleepMilliseconds;
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceMultipleTarget_SendEx(InstanceToSendTo,
                                                        Target,
                                                        sleepIoctlBuffer,
                                                        sizeof(Tests_IoctlHandler_Sleep),
                                                        NULL,
                                                        NULL,
                                                        ContinuousRequestTarget_RequestType_Ioctl,
                                                        IOCTL_Tests_IoctlHandler_SLEEP,
                                                        0,
                                                        Tests_DeviceInterfaceMultipleTarget_SendCompletion,
                                                        moduleContext,
                                                        &DmfRequestId);

    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Cancel the request immediately after sending it.
    //
    requestCanceled = DMF_DeviceInterfaceMultipleTarget_Cancel(InstanceToSendTo,
                                                               Target,
                                                               DmfRequestId);

    /////////////////////////////////////////////////////////////////////////////////////
    // Cancel the request after it is normally completed. It should never cancel unless driver is shutting down.
    //

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    DmfAssert(NT_SUCCESS(ntStatus));

    timeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(MINIMUM_SLEEP_TIME_MS, 
                                                                MAXIMUM_SLEEP_TIME_MS);
    
    DmfAssert(timeToSleepMilliseconds >= MINIMUM_SLEEP_TIME_MS);
    sleepIoctlBuffer->TimeToSleepMilliseconds = timeToSleepMilliseconds;
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "MDI: SEND: sleepIoctlBuffer->TimeToSleepMilliseconds=%d sleepIoctlBuffer=0x%p", 
                timeToSleepMilliseconds,
                sleepIoctlBuffer);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceMultipleTarget_SendEx(InstanceToSendTo,
                                                        Target,
                                                        sleepIoctlBuffer,
                                                        sizeof(Tests_IoctlHandler_Sleep),
                                                        NULL,
                                                        NULL,
                                                        ContinuousRequestTarget_RequestType_Ioctl,
                                                        IOCTL_Tests_IoctlHandler_SLEEP,
                                                        0,
                                                        Tests_DeviceInterfaceMultipleTarget_SendCompletion,
                                                        moduleContext,
                                                        &DmfRequestId);

    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        timeToSleepMilliseconds * 4);

    // Cancel the request if possible.
    // It should never cancel since the time just waited is 4 times what was sent above.
    //
    requestCanceled = DMF_DeviceInterfaceMultipleTarget_Cancel(InstanceToSendTo,
                                                               Target,
                                                               DmfRequestId);

    if (!NT_SUCCESS(ntStatus))
    {
        // Driver is shutting down...get out (after canceling the request).
        //
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "MDI: CANCELED: sleepIoctlBuffer->TimeToSleepMilliseconds=%d sleepIoctlBuffer=0x%p", 
                timeToSleepMilliseconds,
                sleepIoctlBuffer);

    /////////////////////////////////////////////////////////////////////////////////////
    // Cancel the request before it is normally completed. It should always cancel.
    //

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPool,
                                  (VOID**)&sleepIoctlBuffer,
                                  NULL);
    DmfAssert(NT_SUCCESS(ntStatus));

    timeToSleepMilliseconds = TestsUtility_GenerateRandomNumber(MINIMUM_SLEEP_TIME_MS, 
                                                                MAXIMUM_SLEEP_TIME_MS);

    DmfAssert(timeToSleepMilliseconds >= MINIMUM_SLEEP_TIME_MS);
    sleepIoctlBuffer->TimeToSleepMilliseconds = timeToSleepMilliseconds;
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "MDI: SEND: sleepIoctlBuffer->TimeToSleepMilliseconds=%d sleepIoctlBuffer=0x%p", 
                timeToSleepMilliseconds,
                sleepIoctlBuffer);
    bytesWritten = 0;
    ntStatus = DMF_DeviceInterfaceMultipleTarget_SendEx(InstanceToSendTo,
                                                        Target,
                                                        sleepIoctlBuffer,
                                                        sizeof(Tests_IoctlHandler_Sleep),
                                                        NULL,
                                                        NULL,
                                                        ContinuousRequestTarget_RequestType_Ioctl,
                                                        IOCTL_Tests_IoctlHandler_SLEEP,
                                                        0,
                                                        Tests_DeviceInterfaceMultipleTarget_SendCompletionMustBeCancelled,
                                                        moduleContext,
                                                        &DmfRequestId);

    DmfAssert(NT_SUCCESS(ntStatus) || (ntStatus == STATUS_CANCELLED) || (ntStatus == STATUS_INVALID_DEVICE_STATE));
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ntStatus = DMF_AlertableSleep_Sleep(DmfModuleAlertableSleep,
                                        0,
                                        timeToSleepMilliseconds / 4);
    // Cancel the request if possible.
    // It should always cancel since the time just waited is 1/4 the time that was sent above.
    //
    requestCanceled = DMF_DeviceInterfaceMultipleTarget_Cancel(InstanceToSendTo,
                                                               Target,
                                                               DmfRequestId);
    // Even though the attempt to cancel happens in 1/4 of the total time out, it is possible
    // that the cancel call happens just as the underlying driver is going away. In that case,
    // the request is not canceled by this call, but it will be canceled by the underlying
    // driver. (In this case the call to cancel returns FALSE.) Thus, no assert is possible here.
    // This case happens often as the underlying driver comes and goes every second.
    //

    if (!NT_SUCCESS(ntStatus))
    {
        // Driver is shutting down...get out.
        //
        goto Exit;
    }

Exit:
    ;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousCancelDispatchInput(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Sends requests to a Target of a DispatchInput instance and cancels them.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    DmfModuleAlertableSleep - Used to wait.
    Target - The given Target.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousCancel(DmfModule,
                                                                        DmfModuleAlertableSleep,
                                                                        Target,
                                                                        moduleContext->DmfModuleDeviceInterfaceMultipleTargetDispatchInput);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousCancelDispatchInputNonContinuous(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Sends requests to a Target of a DispatchInputNonContinous instance and cancels them.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    DmfModuleAlertableSleep - Used to wait.
    Target - The given Target.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousCancel(DmfModule,
                                                                        DmfModuleAlertableSleep,
                                                                        Target,
                                                                        moduleContext->DmfModuleDeviceInterfaceMultipleTargetDispatchInputNonContinuous);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousCancelPassiveInput(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Sends requests to a Target of a PassiveInput instance and cancels them.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    DmfModuleAlertableSleep - Used to wait.
    Target - The given Target.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousCancel(DmfModule,
                                                                        DmfModuleAlertableSleep,
                                                                        Target,
                                                                        moduleContext->DmfModuleDeviceInterfaceMultipleTargetPassiveInput);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
void
Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousCancelPassiveInputNonContinuous(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE DmfModuleAlertableSleep,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Sends requests to a Target of a PassiveInputNonContinuous instance and cancels them.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    DmfModuleAlertableSleep - Used to wait.
    Target - The given Target.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousCancel(DmfModule,
                                                                        DmfModuleAlertableSleep,
                                                                        Target,
                                                                        moduleContext->DmfModuleDeviceInterfaceMultipleTargetPassiveInputNonContinuous);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_DeviceInterfaceMultipleTarget_WorkThreadDispatchInput(
    _In_ DMFMODULE DmfModuleThread
    )
/*++

Routine Description:

    Thread work callback for DispatchInput instance.

Arguments:

    DmfModule - DMF_Thread.

Return Value:

    None

--*/
{
    TEST_ACTION testAction;
    THREAD_CONTEXT* threadContext;

    PAGED_CODE();

    threadContext = WdfObjectGet_THREAD_CONTEXT(DmfModuleThread);

    // Generate a random test action Id for a current iteration.
    //
    testAction = (TEST_ACTION)TestsUtility_GenerateRandomNumber(TEST_ACTION_MINIUM,
                                                                TEST_ACTION_MAXIMUM);

    // Execute the test action.
    //
    switch (testAction)
    {
        case TEST_ACTION_SYNCHRONOUS:
            Tests_DeviceInterfaceMultipleTarget_ThreadAction_SynchronousDispatchInput(threadContext->DmfModuleTestsDeviceInterfaceMultipleTarget,
                                                                                      threadContext->DmfModuleAlertableSleep,
                                                                                      threadContext->Target);
            break;
        case TEST_ACTION_ASYNCHRONOUS:
            Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousDispatchInput(threadContext->DmfModuleTestsDeviceInterfaceMultipleTarget,
                                                                                       threadContext->DmfModuleAlertableSleep,
                                                                                       threadContext->Target);
            break;
        case TEST_ACTION_ASYNCHRONOUSCANCEL:
            Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousCancelDispatchInput(threadContext->DmfModuleTestsDeviceInterfaceMultipleTarget,
                                                                                             threadContext->DmfModuleAlertableSleep,
                                                                                             threadContext->Target);
            break;
        default:
            DmfAssert(FALSE);
            break;
    }

    // Repeat the test, until stop is signaled.
    //
    if (!DMF_Thread_IsStopPending(DmfModuleThread))
    {
        // Short delay to reduce traffic.
        //
        DMF_Utility_DelayMilliseconds(TIMEOUT_TRAFFIC_DELAY_MS);
        DMF_Thread_WorkReady(DmfModuleThread);
    }

    TestsUtility_YieldExecution();
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_DeviceInterfaceMultipleTarget_WorkThreadDispatchInputNonContinuous(
    _In_ DMFMODULE DmfModuleThread
    )
/*++

Routine Description:

    Thread work callback for DispatchInput instance.

Arguments:

    DmfModule - DMF_Thread.

Return Value:

    None

--*/
{
    TEST_ACTION testAction;
    THREAD_CONTEXT* threadContext;

    PAGED_CODE();

    threadContext = WdfObjectGet_THREAD_CONTEXT(DmfModuleThread);

    // Generate a random test action Id for a current iteration.
    //
    testAction = (TEST_ACTION)TestsUtility_GenerateRandomNumber(TEST_ACTION_MINIUM,
                                                                TEST_ACTION_MAXIMUM);

    // Execute the test action.
    //
    switch (testAction)
    {
        case TEST_ACTION_SYNCHRONOUS:
            Tests_DeviceInterfaceMultipleTarget_ThreadAction_SynchronousDispatchInputNonContinuous(threadContext->DmfModuleTestsDeviceInterfaceMultipleTarget,
                                                                                                   threadContext->DmfModuleAlertableSleep,
                                                                                                   threadContext->Target);
            break;
        case TEST_ACTION_ASYNCHRONOUS:
            Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousDispatchInputNonContinuous(threadContext->DmfModuleTestsDeviceInterfaceMultipleTarget,
                                                                                                    threadContext->DmfModuleAlertableSleep,
                                                                                                    threadContext->Target);
            break;
        case TEST_ACTION_ASYNCHRONOUSCANCEL:
            Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousCancelDispatchInputNonContinuous(threadContext->DmfModuleTestsDeviceInterfaceMultipleTarget,
                                                                                                          threadContext->DmfModuleAlertableSleep,
                                                                                                          threadContext->Target);
            break;
        default:
            DmfAssert(FALSE);
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
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_DeviceInterfaceMultipleTarget_WorkThreadPassiveInput(
    _In_ DMFMODULE DmfModuleThread
    )
/*++

Routine Description:

    Thread work callback for PassiveInput instance.

Arguments:

    DmfModule - DMF_Thread.

Return Value:

    None

--*/
{
    TEST_ACTION testAction;
    THREAD_CONTEXT* threadContext;

    PAGED_CODE();

    threadContext = WdfObjectGet_THREAD_CONTEXT(DmfModuleThread);

    // Generate a random test action Id for a current iteration.
    //
    testAction = (TEST_ACTION)TestsUtility_GenerateRandomNumber(TEST_ACTION_MINIUM,
                                                                TEST_ACTION_MAXIMUM);

    // Execute the test action.
    //
    switch (testAction)
    {
        case TEST_ACTION_SYNCHRONOUS:
            Tests_DeviceInterfaceMultipleTarget_ThreadAction_SynchronousPassiveInput(threadContext->DmfModuleTestsDeviceInterfaceMultipleTarget,
                                                                                     threadContext->DmfModuleAlertableSleep,
                                                                                     threadContext->Target);
            break;
        case TEST_ACTION_ASYNCHRONOUS:
            Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousPassiveInput(threadContext->DmfModuleTestsDeviceInterfaceMultipleTarget,
                                                                                      threadContext->DmfModuleAlertableSleep,
                                                                                      threadContext->Target);
            break;
        case TEST_ACTION_ASYNCHRONOUSCANCEL:
            Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousCancelPassiveInput(threadContext->DmfModuleTestsDeviceInterfaceMultipleTarget,
                                                                                            threadContext->DmfModuleAlertableSleep,
                                                                                            threadContext->Target);
            break;
        default:
            DmfAssert(FALSE);
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
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_DeviceInterfaceMultipleTarget_WorkThreadPassiveInputNonContinuous(
    _In_ DMFMODULE DmfModuleThread
    )
/*++

Routine Description:

    Thread work callback for PassiveInput instance.

Arguments:

    DmfModule - DMF_Thread.

Return Value:

    None

--*/
{
    TEST_ACTION testAction;
    THREAD_CONTEXT* threadContext;

    PAGED_CODE();

    threadContext = WdfObjectGet_THREAD_CONTEXT(DmfModuleThread);

    // Generate a random test action Id for a current iteration.
    //
    testAction = (TEST_ACTION)TestsUtility_GenerateRandomNumber(TEST_ACTION_MINIUM,
                                                                TEST_ACTION_MAXIMUM);

    // Execute the test action.
    //
    switch (testAction)
    {
        case TEST_ACTION_SYNCHRONOUS:
            Tests_DeviceInterfaceMultipleTarget_ThreadAction_SynchronousPassiveInputNonContinuous(threadContext->DmfModuleTestsDeviceInterfaceMultipleTarget,
                                                                                                  threadContext->DmfModuleAlertableSleep,
                                                                                                  threadContext->Target);
            break;
        case TEST_ACTION_ASYNCHRONOUS:
            Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousPassiveInputNonContinuous(threadContext->DmfModuleTestsDeviceInterfaceMultipleTarget,
                                                                                                   threadContext->DmfModuleAlertableSleep,
                                                                                                   threadContext->Target);
            break;
        case TEST_ACTION_ASYNCHRONOUSCANCEL:
            Tests_DeviceInterfaceMultipleTarget_ThreadAction_AsynchronousCancelPassiveInputNonContinuous(threadContext->DmfModuleTestsDeviceInterfaceMultipleTarget,
                                                                                                         threadContext->DmfModuleAlertableSleep,
                                                                                                         threadContext->Target);
            break;
        default:
            DmfAssert(FALSE);
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
NTSTATUS
Tests_DeviceInterfaceMultipleTarget_Start(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Starts the threads for a given Target.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    Target - The given target.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;
    NTSTATUS ntStatus;
    LONG threadIndex;
    TARGET_CONTEXT* targetContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    targetContext = DeviceInterfaceMultipleTarget_TargetContextGet(Target);

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        ntStatus = DMF_Thread_Start(targetContext->DmfModuleThread[threadIndex]);
        if (!NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }
    }

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_Thread_WorkReady(targetContext->DmfModuleThread[threadIndex]);
    }

Exit:
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceMultipleTarget_TargetThreadsStop(
    _In_ DMFMODULE DmfModule,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Stops the threads for a given Target.

Arguments:

    DmfModule - DMF_Tests_DeviceInterfaceMultipleTarget.
    Target - The given target.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;
    LONG threadIndex;
    TARGET_CONTEXT* targetContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    targetContext = DeviceInterfaceMultipleTarget_TargetContextGet(Target);

    // Interrupt any long sleeps for all threds using this target.
    //
    DMF_AlertableSleep_Abort(targetContext->DmfModuleAlertableSleep,
                             0);

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        // Stop thread.
        //
        DMF_Thread_Stop(targetContext->DmfModuleThread[threadIndex]);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
Tests_DeviceInterfaceMultipleTarget_ThreadPreClose(
    _In_ DMFMODULE DmfModuleThread
    )
{
    THREAD_CONTEXT* threadContext;

    threadContext = WdfObjectGet_THREAD_CONTEXT(DmfModuleThread);

    DMF_AlertableSleep_Abort(threadContext->DmfModuleAlertableSleep,
                             0);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceMultipleTarget_OnTargetArrival(
    _In_ DMFMODULE DeviceInterfaceMultipleTarget,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ EVT_DMF_Thread_Function ThreadCallback
    )
/*++

Routine Description:

    Prepare data structures for a newly arrived given Target.

Arguments:

    DeviceInterfaceMultipleTarget - DMF_DeviceInterfaceMultipleTarget.
    Target - The given target.
    ThreadCallback - The thread callback that performs work for the given Target.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;
    NTSTATUS ntStatus;
    TARGET_CONTEXT* targetContext;
    WDFDEVICE device;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMFMODULE dmfModuleParent;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    ULONG threadIndex;

    PAGED_CODE();

    dmfModuleParent = DMF_ParentModuleGet(DeviceInterfaceMultipleTarget);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);
    targetContext = DeviceInterfaceMultipleTarget_TargetContextGet(Target);
    device = DMF_ParentDeviceGet(DeviceInterfaceMultipleTarget);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes,
                                            TARGET_CONTEXT);

    ntStatus = WdfObjectAllocateContext(Target,
                                        &objectAttributes,
                                        (VOID**)&targetContext);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    targetContext->Target = Target;

    DMF_CONFIG_AlertableSleep moduleConfigAlertableSleep;
    DMF_CONFIG_AlertableSleep_AND_ATTRIBUTES_INIT(&moduleConfigAlertableSleep,
                                                  &moduleAttributes);
    moduleConfigAlertableSleep.EventCount = 1;
    ntStatus = DMF_AlertableSleep_Create(device,
                                            &moduleAttributes,
                                            &objectAttributes,
                                            &targetContext->DmfModuleAlertableSleep);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        DMF_CONFIG_Thread moduleConfigThread;
        DMF_MODULE_EVENT_CALLBACKS moduleEventCallbacks;

        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        objectAttributes.ParentObject = Target;

        DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                              &moduleAttributes);
        moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
        moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = ThreadCallback;
        DMF_MODULE_ATTRIBUTES_EVENT_CALLBACKS_INIT(&moduleAttributes,
                                                   &moduleEventCallbacks);
        moduleEventCallbacks.EvtModuleOnDeviceNotificationPreClose = Tests_DeviceInterfaceMultipleTarget_ThreadPreClose;
        ntStatus = DMF_Thread_Create(device,
                                     &moduleAttributes,
                                     &objectAttributes,
                                     &targetContext->DmfModuleThread[threadIndex]);
        if (!NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }

        THREAD_CONTEXT* threadContext;

        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&objectAttributes,
                                               THREAD_CONTEXT);
        WdfObjectAllocateContext(targetContext->DmfModuleThread[threadIndex],
                                 &objectAttributes,
                                 (PVOID*)&threadContext);

        // Each thread context contains the same data as it is common for the target.
        //
        threadContext->DmfModuleTestsDeviceInterfaceMultipleTarget = dmfModuleParent;
        threadContext->Target = Target;
        threadContext->DmfModuleAlertableSleep = targetContext->DmfModuleAlertableSleep;
    }

    ntStatus = Tests_DeviceInterfaceMultipleTarget_Start(dmfModuleParent,
                                                         Target);
    DmfAssert(NT_SUCCESS(ntStatus));

Exit:
    ;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Tests_DeviceInterfaceMultipleTarget_OnTargetRemoval(
    _In_ DMFMODULE DeviceInterfaceMultipleTarget,
    _In_ DeviceInterfaceMultipleTarget_Target Target
    )
/*++

Routine Description:

    Destroy data structures for a newly removed given Target.

Arguments:

    DeviceInterfaceMultipleTarget - DMF_DeviceInterfaceMultipleTarget.
    Target - The given target.

Return Value:

    None

--*/
{
    TARGET_CONTEXT* targetContext;
    ULONG threadIndex;

    PAGED_CODE();

    targetContext = DeviceInterfaceMultipleTarget_TargetContextGet(Target);

    WDFIOTARGET ioTarget;
    DMF_DeviceInterfaceMultipleTarget_Get(DeviceInterfaceMultipleTarget,
                                          Target,
                                          &ioTarget);
    if (ioTarget != NULL)
    {
        // QueryRemove case.
        //
        WdfIoTargetPurge(ioTarget,
                         WdfIoTargetPurgeIoAndWait);
    }
    else
    {
        // Target is already closed.
        //
    }

    Tests_DeviceInterfaceMultipleTarget_TargetThreadsStop(DeviceInterfaceMultipleTarget,
                                                          Target);

    for (threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
    {
        WdfObjectDelete(targetContext->DmfModuleThread[threadIndex]);
        targetContext->DmfModuleThread[threadIndex] = NULL;
    }

    WdfObjectDelete(targetContext->DmfModuleAlertableSleep);
    targetContext->DmfModuleAlertableSleep = NULL;
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
Tests_DeviceInterfaceMultipleTarget_OnStateChangeDispatchInput(
    _In_ DMFMODULE DeviceInterfaceMultipleTarget,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ DeviceInterfaceMultipleTarget_StateType IoTargetState
    )
/*++

Routine Description:

    Called when a given Target arrives or is being removed.

Arguments:

    DeviceInterfaceMultipleTarget - DMF_DeviceInterfaceMultipleTarget.
    Target - The given target.
    IoTargetState - Indicates how the given Target state is changing.

Return Value:

    None

--*/
{
    if ((IoTargetState == DeviceInterfaceMultipleTarget_StateType_Open) ||
        (IoTargetState == DeviceInterfaceMultipleTarget_StateType_QueryRemoveCancelled))
    {
        Tests_DeviceInterfaceMultipleTarget_OnTargetArrival(DeviceInterfaceMultipleTarget,
                                                            Target,
                                                            Tests_DeviceInterfaceMultipleTarget_WorkThreadDispatchInput);
    }
    else if ((IoTargetState == DeviceInterfaceMultipleTarget_StateType_QueryRemove) ||
             (IoTargetState == DeviceInterfaceMultipleTarget_StateType_Close))
    {
        TARGET_CONTEXT* targetContext = DeviceInterfaceMultipleTarget_TargetContextGet(Target);
        if (! targetContext->Closed)
        {
            targetContext->Closed = TRUE;
            Tests_DeviceInterfaceMultipleTarget_OnTargetRemoval(DeviceInterfaceMultipleTarget,
                                                                Target);
        }
    }
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
Tests_DeviceInterfaceMultipleTarget_OnStateChangeDispatchInputNonContinuous(
    _In_ DMFMODULE DeviceInterfaceMultipleTarget,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ DeviceInterfaceMultipleTarget_StateType IoTargetState
    )
/*++

Routine Description:

    Called when a given Target arrives or is being removed.

Arguments:

    DeviceInterfaceMultipleTarget - DMF_DeviceInterfaceMultipleTarget.
    Target - The given target.
    IoTargetState - Indicates how the given Target state is changing.

Return Value:

    None

--*/
{
    if ((IoTargetState == DeviceInterfaceMultipleTarget_StateType_Open) ||
        (IoTargetState == DeviceInterfaceMultipleTarget_StateType_QueryRemoveCancelled))
    {
        Tests_DeviceInterfaceMultipleTarget_OnTargetArrival(DeviceInterfaceMultipleTarget,
                                                            Target,
                                                            Tests_DeviceInterfaceMultipleTarget_WorkThreadDispatchInputNonContinuous);
    }
    else if ((IoTargetState == DeviceInterfaceMultipleTarget_StateType_QueryRemove) ||
             (IoTargetState == DeviceInterfaceMultipleTarget_StateType_Close))
    {
        TARGET_CONTEXT* targetContext = DeviceInterfaceMultipleTarget_TargetContextGet(Target);
        if (! targetContext->Closed)
        {
            targetContext->Closed = TRUE;
            Tests_DeviceInterfaceMultipleTarget_OnTargetRemoval(DeviceInterfaceMultipleTarget,
                                                                Target);
        }
    }
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
Tests_DeviceInterfaceMultipleTarget_OnStateChangePassiveInput(
    _In_ DMFMODULE DeviceInterfaceMultipleTarget,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ DeviceInterfaceMultipleTarget_StateType IoTargetState
    )
/*++

Routine Description:

    Called when a given Target arrives or is being removed.

Arguments:

    DeviceInterfaceMultipleTarget - DMF_DeviceInterfaceMultipleTarget.
    Target - The given target.
    IoTargetState - Indicates how the given Target state is changing.

Return Value:

    None

--*/
{
    if ((IoTargetState == DeviceInterfaceMultipleTarget_StateType_Open) ||
        (IoTargetState == DeviceInterfaceMultipleTarget_StateType_QueryRemoveCancelled))
    {
        Tests_DeviceInterfaceMultipleTarget_OnTargetArrival(DeviceInterfaceMultipleTarget,
                                                            Target,
                                                            Tests_DeviceInterfaceMultipleTarget_WorkThreadPassiveInput);
    }
    else if ((IoTargetState == DeviceInterfaceMultipleTarget_StateType_QueryRemove) ||
             (IoTargetState == DeviceInterfaceMultipleTarget_StateType_Close))
    {
        TARGET_CONTEXT* targetContext = DeviceInterfaceMultipleTarget_TargetContextGet(Target);
        if (! targetContext->Closed)
        {
            targetContext->Closed = TRUE;
            Tests_DeviceInterfaceMultipleTarget_OnTargetRemoval(DeviceInterfaceMultipleTarget,
                                                                Target);
        }
    }
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
Tests_DeviceInterfaceMultipleTarget_OnStateChangePassiveInputNonContinuous(
    _In_ DMFMODULE DeviceInterfaceMultipleTarget,
    _In_ DeviceInterfaceMultipleTarget_Target Target,
    _In_ DeviceInterfaceMultipleTarget_StateType IoTargetState
    )
/*++

Routine Description:

    Called when a given Target arrives or is being removed.

Arguments:

    DeviceInterfaceMultipleTarget - DMF_DeviceInterfaceMultipleTarget.
    Target - The given target.
    IoTargetState - Indicates how the given Target state is changing.

Return Value:

    None

--*/
{
    if ((IoTargetState == DeviceInterfaceMultipleTarget_StateType_Open) ||
        (IoTargetState == DeviceInterfaceMultipleTarget_StateType_QueryRemoveCancelled))
    {
        Tests_DeviceInterfaceMultipleTarget_OnTargetArrival(DeviceInterfaceMultipleTarget,
                                                            Target,
                                                            Tests_DeviceInterfaceMultipleTarget_WorkThreadPassiveInputNonContinuous);
    }
    else if ((IoTargetState == DeviceInterfaceMultipleTarget_StateType_QueryRemove) ||
             (IoTargetState == DeviceInterfaceMultipleTarget_StateType_Close))
    {
        TARGET_CONTEXT* targetContext = DeviceInterfaceMultipleTarget_TargetContextGet(Target);
        if (! targetContext->Closed)
        {
            targetContext->Closed = TRUE;
            Tests_DeviceInterfaceMultipleTarget_OnTargetRemoval(DeviceInterfaceMultipleTarget,
                                                                Target);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_DeviceInterfaceMultipleTarget_ChildModulesAdd(
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
    DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget* moduleContext;
    DMF_CONFIG_DeviceInterfaceMultipleTarget moduleConfigDeviceInterfaceMultipleTarget;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // General purpose buffers for asynchronous transactions.
    //
    DMF_CONFIG_BufferPool moduleConfigBufferPool;
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                              &moduleAttributes);
    moduleConfigBufferPool.BufferPoolMode = BufferPool_Mode_Source;
    moduleConfigBufferPool.Mode.SourceSettings.BufferCount = 10;
    moduleConfigBufferPool.Mode.SourceSettings.BufferSize = sizeof(Tests_IoctlHandler_Sleep);
    moduleConfigBufferPool.Mode.SourceSettings.EnableLookAside = TRUE;
    moduleConfigBufferPool.Mode.SourceSettings.PoolType = NonPagedPoolNx;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPool);

    // DeviceInterfaceMultipleTarget (DISPATCH_LEVEL)
    // Processes Input Buffers.
    //
    DMF_CONFIG_DeviceInterfaceMultipleTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceMultipleTarget,
                                                                 &moduleAttributes);
    moduleConfigDeviceInterfaceMultipleTarget.DeviceInterfaceMultipleTargetGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.BufferCountInput = 1;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.BufferInputSize = sizeof(Tests_IoctlHandler_Sleep);
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 1;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.PoolTypeInput = NonPagedPoolNx;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_SLEEP;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput = Tests_DeviceInterfaceMultipleTarget_BufferInput;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Automatic;
    moduleConfigDeviceInterfaceMultipleTarget.EvtDeviceInterfaceMultipleTargetOnStateChange = Tests_DeviceInterfaceMultipleTarget_OnStateChangeDispatchInput;

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleDeviceInterfaceMultipleTargetDispatchInput);

    // DeviceInterfaceMultipleTarget (DISPATCH_LEVEL)
    // Processes Input Buffers.
    //
    DMF_CONFIG_DeviceInterfaceMultipleTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceMultipleTarget,
                                                                 &moduleAttributes);
    moduleConfigDeviceInterfaceMultipleTarget.DeviceInterfaceMultipleTargetGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
    moduleConfigDeviceInterfaceMultipleTarget.EvtDeviceInterfaceMultipleTargetOnStateChange = Tests_DeviceInterfaceMultipleTarget_OnStateChangeDispatchInputNonContinuous;

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleDeviceInterfaceMultipleTargetDispatchInputNonContinuous);

    // DeviceInterfaceMultipleTarget (PASSIVE_LEVEL)
    // Processes Input Buffers.
    //
    DMF_CONFIG_DeviceInterfaceMultipleTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceMultipleTarget,
                                                                 &moduleAttributes);
    moduleConfigDeviceInterfaceMultipleTarget.DeviceInterfaceMultipleTargetGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.BufferCountInput = 1;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.BufferInputSize = sizeof(Tests_IoctlHandler_Sleep);
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestCount = 1;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.PoolTypeInput = NonPagedPoolNx;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetIoctl = IOCTL_Tests_IoctlHandler_SLEEP;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput = Tests_DeviceInterfaceMultipleTarget_BufferInput;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.RequestType = ContinuousRequestTarget_RequestType_Ioctl;
    moduleConfigDeviceInterfaceMultipleTarget.ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Manual;
    moduleConfigDeviceInterfaceMultipleTarget.EvtDeviceInterfaceMultipleTargetOnStateChange = Tests_DeviceInterfaceMultipleTarget_OnStateChangePassiveInput;

    moduleAttributes.PassiveLevel = TRUE;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleDeviceInterfaceMultipleTargetPassiveInput);

    // DeviceInterfaceMultipleTarget (PASSIVE_LEVEL)
    // Processes Input Buffers.
    //
    DMF_CONFIG_DeviceInterfaceMultipleTarget_AND_ATTRIBUTES_INIT(&moduleConfigDeviceInterfaceMultipleTarget,
                                                                 &moduleAttributes);
    moduleConfigDeviceInterfaceMultipleTarget.DeviceInterfaceMultipleTargetGuid = GUID_DEVINTERFACE_Tests_IoctlHandler;
    moduleConfigDeviceInterfaceMultipleTarget.EvtDeviceInterfaceMultipleTargetOnStateChange = Tests_DeviceInterfaceMultipleTarget_OnStateChangePassiveInputNonContinuous;

    moduleAttributes.PassiveLevel = TRUE;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleDeviceInterfaceMultipleTargetPassiveInputNonContinuous);

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
DMF_Tests_DeviceInterfaceMultipleTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Test_DeviceInterfaceMultipleTarget.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Tests_DeviceInterfaceMultipleTarget;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Tests_DeviceInterfaceMultipleTarget;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Tests_DeviceInterfaceMultipleTarget);
    dmfCallbacksDmf_Tests_DeviceInterfaceMultipleTarget.ChildModulesAdd = DMF_Tests_DeviceInterfaceMultipleTarget_ChildModulesAdd;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Tests_DeviceInterfaceMultipleTarget,
                                            Tests_DeviceInterfaceMultipleTarget,
                                            DMF_CONTEXT_Tests_DeviceInterfaceMultipleTarget,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware);

    dmfModuleDescriptor_Tests_DeviceInterfaceMultipleTarget.CallbacksDmf = &dmfCallbacksDmf_Tests_DeviceInterfaceMultipleTarget;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Tests_DeviceInterfaceMultipleTarget,
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

// eof: Dmf_Tests_DeviceInterfaceMultipleTarget.c
//
