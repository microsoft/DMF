/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_GpioTarget.c

Abstract:

    Supports requests to a GPIO device native GPIO. A similar Module can be created
    to support other buses such as GPIO over HID or GPIO over USB.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_GpioTarget.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Specific external includes for this DMF Module.
//
#define RESHUB_USE_HELPER_ROUTINES
#include <reshub.h>
#include <gpio.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Resources assigned.
    //
    BOOLEAN GpioConnectionAssigned;
    BOOLEAN InterruptAssigned;

    // GPIO Line Index that is instantiated in this object.
    //
    ULONG GpioTargetLineIndex;
    // GPIO Interrupt Index that is instantiated in this object.
    //
    ULONG GpioTargetInterruptIndex;

    // Underlying GPIO device connection.
    //
    WDFIOTARGET GpioTarget;
    // Resource information for GPIO device.
    //
    CM_PARTIAL_RESOURCE_DESCRIPTOR GpioTargetConnection;
    // Optional interrupt instantiation.
    //
    WDFINTERRUPT Interrupt;
    // Optional workitem instantiation.
    //
    WDFWORKITEM Workitem;
    // Queuing of DPCs/workitems may not succeed if they are currently enqueued.
    // These two variables keep track of the exact number of times the DPC/workitem must
    // execute. This Module ensures that each attempt to enqueue causes the same number
    // of actual executions.
    //
    ULONG NumberOfTimesDpcMustExecute;
    ULONG NumberOfTimesWorkitemMustExecute;
} DMF_CONTEXT_GpioTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(GpioTarget)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(GpioTarget)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
GpioTarget_PinWrite(
    _In_ WDFIOTARGET IoTarget,
    _In_ BOOLEAN Value
    )
/*++

Routine Description:

    Set the state of a GPIO pin.

Arguments:

    IoTarget - The GPIO driver's handle.
    Value - The desired state of the pin.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES requestAttributes;
    WDFREQUEST request;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFMEMORY memory;
    UCHAR data;
    ULONG size;
    WDF_REQUEST_SEND_OPTIONS requestOptions;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_GpioTarget);

    size = sizeof(data);
    if (Value)
    {
        data = 1;
    }
    else
    {
        data = 0;
    }
    memory = NULL;
    request = NULL;

    WDF_OBJECT_ATTRIBUTES_INIT(&requestAttributes);
    ntStatus = WdfRequestCreate(&requestAttributes,
                                IoTarget,
                                &request);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "WdfRequestCreate fails: ntStatus=%!STATUS!", ntStatus);
        request = NULL;
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = request;
    ntStatus = WdfMemoryCreatePreallocated(&attributes,
                                           &data,
                                           size,
                                           &memory);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "WdfMemoryCreatePreallocated fails: ntStatus=%!STATUS!", ntStatus);
        memory = NULL;
        goto Exit;
    }

    ntStatus = WdfIoTargetFormatRequestForIoctl(IoTarget,
                                                request,
                                                IOCTL_GPIO_WRITE_PINS,
                                                memory,
                                                0,
                                                memory,
                                                0);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "WdfIoTargetFormatRequestForIoctl fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_REQUEST_SEND_OPTIONS_INIT(&requestOptions,
                                  WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);

    if (! WdfRequestSend(request,
                         IoTarget,
                         &requestOptions))
    {
        ntStatus = WdfRequestGetStatus(request);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "WdfRequestSend fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = WdfRequestGetStatus(request);

Exit:

    if (memory != NULL)
    {
        WdfObjectDelete(memory);
        memory = NULL;
    }

    if (request != NULL)
    {
        WdfObjectDelete(request);
        request = NULL;
    }

    FuncExit(DMF_TRACE_GpioTarget, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
GpioTarget_PinRead(
    _In_ WDFIOTARGET IoTarget,
    _Out_ BOOLEAN* PinValue
    )
/*++

Routine Description:

    Get the state of a GPIO pin.

Arguments:

    IoTarget - The GPIO driver's handle.
    PinValue- The current state of the GPIO pin.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES requestAttributes;
    WDFREQUEST request;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFMEMORY memory;
    UCHAR data;
    ULONG size;
    WDF_REQUEST_SEND_OPTIONS requestOptions;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_GpioTarget);

    size = sizeof(data);
    data = 0;
    memory = NULL;
    request = NULL;

    WDF_OBJECT_ATTRIBUTES_INIT(&requestAttributes);
    ntStatus = WdfRequestCreate(&requestAttributes,
                                IoTarget,
                                &request);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "WdfRequestCreate fails: ntStatus=%!STATUS!", ntStatus);
        request = NULL;
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = request;
    ntStatus = WdfMemoryCreatePreallocated(&attributes,
                                           &data,
                                           size,
                                           &memory);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "WdfMemoryCreatePreallocated fails: ntStatus=%!STATUS!", ntStatus);
        memory = NULL;
        goto Exit;
    }

    ntStatus = WdfIoTargetFormatRequestForIoctl(IoTarget,
                                                request,
                                                IOCTL_GPIO_READ_PINS,
                                                NULL,
                                                0,
                                                memory,
                                                0);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "WdfIoTargetFormatRequestForIoctl fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_REQUEST_SEND_OPTIONS_INIT(&requestOptions,
                                  WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);

    if (! WdfRequestSend(request,
                         IoTarget,
                         &requestOptions))
    {
        ntStatus = WdfRequestGetStatus(request);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "WdfRequestSend fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = WdfRequestGetStatus(request);

Exit:

    if (memory != NULL)
    {
        WdfObjectDelete(memory);
        memory = NULL;
    }

    if (request != NULL)
    {
        WdfObjectDelete(request);
        request = NULL;
    }

    if (NT_SUCCESS(ntStatus) &&
        (data != 0))
    {
        *PinValue = TRUE;
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE_GpioTarget, "GPIO value read = 0x%x", data);
    }
    else
    {
        *PinValue = FALSE;
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE_GpioTarget, "GPIO value read = 0x%x, ntStatus=%!STATUS!", data, ntStatus);
    }

    FuncExit(DMF_TRACE_GpioTarget, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

EVT_WDF_WORKITEM GpioTarget_Workitem;

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
GpioTarget_Workitem(
    _In_ WDFWORKITEM Workitem
    )
/*++
Routine Description:

    Workitem to be queued in DPC.
    The Client callback is called to indicate that an interrupt happened.

Arguments:

    Workitem - handle to a WDF workitem object.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_GpioTarget* moduleContext;
    DMF_CONFIG_GpioTarget* moduleConfig;
    ULONG numberOfTimesWorkitemMustExecute;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_GpioTarget);

    dmfModule = (DMFMODULE)WdfWorkItemGetParentObject(Workitem);

    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    ASSERT(moduleConfig->EvtGpioTargetInterruptPassive != NULL);

    // It is possible attempts to enqueue fail, so make sure to execute exactly the number
    // of times ISR attempted to enqueue.
    //
    WdfInterruptAcquireLock(moduleContext->Interrupt);
    numberOfTimesWorkitemMustExecute = moduleContext->NumberOfTimesWorkitemMustExecute;
    moduleContext->NumberOfTimesWorkitemMustExecute = 0;
    WdfInterruptReleaseLock(moduleContext->Interrupt);

    while (numberOfTimesWorkitemMustExecute > 0)
    {
        moduleConfig->EvtGpioTargetInterruptPassive(dmfModule);
        numberOfTimesWorkitemMustExecute--;
    }

    FuncExitNoReturn(DMF_TRACE_GpioTarget);
}
#pragma code_seg()

EVT_WDF_INTERRUPT_WORKITEM GpioTarget_PasiveLevelCallback;

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
GpioTarget_PasiveLevelCallback(
    _In_ WDFINTERRUPT Interrupt,
    _In_ WDFOBJECT WdfDevice
    )
/*++
Routine Description:

    Passive Level callback for a passive level GPIO interrupt.

Arguments:

    Interrupt - handle to a WDF interrupt object.
    WdfDevice - handle to a WDF device object.

Return Value:

    None

--*/
{
    DMFMODULE* dmfModuleAddress;
    DMF_CONFIG_GpioTarget* moduleConfig;

    UNREFERENCED_PARAMETER(WdfDevice);

    PAGED_CODE();

    FuncEntry(DMF_TRACE_GpioTarget);

    dmfModuleAddress = WdfObjectGet_DMFMODULE(Interrupt);

    // This is just a sanity check.
    //
    ASSERT(WdfDevice == DMF_AttachedDeviceGet(*dmfModuleAddress));

    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

    // Call the optional PASSIVE_LEVEL Client Driver callback.
    //
    ASSERT(moduleConfig->EvtGpioTargetInterruptPassive != NULL);

    moduleConfig->EvtGpioTargetInterruptPassive(*dmfModuleAddress);

    FuncExitNoReturn(DMF_TRACE_GpioTarget);
}
#pragma code_seg()

EVT_WDF_INTERRUPT_DPC GpioTarget_DpcForIsr;

_Use_decl_annotations_
VOID
GpioTarget_DpcForIsr(
    _In_ WDFINTERRUPT Interrupt,
    _In_ WDFOBJECT WdfDevice
    )
/*++
Routine Description:

    DPC callback for a GPIO interrupt.

Arguments:

    Interrupt - handle to a WDF interrupt object.
    WdfDevice - handle to a WDF device object.

Return Value:

    None

--*/
{
    DMFMODULE* dmfModuleAddress;
    DMF_CONFIG_GpioTarget* moduleConfig;
    DMF_CONTEXT_GpioTarget* moduleContext;
    GpioTarget_QueuedWorkItem_Type queuedWorkItem;
    ULONG numberOfTimesDpcMustExecute;

    UNREFERENCED_PARAMETER(WdfDevice);

    FuncEntry(DMF_TRACE_GpioTarget);

    // The Interrupt's Module Context area has the DMF Module.
    //
    dmfModuleAddress = WdfObjectGet_DMFMODULE(Interrupt);

    // This is just a sanity check.
    //
    ASSERT(WdfDevice == DMF_AttachedDeviceGet(*dmfModuleAddress));

    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

    // Call the DISPATCH_LEVEL Client callback.
    //
    ASSERT(moduleConfig->EvtGpioTargetInterruptDpc != NULL);

    // It is possible attempts to enqueue fail, so make sure to execute exactly the number
    // of times ISR attempted to enqueue.
    //
    WdfInterruptAcquireLock(moduleContext->Interrupt);
    numberOfTimesDpcMustExecute = moduleContext->NumberOfTimesDpcMustExecute;
    moduleContext->NumberOfTimesDpcMustExecute = 0;
    WdfInterruptReleaseLock(moduleContext->Interrupt);

    while (numberOfTimesDpcMustExecute > 0)
    {
        moduleConfig->EvtGpioTargetInterruptDpc(*dmfModuleAddress,
                                                &queuedWorkItem);
        if (GpioTarget_QueuedWorkItem_WorkItem == queuedWorkItem)
        {
            moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
            ASSERT(moduleContext != NULL);

            ASSERT(moduleContext->Workitem != NULL);
            WdfInterruptAcquireLock(moduleContext->Interrupt);
            moduleContext->NumberOfTimesWorkitemMustExecute++;
            WdfInterruptReleaseLock(moduleContext->Interrupt);

            WdfWorkItemEnqueue(moduleContext->Workitem);
        }
        numberOfTimesDpcMustExecute--;
    }

    FuncExitNoReturn(DMF_TRACE_GpioTarget);
}

EVT_WDF_INTERRUPT_ISR GpioTarget_Isr;

_Use_decl_annotations_
BOOLEAN
GpioTarget_Isr(
    _In_ WDFINTERRUPT Interrupt,
    _In_ ULONG MessageId
    )
/*++

  Routine Description:

    This routine responds to interrupts generated by the H/W.
    This ISR is called at PASSIVE_LEVEL.

  Arguments:

    Interrupt - A handle to a framework interrupt object.
    MessageId - Message number identifying the device's
                hardware interrupt message (if using MSI).

  Return Value:

    TRUE if interrupt recognized.

--*/
{
    DMFMODULE* dmfModuleAddress;
    DMF_CONTEXT_GpioTarget* moduleContext;
    DMF_CONFIG_GpioTarget* moduleConfig;
    BOOLEAN interruptHandled;
    BOOLEAN enqueued;

    UNREFERENCED_PARAMETER(MessageId);

    FuncEntry(DMF_TRACE_GpioTarget);

    // The Interrupt's Module context area has the DMF Module.
    //
    dmfModuleAddress = WdfObjectGet_DMFMODULE(Interrupt);

    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

    ASSERT((moduleConfig->EvtGpioTargetInterruptDpc != NULL) || 
           (moduleConfig->EvtGpioTargetInterruptPassive != NULL));

    // Option 1: Caller wants to do work in ISR at DIRQL (and optionally, at DPC or PASSIVE
    //           levels.)
    // Option 2: Caller wants to do work at DPC level (and optionally at PASSIVE level).
    // Option 3: Caller wants to do work only at PASSIVE_LEVEL.
    //
    if (moduleConfig->EvtGpioTargetInterruptIsr != NULL)
    {
        GpioTarget_QueuedWorkItem_Type queuedWorkItem;

        interruptHandled = moduleConfig->EvtGpioTargetInterruptIsr(*dmfModuleAddress,
                                                                   MessageId,
                                                                   &queuedWorkItem);
        if (interruptHandled)
        {
            if (GpioTarget_QueuedWorkItem_Dpc == queuedWorkItem)
            {
                ASSERT(moduleConfig->EvtGpioTargetInterruptDpc != NULL);
                // Interrupt SpinLock is held.
                //
                moduleContext->NumberOfTimesDpcMustExecute++;
                enqueued = WdfInterruptQueueDpcForIsr(Interrupt);
            }
            else if (GpioTarget_QueuedWorkItem_WorkItem == queuedWorkItem)
            {
                ASSERT(moduleConfig->EvtGpioTargetInterruptPassive != NULL);
                // Interrupt SpinLock is held.
                //
                moduleContext->NumberOfTimesWorkitemMustExecute++;
                enqueued = WdfInterruptQueueWorkItemForIsr(Interrupt);
            }
        }
    }
    // Call the optional DPC_LEVEL Client Driver callback.
    //
    else if (moduleConfig->EvtGpioTargetInterruptDpc != NULL)
    {
        // Interrupt SpinLock is held.
        //
        moduleContext->NumberOfTimesDpcMustExecute++;
        enqueued = WdfInterruptQueueDpcForIsr(Interrupt);
        interruptHandled = TRUE;
    }
    // If no DPC, launch the optional Client Driver PASSIVE_LEVEL callback.
    //
    else if (moduleConfig->EvtGpioTargetInterruptPassive != NULL)
    {
        // Interrupt SpinLock is held.
        //
        moduleContext->NumberOfTimesWorkitemMustExecute++;
        enqueued = WdfInterruptQueueWorkItemForIsr(Interrupt);
        interruptHandled = TRUE;
    }
    else
    {
        ASSERT(FALSE);
        interruptHandled = TRUE;
    }

    FuncExitNoReturn(DMF_TRACE_GpioTarget);

    return interruptHandled;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Wdf Module Callbacks
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
DMF_GpioTarget_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type GpioTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    UNICODE_STRING resourcePathString;
    WCHAR resourcePathBuffer[RESOURCE_HUB_PATH_SIZE];
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_IO_TARGET_OPEN_PARAMS openParams;
    DMF_CONTEXT_GpioTarget* moduleContext;
    DMF_CONFIG_GpioTarget* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_GpioTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (! moduleContext->GpioConnectionAssigned)
    {
        // In some cases, the minimum number of resources is zero because the same driver
        // is used on different platforms. In that case, this Module still loads and opens
        // but it does nothing.
        //
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE_GpioTarget, "No GPIO Resources Found");
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    device = DMF_AttachedDeviceGet(DmfModule);

    RtlInitEmptyUnicodeString(&resourcePathString,
                              resourcePathBuffer,
                              sizeof(resourcePathBuffer));

    ntStatus = RESOURCE_HUB_CREATE_PATH_FROM_ID(&resourcePathString,
                                                moduleContext->GpioTargetConnection.u.Connection.IdLowPart,
                                                moduleContext->GpioTargetConnection.u.Connection.IdHighPart);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "RESOURCE_HUB_CREATE_PATH_FROM_ID fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;

    ntStatus = WdfIoTargetCreate(device,
                                 &objectAttributes,
                                 &moduleContext->GpioTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "RESOURCE_HUB_CREATE_PATH_FROM_ID fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    //  Open the IoTarget for I/O operation.
    //
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams,
                                                &resourcePathString,
                                                moduleConfig->OpenMode);
    openParams.ShareAccess = moduleConfig->ShareAccess;
    ntStatus = WdfIoTargetOpen(moduleContext->GpioTarget,
                               &openParams);
    if (! NT_SUCCESS(ntStatus))
    {
        WdfObjectDelete(moduleContext->GpioTarget);
        moduleContext->GpioTarget = NULL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "WdfIoTargetOpen fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE_GpioTarget, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_GpioTarget_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type GpioTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_GpioTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_GpioTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Make sure all pending work is complete.
    //
    if (moduleContext->Workitem != NULL)
    {
        WdfWorkItemFlush(moduleContext->Workitem);
    }

    if (moduleContext->GpioTarget != NULL)
    {
        WdfIoTargetClose(moduleContext->GpioTarget);
        WdfObjectDelete(moduleContext->GpioTarget);
        moduleContext->GpioTarget = NULL;
    }

    FuncExitNoReturn(DMF_TRACE_GpioTarget);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_GpioTarget_ResourcesAssign(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Tells this Module instance what Resources are available. This Module then extracts
    the needed Resources and uses them as needed.

Arguments:

    DmfModule - This Module's handle.
    ResourcesRaw - WDF Resource Raw parameter that is passed to the given
                   DMF Module callback.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.

Return Value:

    STATUS_SUCCESS.

--*/
{
    DMF_CONTEXT_GpioTarget* moduleContext;
    ULONG resourceCount;
    ULONG resourceIndex;
    NTSTATUS ntStatus;
    ULONG interruptResourceIndex;
    WDF_INTERRUPT_CONFIG interruptConfig;
    WDF_OBJECT_ATTRIBUTES interruptAttributes;
    WDF_OBJECT_ATTRIBUTES workitemAttributes;
    WDF_WORKITEM_CONFIG  workitemConfig;
    WDFDEVICE device;
    DMF_CONFIG_GpioTarget* moduleConfig;
    ULONG gpioConnectionIndex;
    ULONG interruptIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_GpioTarget);

    ASSERT(ResourcesRaw != NULL);
    ASSERT(ResourcesTranslated != NULL);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ASSERT((FALSE == moduleConfig->InterruptMandatory) ||
           (moduleConfig->EvtGpioTargetInterruptDpc != NULL) ||
           (moduleConfig->EvtGpioTargetInterruptPassive != NULL) ||
           (moduleConfig->EvtGpioTargetInterruptIsr != NULL));

    device = DMF_AttachedDeviceGet(DmfModule);

    moduleContext->GpioConnectionAssigned = FALSE;
    moduleContext->InterruptAssigned = FALSE;
    interruptResourceIndex = 0;

    // Check the number of resources for the button device.
    //
    resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);

    // Parse the resources. This Module cares about GPIO and Interrupt resources.
    //
    gpioConnectionIndex = 0;
    interruptIndex = 0;
    for (resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++)
    {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR resource;

        resource = WdfCmResourceListGetDescriptor(ResourcesTranslated,
                                                  resourceIndex);
        if (NULL == resource)
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "No resources assigned");
            goto Exit;
        }

        if (CmResourceTypeConnection == resource->Type)
        {
            if (resource->u.Connection.Class == CM_RESOURCE_CONNECTION_CLASS_GPIO)
            {
                if (moduleConfig->GpioConnectionIndex == gpioConnectionIndex)
                {
                    // Store the index of the GPIO line that is instantiated.
                    // (For debug purposes only.)
                    //
                    moduleContext->GpioTargetLineIndex = gpioConnectionIndex;

                    // Assign the information needed to open the target.
                    //
                    moduleContext->GpioTargetConnection = *resource;

                    moduleContext->GpioConnectionAssigned = TRUE;

                    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE_GpioTarget, "Assign: GpioTargetLineIndex=%d",
                                moduleContext->GpioTargetLineIndex);
                }

                gpioConnectionIndex++;

                TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE_GpioTarget, "CmResourceTypeConnection 0x%08X:%08X",
                            resource->u.Connection.IdHighPart,
                            resource->u.Connection.IdLowPart);
            }
        }
        else if (CmResourceTypeInterrupt == resource->Type)
        {
            if (moduleConfig->InterruptIndex == interruptIndex)
            {
                // Store the index of the GPIO interrupt that is instantiated.
                // (For debug purposes only.)
                //
                moduleContext->GpioTargetInterruptIndex = interruptIndex;

                // Save the actual resource index that is used later to initialize the interrupt.
                //
                interruptResourceIndex = resourceIndex;

                moduleContext->InterruptAssigned = TRUE;

                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE_GpioTarget, "Assign: GpioTargetInterruptIndex=%d interruptResourceIndex=%d",
                            moduleContext->GpioTargetInterruptIndex,
                            interruptResourceIndex);
            }

            interruptIndex++;

            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE_GpioTarget, "CmResourceTypeInterrupt 0x%08X 0x%IX 0x%08X",
                        resource->u.Interrupt.Vector,
                        resource->u.Interrupt.Affinity,
                        resource->u.Interrupt.Level);
        }
        else
        {
            // All other resources are ignored by this Module.
            //
        }
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE_GpioTarget, "GpioConnectionAssigned=%d GpioConnectionMandatory=%d", moduleContext->GpioConnectionAssigned, moduleConfig->GpioConnectionMandatory);

    //  Validate gpio connection with the Client Driver's requirements.
    //
    if (moduleConfig->GpioConnectionMandatory && 
        (! moduleContext->GpioConnectionAssigned))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "Gpio Connection not assigned");
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
        NT_ASSERT(FALSE);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE_GpioTarget, "InterruptAssigned=%d InterruptMandatory=%d", moduleContext->InterruptAssigned, moduleConfig->InterruptMandatory);

    //  Validate interrupt with the Client Driver's requirements.
    //
    if (moduleConfig->InterruptMandatory && 
        (! moduleContext->InterruptAssigned))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "Interrupt resource not assigned");
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
        NT_ASSERT(FALSE);
        goto Exit;
    }

    // Initialize the interrupt, if necessary.
    //
    if (moduleContext->InterruptAssigned)
    {
        WDF_INTERRUPT_CONFIG_INIT(&interruptConfig,
                                  GpioTarget_Isr,
                                  NULL);

        interruptConfig.PassiveHandling = moduleConfig->PassiveHandling;
        interruptConfig.CanWakeDevice = moduleConfig->CanWakeDevice;

        // Configure either a DPC or a Workitem.
        //
        if (moduleConfig->EvtGpioTargetInterruptDpc != NULL)
        {
            interruptConfig.EvtInterruptDpc = GpioTarget_DpcForIsr;
        }
        else if (moduleConfig->EvtGpioTargetInterruptPassive != NULL)
        {
            interruptConfig.EvtInterruptWorkItem = GpioTarget_PasiveLevelCallback;
        }

        interruptConfig.InterruptTranslated = WdfCmResourceListGetDescriptor(ResourcesTranslated,
                                                                             interruptResourceIndex);
        interruptConfig.InterruptRaw = WdfCmResourceListGetDescriptor(ResourcesRaw,
                                                                      interruptResourceIndex);

        // Prepare to save this DMF Module in the object's context.
        //
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&interruptAttributes,
                                                DMFMODULE);
        // NOTE: ParentDevice must not be specified! (device is passed to the function).
        //       Otherwise, STATUS_WDF_PARENT_ASSIGNMENT_NOT_ALLOWED will occur.
        //
        ntStatus = WdfInterruptCreate(device,
                                      &interruptConfig,
                                      &interruptAttributes,
                                      &moduleContext->Interrupt);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "WdfInterruptCreate ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE_GpioTarget, "GPIO Interrupt Created");

        // NOTE: It is not possible to get the parent of a WDFINTERRUPT.
        // Therefore, it is necessary to save the DmfModule in its context area.
        //
        DMF_ModuleInContextSave(moduleContext->Interrupt,
                                DmfModule);

        // If both DPC and Passive level callback, then ISR queues DPC 
        // and then DPC queues passive level workitem. (It means the Client
        // wants to do work both at DPC and PASSIVE levels.)
        //
        if ((moduleConfig->EvtGpioTargetInterruptDpc != NULL) &&
            (moduleConfig->EvtGpioTargetInterruptPassive != NULL))
        {
            WDF_WORKITEM_CONFIG_INIT(&workitemConfig,
                                     GpioTarget_Workitem);
            workitemConfig.AutomaticSerialization = WdfFalse;

            WDF_OBJECT_ATTRIBUTES_INIT(&workitemAttributes);
            workitemAttributes.ParentObject = DmfModule;

            ntStatus = WdfWorkItemCreate(&workitemConfig,
                                         &workitemAttributes,
                                         &moduleContext->Workitem);

            if (! NT_SUCCESS(ntStatus))
            {
                moduleContext->Workitem = NULL;
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "WdfWorkItemCreate fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }

            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE_GpioTarget, "Workitem Created");
        }
        else
        {
            ASSERT(moduleContext->Workitem == NULL);
        }
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE_GpioTarget, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_GpioTarget;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_GpioTarget;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_GpioTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type GpioTarget.

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

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_GpioTarget);
    DmfCallbacksDmf_GpioTarget.DeviceResourcesAssign = DMF_GpioTarget_ResourcesAssign;
    DmfCallbacksDmf_GpioTarget.DeviceOpen = DMF_GpioTarget_Open;
    DmfCallbacksDmf_GpioTarget.DeviceClose = DMF_GpioTarget_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_GpioTarget,
                                            GpioTarget,
                                            DMF_CONTEXT_GpioTarget,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    DmfModuleDescriptor_GpioTarget.CallbacksDmf = &DmfCallbacksDmf_GpioTarget;
    DmfModuleDescriptor_GpioTarget.ModuleConfigSize = sizeof(DMF_CONFIG_GpioTarget);

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_GpioTarget,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_GpioTarget_InterruptAcquireLock(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Acquire the given Module's interrupt spin lock.

Arguments:

    DmfModule - The given Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_GpioTarget* moduleContext;

    FuncEntry(DMF_TRACE_GpioTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_GpioTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WdfInterruptAcquireLock(moduleContext->Interrupt);

    FuncExitVoid(DMF_TRACE_GpioTarget);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_GpioTarget_InterruptReleaseLock(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Release the given Module's interrupt spin lock.

Arguments:

    DmfModule - The given Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_GpioTarget* moduleContext;

    FuncEntry(DMF_TRACE_GpioTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_GpioTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WdfInterruptReleaseLock(moduleContext->Interrupt);

    FuncExitVoid(DMF_TRACE_GpioTarget);
}

#pragma code_seg("PAGE")
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_GpioTarget_InterruptTryToAcquireLock(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Attempt to acquire the given Module's interrupt passive lock.
    Use this Method to acquire the lock in a non-arbitrary thread context.

Arguments:

    DmfModule - The given Module.

Return Value:

    TRUE if it successfully acquires the interrupt’s lock.
    FALSE otherwise.

--*/
{
    DMF_CONTEXT_GpioTarget* moduleContext;
    BOOLEAN returnValue;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_GpioTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_GpioTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    returnValue = WdfInterruptTryToAcquireLock(moduleContext->Interrupt);

    FuncExit(DMF_TRACE_GpioTarget, "returnValue=%d", returnValue);

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_GpioTarget_IsResourceAssigned(
    _In_ DMFMODULE DmfModule,
    _Out_opt_ BOOLEAN* GpioConnectionAssigned,
    _Out_opt_ BOOLEAN* InterruptAssigned
    )
/*++

Routine Description:

    GPIOs may or may not be present on some systems. This function returns the number of GPIO
    resources assigned for drivers where it is not known the GPIOs exist.

Arguments:

    DmfModule - This Module's handle.
    GpioConnectionAssigned - Is GPIO connection assigned to this Module instance.
    InterruptAssigned - Is Interrupt assigned to this Module instance.

Return Value:

    None

--*/
{
    DMF_CONTEXT_GpioTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_GpioTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_GpioTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (GpioConnectionAssigned != NULL)
    {
        *GpioConnectionAssigned = moduleContext->GpioConnectionAssigned;
    }

    if (InterruptAssigned != NULL)
    {
        *InterruptAssigned = moduleContext->InterruptAssigned;
    }

    FuncExitVoid(DMF_TRACE_GpioTarget);
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_GpioTarget_Read(
    _In_ DMFMODULE DmfModule,
    _Out_ BOOLEAN* PinValue
    )
/*++

Routine Description:

    Module Method that reads the state of this Module's GPIO pin.

Arguments:

    DmfModule - This Module's handle.
    PinValue - The state of the GPIO pin.

Return Value:

    STATUS_SUCCESS.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_GpioTarget* moduleContext;

    FuncEntry(DMF_TRACE_GpioTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_GpioTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(PinValue != NULL);
    *PinValue = 0;

    if (moduleContext->GpioTarget != NULL)
    {
        ntStatus = GpioTarget_PinRead(moduleContext->GpioTarget,
                                PinValue);
    }
    else
    {
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "GPIO Target is invalid. Please make sure GpioTargetIO is configured to read ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE_GpioTarget, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_GpioTarget_Write(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN Value
    )
/*++

Routine Description:

    Module Method that sets the state of this Module's GPIO pin.

Arguments:

    DmfModule - This Module's handle.
    PinValue - The state that is written to the GPIO pin.

Return Value:

    STATUS_SUCCESS.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_GpioTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_GpioTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_GpioTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->GpioTarget != NULL)
    {
        ntStatus = GpioTarget_PinWrite(moduleContext->GpioTarget,
                                  Value);
    }
    else
    {
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_GpioTarget, "GPIO Target is invalid. Please make sure GpioTargetIO is configured. ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE_GpioTarget, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// eof: Dmf_GpioTarget.c
//
