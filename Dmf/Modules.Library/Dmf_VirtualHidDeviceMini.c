/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_VirtualHidDeviceMini.c

Abstract:

    Provides functionality from the VHIDMINI2 sample. This allows Client to create virtual HID device
    in both Kernel-mode and User-mode.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_VirtualHidDeviceMini.tmh"

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
    WDFQUEUE DefaultQueue;
    WDFQUEUE ManualQueue;
    //HID_DEVICE_ATTRIBUTES HidDeviceAttributes;
    //HID_DESCRIPTOR HidDescriptor;
    //VirtualHidDeviceMini_HID_REPORT_DESCRIPTOR* ReportDescriptor;
} DMF_CONTEXT_VirtualHidDeviceMini;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(VirtualHidDeviceMini)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(VirtualHidDeviceMini)

// MemoryTag.
//
#define MemoryTag 'mDHV'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#if 0
typedef struct _QUEUE_CONTEXT
{
    WDFQUEUE Queue;
    DMFMODULE DmfModule;
    UCHAR OutputReport;
} QUEUE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(QUEUE_CONTEXT, QueueContextGet);
#endif

typedef struct _MANUAL_QUEUE_CONTEXT
{
    WDFQUEUE Queue;
    DMFMODULE DmfModule;
    WDFTIMER Timer;
} MANUAL_QUEUE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(MANUAL_QUEUE_CONTEXT, ManualQueueContextGet);

#if !defined(DMF_USER_MODE)

//
// First let's review Buffer Descriptions for I/O Control Codes
//
//   METHOD_BUFFERED
//    - Input buffer:  Irp->AssociatedIrp.SystemBuffer
//    - Output buffer: Irp->AssociatedIrp.SystemBuffer
//
//   METHOD_IN_DIRECT or METHOD_OUT_DIRECT
//    - Input buffer:  Irp->AssociatedIrp.SystemBuffer
//    - Second buffer: Irp->MdlAddress
//
//   METHOD_NEITHER
//    - Input buffer:  Parameters.DeviceIoControl.Type3InputBuffer;
//    - Output buffer: Irp->UserBuffer
//
// HID minidriver IOCTL stores a pointer to HID_XFER_PACKET in Irp->UserBuffer.
// For IOCTLs like IOCTL_HID_GET_FEATURE (which is METHOD_OUT_DIRECT) this is
// not the expected buffer location. So we cannot retrieve UserBuffer from the
// IRP using WdfRequestXxx functions. Instead, we have to escape to WDM.
//

NTSTATUS
RequestGetHidXferPacket_ToReadFromDevice(
    _In_  WDFREQUEST        Request,
    _Out_ HID_XFER_PACKET  *Packet
    )
{
    NTSTATUS                status;
    WDF_REQUEST_PARAMETERS  params;

    WDF_REQUEST_PARAMETERS_INIT(&params);
    WdfRequestGetParameters(Request, &params);

    if (params.Parameters.DeviceIoControl.OutputBufferLength < sizeof(HID_XFER_PACKET)) {
        status = STATUS_BUFFER_TOO_SMALL;
        KdPrint(("RequestGetHidXferPacket: invalid HID_XFER_PACKET\n"));
        return status;
    }

    RtlCopyMemory(Packet, WdfRequestWdmGetIrp(Request)->UserBuffer, sizeof(HID_XFER_PACKET));
    return STATUS_SUCCESS;
}

NTSTATUS
RequestGetHidXferPacket_ToWriteToDevice(
    _In_  WDFREQUEST        Request,
    _Out_ HID_XFER_PACKET  *Packet
    )
{
    NTSTATUS                status;
    WDF_REQUEST_PARAMETERS  params;

    WDF_REQUEST_PARAMETERS_INIT(&params);
    WdfRequestGetParameters(Request, &params);

    if (params.Parameters.DeviceIoControl.InputBufferLength < sizeof(HID_XFER_PACKET)) {
        status = STATUS_BUFFER_TOO_SMALL;
        KdPrint(("RequestGetHidXferPacket: invalid HID_XFER_PACKET\n"));
        return status;
    }

    RtlCopyMemory(Packet, WdfRequestWdmGetIrp(Request)->UserBuffer, sizeof(HID_XFER_PACKET));
    return STATUS_SUCCESS;
}

#else

//
// HID minidriver IOCTL uses HID_XFER_PACKET which contains an embedded pointer.
//
//   typedef struct _HID_XFER_PACKET {
//     PUCHAR reportBuffer;
//     ULONG  reportBufferLen;
//     UCHAR  reportId;
//   } HID_XFER_PACKET, *PHID_XFER_PACKET;
//
// UMDF cannot handle embedded pointers when marshalling buffers between processes.
// Therefore a special driver mshidumdf.sys is introduced to convert such IRPs to
// new IRPs (with new IOCTL name like IOCTL_UMDF_HID_Xxxx) where:
//
//   reportBuffer - passed as one buffer inside the IRP
//   reportId     - passed as a second buffer inside the IRP
//
// The new IRP is then passed to UMDF host and driver for further processing.
//

NTSTATUS
RequestGetHidXferPacket_ToReadFromDevice(
    _In_  WDFREQUEST        Request,
    _Out_ HID_XFER_PACKET  *Packet
    )
{
    //
    // Driver need to write to the output buffer (so that App can read from it)
    //
    //   Report Buffer: Output Buffer
    //   Report Id    : Input Buffer
    //

    NTSTATUS                status;
    WDFMEMORY               inputMemory;
    WDFMEMORY               outputMemory;
    size_t                  inputBufferLength;
    size_t                  outputBufferLength;
    PVOID                   inputBuffer;
    PVOID                   outputBuffer;

    //
    // Get report Id from input buffer
    //
    status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
    if( !NT_SUCCESS(status) ) {
        KdPrint(("WdfRequestRetrieveInputMemory failed 0x%x\n",status));
        return status;
    }
    inputBuffer = WdfMemoryGetBuffer(inputMemory, &inputBufferLength);

    if (inputBufferLength < sizeof(UCHAR)) {
        status = STATUS_INVALID_BUFFER_SIZE;
        KdPrint(("WdfRequestRetrieveInputMemory: invalid input buffer. size %d, expect %d\n",
                            (int)inputBufferLength, (int)sizeof(UCHAR)));
        return status;
    }

    Packet->reportId        = *(PUCHAR)inputBuffer;

    //
    // Get report buffer from output buffer
    //
    status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
    if( !NT_SUCCESS(status) ) {
        KdPrint(("WdfRequestRetrieveOutputMemory failed 0x%x\n",status));
        return status;
    }

    outputBuffer = WdfMemoryGetBuffer(outputMemory, &outputBufferLength);

    Packet->reportBuffer    = (PUCHAR) outputBuffer;
    Packet->reportBufferLen = (ULONG)  outputBufferLength;

    return status;
}

NTSTATUS
RequestGetHidXferPacket_ToWriteToDevice(
    _In_  WDFREQUEST        Request,
    _Out_ HID_XFER_PACKET  *Packet
    )
{
    //
    // Driver need to read from the input buffer (which was written by App)
    //
    //   Report Buffer: Input Buffer
    //   Report Id    : Output Buffer Length
    //
    // Note that the report id is not stored inside the output buffer, as the
    // driver has no read-access right to the output buffer, and trying to read
    // from the buffer will cause an access violation error.
    //
    // The workaround is to store the report id in the OutputBufferLength field,
    // to which the driver does have read-access right.
    //

    NTSTATUS                status;
    WDFMEMORY               inputMemory;
    WDFMEMORY               outputMemory;
    size_t                  inputBufferLength;
    size_t                  outputBufferLength;
    PVOID                   inputBuffer;

    //
    // Get report Id from output buffer length
    //
    status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
    if( !NT_SUCCESS(status) ) {
        KdPrint(("WdfRequestRetrieveOutputMemory failed 0x%x\n",status));
        return status;
    }
    WdfMemoryGetBuffer(outputMemory, &outputBufferLength);
    Packet->reportId        = (UCHAR) outputBufferLength;

    //
    // Get report buffer from input buffer
    //
    status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
    if( !NT_SUCCESS(status) ) {
        KdPrint(("WdfRequestRetrieveInputMemory failed 0x%x\n",status));
        return status;
    }
    inputBuffer = WdfMemoryGetBuffer(inputMemory, &inputBufferLength);

    Packet->reportBuffer    = (PUCHAR) inputBuffer;
    Packet->reportBufferLen = (ULONG)  inputBufferLength;

    return status;
}

#endif

NTSTATUS
RequestCopyFromBuffer(
    _In_  WDFREQUEST        Request,
    _In_  PVOID             SourceBuffer,
    _When_(NumBytesToCopyFrom == 0, __drv_reportError(NumBytesToCopyFrom cannot be zero))
    _In_  size_t            NumBytesToCopyFrom
    )
/*++

Routine Description:

    A helper function to copy specified bytes to the request's output memory

Arguments:

    Request - A handle to a framework request object.

    SourceBuffer - The buffer to copy data from.

    NumBytesToCopyFrom - The length, in bytes, of data to be copied.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS                status;
    WDFMEMORY               memory;
    size_t                  outputBufferLength;
DbgBreakPoint();
    status = WdfRequestRetrieveOutputMemory(Request, &memory);
    if( !NT_SUCCESS(status) ) {
        KdPrint(("WdfRequestRetrieveOutputMemory failed 0x%x\n",status));
        return status;
    }

    WdfMemoryGetBuffer(memory, &outputBufferLength);
    if (outputBufferLength < NumBytesToCopyFrom) {
        status = STATUS_INVALID_BUFFER_SIZE;
        KdPrint(("RequestCopyFromBuffer: buffer too small. Size %d, expect %d\n",
                (int)outputBufferLength, (int)NumBytesToCopyFrom));
        return status;
    }

    status = WdfMemoryCopyFromBuffer(memory,
                                    0,
                                    SourceBuffer,
                                    NumBytesToCopyFrom);
    if( !NT_SUCCESS(status) ) {
        KdPrint(("WdfMemoryCopyFromBuffer failed 0x%x\n",status));
        return status;
    }

    WdfRequestSetInformation(Request, NumBytesToCopyFrom);
    return status;
}

EVT_WDF_TIMER VirtualHidDeviceMini_EvtTimerHandler;

void
VirtualHidDeviceMini_EvtTimerHandler(
    _In_ WDFTIMER Timer
    )
/*++
Routine Description:

    This periodic timer callback routine checks the device's manual queue and
    completes any pending request with data from the device.

Arguments:

    Timer - Handle to a timer object that was obtained from WdfTimerCreate.

Return Value:

    VOID

--*/
{
    NTSTATUS ntStatus;
    WDFREQUEST request;
    DMFMODULE dmfModule;
    DMF_CONTEXT_VirtualHidDeviceMini* moduleContext;
    DMF_CONFIG_VirtualHidDeviceMini* moduleConfig;
    UCHAR* readReport;
    ULONG readReportSize;
DbgBreakPoint();
    dmfModule = (DMFMODULE)WdfTimerGetParentObject(Timer);
    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    // See if we have a request in manual queue.
    //
    ntStatus = WdfIoQueueRetrieveNextRequest(moduleContext->ManualQueue,
                                             &request);
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = moduleConfig->RetrieveNextInputReport(dmfModule,
                                                         &readReport,
                                                         &readReportSize);
        if (NT_SUCCESS(ntStatus))
        {
            ntStatus = RequestCopyFromBuffer(request,
                                             readReport,
                                             readReportSize);
        }

        WdfRequestComplete(request,
                           ntStatus);
    }
}

NTSTATUS
VirtualHidDeviceMini_ManualQueueCreate(
    _In_ DMFMODULE DmfModule,
    _Out_ WDFQUEUE* Queue
    )
/*++
Routine Description:

    This function creates a manual I/O queue to receive IOCTL_HID_READ_REPORT
    forwarded from the device's default queue handler.

    It also creates a periodic timer to check the queue and complete any pending
    request with data from the device. Here timer expiring is used to simulate
    a hardware event that new data is ready.

    The workflow is like this:

    - Hidclass.sys sends an ioctl to the miniport to read input report.

    - The request reaches the driver's default queue. As data may not be avaiable
      yet, the request is forwarded to a second manual queue temporarily.

    - Later when data is ready (as simulated by timer expiring), the driver
      checks for any pending request in the manual queue, and then completes it.

    - Hidclass gets notified for the read request completion and return data to
      the caller.

    On the other hand, for IOCTL_HID_WRITE_REPORT request, the driver simply
    sends the request to the hardware (as simulated by storing the data at
    DeviceContext->DeviceData) and completes the request immediately. There is
    no need to use another queue for write operation.

Arguments:

    Device - Handle to a framework device object.

    Queue - Output pointer to a framework I/O queue handle, on success.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_OBJECT_ATTRIBUTES queueAttributes;
    WDFQUEUE queue;
    MANUAL_QUEUE_CONTEXT* queueContext;
    WDF_TIMER_CONFIG timerConfig;
    WDF_OBJECT_ATTRIBUTES timerAttributes;
    ULONG timerPeriodInSeconds = 5;
    WDFDEVICE device;
DbgBreakPoint();
    device = DMF_ParentDeviceGet(DmfModule);

    WDF_IO_QUEUE_CONFIG_INIT(
                            &queueConfig,
                            WdfIoQueueDispatchManual);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
                            &queueAttributes,
                            MANUAL_QUEUE_CONTEXT);

    ntStatus = WdfIoQueueCreate(device,
                                &queueConfig,
                                &queueAttributes,
                                &queue);
    if( !NT_SUCCESS(ntStatus) ) 
    {
        KdPrint(("WdfIoQueueCreate failed 0x%x\n",ntStatus));
        goto Exit;
    }

    queueContext = ManualQueueContextGet(queue);
    queueContext->Queue = queue;
    queueContext->DmfModule = DmfModule;

    WDF_TIMER_CONFIG_INIT_PERIODIC(
                            &timerConfig,
                            VirtualHidDeviceMini_EvtTimerHandler,
                            timerPeriodInSeconds * 1000);

    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = DmfModule;
    ntStatus = WdfTimerCreate(&timerConfig,
                              &timerAttributes,
                              &queueContext->Timer);

    if( !NT_SUCCESS(ntStatus) )
    {
        KdPrint(("WdfTimerCreate failed 0x%x\n",ntStatus));
        goto Exit;
    }

    WdfTimerStart(queueContext->Timer,
                  WDF_REL_TIMEOUT_IN_SEC(1));

    *Queue = queue;

Exit:

    return ntStatus;
}

NTSTATUS
VirtualHidDeviceMinit_ReadReport(
    _In_ DMFMODULE DmfModule,
    _In_  WDFREQUEST Request,
    _Always_(_Out_) BOOLEAN* CompleteRequest
    )
/*++

Routine Description:

    Handles IOCTL_HID_READ_REPORT for the HID collection. Normally the request
    will be forwarded to a manual queue for further process. In that case, the
    caller should not try to complete the request at this time, as the request
    will later be retrieved back from the manually queue and completed there.
    However, if for some reason the forwarding fails, the caller still need
    to complete the request with proper error code immediately.

Arguments:

    QueueContext - The object context associated with the queue

    Request - Pointer to  Request Packet.

    CompleteRequest - A boolean output value, indicating whether the caller
            should complete the request or not

Return Value:

    NT status code.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_VirtualHidDeviceMini* moduleContext;
DbgBreakPoint();
    KdPrint(("ReadReport\n"));

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    //
    // forward the request to manual queue
    //
    ntStatus = WdfRequestForwardToIoQueue(Request,
                                          moduleContext->ManualQueue);
    if (! NT_SUCCESS(ntStatus)) 
    {
        KdPrint(("WdfRequestForwardToIoQueue failed with 0x%x\n", ntStatus));
        *CompleteRequest = TRUE;
    }
    else 
    {
        *CompleteRequest = FALSE;
    }

    return ntStatus;
}

NTSTATUS
VirtualHidDeviceMini_WriteReport(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    Handles IOCTL_HID_WRITE_REPORT all the collection.

Arguments:

    QueueContext - The object context associated with the queue

    Request - Pointer to  Request Packet.

Return Value:

    NT ntStatus code.

--*/

{
    NTSTATUS ntStatus;
    HID_XFER_PACKET packet;
    ULONG reportSize;
    DMF_CONFIG_VirtualHidDeviceMini* moduleConfig;
DbgBreakPoint();
    KdPrint(("WriteReport\n"));

    ntStatus = RequestGetHidXferPacket_ToWriteToDevice(Request,
                                                       &packet);
    if (!NT_SUCCESS(ntStatus))
    {
        return ntStatus;
    }

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = moduleConfig->WriteReport(DmfModule,
                                        &packet,
                                        &reportSize);
    if (STATUS_PENDING == ntStatus)
    {
        // The Client will complete the request asynchronously.
        //
    }
    else
    {
        // Complete the request on behalf of the Client.
        //
        WdfRequestSetInformation(Request,
                                 reportSize);
    }

    return ntStatus;
}


HRESULT
VirtualHidDeviceMini_GetFeature(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    Handles IOCTL_HID_GET_FEATURE for all the collection.

Arguments:

    QueueContext - The object context associated with the queue

    Request - Pointer to  Request Packet.

Return Value:

    NT ntStatus code.

--*/
{
    NTSTATUS ntStatus;
    HID_XFER_PACKET packet;
    ULONG reportSize;
    DMF_CONTEXT_VirtualHidDeviceMini* moduleContext;
    DMF_CONFIG_VirtualHidDeviceMini* moduleConfig;
DbgBreakPoint();
    KdPrint(("GetFeature\n"));

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = RequestGetHidXferPacket_ToReadFromDevice(Request,
                                                     &packet);
    if (!NT_SUCCESS(ntStatus)) 
    {
        return ntStatus;
    }

    ntStatus = moduleConfig->GetFeature(DmfModule,
                                      &packet,
                                      &reportSize);
    if (STATUS_PENDING == ntStatus)
    {
        // The Client will complete the request asynchronously.
        //
    }
    else
    {
        // Complete the request on behalf of the Client.
        //
        WdfRequestSetInformation(Request,
                                 reportSize);
    }

    return ntStatus;
}

NTSTATUS
VirtualHidDeviceMini_SetFeature(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    Handles IOCTL_HID_SET_FEATURE for all the collection.
    For control collection (custom defined collection) it handles
    the user-defined control codes for sideband communication

Arguments:

    QueueContext - The object context associated with the queue

    Request - Pointer to Request Packet.

Return Value:

    NT ntStatus code.

--*/
{
    NTSTATUS ntStatus;
    HID_XFER_PACKET packet;
    ULONG reportSize;
    DMF_CONTEXT_VirtualHidDeviceMini* moduleContext;
    DMF_CONFIG_VirtualHidDeviceMini* moduleConfig;
DbgBreakPoint();
    KdPrint(("SetFeature\n"));

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = RequestGetHidXferPacket_ToWriteToDevice(Request,
                                                     &packet);
    if( !NT_SUCCESS(ntStatus) ) 
    {
        return ntStatus;
    }

    ntStatus = moduleConfig->SetFeature(DmfModule,
                                      &packet,
                                      &reportSize);
    if (STATUS_PENDING == ntStatus)
    {
        // The Client will complete the request asynchronously.
        //
    }
    else
    {
        // Complete the request on behalf of the Client.
        //
        WdfRequestSetInformation(Request,
                                 reportSize);
    }

    return ntStatus;
}

NTSTATUS
VirtualHidDeviceMini_GetInputReport(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    Handles IOCTL_HID_GET_INPUT_REPORT for all the collection.

Arguments:

    QueueContext - The object context associated with the queue

    Request - Pointer to Request Packet.

Return Value:

    NT ntStatus code.

--*/
{
    NTSTATUS ntStatus;
    HID_XFER_PACKET packet;
    ULONG reportSize;
    DMF_CONTEXT_VirtualHidDeviceMini* moduleContext;
    DMF_CONFIG_VirtualHidDeviceMini* moduleConfig;
DbgBreakPoint();
    KdPrint(("GetInputReport\n"));

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = RequestGetHidXferPacket_ToReadFromDevice(Request,
                                                      &packet);
    if( !NT_SUCCESS(ntStatus) )
    {
        return ntStatus;
    }

    ntStatus = moduleConfig->GetInputReport(DmfModule,
                                          &packet,
                                          &reportSize);
    if (STATUS_PENDING == ntStatus)
    {
        // The Client will complete the request asynchronously.
        //
    }
    else
    {
        // Complete the request on behalf of the Client.
        //
        WdfRequestSetInformation(Request,
                                 reportSize);
    }

    return ntStatus;
}


NTSTATUS
VirtualHidDeviceMini_SetOutputReport(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    Handles IOCTL_HID_SET_OUTPUT_REPORT for all the collection.

Arguments:

    QueueContext - The object context associated with the queue

    Request - Pointer to Request Packet.

Return Value:

    NT ntStatus code.

--*/
{
    NTSTATUS                ntStatus;
    HID_XFER_PACKET         packet;
    ULONG                   reportSize;
    DMF_CONTEXT_VirtualHidDeviceMini* moduleContext;
    DMF_CONFIG_VirtualHidDeviceMini* moduleConfig;
DbgBreakPoint();
    KdPrint(("SetOutputReport\n"));

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = RequestGetHidXferPacket_ToWriteToDevice(Request,
                                                     &packet);
    if ( !NT_SUCCESS(ntStatus) )
    {
        return ntStatus;
    }

    ntStatus = moduleConfig->SetOutputReport(DmfModule,
                                             &packet,
                                             &reportSize);
    if (STATUS_PENDING == ntStatus)
    {
        // The Client will complete the request asynchronously.
        //
    }
    else
    {
        // Complete the request on behalf of the Client.
        //
        WdfRequestSetInformation(Request,
                                 reportSize);
    }

    return ntStatus;
}

NTSTATUS
GetStringId(
    _In_  WDFREQUEST        Request,
    _Out_ ULONG            *StringId,
    _Out_ ULONG            *LanguageId
    )
/*++

Routine Description:

    Helper routine to decode IOCTL_HID_GET_INDEXED_STRING and IOCTL_HID_GET_STRING.

Arguments:

    Request - Pointer to Request Packet.

Return Value:

    NT status code.

--*/
{
    NTSTATUS                status;
    ULONG                   inputValue;
DbgBreakPoint();
#ifdef _KERNEL_MODE

    WDF_REQUEST_PARAMETERS  requestParameters;

    //
    // IOCTL_HID_GET_STRING:                      // METHOD_NEITHER
    // IOCTL_HID_GET_INDEXED_STRING:              // METHOD_OUT_DIRECT
    //
    // The string id (or string index) is passed in Parameters.DeviceIoControl.
    // Type3InputBuffer. However, Parameters.DeviceIoControl.InputBufferLength
    // was not initialized by hidclass.sys, therefore trying to access the
    // buffer with WdfRequestRetrieveInputMemory will fail
    //
    // Another problem with IOCTL_HID_GET_INDEXED_STRING is that METHOD_OUT_DIRECT
    // expects the input buffer to be Irp->AssociatedIrp.SystemBuffer instead of
    // Type3InputBuffer. That will also fail WdfRequestRetrieveInputMemory.
    //
    // The solution to the above two problems is to get Type3InputBuffer directly
    //
    // Also note that instead of the buffer's content, it is the buffer address
    // that was used to store the string id (or index)
    //

    WDF_REQUEST_PARAMETERS_INIT(&requestParameters);
    WdfRequestGetParameters(Request, &requestParameters);

    inputValue = PtrToUlong(
        requestParameters.Parameters.DeviceIoControl.Type3InputBuffer);

    status = STATUS_SUCCESS;

#else

    WDFMEMORY               inputMemory;
    size_t                  inputBufferLength;
    PVOID                   inputBuffer;

    //
    // mshidumdf.sys updates the IRP and passes the string id (or index) through
    // the input buffer correctly based on the IOCTL buffer type
    //

    status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
    if( !NT_SUCCESS(status) ) {
        KdPrint(("WdfRequestRetrieveInputMemory failed 0x%x\n",status));
        return status;
    }
    inputBuffer = WdfMemoryGetBuffer(inputMemory, &inputBufferLength);

    //
    // make sure buffer is big enough.
    //
    if (inputBufferLength < sizeof(ULONG))
    {
        status = STATUS_INVALID_BUFFER_SIZE;
        KdPrint(("GetStringId: invalid input buffer. size %d, expect %d\n",
                            (int)inputBufferLength, (int)sizeof(ULONG)));
        return status;
    }

    inputValue = (*(PULONG)inputBuffer);

#endif

    //
    // The least significant two bytes of the INT value contain the string id.
    //
    *StringId = (inputValue & 0x0ffff);

    //
    // The most significant two bytes of the INT value contain the language
    // ID (for example, a value of 1033 indicates English).
    //
    *LanguageId = (inputValue >> 16);

    return status;
}

NTSTATUS
VirtualHidDeviceMini_IndexedStringGet(
    _In_ DMFMODULE DmfModule,
    _In_  WDFREQUEST Request
    )
/*++

Routine Description:

    Handles IOCTL_HID_GET_INDEXED_STRING

Arguments:

    Request - Pointer to Request Packet.

Return Value:

    NT status code.

--*/
{
    NTSTATUS ntStatus;
    ULONG languageId, stringIndex;
    DMF_CONFIG_VirtualHidDeviceMini* moduleConfig;
DbgBreakPoint();
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = GetStringId(Request,
                           &stringIndex,
                           &languageId);

    // While we don't use the language id, some minidrivers might.
    //
    UNREFERENCED_PARAMETER(languageId);

    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    if (stringIndex >= moduleConfig->NumberOfStrings)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        KdPrint(("GetString: unkown string index %d\n", stringIndex));
        goto Exit;
    }

    ntStatus = RequestCopyFromBuffer(Request,
                                    moduleConfig->Strings[stringIndex],
                                    wcslen(moduleConfig->Strings[stringIndex]));

Exit:

    return ntStatus;
}

NTSTATUS
VirtualHidDeviceMini_StringGet(
    _In_ DMFMODULE DmfModule,
    _In_  WDFREQUEST Request
    )
/*++

Routine Description:

    Handles IOCTL_HID_GET_STRING.

Arguments:

    Request - Pointer to Request Packet.

Return Value:

    NT status code.

--*/
{
    NTSTATUS ntStatus;
    ULONG languageId;
    ULONG stringId;
    size_t stringSizeCb;
    PWSTR string;
    DMF_CONFIG_VirtualHidDeviceMini* moduleConfig;
DbgBreakPoint();
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = GetStringId(Request,
                           &stringId,
                           &languageId);

    // While we don't use the language id, some minidrivers might.
    //
    UNREFERENCED_PARAMETER(languageId);

    if (!NT_SUCCESS(ntStatus)) 
    {
        goto Exit;
    }

    switch (stringId)
    {
        case HID_STRING_ID_IMANUFACTURER:
            stringSizeCb = moduleConfig->StringSizeCbManufacturer;
            string = moduleConfig->StringManufacturer;
            break;
        case HID_STRING_ID_IPRODUCT:
            stringSizeCb = moduleConfig->StringSizeCbProduct;
            string = moduleConfig->StringProduct;
            break;
        case HID_STRING_ID_ISERIALNUMBER:
            stringSizeCb = moduleConfig->StringSizeCbSerialNumber;
            string = moduleConfig->StringSerialNumber;
            break;
        default:
            ntStatus = STATUS_INVALID_PARAMETER;
            KdPrint(("GetString: unkown string id %d\n", stringId));
            goto Exit;
    }

    ntStatus = RequestCopyFromBuffer(Request,
                                     string,
                                     stringSizeCb);

Exit:

    return ntStatus;
}

_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_VirtualHidDeviceMini_ModuleDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    Template callback for ModuleInternalDeviceIoControl for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    Queue - WDF file object to where the Request is sent.
    Request - The Request that contains the IOCTL parameters.
    OutputBufferLength - Output buffer length in the Request.
    InputBufferLength - Input buffer length in the Request.
    IoControlCode - IOCTL of the Request.

Return Value:

    If this Module handles the IOCTL (it recognizes it), returns TRUE.
    If this Module does not recognize the IOCTL, returns FALSE.

--*/
{
    BOOLEAN handled;
    NTSTATUS ntStatus;
    BOOLEAN completeRequest;
    DMF_CONTEXT_VirtualHidDeviceMini* moduleContext;
    DMF_CONFIG_VirtualHidDeviceMini* moduleConfig;

DbgBreakPoint();
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(IoControlCode);

    FuncEntry(DMF_TRACE);

    handled = TRUE;
    completeRequest = TRUE;

    UNREFERENCED_PARAMETER  (OutputBufferLength);
    UNREFERENCED_PARAMETER  (InputBufferLength);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    switch (IoControlCode)
    {
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:   // METHOD_NEITHER
        //
        // Retrieves the device's HID descriptor.
        //
        _Analysis_assume_(moduleContext->HidDescriptor.bLength != 0);
        ntStatus = RequestCopyFromBuffer(Request,
                                         (VOID*)moduleConfig->HidDescriptor,
                                         moduleConfig->HidDescriptor->bLength);
        break;

    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:   // METHOD_NEITHER
        //
        //Retrieves a device's attributes in a HID_DEVICE_ATTRIBUTES structure.
        //
        ntStatus = RequestCopyFromBuffer(Request,
                                         &moduleConfig->HidDeviceAttributes,
                                         sizeof(HID_DEVICE_ATTRIBUTES));
        break;

    case IOCTL_HID_GET_REPORT_DESCRIPTOR:   // METHOD_NEITHER
        //
        //Obtains the report descriptor for the HID device.
        //
        ntStatus = RequestCopyFromBuffer(Request,
                                         (VOID*)moduleConfig->HidReportDescriptor,
                                         moduleConfig->HidDescriptor->DescriptorList[0].wReportLength);
        break;

    case IOCTL_HID_READ_REPORT:             // METHOD_NEITHER
        //
        // Returns a report from the device into a class driver-supplied
        // buffer.
        //
        ntStatus = VirtualHidDeviceMinit_ReadReport(DmfModule,
                                                    Request,
                                                    &completeRequest);
        break;

    case IOCTL_HID_WRITE_REPORT:            // METHOD_NEITHER
        // Transmits a class driver-supplied report to the device.
        //
        ntStatus = VirtualHidDeviceMini_WriteReport(DmfModule,
                                                    Request);
        break;

#ifdef _KERNEL_MODE

    case IOCTL_HID_GET_FEATURE:             // METHOD_OUT_DIRECT

        ntStatus = VirtualHidDeviceMini_GetFeature(DmfModule,
                                                   Request);
        break;

    case IOCTL_HID_SET_FEATURE:             // METHOD_IN_DIRECT

        ntStatus = VirtualHidDeviceMini_SetFeature(DmfModule,
                                                   Request);
        break;

    case IOCTL_HID_GET_INPUT_REPORT:        // METHOD_OUT_DIRECT

        ntStatus = VirtualHidDeviceMini_GetInputReport(DmfModule,
                                                       Request);
        break;

    case IOCTL_HID_SET_OUTPUT_REPORT:       // METHOD_IN_DIRECT

        ntStatus = VirtualHidDeviceMini_SetOutputReport(DmfModule,
                                                        Request);
        break;

#else // UMDF specific

    //
    // HID minidriver IOCTL uses HID_XFER_PACKET which contains an embedded pointer.
    //
    //   typedef struct _HID_XFER_PACKET {
    //     PUCHAR reportBuffer;
    //     ULONG  reportBufferLen;
    //     UCHAR  reportId;
    //   } HID_XFER_PACKET, *PHID_XFER_PACKET;
    //
    // UMDF cannot handle embedded pointers when marshalling buffers between processes.
    // Therefore a special driver mshidumdf.sys is introduced to convert such IRPs to
    // new IRPs (with new IOCTL name like IOCTL_UMDF_HID_Xxxx) where:
    //
    //   reportBuffer - passed as one buffer inside the IRP
    //   reportId     - passed as a second buffer inside the IRP
    //
    // The new IRP is then passed to UMDF host and driver for further processing.
    //

    case IOCTL_UMDF_HID_GET_FEATURE:        // METHOD_NEITHER

        ntStatus = VirtualHidDeviceMini_GetFeature(DmfModule,
                                                   Request);
        break;

    case IOCTL_UMDF_HID_SET_FEATURE:        // METHOD_NEITHER

        ntStatus = VirtualHidDeviceMini_SetFeature(DmfModule,
                                                   Request);
        break;

    case IOCTL_UMDF_HID_GET_INPUT_REPORT:  // METHOD_NEITHER

        ntStatus = VirtualHidDeviceMini_GetInputReport(DmfModule,
                                                       Request);
        break;

    case IOCTL_UMDF_HID_SET_OUTPUT_REPORT: // METHOD_NEITHER

        ntStatus = VirtualHidDeviceMini_SetOutputReport(DmfModule,
                                                        Request);
        break;

#endif // _KERNEL_MODE

    case IOCTL_HID_GET_STRING:                      // METHOD_NEITHER

        ntStatus = VirtualHidDeviceMini_StringGet(DmfModule,
                                                  Request);
        break;

    case IOCTL_HID_GET_INDEXED_STRING:              // METHOD_OUT_DIRECT

        ntStatus = VirtualHidDeviceMini_IndexedStringGet(DmfModule,
                                                         Request);
        break;

    case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST:  // METHOD_NEITHER
        //
        // This has the USBSS Idle notification callback. If the lower driver
        // can handle it (e.g. USB stack can handle it) then pass it down
        // otherwise complete it here as not inplemented. For a virtual
        // device, idling is not needed.
        //
        // Not implemented. fall through...
        //
        // TODO: Callback Client
        //
    case IOCTL_HID_ACTIVATE_DEVICE:                 // METHOD_NEITHER
    case IOCTL_HID_DEACTIVATE_DEVICE:               // METHOD_NEITHER
    case IOCTL_GET_PHYSICAL_DESCRIPTOR:             // METHOD_OUT_DIRECT
        //
        // We don't do anything for these IOCTLs but some minidrivers might.
        //
        // Not implemented. fall through...
        //
        // TODO: Callback Client
        //
        ntStatus = STATUS_NOT_IMPLEMENTED;
        break;
    default:
        // Let other Modules handle the IOCTL.
        //
        handled = FALSE;
        ntStatus = STATUS_NOT_SUPPORTED;
        break;
    }

    //
    // Complete the request. Information value has already been set by request
    // handlers.
    //
    if (handled && completeRequest) 
    {
        WdfRequestComplete(Request,
                           ntStatus);
    }

    FuncExit(DMF_TRACE, "returnValue=%d", handled);

    return handled;
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
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_VirtualHidDeviceMini_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type VirtualHidDeviceMini.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_VirtualHidDeviceMini* moduleConfig;
    DMF_CONTEXT_VirtualHidDeviceMini* moduleContext;
    WDFDEVICE device;

    PAGED_CODE();
DbgBreakPoint();
    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);


    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_VirtualHidDeviceMini_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type VirtualHidDeviceMini.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);
DbgBreakPoint();
    FuncEntry(DMF_TRACE);

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
DMF_VirtualHidDeviceMini_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type VirtualHidDeviceMini.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_VirtualHidDeviceMini;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_VirtualHidDeviceMini;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_VirtualHidDeviceMini;
DbgBreakPoint();
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_VirtualHidDeviceMini);
    dmfCallbacksDmf_VirtualHidDeviceMini.DeviceOpen = DMF_VirtualHidDeviceMini_Open;
    dmfCallbacksDmf_VirtualHidDeviceMini.DeviceClose = DMF_VirtualHidDeviceMini_Close;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_VirtualHidDeviceMini);
#if defined(DMF_USER_MODE)
    dmfCallbacksWdf_VirtualHidDeviceMini.ModuleDeviceIoControl = DMF_VirtualHidDeviceMini_ModuleDeviceIoControl;
#else
    dmfCallbacksWdf_VirtualHidDeviceMini.ModuleInternalDeviceIoControl = DMF_VirtualHidDeviceMini_ModuleDeviceIoControl;
#endif
    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_VirtualHidDeviceMini,
                                            VirtualHidDeviceMini,
                                            DMF_CONTEXT_VirtualHidDeviceMini,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_VirtualHidDeviceMini.CallbacksDmf = &dmfCallbacksDmf_VirtualHidDeviceMini;
    dmfModuleDescriptor_VirtualHidDeviceMini.CallbacksWdf = &dmfCallbacksWdf_VirtualHidDeviceMini;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_VirtualHidDeviceMini,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    DMF_CONTEXT_VirtualHidDeviceMini* moduleContext = DMF_CONTEXT_GET(*DmfModule);

    // NOTE: Queues associated with DMFMODULE must be created in the Create callback.
    //
    ntStatus = VirtualHidDeviceMini_ManualQueueCreate(*DmfModule,
                                                      &moduleContext->ManualQueue);
    if (! NT_SUCCESS(ntStatus))
    {
        WdfObjectDelete(*DmfModule);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_VirtualHidDeviceMini.c
//
