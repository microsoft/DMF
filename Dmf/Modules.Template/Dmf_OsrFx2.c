/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_OsrFx2.c

Abstract:

    OsrFx2 sample as a Module. One way to use DMF is to make the whole driver a Module. This example
    shows how that can be done.

Environment:

    Kernel-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Template.h"
#include "DmfModules.Template.Trace.h"

#include "Dmf_OsrFx2.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Handles IOCTLs for FX2.
    //
    DMFMODULE DmfModuleIoctlHandler;
    // Allows callbacks to Client at PASSIVE_LEVEL if Client requests that.
    //
    DMFMODULE DmfModuleQueuedWorkitem;
    // WDF USB Device handle.
    //
    WDFUSBDEVICE UsbDevice;
    // WDF USB Interface handle.
    //
    WDFUSBINTERFACE UsbInterface;
    // The FX2 Bulk Reader pipe.
    //
    WDFUSBPIPE BulkReadPipe;
    // The FX2 Bulk Writer pipe.
    //
    WDFUSBPIPE BulkWritePipe;
    // The FX2 Bulk Interrupt pipe (for switches).
    //
    WDFUSBPIPE InterruptPipe;
    // Stores current switch state.
    //
    UCHAR CurrentSwitchState;
    // Stores USB device traits.
    //
    ULONG UsbDeviceTraits;
} DMF_CONTEXT_OsrFx2;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(OsrFx2)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(OsrFx2)

// Memory Pool Tag.
//
#define MemoryTag 'MpmT'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

const LONGLONG DEFAULT_CONTROL_TRANSFER_TIMEOUT = 5 * -1 * WDF_TIMEOUT_TO_SEC;

// Define the vendor commands supported by our device.
//
#define USBFX2LK_READ_7SEGMENT_DISPLAY      0xD4
#define USBFX2LK_READ_SWITCHES              0xD6
#define USBFX2LK_READ_BARGRAPH_DISPLAY      0xD7
#define USBFX2LK_SET_BARGRAPH_DISPLAY       0xD8
#define USBFX2LK_IS_HIGH_SPEED              0xD9
#define USBFX2LK_REENUMERATE                0xDA
#define USBFX2LK_SET_7SEGMENT_DISPLAY       0xDB

// Define the features that we can clear and set on our device.
//
#define USBFX2LK_FEATURE_EPSTALL            0x00
#define USBFX2LK_FEATURE_WAKE               0x01

// Order of endpoints in the interface descriptor.
//
#define INTERRUPT_IN_ENDPOINT_INDEX         0
#define BULK_OUT_ENDPOINT_INDEX             1
#define BULK_IN_ENDPOINT_INDEX              2

#define TEST_BOARD_TRANSFER_BUFFER_SIZE     (64*1024)
#define DEVICE_DESCRIPTOR_LENGTH            256

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
ScheduledTask_Result_Type 
OsrFx2_QueuedWorkitemFunction(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext
    )
{
    UNREFERENCED_PARAMETER(ClientBufferContext);

    DMFMODULE parentDmfModule;
    DMF_CONTEXT_OsrFx2* moduleContext;
    DMF_CONFIG_OsrFx2* moduleConfig;
    UCHAR* switchState;

    FuncEntry(DMF_TRACE);

    switchState = (UCHAR*)ClientBuffer;
    parentDmfModule = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(parentDmfModule);
    moduleConfig = DMF_CONFIG_GET(parentDmfModule);

    moduleConfig->InterruptPipeCallbackPassive(parentDmfModule,
                                               *switchState,
                                               STATUS_SUCCESS);

    FuncExit(DMF_TRACE, "returnValue=ScheduledTask_WorkResult_Success");

    return ScheduledTask_WorkResult_Success;
}

static
VOID
OsrFx2_EvtRequestReadCompletionRoutine(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ WDF_REQUEST_COMPLETION_PARAMS* CompletionParams,
    _In_ WDFCONTEXT Context
    )
/*++

Routine Description:

    This is the completion routine for reads. Completes the given read request.

Arguments:

    Request - The given write request handle.
    Target - The WDFIOTARGET to which the Request was sent.
    CompletionParams - Request completion params.
    Context - Driver supplied context. It is the corresponding DMFHANDLE.

Returns:

    None

--*/
{
    NTSTATUS ntStatus;
    size_t bytesRead;
    WDF_USB_REQUEST_COMPLETION_PARAMS* usbCompletionParams;

    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Context);

    FuncEntry(DMF_TRACE);

    ntStatus = CompletionParams->IoStatus.Status;

    usbCompletionParams = CompletionParams->Parameters.Usb.Completion;

    bytesRead =  usbCompletionParams->Parameters.PipeRead.Length;

    if (NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Number of bytes read: %I64d", (INT64)bytesRead);
    } 
    else 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Read fails: ntStatus=%!STATUS! UsbdStatus 0x%x", 
                    ntStatus, 
                    usbCompletionParams->UsbdStatus);
    }

    WDFQUEUE queue = WdfRequestGetIoQueue(Request);
    DMFMODULE* dmfModuleAddress = WdfObjectGet_DMFMODULE(queue);
    DMF_CONFIG_OsrFx2* moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

    if (moduleConfig->EventWriteCallback != NULL)
    {
        moduleConfig->EventWriteCallback(*dmfModuleAddress,
                                         OsrFx2_EventWriteMessage_ReadStop,
                                         NULL,
                                         (ULONG_PTR)Request,
                                         ntStatus,
                                         usbCompletionParams->UsbdStatus,
                                         bytesRead);
    }

    WdfRequestCompleteWithInformation(Request, 
                                      ntStatus, 
                                      bytesRead);

    FuncExitVoid(DMF_TRACE);
}

static
VOID
OsrFx2_EvtIoRead(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    )
/*++

Routine Description:

    Called by WDF when it receives Read requests.

Arguments:

    Queue - Read/Write Queue handle. Context contains DMFMODULE.
    Request - Handle to the read/write request.
    Length - Length of the data buffer associated with the request.
             The default property of the queue is to not dispatch
             zero length read requests to the driver and
             complete is with ntStatus success. So we will never get
             a zero length request.

Returns:

    None

--*/
{
    WDFUSBPIPE pipe;
    NTSTATUS ntStatus;
    WDFMEMORY requestMemory;
    DMFMODULE* dmfModuleAddress;
    DMF_CONTEXT_OsrFx2* moduleContext;
    DMF_CONFIG_OsrFx2* moduleConfig;

    UNREFERENCED_PARAMETER(Queue);

    FuncEntry(DMF_TRACE);

    // The Queue's Module context area has the DMF Module.
    //
    dmfModuleAddress = WdfObjectGet_DMFMODULE(Queue);
    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

    if (moduleConfig->EventWriteCallback != NULL)
    {
        moduleConfig->EventWriteCallback(*dmfModuleAddress,
                                         OsrFx2_EventWriteMessage_ReadStart,
                                         NULL,
                                         (ULONG_PTR)Request,
                                         Length,
                                         NULL,
                                         NULL);
    }

    // First validate input parameters.
    //
    if (Length > TEST_BOARD_TRANSFER_BUFFER_SIZE) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Transfer exceeds %d", TEST_BOARD_TRANSFER_BUFFER_SIZE);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    pipe = moduleContext->BulkReadPipe;

    ntStatus = WdfRequestRetrieveOutputMemory(Request, 
                                              &requestMemory);
    if(!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestRetrieveOutputMemory fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // The format call validates to make sure that you are reading or
    // writing to the right pipe type, sets the appropriate transfer flags,
    // creates an URB and initializes the request.
    //
    ntStatus = WdfUsbTargetPipeFormatRequestForRead(pipe,
                                                    Request,
                                                    requestMemory,
                                                    NULL);
    if (!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfUsbTargetPipeFormatRequestForRead fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WdfRequestSetCompletionRoutine(Request,
                                   OsrFx2_EvtRequestReadCompletionRoutine,
                                   pipe);

    // Send the request asynchronously.
    //
    WDFIOTARGET ioTarget = WdfUsbTargetPipeGetIoTarget(pipe);
    if (WdfRequestSend(Request, 
                       ioTarget, 
                       WDF_NO_SEND_OPTIONS) == FALSE) 
    {
        //
        // Framework couldn't send the request for some reason.
        //
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestSend fails");
        ntStatus = WdfRequestGetStatus(Request);
        goto Exit;
    }

Exit:

    if (!NT_SUCCESS(ntStatus))
    {
        if (moduleConfig->EventWriteCallback != NULL)
        {
            moduleConfig->EventWriteCallback(*dmfModuleAddress,
                                             OsrFx2_EventWriteMessage_ReadFail,
                                             NULL,
                                             (ULONG_PTR)Request,
                                             ntStatus,
                                             NULL,
                                             NULL);
        }

        WdfRequestCompleteWithInformation(Request, 
                                          ntStatus, 
                                          0);
    }

    FuncExitVoid(DMF_TRACE);
}

static
VOID
OsrFx2_EvtRequestWriteCompletionRoutine(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ WDF_REQUEST_COMPLETION_PARAMS* CompletionParams,
    _In_ WDFCONTEXT Context
    )
/*++

Routine Description:

    This is the completion routine for writes. Completes the given write request.

Arguments:

    Request - The given write request handle.
    Target - The WDFIOTARGET to which the Request was sent.
    CompletionParams - Request completion params.
    Context - Driver supplied context. It is the corresponding DMFHANDLE.

Returns:

    None

--*/
{
    NTSTATUS ntStatus;
    size_t bytesWritten;
    WDF_USB_REQUEST_COMPLETION_PARAMS* usbCompletionParams;

    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Context);

    FuncEntry(DMF_TRACE);

    ntStatus = CompletionParams->IoStatus.Status;

    // For usb devices, we should look at the Usb.Completion param.
    //
    usbCompletionParams = CompletionParams->Parameters.Usb.Completion;

    bytesWritten =  usbCompletionParams->Parameters.PipeWrite.Length;

    if (NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Number of bytes written: %I64d", (INT64)bytesWritten);
    } 
    else 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Write fails: ntStatus=%!STATUS! UsbdStatus 0x%x",
                    ntStatus, 
                    usbCompletionParams->UsbdStatus);
    }

    WDFQUEUE queue = WdfRequestGetIoQueue(Request);
    DMFMODULE* dmfModuleAddress = WdfObjectGet_DMFMODULE(queue);
    DMF_CONFIG_OsrFx2* moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

    if (moduleConfig->EventWriteCallback != NULL)
    {
        moduleConfig->EventWriteCallback(*dmfModuleAddress,
                                         OsrFx2_EventWriteMessage_WriteStop,
                                         NULL,
                                         (ULONG_PTR)Request,
                                         ntStatus,
                                         usbCompletionParams->UsbdStatus,
                                         bytesWritten);
    }

    WdfRequestCompleteWithInformation(Request, 
                                      ntStatus, 
                                      bytesWritten);

    FuncExitVoid(DMF_TRACE);
}

static
VOID 
OsrFx2_EvtIoWrite(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    ) 
/*++

    Called by WDF when it receives Write requests.

Arguments:

    Queue - Read/Write Queue handle. Context contains DMFMODULE.
    Request - Handle to the write request.
    Length - Length of the data buffer associated with the request.
             The default property of the queue is to not dispatch
             zero length write requests to the driver and
             complete is with ntStatus success. So we will never get
             a zero length request.

Returns:

    None

--*/
{
    NTSTATUS ntStatus;
    WDFUSBPIPE pipe;
    WDFMEMORY requestMemory;
    DMFMODULE* dmfModuleAddress;
    DMF_CONTEXT_OsrFx2* moduleContext;
    DMF_CONFIG_OsrFx2* moduleConfig;

    UNREFERENCED_PARAMETER(Queue);

    FuncEntry(DMF_TRACE);

    // The Queue's Module context area has the DMF Module.
    //
    dmfModuleAddress = WdfObjectGet_DMFMODULE(Queue);
    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

    if (moduleConfig->EventWriteCallback != NULL)
    {
        moduleConfig->EventWriteCallback(*dmfModuleAddress,
                                            OsrFx2_EventWriteMessage_WriteStart,
                                            NULL,
                                            (ULONG_PTR)Request,
                                            Length,
                                            NULL,
                                            NULL);
    }

    // First validate input parameters.
    //
    if (Length > TEST_BOARD_TRANSFER_BUFFER_SIZE) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Transfer exceeds %d", TEST_BOARD_TRANSFER_BUFFER_SIZE);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    pipe = moduleContext->BulkWritePipe;

    ntStatus = WdfRequestRetrieveInputMemory(Request, 
                                             &requestMemory);
    if(!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestRetrieveInputBuffer failed");
        goto Exit;
    }

    ntStatus = WdfUsbTargetPipeFormatRequestForWrite(pipe,
                                                     Request,
                                                     requestMemory,
                                                     NULL);
    if (!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfUsbTargetPipeFormatRequestForWrite fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WdfRequestSetCompletionRoutine(Request,
                                   OsrFx2_EvtRequestWriteCompletionRoutine,
                                   pipe);

    // Send the request asynchronously.
    //
    if (WdfRequestSend(Request, WdfUsbTargetPipeGetIoTarget(pipe), WDF_NO_SEND_OPTIONS) == FALSE)
    {
        // Framework couldn't send the request for some reason.
        //
        ntStatus = WdfRequestGetStatus(Request);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestSend fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    if (!NT_SUCCESS(ntStatus)) 
    {
        if (moduleConfig->EventWriteCallback != NULL)
        {
            moduleConfig->EventWriteCallback(*dmfModuleAddress,
                                             OsrFx2_EventWriteMessage_WriteFail,
                                             NULL,
                                             (ULONG_PTR)Request,
                                             ntStatus,
                                             NULL,
                                             NULL);
        }

        WdfRequestCompleteWithInformation(Request, 
                                          ntStatus, 
                                          0);
    }

    FuncExitVoid(DMF_TRACE);
}

static
VOID
OsrFx2_EvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
    )
/*++

Routine Description:

    This callback is invoked on every in-flight request when the device
    is suspended or removed. Since our in-flight read and write requests
    are actually pending in the target device, we will just acknowledge
    its presence. Until we acknowledge, complete, or requeue the requests
    framework will wait before allowing the device suspend or remove to
    proceed. When the underlying USB stack gets the request to suspend or
    remove, it will fail all the pending requests.

Arguments:

    Queue - Handle to queue object that is associated with the I/O request
    Request - Handle to a request object
    ActionFlags - Bitwise OR of one or more WDF_REQUEST_STOP_ACTION_FLAGS flags.

Returns:

    None

--*/
{
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(ActionFlags);

    FuncEntry(DMF_TRACE);

    if (ActionFlags &  WdfRequestStopActionSuspend ) 
    {
        WdfRequestStopAcknowledge(Request, 
                                  FALSE);
    } 
    else if (ActionFlags & WdfRequestStopActionPurge) 
    {
        WdfRequestCancelSentRequest(Request);
    }

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
OsrFx2_SetPowerPolicy(
    _In_ WDFDEVICE Device
    )
{
    WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS idleSettings;
    WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS wakeSettings;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // Initialize the idle policy structure. Wait 10 seconds.
    //
    WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings, 
                                               IdleUsbSelectiveSuspend);
    idleSettings.IdleTimeout = 10000;

    ntStatus = WdfDeviceAssignS0IdleSettings(Device, 
                                             &idleSettings);
    if ( !NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceSetPowerPolicyS0IdlePolicy fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Initialize wait-wake policy structure.
    //
    WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS_INIT(&wakeSettings);

    ntStatus = WdfDeviceAssignSxWakeSettings(Device, 
                                             &wakeSettings);
    if (!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceAssignSxWakeSettings fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
OsrFx2_SelectInterfaces(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This helper routine selects the configuration, interface and creates a context for every pipe 
    (end point) in that interface.

Arguments:

    DmfModule - This Module's DMF handle.

Returns:

    NTSTATUS

--*/
{
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS configParams;
    NTSTATUS ntStatus;
    DMF_CONTEXT_OsrFx2* moduleContext;
    WDFUSBPIPE pipe;
    WDF_USB_PIPE_INFORMATION pipeInformation;
    UCHAR pipeIndex;
    UCHAR numberConfiguredPipes;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    device = DMF_ParentDeviceGet(DmfModule);

    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&configParams);

    ntStatus = WdfUsbTargetDeviceSelectConfig(moduleContext->UsbDevice,
                                              WDF_NO_OBJECT_ATTRIBUTES,
                                              &configParams);
    if(!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfUsbTargetDeviceSelectConfig fails ntStatus=%!STATUS!", ntStatus);

        // Since the Osr USB fx2 device is capable of working at high speed, the only reason
        // the device would not be working at high speed is if the port doesn't
        // support it. If the port doesn't support high speed it is a 1.1 port.
        //
        if ((moduleContext->UsbDeviceTraits & WDF_USB_DEVICE_TRAIT_AT_HIGH_SPEED) == 0) 
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                        " On a 1.1 USB port on Windows Vista"
                        " this is expected as the OSR USB Fx2 board's Interrupt EndPoint descriptor"
                        " doesn't conform to the USB specification. Windows Vista detects this and"
                        " returns an error.");
        }

        DMF_CONFIG_OsrFx2* moduleConfig = DMF_CONFIG_GET(DmfModule);

        if (moduleConfig->EventWriteCallback != NULL)
        {
            moduleConfig->EventWriteCallback(DmfModule,
                                             OsrFx2_EventWriteMessage_SelectConfigFailure,
                                             NULL,
                                             NULL,
                                             ntStatus,
                                             NULL,
                                             NULL);
        }

        goto Exit;
    }

    moduleContext->UsbInterface = configParams.Types.SingleInterface.ConfiguredUsbInterface;

    numberConfiguredPipes = configParams.Types.SingleInterface.NumberConfiguredPipes;

    // Get pipe handles
    //
    for (pipeIndex = 0; pipeIndex < numberConfiguredPipes; pipeIndex++) 
    {
        WDF_USB_PIPE_INFORMATION_INIT(&pipeInformation);

        pipe = WdfUsbInterfaceGetConfiguredPipe(moduleContext->UsbInterface,
                                                pipeIndex,
                                                &pipeInformation);

        // Tell the framework that it's okay to read less than MaximumPacketSize.
        //
        WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(pipe);

        if (WdfUsbPipeTypeInterrupt == pipeInformation.PipeType) 
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Interrupt Pipe is 0x%p", pipe);
            moduleContext->InterruptPipe = pipe;
        }

        if (WdfUsbPipeTypeBulk == pipeInformation.PipeType &&
            WdfUsbTargetPipeIsInEndpoint(pipe)) 
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "BulkInput Pipe is 0x%p", pipe);
            moduleContext->BulkReadPipe = pipe;
        }

        if (WdfUsbPipeTypeBulk == pipeInformation.PipeType &&
            WdfUsbTargetPipeIsOutEndpoint(pipe)) 
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "BulkOutput Pipe is 0x%p", pipe);
            moduleContext->BulkWritePipe = pipe;
        }

        // Allow this Module to access DMFMODULE given a WDFUSBPIPE.
        //
        DMFMODULE* dmfModuleAddress;
        WDF_OBJECT_ATTRIBUTES objectAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes,
                                                DMFMODULE);
        ntStatus = WdfObjectAllocateContext(pipe,
                                            &objectAttributes,
                                            (VOID**)&dmfModuleAddress);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfObjectAllocateContext fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
        
        // Store the Module in the context space of the WDFUSBPIPE.
        //
        *dmfModuleAddress = DmfModule;

        // This is just a check to verify the above is correct and to show how to access the Module 
        // from the handle (as an example). This is used in the Pipe's error callback.
        //
        dmfModuleAddress = WdfObjectGet_DMFMODULE(pipe);
        ASSERT(*dmfModuleAddress == DmfModule);
    }

    // If we didn't find all the 3 pipes, fail the start.
    //
    if (! (moduleContext->BulkWritePipe && 
           moduleContext->BulkReadPipe && 
           moduleContext->InterruptPipe)) 
    {
        ntStatus = STATUS_INVALID_DEVICE_STATE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Device is not configured properly: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
OsrFx2_GetBarGraphState(
    _In_ DMFMODULE DmfModule,
    _Out_ BAR_GRAPH_STATE* BarGraphState
    )
/*++

Routine Description

    This routine gets the state of the bar graph on the board.

Arguments:

    DmfModule - This Module's handle.
    BarGraphState - Structure that receives the bar graph's state.

Returns:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_USB_CONTROL_SETUP_PACKET controlSetupPacket;
    WDF_REQUEST_SEND_OPTIONS sendOptions;
    WDF_MEMORY_DESCRIPTOR memoryDescriptor;
    ULONG bytesTransferred;
    DMF_CONTEXT_OsrFx2* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions,
                                  WDF_REQUEST_SEND_OPTION_TIMEOUT);

    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions,
                                         DEFAULT_CONTROL_TRANSFER_TIMEOUT);

    WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&controlSetupPacket,
                                             BmRequestDeviceToHost,
                                             BmRequestToDevice,
                                             USBFX2LK_READ_BARGRAPH_DISPLAY,
                                             0,
                                             0);

    // Set the buffer to 0, the board will OR in everything that is set
    //
    BarGraphState->BarsAsUChar = 0;

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memoryDescriptor,
                                      BarGraphState,
                                      sizeof(BAR_GRAPH_STATE));

    ntStatus = WdfUsbTargetDeviceSendControlTransferSynchronously(moduleContext->UsbDevice,
                                                                  WDF_NO_HANDLE,
                                                                  &sendOptions,
                                                                  &controlSetupPacket,
                                                                  &memoryDescriptor,
                                                                  &bytesTransferred);
    if(!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfUsbTargetDeviceSendControlTransferSynchronously fails: ntStatus=%!STATUS!", ntStatus);
    } else 
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "LED mask is 0x%x", BarGraphState->BarsAsUChar);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
OsrFx2_SetBarGraphState(
    _In_ DMFMODULE DmfModule,
    _In_ BAR_GRAPH_STATE* BarGraphState
    )
/*++

Routine Description

    This routine sets the state of the bar graph on the board.

Arguments:

    DmfModule - This Module's handle.
    BarGraphState - Structure that describes the bar graph's desired state.

Returns:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_USB_CONTROL_SETUP_PACKET controlSetupPacket;
    WDF_REQUEST_SEND_OPTIONS sendOptions;
    WDF_MEMORY_DESCRIPTOR memoryDescriptor;
    ULONG bytesTransferred;
    DMF_CONTEXT_OsrFx2* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions,
                                  WDF_REQUEST_SEND_OPTION_TIMEOUT);

    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions,
                                         DEFAULT_CONTROL_TRANSFER_TIMEOUT);

    WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&controlSetupPacket,
                                             BmRequestHostToDevice,
                                             BmRequestToDevice,
                                             USBFX2LK_SET_BARGRAPH_DISPLAY,
                                             0,
                                             0);

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memoryDescriptor,
                                      BarGraphState,
                                      sizeof(BAR_GRAPH_STATE));

    ntStatus = WdfUsbTargetDeviceSendControlTransferSynchronously(moduleContext->UsbDevice,
                                                                NULL, 
                                                                &sendOptions,
                                                                &controlSetupPacket,
                                                                &memoryDescriptor,
                                                                &bytesTransferred);
    if(!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfUsbTargetDeviceSendControlTransferSynchronously fails: ntStatus=%!STATUS!", ntStatus);
    } else 
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "SetBarGraphState: LED mask is 0x%x", BarGraphState->BarsAsUChar);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
OsrFx2_GetSevenSegmentState(
    _In_ DMFMODULE DmfModule,
    _Out_ PUCHAR SevenSegment
    )
/*++

Routine Description

    This routine gets the state of the 7 segment display on the board by sending a 
    synchronous control command.

    NOTE: It's not a good practice to send a synchronous request in the
          context of the user thread because if the transfer takes long
          time to complete, you end up holding the user thread.

          I'm choosing to do synchronous transfer because a) I know this one
          completes immediately b) and for demonstration.

Arguments:

    DmfModule - This Module's handle.
    SevenSegment - Receives the state of the 7 segment display.

Returns:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_USB_CONTROL_SETUP_PACKET controlSetupPacket;
    WDF_REQUEST_SEND_OPTIONS sendOptions;
    WDF_MEMORY_DESCRIPTOR memoryDescriptor;
    ULONG bytesTransferred;
    DMF_CONTEXT_OsrFx2* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions,
                                  WDF_REQUEST_SEND_OPTION_TIMEOUT);

    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions,
                                         DEFAULT_CONTROL_TRANSFER_TIMEOUT);

    WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&controlSetupPacket,
                                             BmRequestDeviceToHost,
                                             BmRequestToDevice,
                                             USBFX2LK_READ_7SEGMENT_DISPLAY,
                                             0,
                                             0);

    // Set the buffer to 0, the board will OR in everything that is set.
    //
    *SevenSegment = 0;

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memoryDescriptor,
                                      SevenSegment,
                                      sizeof(UCHAR));

    ntStatus = WdfUsbTargetDeviceSendControlTransferSynchronously(moduleContext->UsbDevice,
                                                                  NULL,
                                                                  &sendOptions,
                                                                  &controlSetupPacket,
                                                                  &memoryDescriptor,
                                                                  &bytesTransferred);
    if(!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfUsbTargetDeviceSendControlTransferSynchronously fails: ntStatus=%!STATUS!", ntStatus);
    } 
    else
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "GetSevenSegmentState: 7 Segment mask is 0x%x", *SevenSegment);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
OsrFx2_SetSevenSegmentState(
    _In_ DMFMODULE DmfModule,
    _In_ PUCHAR SevenSegment
    )
/*++

Routine Description

    This routine sets the state of the 7 segment display on the board.

Arguments:

    DmfModule - This Module's handle.
    SevenSegment - Desired state of the 7 segment display.

Returns:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_USB_CONTROL_SETUP_PACKET controlSetupPacket;
    WDF_REQUEST_SEND_OPTIONS sendOptions;
    WDF_MEMORY_DESCRIPTOR memoryDescriptor;
    ULONG bytesTransferred;
    DMF_CONTEXT_OsrFx2* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions,
                                  WDF_REQUEST_SEND_OPTION_TIMEOUT);

    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions,
                                         DEFAULT_CONTROL_TRANSFER_TIMEOUT);

    WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&controlSetupPacket,
                                             BmRequestHostToDevice,
                                             BmRequestToDevice,
                                             USBFX2LK_SET_7SEGMENT_DISPLAY,
                                             0,
                                             0);

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memoryDescriptor,
                                      SevenSegment,
                                      sizeof(UCHAR));

    ntStatus = WdfUsbTargetDeviceSendControlTransferSynchronously(moduleContext->UsbDevice,
                                                                  NULL,
                                                                  &sendOptions,
                                                                  &controlSetupPacket,
                                                                  &memoryDescriptor,
                                                                  &bytesTransferred);
    if(!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfUsbTargetDeviceSendControlTransferSynchronously fails: ntStatus=%!STATUS!", ntStatus);
    } 
    else 
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "SetSevenSegmentState: 7 Segment mask is 0x%x", *SevenSegment);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
OsrFx2_GetSwitchState(
    _In_ DMFMODULE DmfModule,
    _Out_ SWITCH_STATE* SwitchState
    )
/*++

Routine Description

    This routine gets the state of the switches on the board.

Arguments:

    DmfModule - This Module's handle.
    SwitchState - Current switch state is stored in this buffer.

Returns:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_USB_CONTROL_SETUP_PACKET controlSetupPacket;
    WDF_REQUEST_SEND_OPTIONS sendOptions;
    WDF_MEMORY_DESCRIPTOR memoryDescriptor;
    ULONG bytesTransferred;
    DMF_CONTEXT_OsrFx2* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions,
                                  WDF_REQUEST_SEND_OPTION_TIMEOUT);

    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions,
                                         DEFAULT_CONTROL_TRANSFER_TIMEOUT);

    WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&controlSetupPacket,
                                             BmRequestDeviceToHost,
                                             BmRequestToDevice,
                                             USBFX2LK_READ_SWITCHES,
                                             0,
                                             0);

    SwitchState->SwitchesAsUChar = 0;

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memoryDescriptor,
                                      SwitchState,
                                      sizeof(SWITCH_STATE));

    ntStatus = WdfUsbTargetDeviceSendControlTransferSynchronously(moduleContext->UsbDevice,
                                                                  NULL,
                                                                  &sendOptions,
                                                                  &controlSetupPacket,
                                                                  &memoryDescriptor,
                                                                  &bytesTransferred);
    if(!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfUsbTargetDeviceSendControlTransferSynchronously fails: ntStatus=%!STATUS!", ntStatus);
    } 
    else 
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "GetSwitchState: Switch mask is 0x%x", SwitchState->SwitchesAsUChar);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
OsrFx2_StopAllPipes(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description

    Stops all the USB pipes from transferring data.

Arguments:

    DmfModule - This Module's handle.

Returns:

    None

--*/
{
    DMF_CONTEXT_OsrFx2* moduleContext;
    WDFIOTARGET ioTarget;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ioTarget = WdfUsbTargetPipeGetIoTarget(moduleContext->InterruptPipe);
    WdfIoTargetStop(ioTarget,
                    WdfIoTargetCancelSentIo);
    
    ioTarget = WdfUsbTargetPipeGetIoTarget(moduleContext->BulkReadPipe);
    WdfIoTargetStop(ioTarget,
                    WdfIoTargetCancelSentIo);
    
    ioTarget = WdfUsbTargetPipeGetIoTarget(moduleContext->BulkWritePipe);
    WdfIoTargetStop(ioTarget,
                    WdfIoTargetCancelSentIo);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
OsrFx2_StartAllPipes(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description

    Stops all the USB pipes transfering data.

Arguments:

    DmfModule - This Module's handle.

Returns:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_OsrFx2* moduleContext;
    WDFIOTARGET ioTarget;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ioTarget = WdfUsbTargetPipeGetIoTarget(moduleContext->InterruptPipe);
    ntStatus = WdfIoTargetStart(ioTarget);
    if (!NT_SUCCESS(ntStatus)) 
    {
        goto Exit;
    }

    ioTarget = WdfUsbTargetPipeGetIoTarget(moduleContext->BulkReadPipe);
    ntStatus = WdfIoTargetStart(ioTarget);
    if (!NT_SUCCESS(ntStatus)) 
    {
        goto Exit;
    }

    ioTarget = WdfUsbTargetPipeGetIoTarget(moduleContext->BulkWritePipe);
    ntStatus = WdfIoTargetStart(ioTarget);
    if (!NT_SUCCESS(ntStatus)) 
    {
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
OsrFx2_ResetDevice(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This routine calls WdfUsbTargetDeviceResetPortSynchronously to reset the device if it's still
    connected.

Arguments:

    DmfModule - This Module's handle.

Returns:

    NTSTATUS

--*/
{
    DMF_CONTEXT_OsrFx2* moduleContext;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    OsrFx2_StopAllPipes(DmfModule);

    ntStatus = WdfUsbTargetDeviceResetPortSynchronously(moduleContext->UsbDevice);
    if (!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfUsbTargetDeviceResetPortSynchronously fails: ntStatus=%!STATUS!", ntStatus);
    }

    ntStatus = OsrFx2_StartAllPipes(DmfModule);
    if (!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "OsrFx2_StartAllPipes fails: ntStatus=%!STATUS!", ntStatus);
    }

    DMF_ModuleUnlock(DmfModule);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
OsrFx2_ReenumerateDevice(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description

    This routine re-enumerates the USB device.

Arguments:

    DmfModule - This Module's handle.

Returns:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_USB_CONTROL_SETUP_PACKET controlSetupPacket;
    WDF_REQUEST_SEND_OPTIONS sendOptions;
    DMF_CONTEXT_OsrFx2* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions,
                                  WDF_REQUEST_SEND_OPTION_TIMEOUT);

    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions,
                                         DEFAULT_CONTROL_TRANSFER_TIMEOUT);

    WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&controlSetupPacket,
                                        BmRequestHostToDevice,
                                        BmRequestToDevice,
                                        USBFX2LK_REENUMERATE,
                                        0,
                                        0);

    ntStatus = WdfUsbTargetDeviceSendControlTransferSynchronously(moduleContext->UsbDevice,
                                                                  WDF_NO_HANDLE,
                                                                  &sendOptions,
                                                                  &controlSetupPacket,
                                                                  NULL,
                                                                  NULL);
    if(!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfUsbTargetDeviceSendControlTransferSynchronously fails: ntStatus=%!STATUS!", ntStatus);
    }

    DMF_CONFIG_OsrFx2* moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (moduleConfig->EventWriteCallback != NULL)
    {
        moduleConfig->EventWriteCallback(DmfModule,
                                         OsrFx2_EventWriteMessage_DeviceReenumerated,
                                         NULL,
                                         NULL,
                                         ntStatus,
                                         NULL,
                                         NULL);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
OsrFx2_EvtUsbInterruptPipeReadComplete(
    _In_ WDFUSBPIPE Pipe,
    _In_ WDFMEMORY Buffer,
    _In_ size_t NumberOfBytesTransferred,
    _In_ WDFCONTEXT Context
    )
/*++

Routine Description:

    This the completion routine of the continuous reader. This can
    called concurrently on multiprocessor system if there are
    more than one readers configured. So make sure to protect
    access to global resources.

Arguments:

    Buffer - This buffer is freed when this call returns.
             If the driver wants to delay processing of the buffer, it
             can take an additional reference.

    Context - Provided in the WDF_USB_CONTINUOUS_READER_CONFIG_INIT macro

Returns:

    NTSTATUS

--*/
{
    UCHAR* switchState;
    WDFDEVICE device;
    DMFMODULE dmfModule;
    DMF_CONTEXT_OsrFx2* moduleContext;
    DMF_CONFIG_OsrFx2* moduleConfig;

    UNREFERENCED_PARAMETER(Pipe);

    FuncEntry(DMF_TRACE);

    dmfModule = (DMFMODULE)Context;
    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);
    device = DMF_ParentDeviceGet(dmfModule);

    // Switches have changed so user is using it. Reset the idle timer.
    //
    WdfDeviceStopIdle(device,
                      FALSE);

    // Make sure that there is data in the read packet.  Depending on the device
    // specification, it is possible for it to return a 0 length read in
    // certain conditions.
    //

    if (NumberOfBytesTransferred == 0) 
    {
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "OsrFx2_EvtUsbInterruptPipeReadComplete Zero length read occurred on the Interrupt Pipe's Continuous Reader");
        goto Exit;
    }

    ASSERT(NumberOfBytesTransferred == sizeof(UCHAR));

    switchState = (UCHAR*)WdfMemoryGetBuffer(Buffer,
                                             NULL);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "OsrFx2_EvtUsbInterruptPipeReadComplete SwitchState=0x%x", *switchState);

    // Save of the current state.
    //
    moduleContext->CurrentSwitchState = *switchState;

    // Allow the Client to perform a Client specific action given the updated
    // switch data.
    //
    if (moduleConfig->InterruptPipeCallback != NULL)
    {
        // This call happens at DISPATCH_LEVEL.
        //
        moduleConfig->InterruptPipeCallback(dmfModule,
                                            *switchState,
                                            STATUS_SUCCESS);
    }
    if (moduleConfig->InterruptPipeCallbackPassive != NULL)
    {
        // This call happens at PASSIVE_LEVEL.
        //
        DMF_QueuedWorkItem_Enqueue(moduleContext->DmfModuleQueuedWorkitem,
                                   switchState,
                                   sizeof(UCHAR));
    }

Exit:

    // Allow device to sleep again.
    //
    WdfDeviceResumeIdle(device);

    FuncExitVoid(DMF_TRACE);
}

_Must_inspect_result_
static
BOOLEAN
OsrFx2_EvtUsbInterruptReadersFailed(
    _In_ WDFUSBPIPE Pipe,
    _In_ NTSTATUS NtStatus,
    _In_ USBD_STATUS UsbdStatus
    )
{
    DMFMODULE* dmfModuleAddress;
    DMF_CONTEXT_OsrFx2* moduleContext;
    DMF_CONFIG_OsrFx2* moduleConfig;
    DMFMODULE dmfModuleOsrFx2;

    UNREFERENCED_PARAMETER(UsbdStatus);

    FuncEntry(DMF_TRACE);

    // Access the DMFMODULE from the additional context.
    //
    dmfModuleAddress = WdfObjectGet_DMFMODULE(Pipe);
    dmfModuleOsrFx2 = *dmfModuleAddress;

    moduleContext = DMF_CONTEXT_GET(dmfModuleOsrFx2);
    moduleConfig = DMF_CONFIG_GET(dmfModuleOsrFx2);

    // Clear the current switch state.
    //
    moduleContext->CurrentSwitchState = 0;

    // Service the pending interrupt switch change request.
    //
    if (moduleConfig->InterruptPipeCallback != NULL)
    {
        // This call happens at DISPATCH_LEVEL.
        //
        moduleConfig->InterruptPipeCallback(dmfModuleOsrFx2,
                                            moduleContext->CurrentSwitchState,
                                            NtStatus);
    }
    if (moduleConfig->InterruptPipeCallbackPassive != NULL)
    {
        // This call happens at PASSIVE_LEVEL.
        //
        DMF_QueuedWorkItem_Enqueue(moduleContext->DmfModuleQueuedWorkitem,
                                   &moduleContext->CurrentSwitchState,
                                   sizeof(UCHAR));
    }

    FuncExit(DMF_TRACE, "returnValue=%d", TRUE);

    return TRUE;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
OsrFx2_ConfigureContinuousReaderForInterruptEndpoint(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This routine configures a continuous reader on the
    interrupt endpoint. It's called from the PrepareHarware event.

Arguments:

    DmfModule - This Module's handle.

Returns:

    NTSTATUS

--*/
{
    WDF_USB_CONTINUOUS_READER_CONFIG continuousReaderConfig;
    NTSTATUS ntStatus;
    DMF_CONTEXT_OsrFx2* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WDF_USB_CONTINUOUS_READER_CONFIG_INIT(&continuousReaderConfig,
                                          OsrFx2_EvtUsbInterruptPipeReadComplete,
                                          DmfModule,
                                          sizeof(UCHAR));
    continuousReaderConfig.EvtUsbTargetPipeReadersFailed = OsrFx2_EvtUsbInterruptReadersFailed;

    // Reader requests are not posted to the target automatically.
    // Driver must explicitly call WdfIoTargetStart to kick start the
    // reader.  In this sample, it's done in D0Entry.
    // By default, framework queues two requests to the target
    // endpoint. Driver can configure up to 10 requests with CONFIG macro.
    //
    ntStatus = WdfUsbTargetPipeConfigContinuousReader(moduleContext->InterruptPipe,
                                                      &continuousReaderConfig);
    if (!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfUsbTargetPipeConfigContinuousReader fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
NTSTATUS
OsrFx2_IoctlHandler(
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

    This callback is called when the Child Module (Dmf_IoctlHandler) has validated the IOCTL and the
    input/output buffer sizes per the table supplied.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    Queue - The WDFQUEUE associated with Request.
    Request - Request data, not used.
    IoctlCode - IOCTL that has been validated to be supported by this Module.
    InputBuffer - Input data buffer.
    InputBufferSize - Input data buffer size.
    OutputBuffer - Output data buffer.
    OutputBufferSize - Output data buffer size.
    BytesReturned - Amount of data to be sent back.

Returns:

    STATUS_PENDING - This Module owns the given Request. It will not be completed by the Child Module. This
                     Module must complete the request eventually.
    Any other NTSTATUS - The given request will be completed with this NTSTATUS.

--*/
{
    DMFMODULE dmfModuleOsrFx2;
    DMF_CONTEXT_OsrFx2* moduleContext;
    size_t bytesReturned;
    BAR_GRAPH_STATE* barGraphState;
    SWITCH_STATE* switchState;
    UCHAR* sevenSegment;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(InputBufferSize);
    UNREFERENCED_PARAMETER(OutputBufferSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    //
    // DMF: A frequent programming DMF programming pattern is that callbacks made by DMF Modules
    //      pass the corresponding DMFMODULE handle. From that handle, it is possible to get the
    //      Client driver's WDFDEVICE and device context.
    //
    dmfModuleOsrFx2 = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleOsrFx2);

    bytesReturned = 0;

    // NOTE: This is only to fix SAL as this case is already handled by caller.
    //
    ntStatus = STATUS_NOT_SUPPORTED;

    switch(IoControlCode) 
    {
        case IOCTL_OSRUSBFX2_GET_CONFIG_DESCRIPTOR: 
        {
            // In this case, the buffer sizes were not validated because they change depending on the call.
            // So, validate them here.
            //

            USB_CONFIGURATION_DESCRIPTOR* configurationDescriptor;
            USHORT requiredSize = 0;

            // First, get the size of the config descriptor.
            //
            ntStatus = WdfUsbTargetDeviceRetrieveConfigDescriptor(moduleContext->UsbDevice,
                                                                  NULL,
                                                                  &requiredSize);
            if (ntStatus != STATUS_BUFFER_TOO_SMALL) 
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfUsbTargetDeviceRetrieveConfigDescriptor fails: ntStatus=%!STATUS!", ntStatus);
                break;
            }

            // Get the buffer. Make sure the buffer is big enough
            //
            ntStatus = WdfRequestRetrieveOutputBuffer(Request,
                                                      (size_t)requiredSize,
                                                      (PVOID*)&configurationDescriptor,
                                                      NULL);
            if(!NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestRetrieveOutputBuffer fails: ntStatus=%!STATUS!", ntStatus);
                break;
            }

            ntStatus = WdfUsbTargetDeviceRetrieveConfigDescriptor(moduleContext->UsbDevice,
                                                                  configurationDescriptor,
                                                                  &requiredSize);
            if (!NT_SUCCESS(ntStatus)) 
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfUsbTargetDeviceRetrieveConfigDescriptor fails: ntStatus=%!STATUS!", ntStatus);
                break;
            }

            bytesReturned = requiredSize;
            break;
        }
        case IOCTL_OSRUSBFX2_RESET_DEVICE:
        {
            ntStatus = OsrFx2_ResetDevice(dmfModuleOsrFx2);
            break;
        }
        case IOCTL_OSRUSBFX2_REENUMERATE_DEVICE:
        {
            // Otherwise, call our function to reenumerate the device
            //
            ntStatus = OsrFx2_ReenumerateDevice(dmfModuleOsrFx2);

            bytesReturned = 0;
            break;
        }
        case IOCTL_OSRUSBFX2_GET_BAR_GRAPH_DISPLAY:
        {
            barGraphState = (BAR_GRAPH_STATE*)OutputBuffer;

            // Call our function to get the bar graph state
            //
            ntStatus = OsrFx2_GetBarGraphState(dmfModuleOsrFx2, 
                                               barGraphState);

            // If we succeeded return the user their data
            //
            if (NT_SUCCESS(ntStatus)) 
            {
                bytesReturned = sizeof(BAR_GRAPH_STATE);
            } 
            else 
            {
                bytesReturned = 0;
            }
            break;
        }
        case IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY:
        {
            barGraphState = (BAR_GRAPH_STATE*)InputBuffer;

            //
            // Call our routine to set the bar graph state
            //
            ntStatus = OsrFx2_SetBarGraphState(dmfModuleOsrFx2,
                                               barGraphState);

            // There's no data returned for this call
            //
            bytesReturned = 0;
            break;
        }
        case IOCTL_OSRUSBFX2_GET_7_SEGMENT_DISPLAY:
        {
            sevenSegment = (UCHAR*)OutputBuffer;

            // Call our function to get the 7 segment state
            //
            ntStatus = OsrFx2_GetSevenSegmentState(dmfModuleOsrFx2, 
                                                   sevenSegment);

            // If we succeeded return the user their data
            //
            if (NT_SUCCESS(ntStatus)) 
            {
                bytesReturned = sizeof(UCHAR);
            } 
            else 
            {
                bytesReturned = 0;
            }
            break;
        }
        case IOCTL_OSRUSBFX2_SET_7_SEGMENT_DISPLAY:
        {
            sevenSegment = (UCHAR*)InputBuffer;

            // Call our routine to set the 7 segment state
            //
            ntStatus = OsrFx2_SetSevenSegmentState(dmfModuleOsrFx2, 
                                                   sevenSegment);

            // There's no data returned for this call
            //
            bytesReturned = 0;
            break;
        }
        case IOCTL_OSRUSBFX2_READ_SWITCHES:
        {
            switchState = (SWITCH_STATE*)OutputBuffer;

            // Call our routine to get the state of the switches
            //
            ntStatus = OsrFx2_GetSwitchState(dmfModuleOsrFx2, 
                                             switchState);

            // If successful, return the user their data
            //
            if (NT_SUCCESS(ntStatus)) 
            {
                bytesReturned = sizeof(SWITCH_STATE);
            } 
            else 
            {
                // Don't return any data
                //
                bytesReturned = 0;
            }
            break;
        }
    }

    // DMF: Dmf_IoctlHandler will return this information with the request if it completes it.
    //      Local variable is not necessary, of course. It is left here to reduce changes.
    //
    *BytesReturned = bytesReturned;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Wdf Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_OsrFx2_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Called when board is powering up.

Arguments:

    DmfModule - This Module's handle.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Returns:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_OsrFx2* moduleContext;
    BOOLEAN isTargetStarted;

    UNREFERENCED_PARAMETER(PreviousState);

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    isTargetStarted = FALSE;

    WDFIOTARGET pipeIoTarget = WdfUsbTargetPipeGetIoTarget(moduleContext->InterruptPipe);

    // Since continuous reader is configured for this interrupt-pipe, we must explicitly start
    // the I/O target to get the framework to post read requests.
    //
    ntStatus = WdfIoTargetStart(pipeIoTarget);
    if (!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetStart fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    isTargetStarted = TRUE;

Exit:

    if (!NT_SUCCESS(ntStatus)) 
    {
        // Failure in D0Entry will lead to device being removed. So let us stop the continuous
        // reader in preparation for the ensuing remove.
        //
        if (isTargetStarted) 
        {
            WdfIoTargetStop(pipeIoTarget, 
                            WdfIoTargetCancelSentIo);
        }
    }
    else
    {
        DMF_CONFIG_OsrFx2* moduleConfig = DMF_CONFIG_GET(DmfModule);

        if (moduleConfig->Settings & OsrFx2_Settings_IdleIndication)
        {
            BAR_GRAPH_STATE barGraphState;
        
            barGraphState.BarsAsUChar = 0xFF;
            OsrFx2_SetBarGraphState(DmfModule,
                                    &barGraphState);
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_OsrFx2_ModuleD0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Called when board is powering down.

Arguments:

    DmfModule - This Module's handle.
    TargetState - The WDF Power State that the given DMF Module will enter.

Returns:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_OsrFx2* moduleContext;

    UNREFERENCED_PARAMETER(TargetState);

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_CONFIG_OsrFx2* moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (moduleConfig->Settings & OsrFx2_Settings_IdleIndication)
    {
        BAR_GRAPH_STATE barGraphState;
        
        barGraphState.BarsAsUChar = 0x00;
        OsrFx2_SetBarGraphState(DmfModule,
                                &barGraphState);
    }

    WDFIOTARGET pipeIoTarget = WdfUsbTargetPipeGetIoTarget(moduleContext->InterruptPipe);

    WdfIoTargetStop(pipeIoTarget,
                    WdfIoTargetCancelSentIo);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
VOID
DMF_OsrFx2_SelfManagedIoFlush(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This Module's SelfManagedIoFlush callback. If Client has registered for notification, this
    callback informs the Client the FX2 device is gone.

Arguments:

    DmfModule - The given DMF Module.

Returns:

    None

--*/
{
    DMF_CONFIG_OsrFx2* moduleConfig;

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (moduleConfig->InterruptPipeCallback != NULL)
    {
        moduleConfig->InterruptPipeCallback(DmfModule,
                                            0,
                                            STATUS_DEVICE_REMOVED);
    }

    FuncExitVoid(DMF_TRACE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// The table of IOCTLS that this Module supports.
//
IoctlHandler_IoctlRecord OsrFx2_IoctlHandlerTable[] =
{
    { (LONG)IOCTL_OSRUSBFX2_GET_CONFIG_DESCRIPTOR,       0,                      0,                               OsrFx2_IoctlHandler, FALSE },
    { (LONG)IOCTL_OSRUSBFX2_RESET_DEVICE,                0,                      0,                               OsrFx2_IoctlHandler, FALSE },
    { (LONG)IOCTL_OSRUSBFX2_REENUMERATE_DEVICE,          0,                      0,                               OsrFx2_IoctlHandler, FALSE },
    { (LONG)IOCTL_OSRUSBFX2_GET_BAR_GRAPH_DISPLAY,       0,                      sizeof(BAR_GRAPH_STATE),         OsrFx2_IoctlHandler, FALSE },
    { (LONG)IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY,       sizeof(BAR_GRAPH_STATE),0,                               OsrFx2_IoctlHandler, FALSE },
    { (LONG)IOCTL_OSRUSBFX2_GET_7_SEGMENT_DISPLAY,       0,                      sizeof(UCHAR),                   OsrFx2_IoctlHandler, FALSE },
    { (LONG)IOCTL_OSRUSBFX2_SET_7_SEGMENT_DISPLAY,       sizeof(UCHAR),          0,                               OsrFx2_IoctlHandler, FALSE },
    { (LONG)IOCTL_OSRUSBFX2_READ_SWITCHES,               0,                      sizeof(SWITCH_STATE),            OsrFx2_IoctlHandler, FALSE },
};

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_OsrFx2_ChildModulesAdd(
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
    DMF_CONFIG_OsrFx2* moduleConfig;
    DMF_CONTEXT_OsrFx2* moduleContext;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Client has option of allowing Device Interface to be created to allow drivers or applications to 
    // send IOCTLS.
    //
    if (!(moduleConfig->Settings & OsrFx2_Settings_NoDeviceInterface))
    {
        // IoctlHandler
        // ------------
        //
        DMF_CONFIG_IoctlHandler moduleConfigIoctlHandler;
        DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(&moduleConfigIoctlHandler,
                                                    &moduleAttributes);
        moduleConfigIoctlHandler.DeviceInterfaceGuid = GUID_DEVINTERFACE_OSRUSBFX2;
        moduleConfigIoctlHandler.IoctlRecordCount = _countof(OsrFx2_IoctlHandlerTable);
        moduleConfigIoctlHandler.IoctlRecords = OsrFx2_IoctlHandlerTable;
        moduleConfigIoctlHandler.AccessModeFilter = IoctlHandler_AccessModeDefault;
        moduleConfigIoctlHandler.CustomCapabilities = L"microsoft.hsaTestCustomCapability_q536wpkpf5cy2\0";
        moduleConfigIoctlHandler.IsRestricted = DEVPROP_TRUE;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleIoctlHandler);
    }

    // Dmf_QueuedWorkitem
    // ------------------
    // (This Module is used to allow this Module to callback to the Client at PASSIVE_LEVEL from the
    // interrupt pipe completion routine. Original sample does not have this feature.)
    //
    DMF_CONFIG_QueuedWorkItem moduleConfigQueuedWorkitem;
    DMF_CONFIG_QueuedWorkItem_AND_ATTRIBUTES_INIT(&moduleConfigQueuedWorkitem,
                                                  &moduleAttributes);
    moduleConfigQueuedWorkitem.BufferQueueConfig.SourceSettings.BufferCount = 4;
    moduleConfigQueuedWorkitem.BufferQueueConfig.SourceSettings.BufferSize = sizeof(SWITCH_STATE);
    moduleConfigQueuedWorkitem.BufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
    moduleConfigQueuedWorkitem.EvtQueuedWorkitemFunction = OsrFx2_QueuedWorkitemFunction;
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        &moduleContext->DmfModuleQueuedWorkitem);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_OsrFx2_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type OsrFx2.

Arguments:

    DmfModule - This Module's handle.

Returns:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_OsrFx2* moduleContext;
    DMF_CONFIG_OsrFx2* moduleConfig;
    WDF_USB_DEVICE_INFORMATION deviceInfo;
    ULONG waitWakeEnable;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    waitWakeEnable = FALSE;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    device = DMF_ParentDeviceGet(DmfModule);

    // Create a USB device handle so that we can communicate with the
    // underlying USB stack. The WDFUSBDEVICE handle is used to query,
    // configure, and manage all aspects of the USB device.
    // These aspects include device properties, bus properties,
    // and I/O creation and synchronization. We only create device the first
    // the PrepareHardware is called. If the device is restarted by pnp manager
    // for resource rebalance, we will use the same device handle but then select
    // the interfaces again because the USB stack could reconfigure the device on
    // restart.
    //
    if (moduleContext->UsbDevice == NULL) 
    {
        WDF_USB_DEVICE_CREATE_CONFIG config;

        WDF_USB_DEVICE_CREATE_CONFIG_INIT(&config,
                                          USBD_CLIENT_CONTRACT_VERSION_602);

        ntStatus = WdfUsbTargetDeviceCreateWithParameters(device,
                                                          &config,
                                                          WDF_NO_OBJECT_ATTRIBUTES,
                                                          &moduleContext->UsbDevice);
        if (!NT_SUCCESS(ntStatus)) 
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfUsbTargetDeviceCreateWithParameters fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // TODO: If you are fetching configuration descriptor from device for
        // selecting a configuration or to parse other descriptors, call OsrFxValidateConfigurationDescriptor
        // to do basic validation on the descriptors before you access them .
        //
    }

    // Retrieve USBD version information, port driver capabilities and device
    // capabilities such as speed, power, etc.
    //
    WDF_USB_DEVICE_INFORMATION_INIT(&deviceInfo);

    ntStatus = WdfUsbTargetDeviceRetrieveInformation(moduleContext->UsbDevice,
                                                     &deviceInfo);
    if (NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "IsDeviceHighSpeed: %s", (deviceInfo.Traits & WDF_USB_DEVICE_TRAIT_AT_HIGH_SPEED) ? "TRUE" : "FALSE");
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "IsDeviceSelfPowered: %s", (deviceInfo.Traits & WDF_USB_DEVICE_TRAIT_SELF_POWERED) ? "TRUE" : "FALSE");

        waitWakeEnable = deviceInfo.Traits & WDF_USB_DEVICE_TRAIT_REMOTE_WAKE_CAPABLE;

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "IsDeviceRemoteWakeable: %s", waitWakeEnable ? "TRUE" : "FALSE");

        // Save these for use later.
        //
        moduleContext->UsbDeviceTraits = deviceInfo.Traits;
    }
    else 
    {
        moduleContext->UsbDeviceTraits = 0;
    }

    ntStatus = OsrFx2_SelectInterfaces(DmfModule);
    if (!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "OsrFx2_SelectInterfaces fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Enable wait-wake and idle timeout if the device supports it
    //
    if (waitWakeEnable &&
        (! (moduleConfig->Settings & OsrFx2_Settings_NoEnterIdle)) )
    {
        ntStatus = OsrFx2_SetPowerPolicy(device);
        if (!NT_SUCCESS (ntStatus)) 
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "OsrFx2_SetPowerPolicy fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    ntStatus = OsrFx2_ConfigureContinuousReaderForInterruptEndpoint(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_OsrFx2;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_OsrFx2;
static DMF_CALLBACKS_WDF DmfCallbacksWdf_OsrFx2;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_OsrFx2_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type OsrFx2.

Arguments:

    Device - Client driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Returns:

    NTSTATUS

--*/
{
    DMF_CONFIG_OsrFx2* moduleConfig;
    WDFDEVICE device;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_OsrFx2);
    DmfCallbacksDmf_OsrFx2.ChildModulesAdd = DMF_OsrFx2_ChildModulesAdd;
    DmfCallbacksDmf_OsrFx2.DeviceOpen = DMF_OsrFx2_Open;

    DMF_CALLBACKS_WDF_INIT(&DmfCallbacksWdf_OsrFx2);
    DmfCallbacksWdf_OsrFx2.ModuleD0Entry = DMF_OsrFx2_ModuleD0Entry;
    DmfCallbacksWdf_OsrFx2.ModuleD0Exit = DMF_OsrFx2_ModuleD0Exit;
    DmfCallbacksWdf_OsrFx2.ModuleSelfManagedIoFlush = DMF_OsrFx2_SelfManagedIoFlush;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_OsrFx2,
                                            OsrFx2,
                                            DMF_CONTEXT_OsrFx2,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    DmfModuleDescriptor_OsrFx2.CallbacksDmf = &DmfCallbacksDmf_OsrFx2;
    DmfModuleDescriptor_OsrFx2.CallbacksWdf = &DmfCallbacksWdf_OsrFx2;
    DmfModuleDescriptor_OsrFx2.ModuleConfigSize = sizeof(DMF_CONFIG_OsrFx2);

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_OsrFx2,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    moduleConfig = DMF_CONFIG_GET(*DmfModule);
    device = DMF_ParentDeviceGet(*DmfModule);

    // Client has option of allowing Device Interface to be created to allow drivers or applications to 
    // send IOCTLS.
    //
    if (!(moduleConfig->Settings & OsrFx2_Settings_NoDeviceInterface))
    {
        // NOTE: Currently DMF has no AddDevice() callback. Operations that are done in AddDevice() should go here as this call is 
        //       performed in AddDevice(). 
        //
        WDF_IO_QUEUE_CONFIG ioQueueConfig;
        WDF_OBJECT_ATTRIBUTES objectAttributes;
        WDFQUEUE queue;

        // We will create a separate sequential queue and configure it
        // to receive read requests.  We also need to register a EvtIoStop
        // handler so that we can acknowledge requests that are pending
        // at the target driver.
        //
        WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig, 
                                 WdfIoQueueDispatchSequential);
    
        // NOTE: It is not possible to get the parent of a WDFQUEUE.
        // Therefore, it is necessary to save the DmfModule in its context area.
        // This call allocates space for the context which needs to be large enough to
        // hold a DMFMODULE.
        //
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes,
                                                DMFMODULE);

        ioQueueConfig.EvtIoRead = OsrFx2_EvtIoRead;
        ioQueueConfig.EvtIoStop = OsrFx2_EvtIoStop;

        ntStatus = WdfIoQueueCreate(device,
                                    &ioQueueConfig,
                                    &objectAttributes,
                                    &queue);
        if (!NT_SUCCESS (ntStatus)) 
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoQueueCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // NOTE: It is not possible to get the parent of a WDFQUEUE.
        // Therefore, it is necessary to save the DmfModule in its context area.
        //
        DMF_ModuleInContextSave(queue,
                                *DmfModule);

        ntStatus = WdfDeviceConfigureRequestDispatching(device,
                                                        queue,
                                                        WdfRequestTypeRead);
        if (!NT_SUCCESS(ntStatus))
        {
            ASSERT(NT_SUCCESS(ntStatus));
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceConfigureRequestDispatching fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        //
        // We will create another sequential queue and configure it
        // to receive write requests.
        //
        WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig, 
                                 WdfIoQueueDispatchSequential);

        // NOTE: It is not possible to get the parent of a WDFQUEUE.
        // Therefore, it is necessary to save the DmfModule in its context area.
        // This call allocates space for the context which needs to be large enough to
        // hold a DMFMODULE.
        //
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes,
                                                DMFMODULE);

        ioQueueConfig.EvtIoWrite = OsrFx2_EvtIoWrite;
        ioQueueConfig.EvtIoStop  = OsrFx2_EvtIoStop;

        ntStatus = WdfIoQueueCreate(device,
                                    &ioQueueConfig,
                                    &objectAttributes,
                                    &queue);

        if (!NT_SUCCESS (ntStatus)) 
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoQueueCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // NOTE: It is not possible to get the parent of a WDFQUEUE.
        // Therefore, it is necessary to save the DmfModule in its context area.
        //
        DMF_ModuleInContextSave(queue,
                                *DmfModule);

        ntStatus = WdfDeviceConfigureRequestDispatching(device,
                                                        queue,
                                                        WdfRequestTypeWrite);
        if (!NT_SUCCESS(ntStatus))
        {
            ASSERT(NT_SUCCESS(ntStatus));
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceConfigureRequestDispatching fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_OsrFx2_SwitchStateGet(
    _In_ DMFMODULE DmfModule,
    _Out_ UCHAR* SwitchState
    )
/*++

Routine Description:

    Reads the current state of the switches.

Arguments:

    DmfModule - This Module's handle.
    SwitchState - The current switch state is returned here.

Returns:

    None

--*/
{
    DMF_CONTEXT_OsrFx2* moduleContext;

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_OsrFx2);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    *SwitchState = moduleContext->CurrentSwitchState;

    FuncExitVoid(DMF_TRACE);
}

// eof: Dmf_OsrFx2.c
//
