/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_NotifyUserWithRequest.c

Abstract:

    Functional tests for Dmf_NotifyUserWithRequest Module.

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
#include "Dmf_Tests_NotifyUserWithRequest.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_Tests_NotifyUserWithRequest
{
    // NotifyUserWithRequest Module under test.
    //
    DMFMODULE DmfModuleNotifyUserWithRequest;
    // IoctlHandler Module to handle IOCTLs.
    //
    DMFMODULE DmfModuleIoctlHandler;
    // Timer for generating test data.
    //
    WDFTIMER Timer;
    // Incrementing data counter.
    //
    LONG DataCounter;
} DMF_CONTEXT_Tests_NotifyUserWithRequest;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_NotifyUserWithRequest)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_NotifyUserWithRequest)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define TIMER_PERIOD_MINIMUM_MS         100
#define TIMER_PERIOD_MAXIMUM_MS         5000
#define MAXIMUM_PENDING_REQUESTS        10
#define MAXIMUM_PENDING_DATA_BUFFERS    10

_Function_class_(EVT_DMF_NotifyUserWithRequest_Complete)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
Tests_NotifyUserWithRequest_CompleteCallback(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _In_opt_ ULONG_PTR Context,
    _In_ NTSTATUS NtStatus
    )
/*++

Routine Description:

    Completion callback for NotifyUserWithRequest.

Arguments:

    DmfModule - This Module's handle.
    Request - The request to complete.
    Context - Context containing event data.
    NtStatus - Status to complete the request with.

Return Value:

    None

--*/
{
    Tests_NotifyUserWithRequest_EventData* eventData;
    Tests_NotifyUserWithRequest_EventData* outputBuffer;
    size_t outputBufferSize;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(DmfModule);

    eventData = (Tests_NotifyUserWithRequest_EventData*)Context;

    // Get the output buffer from the request.
    //
    ntStatus = WdfRequestRetrieveOutputBuffer(Request,
                                              sizeof(Tests_NotifyUserWithRequest_EventData),
                                              (VOID**)&outputBuffer,
                                              &outputBufferSize);
    if (NT_SUCCESS(ntStatus))
    {
        if (outputBufferSize < sizeof(Tests_NotifyUserWithRequest_EventData))
        {
            DmfAssert(FALSE);
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Output buffer too small: outputBufferSize=%llu", outputBufferSize);
            ntStatus = STATUS_BUFFER_TOO_SMALL;
        }
        else
        {
            // Copy the event data to the output buffer.
            //
            RtlCopyMemory(outputBuffer,
                          eventData,
                          sizeof(Tests_NotifyUserWithRequest_EventData));
            WdfRequestSetInformation(Request,
                                    sizeof(Tests_NotifyUserWithRequest_EventData));
        }

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Completing request with DataValue=%d", eventData->DataValue);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestRetrieveOutputBuffer fails: ntStatus=%!STATUS!", ntStatus);
    }

    // Complete the request.
    //
    WdfRequestComplete(Request,
                       NtStatus);
}

_Function_class_(EVT_WDF_TIMER)
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
Tests_NotifyUserWithRequest_TimerCallback(
    _In_ WDFTIMER Timer
    )
/*++

Routine Description:

    Timer callback that generates data and processes it through NotifyUserWithRequest.

Arguments:

    Timer - WDF timer object.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_NotifyUserWithRequest* moduleContext;
    Tests_NotifyUserWithRequest_EventData eventData;

    dmfModule = (DMFMODULE)WdfTimerGetParentObject(Timer);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    // Increment the counter and create event data.
    //
    eventData.DataValue = InterlockedIncrement(&moduleContext->DataCounter);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Timer fired: DataValue=%d", eventData.DataValue);

    // Process the data through NotifyUserWithRequest.
    //
    DMF_NotifyUserWithRequest_DataProcess(moduleContext->DmfModuleNotifyUserWithRequest,
                                          Tests_NotifyUserWithRequest_CompleteCallback,
                                          &eventData,
                                          STATUS_SUCCESS);

    // Restart timer with a random duration.
    //
    LONG randomPeriodMs = TestsUtility_GenerateRandomNumber(TIMER_PERIOD_MINIMUM_MS,
                                                            TIMER_PERIOD_MAXIMUM_MS);
    WdfTimerStart(moduleContext->Timer,
                  WDF_REL_TIMEOUT_IN_MS(randomPeriodMs));
}

NTSTATUS
Tests_NotifyUserWithRequest_IoctlCallback(
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

    IOCTL handler callback.

Arguments:

    DmfModule - This Module's handle (IoctlHandler).
    Queue - WDF queue object.
    Request - WDF request object.
    IoControlCode - IOCTL code.
    InputBuffer - Input buffer.
    InputBufferSize - Size of input buffer.
    OutputBuffer - Output buffer.
    OutputBufferSize - Size of output buffer.
    BytesReturned - Number of bytes returned.

Return Value:

    NTSTATUS

--*/
{
    DMFMODULE dmfModuleParent;
    DMF_CONTEXT_Tests_NotifyUserWithRequest* moduleContext;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferSize);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);

    dmfModuleParent = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleParent);
    ntStatus = STATUS_NOT_SUPPORTED;
    *BytesReturned = 0;

    switch (IoControlCode)
    {
        case IOCTL_Tests_NotifyUserWithRequest_GET_EVENT:
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "IOCTL_Tests_NotifyUserWithRequest_GET_EVENT: Request=0x%p", Request);

            // Process the request through NotifyUserWithRequest.
            // This will pend the request until data is available.
            //
            ntStatus = DMF_NotifyUserWithRequest_RequestProcess(moduleContext->DmfModuleNotifyUserWithRequest,
                                                                Request);
            if (NT_SUCCESS(ntStatus))
            {
                ntStatus = STATUS_PENDING;
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Request processed successfully");
            }
            else
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_NotifyUserWithRequest_RequestProcess fails: ntStatus=%!STATUS!", ntStatus);
            }
            break;
        }
        default:
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Unknown IOCTL: 0x%x", IoControlCode);
            ntStatus = STATUS_NOT_SUPPORTED;
            break;
        }
    }

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

IoctlHandler_IoctlRecord Tests_NotifyUserWithRequest_IoctlHandlerTable[] =
{
    { (LONG)IOCTL_Tests_NotifyUserWithRequest_GET_EVENT, 0, sizeof(Tests_NotifyUserWithRequest_EventData), Tests_NotifyUserWithRequest_IoctlCallback, FALSE },
};

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Tests_NotifyUserWithRequest_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Tests_NotifyUserWithRequest.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Tests_NotifyUserWithRequest* moduleContext;
    WDF_TIMER_CONFIG timerConfig;
    WDF_OBJECT_ATTRIBUTES timerAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Create a timer for generating test data.
    //
    WDF_TIMER_CONFIG_INIT(&timerConfig,
                                   Tests_NotifyUserWithRequest_TimerCallback);
    timerConfig.AutomaticSerialization = FALSE;

    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = DmfModule;
    timerAttributes.ExecutionLevel = WdfExecutionLevelPassive;

    ntStatus = WdfTimerCreate(&timerConfig,
                             &timerAttributes,
                             &moduleContext->Timer);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfTimerCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Start the timer with a random duration.
    //
    LONG randomPeriodMs = TestsUtility_GenerateRandomNumber(TIMER_PERIOD_MINIMUM_MS,
                                                            TIMER_PERIOD_MAXIMUM_MS);
    WdfTimerStart(moduleContext->Timer,
                  WDF_REL_TIMEOUT_IN_MS(randomPeriodMs));

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Timer started");

    ntStatus = STATUS_SUCCESS;

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
DMF_Tests_NotifyUserWithRequest_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Tests_NotifyUserWithRequest.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_NotifyUserWithRequest* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Stop the timer.
    //
    WdfTimerStop(moduleContext->Timer,
                 TRUE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Timer stopped");

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_NotifyUserWithRequest_ChildModulesAdd(
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
    DMF_CONTEXT_Tests_NotifyUserWithRequest* moduleContext;
    DMF_CONFIG_NotifyUserWithRequest moduleConfigNotifyUserWithRequest;
    DMF_CONFIG_IoctlHandler moduleConfigIoctlHandler;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // NotifyUserWithRequest
    // ---------------------
    //
    DMF_CONFIG_NotifyUserWithRequest_AND_ATTRIBUTES_INIT(&moduleConfigNotifyUserWithRequest,
                                                         &moduleAttributes);
    moduleConfigNotifyUserWithRequest.MaximumNumberOfPendingRequests = MAXIMUM_PENDING_REQUESTS;
    moduleConfigNotifyUserWithRequest.MaximumNumberOfPendingDataBuffers = MAXIMUM_PENDING_DATA_BUFFERS;
    moduleConfigNotifyUserWithRequest.SizeOfDataBuffer = sizeof(Tests_NotifyUserWithRequest_EventData);
    moduleAttributes.PassiveLevel = TRUE;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleNotifyUserWithRequest);

    // IoctlHandler
    // ------------
    //
    DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(&moduleConfigIoctlHandler,
                                                &moduleAttributes);
    moduleConfigIoctlHandler.IoctlRecords = Tests_NotifyUserWithRequest_IoctlHandlerTable;
    moduleConfigIoctlHandler.IoctlRecordCount = _countof(Tests_NotifyUserWithRequest_IoctlHandlerTable);
    moduleConfigIoctlHandler.AccessModeFilter = IoctlHandler_AccessModeDefault;
    moduleConfigIoctlHandler.DeviceInterfaceGuid = GUID_DEVINTERFACE_Tests_NotifyUserWithRequest;
    moduleConfigIoctlHandler.ReferenceString = NULL;
    moduleAttributes.PassiveLevel = TRUE;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleIoctlHandler);

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
DMF_Tests_NotifyUserWithRequest_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Tests_NotifyUserWithRequest.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Tests_NotifyUserWithRequest;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Tests_NotifyUserWithRequest;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Tests_NotifyUserWithRequest);
    dmfCallbacksDmf_Tests_NotifyUserWithRequest.ChildModulesAdd = DMF_Tests_NotifyUserWithRequest_ChildModulesAdd;
    dmfCallbacksDmf_Tests_NotifyUserWithRequest.DeviceOpen = DMF_Tests_NotifyUserWithRequest_Open;
    dmfCallbacksDmf_Tests_NotifyUserWithRequest.DeviceClose = DMF_Tests_NotifyUserWithRequest_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Tests_NotifyUserWithRequest,
                                            Tests_NotifyUserWithRequest,
                                            DMF_CONTEXT_Tests_NotifyUserWithRequest,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Tests_NotifyUserWithRequest.CallbacksDmf = &dmfCallbacksDmf_Tests_NotifyUserWithRequest;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Tests_NotifyUserWithRequest,
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

// eof: Dmf_Tests_NotifyUserWithRequest.c
//
