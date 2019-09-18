/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfContainer.c

Abstract:

    DMF Implementation:

    This file provides support for the Container Driver that "contains" all 
    DMF drivers. This file contains the callbacks that WDF calls. These
    callbacks then dispatch to Modules and their Child Modules.

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#include "DmfIncludeInternal.h"

#include "DmfContainer.tmh"

// Callbacks called by Windows directly should use C calling convention (not C++).
//
#if defined(__cplusplus)
extern "C" {
#endif // defined(__cplusplus)

EVT_WDF_DEVICE_PREPARE_HARDWARE DmfContainerEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE DmfContainerEvtDeviceReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY DmfContainerEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_ENTRY_POST_INTERRUPTS_ENABLED DmfContainerEvtDeviceD0EntryPostInterruptsEnabled;
EVT_WDF_DEVICE_D0_EXIT DmfContainerEvtDeviceD0Exit;
EVT_WDF_DEVICE_D0_EXIT_PRE_INTERRUPTS_DISABLED DmfContainerEvtDeviceD0ExitPreInterruptsDisabled;
EVT_WDF_IO_QUEUE_IO_READ DmfContainerEvtQueueIoRead;
EVT_WDF_IO_QUEUE_IO_WRITE DmfContainerEvtQueueIoWrite;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL DmfContainerEvtDeviceIoControl;
#if !defined(DMF_USER_MODE)
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL DmfContainerEvtInternalDeviceIoControl;
#endif // !defined(DMF_USER_MODE)
EVT_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP DmfContainerEvtDeviceSelfManagedIoCleanup;
EVT_WDF_DEVICE_SELF_MANAGED_IO_FLUSH DmfContainerEvtDeviceSelfManagedIoFlush;
EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT DmfContainerEvtDeviceSelfManagedIoInit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_SUSPEND DmfContainerEvtDeviceSelfManagedIoSuspend;
EVT_WDF_DEVICE_SELF_MANAGED_IO_RESTART DmfContainerEvtDeviceSelfManagedIoRestart;
EVT_WDF_DEVICE_SURPRISE_REMOVAL DmfContainerEvtDeviceSurpriseRemoval;
EVT_WDF_DEVICE_QUERY_REMOVE DmfContainerEvtDeviceQueryRemove;
EVT_WDF_DEVICE_QUERY_STOP DmfContainerEvtDeviceQueryStop;
EVT_WDF_DEVICE_RELATIONS_QUERY DmfContainerEvtDeviceRelationsQuery;
EVT_WDF_DEVICE_USAGE_NOTIFICATION_EX DmfContainerEvtDeviceUsageNotificationEx;
EVT_WDF_DEVICE_ARM_WAKE_FROM_S0 DmfContainerEvtDeviceArmWakeFromS0;
EVT_WDF_DEVICE_DISARM_WAKE_FROM_S0 DmfContainerEvtDeviceDisarmWakeFromS0;
EVT_WDF_DEVICE_WAKE_FROM_S0_TRIGGERED DmfContainerEvtDeviceWakeFromS0Triggered;
EVT_WDF_DEVICE_ARM_WAKE_FROM_SX_WITH_REASON DmfContainerEvtDeviceArmWakeFromSxWithReason;
EVT_WDF_DEVICE_DISARM_WAKE_FROM_SX DmfContainerEvtDeviceDisarmWakeFromSx;
EVT_WDF_DEVICE_WAKE_FROM_SX_TRIGGERED DmfContainerEvtDeviceWakeFromSxTriggered;
EVT_WDF_DEVICE_FILE_CREATE DmfContainerEvtFileCreate;
EVT_WDF_FILE_CLEANUP DmfContainerEvtFileCleanup;
EVT_WDF_FILE_CLOSE DmfContainerEvtFileClose;

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WDF callbacks to the DMF Container Driver.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
DmfContainerEvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    DMF Container Driver PrepareHardware Callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.
    ResourcesRaw - WDF Resource Raw parameter that is passed to the given
                   DMF Module callback.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.

Return Value:

   STATUS_SUCCESS if all the DMF Modules in the collection succeed; or an error code
   of the first one that fails.

--*/
{
    NTSTATUS ntStatus;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    ntStatus = DMF_ModuleCollectionPrepareHardware(dmfDeviceContext->DmfCollection,
                                                   ResourcesRaw,
                                                   ResourcesTranslated);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCollectionPrepareHardware fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
DmfContainerEvtDeviceReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    DMF Container Driver ReleaseHardware Callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.

Return Value:

   STATUS_SUCCESS if all the DMF Modules in the collection succeed; or an error code
   of the first one that fails.

--*/
{
    NTSTATUS ntStatus;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    ntStatus = DMF_ModuleCollectionReleaseHardware(dmfDeviceContext->DmfCollection,
                                                   ResourcesTranslated);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCollectionPrepareHardware fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

_Use_decl_annotations_
NTSTATUS
DmfContainerEvtDeviceD0Entry(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    DMF Container Driver D0Entry Callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.
    PreviousState - The WDF Power State that the Container Driver should exit from.

Return Value:

   STATUS_SUCCESS if all the DMF Modules in the collection succeed; or an error code
   of the first one that fails.

--*/
{
    NTSTATUS ntStatus;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    ntStatus = DMF_ModuleCollectionD0Entry(dmfDeviceContext->DmfCollection,
                                           PreviousState);
    if (! NT_SUCCESS(ntStatus))
    {
        DMF_ModuleCollectionD0EntryCleanup(dmfDeviceContext->DmfCollection);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCollectionD0Entry fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Use_decl_annotations_
NTSTATUS
DmfContainerEvtDeviceD0EntryPostInterruptsEnabled(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    DMF Container Driver D0EntryPostInterruptsEnabled Callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.
    PreviousState - The WDF Power State that the Container Driver should exit from.

Return Value:

    STATUS_SUCCESS if all the DMF Modules in the collection succeed; or an error code
    of the first one that fails.

--*/
{
    NTSTATUS ntStatus;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    ntStatus = DMF_ModuleCollectionD0EntryPostInterruptsEnabled(dmfDeviceContext->DmfCollection,
                                                                PreviousState);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCollectionD0EntryPostInterruptsEnabled fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Use_decl_annotations_
NTSTATUS
DmfContainerEvtDeviceD0ExitPreInterruptsDisabled(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    DMF Container Driver D0ExitPreInterruptsDisabled Callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.
    TargetState - The WDF Power State that the Container Driver will enter into.

Return Value:

    STATUS_SUCCESS if all the DMF Modules in the collection succeed; or an error code
    of the first one that fails.

--*/
{
    NTSTATUS ntStatus;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    ntStatus = DMF_ModuleCollectionD0ExitPreInterruptsDisabled(dmfDeviceContext->DmfCollection,
                                                               TargetState);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCollectionD0ExitPreInterruptsDisabled fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Use_decl_annotations_
NTSTATUS
DmfContainerEvtDeviceD0Exit(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    DMF Container Driver D0Exit Callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.
    TargetState - The WDF Power State that the Container Driver will enter into.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    ntStatus = DMF_ModuleCollectionD0Exit(dmfDeviceContext->DmfCollection,
                                          TargetState);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCollectionD0Exit fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Use_decl_annotations_
VOID
DmfContainerEvtQueueIoRead(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    )
/*++

Routine Description:

    This event is called when the framework receives IRP_MJ_DEVICE_CONTROL
    requests from the system.

Arguments:

    Queue - Handle to the framework queue object that is associated with the I/O request.
    Request - Handle to a framework request object.
    Length - Length of the request's buffer.

Return Value:

    None

--*/
{
    WDFDEVICE device;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    BOOLEAN handled;

    FuncEntry(DMF_TRACE);

    device = WdfIoQueueGetDevice(Queue);
    dmfDeviceContext = DmfDeviceContextGet(device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    handled = DMF_ModuleCollectionQueueIoRead(dmfDeviceContext->DmfCollection,
                                              Queue,
                                              Request,
                                              Length);
    if (! handled)
    {
        // No Module handled this Request. If the Client Driver is a Filter driver, pass the 
        // Request to the next driver in the stack. If the Client Driver is not a Filter driver,
        // complete the Request indicating that the Client Driver does not support the Request.
        //
        if (dmfDeviceContext->IsFilterDevice)
        {
            // Completes this Request with error if it cannot passthru.
            //
            DMF_RequestPassthru(device,
                                Request);
        }
        else
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Request not supported: Request=%p", Request);
            WdfRequestComplete(Request,
                               STATUS_NOT_SUPPORTED);
        }
    }

    FuncExitVoid(DMF_TRACE);
}

_Use_decl_annotations_
VOID
DmfContainerEvtQueueIoWrite(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    )
/*++

Routine Description:

    This event is called when the framework receives IRP_MJ_DEVICE_CONTROL
    requests from the system.

Arguments:

    Queue - Handle to the framework queue object that is associated with the I/O request.
    Request - Handle to a framework request object.
    Length - Length of the request's buffer.

Return Value:

    None

--*/
{
    WDFDEVICE device;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    BOOLEAN handled;

    FuncEntry(DMF_TRACE);

    device = WdfIoQueueGetDevice(Queue);
    dmfDeviceContext = DmfDeviceContextGet(device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    handled = DMF_ModuleCollectionQueueIoWrite(dmfDeviceContext->DmfCollection,
                                               Queue,
                                               Request,
                                               Length);
    if (! handled)
    {
        // No Module handled this Request. If the Client Driver is a Filter driver, pass the 
        // Request to the next driver in the stack. If the Client Driver is not a Filter driver,
        // complete the Request indicating that the Client Driver does not support the Request.
        //
        if (dmfDeviceContext->IsFilterDevice)
        {
            // Completes this Request with error if it cannot passthru.
            //
            DMF_RequestPassthru(device,
                                Request);
        }
        else
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Request not supported: Request=%p", Request);
            WdfRequestComplete(Request,
                               STATUS_NOT_SUPPORTED);
        }
    }

    FuncExitVoid(DMF_TRACE);
}

_Use_decl_annotations_
VOID
DmfContainerEvtDeviceIoControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    This event is called when the framework receives IRP_MJ_DEVICE_CONTROL
    requests from the system.

Arguments:

    Queue - Handle to the framework queue object that is associated with the I/O request.
    Request - Handle to a framework request object.
    OutputBufferLength - Length of the request's output buffer if an output buffer is available.
    InputBufferLength - Length of the request's input buffer if an input buffer is available.
    IoControlCode - The driver-defined or system-defined I/O control code (IOCTL) that is associated with the request.

Return Value:

    None

--*/
{
    WDFDEVICE device;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    BOOLEAN handled;

    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    FuncEntry(DMF_TRACE);

    device = WdfIoQueueGetDevice(Queue);
    dmfDeviceContext = DmfDeviceContextGet(device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    handled = DMF_ModuleCollectionDeviceIoControl(dmfDeviceContext->DmfCollection,
                                                  Queue,
                                                  Request,
                                                  OutputBufferLength,
                                                  InputBufferLength,
                                                  IoControlCode);
    if (! handled)
    {
        // No Module handled this Request. If the Client Driver is a Filter driver, pass the 
        // Request to the next driver in the stack. If the Client Driver is not a Filter driver,
        // complete the Request indicating that the Client Driver does not support the Request.
        //
        if (dmfDeviceContext->IsFilterDevice)
        {
            // Completes this Request with error if it cannot passthru.
            //
            DMF_RequestPassthru(device,
                                Request);
        }
        else
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Request not supported: Request=%p", Request);
            WdfRequestComplete(Request,
                               STATUS_NOT_SUPPORTED);
        }
    }

    FuncExitVoid(DMF_TRACE);
}

#if !defined(DMF_USER_MODE)
_Use_decl_annotations_
VOID
DmfContainerEvtInternalDeviceIoControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    This event is called when the framework receives IRP_MJ_INTERNAL_DEVICE_CONTROL
    requests from the system.

Arguments:

    Queue - Handle to the framework queue object that is associated with the I/O request.
    Request - Handle to a framework request object.
    OutputBufferLength - Length of the request's output buffer if an output buffer is available.
    InputBufferLength - Length of the request's input buffer if an input buffer is available.
    IoControlCode - The driver-defined or system-defined I/O control code (IOCTL) that is associated with the request.

Return Value:

    None

--*/
{
    WDFDEVICE device;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    BOOLEAN handled;

    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    FuncEntry(DMF_TRACE);

    device = WdfIoQueueGetDevice(Queue);
    dmfDeviceContext = DmfDeviceContextGet(device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    handled = DMF_ModuleCollectionInternalDeviceIoControl(dmfDeviceContext->DmfCollection,
                                                          Queue,
                                                          Request,
                                                          OutputBufferLength,
                                                          InputBufferLength,
                                                          IoControlCode);
    if (! handled)
    {
        // No Module handled this Request. If the Client Driver is a Filter driver, pass the 
        // Request to the next driver in the stack. If the Client Driver is not a Filter driver,
        // complete the Request indicating that the Client Driver does not support the Request.
        //
        if (dmfDeviceContext->IsFilterDevice)
        {
            // Completes this Request with error if it cannot passthru.
            //
            DMF_RequestPassthru(device,
                                Request);
        }
        else
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Request not supported: Request=%p", Request);
            WdfRequestComplete(Request,
                               STATUS_NOT_SUPPORTED);
        }
    }

    FuncExitVoid(DMF_TRACE);
}
#endif // !defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
DmfContainerEvtDeviceSelfManagedIoCleanup(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    WDF Self Managed Io Clean Up callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    DMF_ModuleCollectionSelfManagedIoCleanup(dmfDeviceContext->DmfCollection);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
DmfContainerEvtDeviceSelfManagedIoFlush(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    WDF Self Managed Io Flush callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    DMF_ModuleCollectionSelfManagedIoFlush(dmfDeviceContext->DmfCollection);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
DmfContainerEvtDeviceSelfManagedIoInit(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    WDF Self Managed Io Init callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    ntStatus = DMF_ModuleCollectionSelfManagedIoInit(dmfDeviceContext->DmfCollection);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
DmfContainerEvtDeviceSelfManagedIoSuspend(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    WDF Self Managed Io Suspend callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    ntStatus = DMF_ModuleCollectionSelfManagedIoSuspend(dmfDeviceContext->DmfCollection);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
DmfContainerEvtDeviceSelfManagedIoRestart(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    WDF Self Managed Io Restart callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    ntStatus = DMF_ModuleCollectionSelfManagedIoRestart(dmfDeviceContext->DmfCollection);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
DmfContainerEvtDeviceSurpriseRemoval(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    WDF Surprise Removal callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    DMF_ModuleCollectionSurpriseRemoval(dmfDeviceContext->DmfCollection);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
DmfContainerEvtDeviceQueryRemove(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    WDF Query Remove callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    ntStatus = DMF_ModuleCollectionQueryRemove(dmfDeviceContext->DmfCollection);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
DmfContainerEvtDeviceQueryStop(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    WDF Query Stop callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    ntStatus = DMF_ModuleCollectionQueryStop(dmfDeviceContext->DmfCollection);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
DmfContainerEvtDeviceRelationsQuery(
    _In_ WDFDEVICE Device,
    _In_ DEVICE_RELATION_TYPE RelationType
    )
/*++

Routine Description:

    WDF Relations Query callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.
    RelationType - Parameter passed by WDF that is dispatched.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    DMF_ModuleCollectionRelationsQuery(dmfDeviceContext->DmfCollection,
                                       RelationType);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
DmfContainerEvtDeviceUsageNotificationEx(
    _In_ WDFDEVICE Device,
    _In_ WDF_SPECIAL_FILE_TYPE NotificationType,
    _In_ BOOLEAN IsInNotificationPath
    )
/*++

Routine Description:

    WDF Device Usage Notification callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    ntStatus = DMF_ModuleCollectionUsageNotificationEx(dmfDeviceContext->DmfCollection,
                                                       NotificationType,
                                                       IsInNotificationPath);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
DmfContainerEvtDeviceArmWakeFromS0(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    WDF Arm Wake From S0 callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    ntStatus = DMF_ModuleCollectionArmWakeFromS0(dmfDeviceContext->DmfCollection);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
DmfContainerEvtDeviceDisarmWakeFromS0(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    WDF Disarm Wake From S0 callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    DMF_ModuleCollectionDisarmWakeFromS0(dmfDeviceContext->DmfCollection);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
DmfContainerEvtDeviceWakeFromS0Triggered(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    WDF Wake From S0 Triggered callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    DMF_ModuleCollectionWakeFromS0Triggered(dmfDeviceContext->DmfCollection);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
DmfContainerEvtDeviceArmWakeFromSxWithReason(
    _In_ WDFDEVICE Device,
    _In_ BOOLEAN DeviceWakeEnabled,
    _In_ BOOLEAN ChildrenArmedForWake
    )
/*++

Routine Description:

    WDF Arm Wake From Sx With Reason callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    ntStatus = DMF_ModuleCollectionArmWakeFromSxWithReason(dmfDeviceContext->DmfCollection,
                                                           DeviceWakeEnabled,
                                                           ChildrenArmedForWake);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
DmfContainerEvtDeviceDisarmWakeFromSx(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    WDF Disarm Wake From Sx callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    DMF_ModuleCollectionDisarmWakeFromSx(dmfDeviceContext->DmfCollection);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
DmfContainerEvtDeviceWakeFromSxTriggered(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    WDF Wake From Sx Triggered callback.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    DMF_ModuleCollectionWakeFromSxTriggered(dmfDeviceContext->DmfCollection);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

EVT_WDF_REQUEST_COMPLETION_ROUTINE DmfContainerEvtWdfRequestCompletionRoutine_FileCreate;

VOID
DmfContainerEvtWdfRequestCompletionRoutine_FileCreate(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context
    )
/*++

Routine Description:

    File Create WDFREQUEST must have a completion routine before being passed down the
    stack, per WDF Verifier. This completion routine satisfies that requirement.

    TODO: Use this completion routine later to allow Modules/Client to perform 
          post processing for filtering purposes.

Arguments:

    Request - A handle to a framework request object that represents the completed I/O request.
    Target - A handle to an I/O target object that represents the I/O target that completed the request.
    Params - A pointer to a WDF_REQUEST_COMPLETION_PARAMS structure that contains
             information about the completed request.
    Context - Driver supplied context information (DMF_DEVICE_CONTEXT*).

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);
    UNREFERENCED_PARAMETER(Context);

    // Simply complete the request using its current NTSTATUS.
    // TODO: Allow Modules and Client Driver to post-process the request.
    //
    WdfRequestComplete(Request,
                       Params->IoStatus.Status);
}

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
DmfContainerEvtFileCreate(
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    The framework calls a driver's EvtDeviceFileCreate callback when the framework
    receives an IRP_MJ_CREATE request. The system sends this request when a user
    application opens the device to perform an I/O operation, such as reading or
    writing to a device. This callback is called in the context of the thread
    that created the IRP_MJ_CREATE request.

Arguments:

    Device - Handle to a framework device object.
    Request - Corresponding WDFREQUEST.
    FileObject - Corresponding WDFFILEOBJECT.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    BOOLEAN handled;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    DmfAssert(dmfDeviceContext->DmfCollection != NULL);
    handled = DMF_ModuleCollectionFileCreate(dmfDeviceContext->DmfCollection,
                                             Device,
                                             Request,
                                             FileObject);
    if (! handled)
    {
        // No Module handled this Request. If the Client Driver is a Filter driver, pass the 
        // Request to the next driver in the stack. If the Client Driver is not a Filter driver,
        // complete the Request indicating the file can be opened (see below).
        //
        if (dmfDeviceContext->IsFilterDevice)
        {
            // Completes this Request with error if it cannot passthru.
            // File Create must have a completion routine passed to avoid Verifier issue.
            //
            DMF_RequestPassthruWithCompletion(Device,
                                              Request,
                                              DmfContainerEvtWdfRequestCompletionRoutine_FileCreate,
                                              dmfDeviceContext);
        }
        else
        {
            // Do what WDF would have done had this driver not supported the File Create callback.
            // This is necessary so that a driver or application can open a device interface to 
            // send IOCTLs without explicitly supporting this callback.
            //
            WdfRequestComplete(Request,
                               STATUS_SUCCESS);
        }
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
DmfContainerEvtFileCleanup(
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    The framework calls a driver's EvtDeviceFileCleanup callback when the framework
    receives an IRP_MJ_CLEANUP request.

Arguments:

    FileObject - Pointer to WDFFILEOBJECT that represents the open handle.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    BOOLEAN handled;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // There *appears* to be a bug in OS where it calls CLEANUP without
    // calling CREATE and in that case FileObject is NULL.
    // TODO: Repro for this is ButtonDriver load with HidMiniDriver.
    // TODO: Investigate this.
    //
    if (NULL == FileObject)
    {
        handled = TRUE;
        goto Exit;
    }

    device = WdfFileObjectGetDevice(FileObject);
    dmfDeviceContext = DmfDeviceContextGet(device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    DmfAssert(dmfDeviceContext->DmfCollection != NULL);
    handled = DMF_ModuleCollectionFileCleanup(dmfDeviceContext->DmfCollection,
                                              FileObject);

    // If this is a filter driver, the framework will automatically forward the request to
    // the next lower driver.
    //
    if ((! handled) && 
        (! dmfDeviceContext->IsFilterDevice))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Unhandled Request: FileObject=0x%p", FileObject);
    }

Exit:

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
DmfContainerEvtFileClose(
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    The framework calls a driver's EvtDeviceFileClose callback when the framework
    receives an IRP_MJ_CLOSE request.

Arguments:

    FileObject - Pointer to WDFFILEOBJECT that represents the open handle.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    BOOLEAN handled;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    device = WdfFileObjectGetDevice(FileObject);
    dmfDeviceContext = DmfDeviceContextGet(device);
    DmfAssert(dmfDeviceContext != NULL);

    // Dispatch to the Module Collection.
    //
    DmfAssert(dmfDeviceContext->DmfCollection != NULL);
    handled = DMF_ModuleCollectionFileClose(dmfDeviceContext->DmfCollection,
                                            FileObject);

    // If this is a filter driver, the framework will automatically forward the request to
    // the next lower driver.
    //
    if ((!handled) &&
        (!dmfDeviceContext->IsFilterDevice))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Unhandled Request: FileObject=0x%p", FileObject);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Config initialization functions to initialize set WDF callbacks in the DMF Container Driver.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ContainerPnpPowerCallbacksInit(
    _Out_ WDF_PNPPOWER_EVENT_CALLBACKS* PnpPowerEventCallbacks
    )
/*++

Routine Description:

    Tells WDF to call the DMF Container Driver's PnP Power callbacks when those
    callbacks are dispatched by WDF.

Arguments:

    PnpPowerEventCallbacks - Target buffer where callback information is written.

Return Value:

    None

--*/
{
    PAGED_CODE();

    DmfAssert(PnpPowerEventCallbacks != NULL);

    PnpPowerEventCallbacks->EvtDevicePrepareHardware = DmfContainerEvtDevicePrepareHardware;
    PnpPowerEventCallbacks->EvtDeviceReleaseHardware = DmfContainerEvtDeviceReleaseHardware;
    PnpPowerEventCallbacks->EvtDeviceD0Entry = DmfContainerEvtDeviceD0Entry;
    PnpPowerEventCallbacks->EvtDeviceD0EntryPostInterruptsEnabled = DmfContainerEvtDeviceD0EntryPostInterruptsEnabled;
    PnpPowerEventCallbacks->EvtDeviceD0ExitPreInterruptsDisabled = DmfContainerEvtDeviceD0ExitPreInterruptsDisabled;
    PnpPowerEventCallbacks->EvtDeviceD0Exit = DmfContainerEvtDeviceD0Exit;
    PnpPowerEventCallbacks->EvtDeviceSelfManagedIoCleanup = DmfContainerEvtDeviceSelfManagedIoCleanup;
    PnpPowerEventCallbacks->EvtDeviceSelfManagedIoFlush = DmfContainerEvtDeviceSelfManagedIoFlush;
    PnpPowerEventCallbacks->EvtDeviceSelfManagedIoInit = DmfContainerEvtDeviceSelfManagedIoInit;
    PnpPowerEventCallbacks->EvtDeviceSelfManagedIoSuspend = DmfContainerEvtDeviceSelfManagedIoSuspend;
    PnpPowerEventCallbacks->EvtDeviceSelfManagedIoRestart = DmfContainerEvtDeviceSelfManagedIoRestart;
    PnpPowerEventCallbacks->EvtDeviceSurpriseRemoval = DmfContainerEvtDeviceSurpriseRemoval;
    PnpPowerEventCallbacks->EvtDeviceQueryRemove = DmfContainerEvtDeviceQueryRemove;
    PnpPowerEventCallbacks->EvtDeviceQueryStop = DmfContainerEvtDeviceQueryStop;
    PnpPowerEventCallbacks->EvtDeviceRelationsQuery = DmfContainerEvtDeviceRelationsQuery;
    PnpPowerEventCallbacks->EvtDeviceUsageNotificationEx = DmfContainerEvtDeviceUsageNotificationEx;
}
#pragma code_seg()

#define WDF_FILEOBJECT_CLASS_DEFAULT_SETTINGS       ((WDF_FILEOBJECT_CLASS)(WdfFileObjectWdfCannotUseFsContexts | WdfFileObjectCanBeOptional))

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ContainerFileObjectConfigInit(
    _Out_ WDF_FILEOBJECT_CONFIG* FileObjectConfig
    )
/*++

Routine Description:

    Tells WDF to call the DMF Container Driver's File Object callbacks when those
    callbacks are dispatched by WDF.

Arguments:

    FileObjectConfig - Target buffer where callback information is written.

Return Value:

    None

--*/
{
    PAGED_CODE();

    DmfAssert(FileObjectConfig != NULL);

    WDF_FILEOBJECT_CONFIG_INIT(FileObjectConfig,
                               DmfContainerEvtFileCreate,
                               DmfContainerEvtFileClose,
                               DmfContainerEvtFileCleanup);

    // This is the default setting if the Client Driver does not set it using
    // DMF_DmfDeviceInitHookFileObjectConfig().
    //
    // For filter/miniport drivers we don't know the policy on FileObject usage.
    // Make sure we don't use FsContexts by default, and allow FileObject to be optional.
    //
    // TODO: Not setting this default will cause a crash inside WDF during Cleanup calls.
    //       We should revisit this with WDF experts to see if we can eliminate this code.
    //
    FileObjectConfig->FileObjectClass = WDF_FILEOBJECT_CLASS_DEFAULT_SETTINGS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ContainerPowerPolicyCallbacksInit(
    _Out_ WDF_POWER_POLICY_EVENT_CALLBACKS* PowerPolicyCallbacks
    )
/*++

Routine Description:

    Tells WDF to call the DMF Container Driver's Power Policy callbacks when those
    callbacks are dispatched by WDF.

Arguments:

    PowerPolicyCallbacks - Target buffer where callback information is written.

Return Value:

    None

--*/
{
    PAGED_CODE();

    DmfAssert(PowerPolicyCallbacks != NULL);

    PowerPolicyCallbacks->EvtDeviceArmWakeFromS0 = DmfContainerEvtDeviceArmWakeFromS0;
    PowerPolicyCallbacks->EvtDeviceDisarmWakeFromS0 = DmfContainerEvtDeviceDisarmWakeFromS0;
    PowerPolicyCallbacks->EvtDeviceWakeFromS0Triggered = DmfContainerEvtDeviceWakeFromS0Triggered;
    PowerPolicyCallbacks->EvtDeviceArmWakeFromSxWithReason = DmfContainerEvtDeviceArmWakeFromSxWithReason;
    PowerPolicyCallbacks->EvtDeviceDisarmWakeFromSx = DmfContainerEvtDeviceDisarmWakeFromSx;
    PowerPolicyCallbacks->EvtDeviceWakeFromSxTriggered = DmfContainerEvtDeviceWakeFromSxTriggered;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ContainerQueueConfigCallbacksInit(
    _Out_ WDF_IO_QUEUE_CONFIG* IoQueueConfig
    )
/*++

Routine Description:

    Tells WDF to call the DMF Container Driver's IO Queue callbacks when those
    callbacks are dispatched by WDF.

Arguments:

    IoQueueConfig - Target buffer where callback information is written.

Return Value:

    None

--*/
{
    PAGED_CODE();

    DmfAssert(IoQueueConfig != NULL);

    IoQueueConfig->EvtIoRead = DmfContainerEvtQueueIoRead;
    IoQueueConfig->EvtIoWrite = DmfContainerEvtQueueIoWrite;
    IoQueueConfig->EvtIoDeviceControl = DmfContainerEvtDeviceIoControl;
#if !defined(DMF_USER_MODE)
    IoQueueConfig->EvtIoInternalDeviceControl = DmfContainerEvtInternalDeviceIoControl;
#endif // !defined(DMF_USER_MODE)
}
#pragma code_seg()

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DMF_Invoke_* Helpers that allow Client Driver to execute Module's PrepareHardware/D0Entry 
// callbacks. (Used by Miniport and Control Drivers.)
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
DMF_Invoke_DeviceCallbacksCreate(
    _In_ WDFDEVICE Device,
    _In_opt_ WDFCMRESLIST ResourcesRaw,
    _In_opt_ WDFCMRESLIST ResourcesTranslated,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Invoke DMF Device PrepareHardware and Device D0Entry Callbacks.
    NOTE: This function is used by Miniport or Control drivers.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.
    ResourcesRaw - WDF Resource Raw parameter that is passed to the given
                   DMF Module callback.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.
    PreviousState - The WDF Power State that the Container Driver should exit from.

Return Value:

    STATUS_SUCCESS if all the DMF Modules in the collection succeed; or an error code
    of the first one that fails.

--*/
{
    NTSTATUS ntStatus;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    if (dmfDeviceContext->ClientImplementsEvtWdfDriverDeviceAdd)
    {
        // Invoke APIs should not be called if Client
        // implements EVT_WDF_DRIVER_DEVICE_ADD callback
        //
        ntStatus = STATUS_NOT_SUPPORTED;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Invoke_DeviceCallbacksCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Remember that DMF_Invoke_DeviceCallbacksDestroy() needs to be called before the Module
    // Collection is destroyed.
    //
    DMF_MODULE_COLLECTION* moduleCollection = DMF_CollectionToHandle(dmfDeviceContext->DmfCollection);

    moduleCollection->ManualDestroyCallbackIsPending = TRUE;

    // Dispatch Device Prepare Hardware.
    // 'ResourcesRaw' could be '0':  this does not adhere to the specification for the function 'DmfContainerEvtDevicePrepareHardware'
    //
    #pragma warning(suppress:6387)
    ntStatus = DmfContainerEvtDevicePrepareHardware(Device,
                                                    ResourcesRaw,
                                                    ResourcesTranslated);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfContainerEvtDevicePrepareHardware fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Dispatch Device D0 Entry.
    //
    ntStatus = DmfContainerEvtDeviceD0Entry(Device,
                                            PreviousState);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfContainerEvtDeviceD0Entry fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS
DMF_Invoke_DeviceCallbacksDestroy(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Invoke DMF Device ReleaseHardware and Device D0Exit Callbacks.

Arguments:

    Device - Client Driver's WDF Device. DMF use it to access its private Device Context.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.
    TargetState - The WDF Power State that the Container Driver will enter into.

Return Value:

   STATUS_SUCCESS if all the DMF Modules in the collection succeed; or an error code
   of the first one that fails.

--*/
{
    NTSTATUS ntStatus;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    if (dmfDeviceContext->ClientImplementsEvtWdfDriverDeviceAdd)
    {
        // Invoke APIs should not be called if Client
        // implements EVT_WDF_DRIVER_DEVICE_ADD callback
        //
        ntStatus = STATUS_NOT_SUPPORTED;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Invoke_DeviceCallbacksCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Dispatch Device D0 Exit.
    //
    ntStatus = DmfContainerEvtDeviceD0Exit(Device,
                                           TargetState);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfContainerEvtDeviceD0Exit fails: ntStatus=%!STATUS!", ntStatus);
        // Do not exit on this failure. Continue to dispatch Release Hardware.
        //
    }

    // Dispatch Device Release Hardware.
    //
    ntStatus = DmfContainerEvtDeviceReleaseHardware(Device,
                                                    ResourcesTranslated);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfContainerEvtDeviceReleaseHardware fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Remember that DMF_Invoke_DeviceCallbacksDestroy() has been called before the Module
    // Collection is destroyed.
    //
    DMF_MODULE_COLLECTION* moduleCollection = DMF_CollectionToHandle(dmfDeviceContext->DmfCollection);
    moduleCollection->ManualDestroyCallbackIsPending = FALSE;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// eof: DmfContainer.c
//
