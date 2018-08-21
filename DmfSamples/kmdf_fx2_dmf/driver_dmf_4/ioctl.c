/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Ioctl.c

Abstract:

    USB device driver for OSR USB-FX2 Learning Kit (DMF Version)

Environment:

    Kernel mode only

--*/

#include <osrusbfx2.h>

#if defined(EVENT_TRACING)
#include "ioctl.tmh"
#endif

#pragma alloc_text(PAGE, OsrFxIoDeviceControl)

VOID
OsrUsbIoctlGetInterruptMessage(
    _In_ WDFDEVICE Device,
    _In_ NTSTATUS  ReaderStatus
    )
/*++

Routine Description

    This method handles the completion of the pended request for the IOCTL
    IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE.

Arguments:

    Device - Handle to a framework device.

Return Value:

    None.

--*/
{
    NTSTATUS            status;
    WDFREQUEST          request;
    PDEVICE_CONTEXT     pDevContext;
    size_t              bytesReturned = 0;
    PSWITCH_STATE       switchState = NULL;

    pDevContext = GetDeviceContext(Device);

    do {

        //
        // Check if there are any pending requests in the Interrupt Message Queue.
        // If a request is found then complete the pending request.
        //
        status = WdfIoQueueRetrieveNextRequest(pDevContext->InterruptMsgQueue, &request);

        if (NT_SUCCESS(status)) {
            status = WdfRequestRetrieveOutputBuffer(request,
                                                    sizeof(SWITCH_STATE),
                                                    &switchState,
                                                    NULL);// BufferLength

            if (!NT_SUCCESS(status)) {

                TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
                    "User's output buffer is too small for this IOCTL, expecting a SWITCH_STATE\n");
                bytesReturned = sizeof(SWITCH_STATE);

            } else {

                //
                // Copy the state information saved by the continuous reader.
                //
                if (NT_SUCCESS(ReaderStatus)) {
                    //
                    // DMF: Call a Module Method using the Module handle stored in the device context.
                    //
                    DMF_OsrFx2_SwitchStateGet(pDevContext->DmfModuleOsrFx2, &switchState->SwitchesAsUChar);
                    bytesReturned = sizeof(SWITCH_STATE);
                } else {
                    bytesReturned = 0;
                }
            }

            //
            // Complete the request.  If we failed to get the output buffer then
            // complete with that status.  Otherwise complete with the status from the reader.
            //
            WdfRequestCompleteWithInformation(request,
                                              NT_SUCCESS(status) ? ReaderStatus : status,
                                              bytesReturned);
            status = STATUS_SUCCESS;

        } else if (status != STATUS_NO_MORE_ENTRIES) {
            KdPrint(("WdfIoQueueRetrieveNextRequest status %08x\n", status));
        }

        request = NULL;

    } while (status == STATUS_SUCCESS);

    return;

}

//
// DMF: This callback is called by Dmf_IoctlHandler based on the CONIFG set by the Client driver 
//      earlier. When this callback runs, the IOCTL, input/output buffers have been validated.
//
NTSTATUS
OsrFxIoDeviceControl(
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
    WDFDEVICE           device;
    PDEVICE_CONTEXT     pDevContext;
    size_t              bytesReturned = 0;
    NTSTATUS            status = STATUS_INVALID_DEVICE_REQUEST;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferSize);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);

    //
    // If your driver is at the top of its driver stack, EvtIoDeviceControl is called
    // at IRQL = PASSIVE_LEVEL.
    //
    _IRQL_limited_to_(PASSIVE_LEVEL);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "--> OsrFxEvtIoDeviceControl\n");
    //
    // initialize variables
    //

    //
    // DMF: A frequent programming DMF programming pattern is that callbacks made by DMF Modules
    //      pass the corresponding DMFMODULE handle. From that handle, it is possible to get the
    //      Client driver's WDFDEVICE and device context.
    //
    device = DMF_ParentDeviceGet(DmfModule);
    pDevContext = GetDeviceContext(device);

    switch(IoControlCode) {

    case IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE:

        //
        // Forward the request to an interrupt message queue and dont complete
        // the request until an interrupt from the USB device occurs.
        //
        status = WdfRequestForwardToIoQueue(Request, pDevContext->InterruptMsgQueue);
        if (NT_SUCCESS(status)) {
            //
            // DMF: Dmf_IoctlHandler will complete all requests unless status == STATUS_PENDING. 
            //
            status = STATUS_PENDING;
        }

        break;

    //
    // DMF: "default" will never happen because IOCTL codes have bee validated already.
    //
    }

    //
    // DMF: Dmf_IoctlHandler will complete all requests unless status == STATUS_PENDING. 
    //

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "<-- OsrFxEvtIoDeviceControl\n");
    
    //
    // DMF: Dmf_IoctlHandler will return this information with the request if it completes it.
    //      Local variable is not necessary, of course. It is left here to reduce changes.
    //
    *BytesReturned = bytesReturned;

    return status;
}

