/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_SpbTarget.c

Abstract:

    Supports SPB targets.

Environment:

    Kernel-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_SpbTarget.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Specific external includes for this DMF Module.
//
#define RESHUB_USE_HELPER_ROUTINES
#include <reshub.h>
#include <spb.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma warning(pop)
typedef struct
{
    // Resources assigned.
    //
    BOOLEAN SpbConnectionAssigned;
    BOOLEAN InterruptAssigned;

    // SPB Line Index that is instantiated in this object.
    //
    ULONG SpbTargetLineIndex;
    // SPB Interrupt Index that is instantiated in this object.
    //
    ULONG SpbTargetInterruptIndex;

    // Resource information for SPB device.
    //
    CM_PARTIAL_RESOURCE_DESCRIPTOR SpbTargetConnection;

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

    // Interrupt object.
    //
    WDFINTERRUPT Interrupt;

    // SPB controller target
    //
    WDFIOTARGET SpbController;

    // Support for building and sending WDFREQUESTS.
    //
    DMFMODULE DmfModuleRequestTarget;
} DMF_CONTEXT_SpbTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(SpbTarget)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(SpbTarget)

// Memory Pool Tag.
//
#define MemoryTag 'TbpS'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

NTSTATUS
SpbTarget_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This routine opens a handle to the SPB controller.

Arguments:

    ModuleContext - a pointer to the device context

Return Value:

    NTSTATUS

--*/
{
    WDF_IO_TARGET_OPEN_PARAMS openParams;
    NTSTATUS ntStatus;
    DMF_CONTEXT_SpbTarget* moduleContext;
    DMF_CONFIG_SpbTarget* moduleConfig;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Create the device path using the connection ID.
    //
    DECLARE_UNICODE_STRING_SIZE(DevicePath, 
                                RESOURCE_HUB_PATH_SIZE);

    RESOURCE_HUB_CREATE_PATH_FROM_ID(&DevicePath,
                                     moduleContext->SpbTargetConnection.u.Connection.IdLowPart,
                                     moduleContext->SpbTargetConnection.u.Connection.IdHighPart);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Opening handle to SPB target via %wZ", &DevicePath);

    // Open a handle to the SPB controller.
    //
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams,
                                                &DevicePath,
                                                moduleConfig->OpenMode);
    openParams.ShareAccess = moduleConfig->ShareAccess;
    openParams.CreateDisposition = FILE_OPEN;
    openParams.FileAttributes = FILE_ATTRIBUTE_NORMAL;
    
    ntStatus = WdfIoTargetOpen(moduleContext->SpbController,
                               &openParams);
    if (!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to open SPB target - %!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

NTSTATUS
SpbTarget_Close(
    _In_  DMF_CONTEXT_SpbTarget* ModuleContext
    )
/*++

Routine Description:

    This routine closes a handle to the SPB controller.

Arguments:

    ModuleContext - a pointer to the device context

Return Value:

    NTSTATUS

--*/
{
    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Closing handle to SPB target");

    WdfIoTargetClose(ModuleContext->SpbController);

    FuncExitVoid(DMF_TRACE);
    
    return STATUS_SUCCESS;
}

NTSTATUS
SpbTarget_SendRequestLocal(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST SpbRequest
    )
/*++

Routine Description:

    Send a given WDFREQUEST to the Spb Controller.

Arguments:

    DmfModule - This Module's Module handle.
    SpbRequest - The given WDFREQUEST to send.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_SpbTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    NTSTATUS ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "sending SPB request %p", SpbRequest);

    // Send the SPB request.
    //
    WDF_REQUEST_SEND_OPTIONS requestOptions;
    WDF_REQUEST_SEND_OPTIONS_INIT(&requestOptions,
                                  WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&requestOptions,
                                         WDF_REL_TIMEOUT_IN_SEC(2));
    WdfRequestSend(SpbRequest,
                   moduleContext->SpbController,
                   &requestOptions);

    ntStatus = WdfRequestGetStatus(SpbRequest);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

EVT_WDF_WORKITEM SpbTarget_Workitem;

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
SpbTarget_Workitem(
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
    DMF_CONTEXT_SpbTarget* moduleContext;
    DMF_CONFIG_SpbTarget* moduleConfig;
    ULONG numberOfTimesWorkitemMustExecute;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    dmfModule = (DMFMODULE)WdfWorkItemGetParentObject(Workitem);

    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    ASSERT(moduleConfig->EvtSpbTargetInterruptPassive != NULL);

    // It is possible attempts to enqueue fail, so make sure to execute exactly the number
    // of times ISR attempted to enqueue.
    //
    WdfInterruptAcquireLock(moduleContext->Interrupt);
    numberOfTimesWorkitemMustExecute = moduleContext->NumberOfTimesWorkitemMustExecute;
    moduleContext->NumberOfTimesWorkitemMustExecute = 0;
    WdfInterruptReleaseLock(moduleContext->Interrupt);

    while (numberOfTimesWorkitemMustExecute > 0)
    {
        moduleConfig->EvtSpbTargetInterruptPassive(dmfModule);
        numberOfTimesWorkitemMustExecute--;
    }

    FuncExitNoReturn(DMF_TRACE);
}
#pragma code_seg()

EVT_WDF_INTERRUPT_WORKITEM SpbTarget_PasiveLevelCallback;

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
SpbTarget_PasiveLevelCallback(
    _In_ WDFINTERRUPT Interrupt,
    _In_ WDFOBJECT WdfDevice
    )
/*++
Routine Description:

    Passive Level callback for a passive level SPB interrupt.

Arguments:

    Interrupt - handle to a WDF interrupt object.
    WdfDevice - handle to a WDF device object.

Return Value:

    None

--*/
{
    DMFMODULE* dmfModuleAddress;
    DMF_CONFIG_SpbTarget* moduleConfig;

    UNREFERENCED_PARAMETER(WdfDevice);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    dmfModuleAddress = WdfObjectGet_DMFMODULE(Interrupt);

    // This is just a sanity check.
    //
    ASSERT(WdfDevice == DMF_ParentDeviceGet(*dmfModuleAddress));

    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

    // Call the optional PASSIVE_LEVEL Client Driver callback.
    //
    ASSERT(moduleConfig->EvtSpbTargetInterruptPassive != NULL);

    moduleConfig->EvtSpbTargetInterruptPassive(*dmfModuleAddress);

    FuncExitNoReturn(DMF_TRACE);
}
#pragma code_seg()

EVT_WDF_INTERRUPT_DPC SpbTarget_DpcForIsr;

_Use_decl_annotations_
VOID
SpbTarget_DpcForIsr(
    _In_ WDFINTERRUPT Interrupt,
    _In_ WDFOBJECT WdfDevice
    )
/*++
Routine Description:

    DPC callback for a SPB interrupt.

Arguments:

    Interrupt - handle to a WDF interrupt object.
    WdfDevice - handle to a WDF device object.

Return Value:

    None

--*/
{
    DMFMODULE* dmfModuleAddress;
    DMF_CONFIG_SpbTarget* moduleConfig;
    DMF_CONTEXT_SpbTarget* moduleContext;
    SpbTarget_QueuedWorkItem_Type queuedWorkItem;
    ULONG numberOfTimesDpcMustExecute;

    UNREFERENCED_PARAMETER(WdfDevice);

    FuncEntry(DMF_TRACE);

    // The Interrupt's Module Context area has the DMF Module.
    //
    dmfModuleAddress = WdfObjectGet_DMFMODULE(Interrupt);

    // This is just a sanity check.
    //
    ASSERT(WdfDevice == DMF_ParentDeviceGet(*dmfModuleAddress));

    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

    // Call the DISPATCH_LEVEL Client callback.
    //
    ASSERT(moduleConfig->EvtSpbTargetInterruptDpc != NULL);

    // It is possible attempts to enqueue fail, so make sure to execute exactly the number
    // of times ISR attempted to enqueue.
    //
    WdfInterruptAcquireLock(moduleContext->Interrupt);
    numberOfTimesDpcMustExecute = moduleContext->NumberOfTimesDpcMustExecute;
    moduleContext->NumberOfTimesDpcMustExecute = 0;
    WdfInterruptReleaseLock(moduleContext->Interrupt);

    while (numberOfTimesDpcMustExecute > 0)
    {
        moduleConfig->EvtSpbTargetInterruptDpc(*dmfModuleAddress,
                                                &queuedWorkItem);
        if (SpbTarget_QueuedWorkItem_WorkItem == queuedWorkItem)
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

    FuncExitNoReturn(DMF_TRACE);
}

EVT_WDF_INTERRUPT_ISR SpbTarget_Isr;

_Use_decl_annotations_
BOOLEAN
SpbTarget_Isr(
    _In_ WDFINTERRUPT Interrupt,
    _In_ ULONG MessageId
    )
/*++

  Routine Description:

    This routine responds to interrupts generated by the H/W.

  Arguments:

    Interrupt - A handle to a framework interrupt object.
    MessageId - Message number identifying the device's
                hardware interrupt message (if using MSI).

  Return Value:

    TRUE if interrupt recognized.

--*/
{
    DMFMODULE* dmfModuleAddress;
    DMF_CONTEXT_SpbTarget* moduleContext;
    DMF_CONFIG_SpbTarget* moduleConfig;
    BOOLEAN interruptHandled;
    BOOLEAN enqueued;

    UNREFERENCED_PARAMETER(MessageId);

    FuncEntry(DMF_TRACE);

    // The Interrupt's Module context area has the DMF Module.
    //
    dmfModuleAddress = WdfObjectGet_DMFMODULE(Interrupt);

    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

    ASSERT((moduleConfig->EvtSpbTargetInterruptDpc != NULL) || 
           (moduleConfig->EvtSpbTargetInterruptPassive != NULL)  ||
           (moduleConfig->EvtSpbTargetInterruptIsr != NULL));

    // Option 1: Caller wants to do work in ISR at DIRQL (and optionally, at DPC or PASSIVE
    //           levels.)
    // Option 2: Caller wants to do work at DPC level (and optionally at PASSIVE level).
    // Option 3: Caller wants to do work only at PASSIVE_LEVEL.
    //
    if (moduleConfig->EvtSpbTargetInterruptIsr != NULL)
    {
        SpbTarget_QueuedWorkItem_Type queuedWorkItem;

        interruptHandled = moduleConfig->EvtSpbTargetInterruptIsr(*dmfModuleAddress,
                                                                   MessageId,
                                                                   &queuedWorkItem);
        if (interruptHandled)
        {
            if (SpbTarget_QueuedWorkItem_Dpc == queuedWorkItem)
            {
                ASSERT(moduleConfig->EvtSpbTargetInterruptDpc != NULL);
                // Interrupt SpinLock is held.
                //
                moduleContext->NumberOfTimesDpcMustExecute++;
                enqueued = WdfInterruptQueueDpcForIsr(Interrupt);
            }
            else if (SpbTarget_QueuedWorkItem_WorkItem == queuedWorkItem)
            {
                ASSERT(moduleConfig->EvtSpbTargetInterruptPassive != NULL);
                // Interrupt SpinLock is held.
                //
                moduleContext->NumberOfTimesWorkitemMustExecute++;
                enqueued = WdfInterruptQueueWorkItemForIsr(Interrupt);
            }
        }
    }
    // Call the optional DPC_LEVEL Client Driver callback.
    //
    else if (moduleConfig->EvtSpbTargetInterruptDpc != NULL)
    {
        // Interrupt SpinLock is held.
        //
        moduleContext->NumberOfTimesDpcMustExecute++;
        enqueued = WdfInterruptQueueDpcForIsr(Interrupt);
        interruptHandled = TRUE;
    }
    // If no DPC, launch the optional Client Driver PASSIVE_LEVEL callback.
    //
    else if (moduleConfig->EvtSpbTargetInterruptPassive != NULL)
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

    FuncExitNoReturn(DMF_TRACE);

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
static
NTSTATUS
DMF_SpbTarget_ResourcesAssign(
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
    DMF_CONTEXT_SpbTarget* moduleContext;
    ULONG resourceCount;
    ULONG resourceIndex;
    NTSTATUS ntStatus;
    ULONG interruptResourceIndex;
    WDF_INTERRUPT_CONFIG interruptConfig;
    WDF_OBJECT_ATTRIBUTES interruptAttributes;
    WDF_OBJECT_ATTRIBUTES workitemAttributes;
    WDF_WORKITEM_CONFIG  workitemConfig;
    WDFDEVICE device;
    DMF_CONFIG_SpbTarget* moduleConfig;
    ULONG gpioConnectionIndex;
    ULONG interruptIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ASSERT(ResourcesRaw != NULL);
    ASSERT(ResourcesTranslated != NULL);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ASSERT((FALSE == moduleConfig->InterruptMandatory) ||
           (moduleConfig->EvtSpbTargetInterruptDpc != NULL) ||
           (moduleConfig->EvtSpbTargetInterruptPassive != NULL) ||
           (moduleConfig->EvtSpbTargetInterruptIsr != NULL));

    device = DMF_ParentDeviceGet(DmfModule);

    moduleContext->SpbConnectionAssigned = FALSE;
    moduleContext->InterruptAssigned = FALSE;
    interruptResourceIndex = 0;

    // Check the number of resources for the button device.
    //
    resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);

    // Parse the resources. This Module cares about SPB and Interrupt resources.
    //
    gpioConnectionIndex = 0;
    interruptIndex = 0;
    for (resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++)
    {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;
        UCHAR connectionClass;
        UCHAR connectionType;

        descriptor = WdfCmResourceListGetDescriptor(ResourcesTranslated,
                                                    resourceIndex);

        if (CmResourceTypeConnection == descriptor->Type)
        {
            // Look for I2C or SPI resource and save connection ID.
            //
            connectionClass = descriptor->u.Connection.Class;
            connectionType = descriptor->u.Connection.Type;

            if ((connectionClass == CM_RESOURCE_CONNECTION_CLASS_SERIAL) &&
                ((connectionType == CM_RESOURCE_CONNECTION_TYPE_SERIAL_I2C) ||
                    (connectionType == CM_RESOURCE_CONNECTION_TYPE_SERIAL_SPI)))
            {
                if (moduleConfig->SpbConnectionIndex == gpioConnectionIndex)
                {
                    // Store the index of the SPB line that is instantiated.
                    // (For debug purposes only.)
                    //
                    moduleContext->SpbTargetLineIndex = gpioConnectionIndex;

                    // Assign the information needed to open the target.
                    //
                    moduleContext->SpbTargetConnection = *descriptor;

                    moduleContext->SpbConnectionAssigned = TRUE;

                    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Assign: SpbTargetLineIndex=%d",
                                moduleContext->SpbTargetLineIndex);
                }

                gpioConnectionIndex++;

                TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "CmResourceTypeConnection 0x%08X:%08X",
                            descriptor->u.Connection.IdHighPart,
                            descriptor->u.Connection.IdLowPart);
            }
        }
        else if (CmResourceTypeInterrupt == descriptor->Type)
        {
            if (moduleConfig->InterruptIndex == interruptIndex)
            {
                // Store the index of the SPB interrupt that is instantiated.
                // (For debug purposes only.)
                //
                moduleContext->SpbTargetInterruptIndex = interruptIndex;

                // Save the actual resource index that is used later to initialize the interrupt.
                //
                interruptResourceIndex = resourceIndex;

                moduleContext->InterruptAssigned = TRUE;

                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Assign: SpbTargetInterruptIndex=%d interruptResourceIndex=%d",
                            moduleContext->SpbTargetInterruptIndex,
                            interruptResourceIndex);
            }

            interruptIndex++;

            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "CmResourceTypeInterrupt 0x%08X 0x%IX 0x%08X",
                        descriptor->u.Interrupt.Vector,
                        descriptor->u.Interrupt.Affinity,
                        descriptor->u.Interrupt.Level);
        }
        else
        {
            // All other resources are ignored by this Module.
            //
        }
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "SpbConnectionAssigned=%d SpbConnectionMandatory=%d", moduleContext->SpbConnectionAssigned, moduleConfig->SpbConnectionMandatory);

    //  Validate SPB connection with the Client Driver's requirements.
    //
    if (moduleConfig->SpbConnectionMandatory && 
        (! moduleContext->SpbConnectionAssigned))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Spb Connection not assigned");
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
        NT_ASSERT(FALSE);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "InterruptAssigned=%d InterruptMandatory=%d", moduleContext->InterruptAssigned, moduleConfig->InterruptMandatory);

    //  Validate interrupt with the Client Driver's requirements.
    //
    if (moduleConfig->InterruptMandatory && 
        (! moduleContext->InterruptAssigned))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Interrupt resource not assigned");
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
        NT_ASSERT(FALSE);
        goto Exit;
    }

    // Initialize the interrupt, if necessary.
    //
    if (moduleContext->InterruptAssigned)
    {
        WDF_INTERRUPT_CONFIG_INIT(&interruptConfig,
                                  SpbTarget_Isr,
                                  NULL);

        interruptConfig.PassiveHandling = moduleConfig->PassiveHandling;
        interruptConfig.CanWakeDevice = moduleConfig->CanWakeDevice;

        // Configure either a DPC or a Workitem.
        //
        if (moduleConfig->EvtSpbTargetInterruptDpc != NULL)
        {
            interruptConfig.EvtInterruptDpc = SpbTarget_DpcForIsr;
        }
        else if (moduleConfig->EvtSpbTargetInterruptPassive != NULL)
        {
            interruptConfig.EvtInterruptWorkItem = SpbTarget_PasiveLevelCallback;
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
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfInterruptCreate ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "SPB Interrupt Created");

        // NOTE: It is not possible to get the parent of a WDFINTERRUPT.
        // Therefore, it is necessary to save the DmfModule in its context area.
        //
        DMF_ModuleInContextSave(moduleContext->Interrupt,
                                DmfModule);

        // If both DPC and Passive level callback, then ISR queues DPC 
        // and then DPC queues passive level workitem. (It means the Client
        // wants to do work both at DPC and PASSIVE levels.)
        //
        if ((moduleConfig->EvtSpbTargetInterruptDpc != NULL) &&
            (moduleConfig->EvtSpbTargetInterruptPassive != NULL))
        {
            WDF_WORKITEM_CONFIG_INIT(&workitemConfig,
                                     SpbTarget_Workitem);
            workitemConfig.AutomaticSerialization = WdfFalse;

            WDF_OBJECT_ATTRIBUTES_INIT(&workitemAttributes);
            workitemAttributes.ParentObject = DmfModule;

            ntStatus = WdfWorkItemCreate(&workitemConfig,
                                         &workitemAttributes,
                                         &moduleContext->Workitem);

            if (! NT_SUCCESS(ntStatus))
            {
                moduleContext->Workitem = NULL;
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfWorkItemCreate fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }

            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Workitem Created");
        }
        else
        {
            ASSERT(moduleContext->Workitem == NULL);
        }
    }

    ntStatus = STATUS_SUCCESS;

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
DMF_SpbTarget_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type ContinuousRequestTarget.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SpbTarget* moduleContext;
    DMF_CONFIG_SpbTarget* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Create the SPB target.
    //
    WDF_OBJECT_ATTRIBUTES targetAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&targetAttributes);
    
    WDFDEVICE device = DMF_ParentDeviceGet(DmfModule);
    ntStatus = WdfIoTargetCreate(device,
                                 &targetAttributes,
                                 &moduleContext->SpbController);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = SpbTarget_Open(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "SpbTarget_Open fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    DMF_RequestTarget_IoTargetSet(moduleContext->DmfModuleRequestTarget,
                                  moduleContext->SpbController);

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_SpbTarget_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type ContinuousRequestTarget.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_SpbTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->Interrupt != nullptr)
    {
        WdfObjectDelete(moduleContext->Interrupt);
    }

    if (moduleContext->SpbController != WDF_NO_HANDLE)
    {
        SpbTarget_Close(moduleContext);
        WdfObjectDelete(moduleContext->SpbController);
        moduleContext->SpbController = WDF_NO_HANDLE;
    }

    DMF_RequestTarget_IoTargetClear(moduleContext->DmfModuleRequestTarget);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_SpbTarget_ChildModulesAdd(
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
    DMF_CONTEXT_SpbTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // RequestTarget
    // -------------
    //

    // Streaming functionality is not required. 
    // Create DMF_RequestTarget instead of DMF_ContinuousRequestTarget.
    //

    DMF_RequestTarget_ATTRIBUTES_INIT(&moduleAttributes);
    moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        &moduleContext->DmfModuleRequestTarget);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_SpbTarget;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_SpbTarget;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SpbTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type SpbTarget.

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

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_SpbTarget);
    DmfCallbacksDmf_SpbTarget.ChildModulesAdd = DMF_SpbTarget_ChildModulesAdd;
    DmfCallbacksDmf_SpbTarget.DeviceResourcesAssign = DMF_SpbTarget_ResourcesAssign;
    DmfCallbacksDmf_SpbTarget.DeviceOpen = DMF_SpbTarget_Open;
    DmfCallbacksDmf_SpbTarget.DeviceClose = DMF_SpbTarget_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_SpbTarget,
                                            SpbTarget,
                                            DMF_CONTEXT_SpbTarget,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    DmfModuleDescriptor_SpbTarget.CallbacksDmf = &DmfCallbacksDmf_SpbTarget;

    DmfModuleDescriptor_SpbTarget.ModuleConfigSize = sizeof(DMF_CONFIG_SpbTarget);

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_SpbTarget,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

#define SPBTARGET_SIMPLE_METHOD_TEMPLATE(MethodName, Ioctl)                                                         \
_IRQL_requires_max_(PASSIVE_LEVEL)                                                                                  \
NTSTATUS                                                                                                            \
MethodName(                                                                                                         \
    _In_ DMFMODULE DmfModule                                                                                        \
    )                                                                                                               \
{                                                                                                                   \
    DMF_CONTEXT_SpbTarget* moduleContext;                                                                           \
    NTSTATUS ntStatus;                                                                                              \
    size_t bytesWritten;                                                                                            \
                                                                                                                    \
    PAGED_CODE();                                                                                                   \
                                                                                                                    \
    moduleContext = DMF_CONTEXT_GET(DmfModule);                                                                     \
                                                                                                                    \
    ntStatus = DMF_RequestTarget_SendSynchronously(moduleContext->DmfModuleRequestTarget,                           \
                                                   NULL,                                                            \
                                                   0,                                                               \
                                                   NULL,                                                            \
                                                   0,                                                               \
                                                   ContinuousRequestTarget_RequestType_Ioctl,                       \
                                                   Ioctl,                                                           \
                                                   0,                                                               \
                                                   &bytesWritten);                                                  \
                                                                                                                    \
    return ntStatus;                                                                                                \
}                                                                                                                   \

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_SpbTarget_BufferFullDuplex(
    _In_ DMFMODULE DmfModule,
    _In_reads_(InputBufferLength) UCHAR* InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_(OutputBufferLength) UCHAR* OutputBuffer,
    _In_ ULONG OutputBufferLength
    )
/*++
 
  Routine Description:

    This routine sends a write-read sequence to the SPB controller. It reads a buffer from a particular address.

  Arguments:

    DmfModule - This Module's Module handle.
    InputBuffer - Address to read buffer from.
    InputBufferLength - Length of InputBuffer.
    OutputBuffer - Data read from device.
    OutputBufferLength - Length of OutputBuffer.

  Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_SpbTarget* moduleContext;
    NTSTATUS ntStatus;
    size_t bytesWritten;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Build SPB sequence.
    //
    const ULONG transfers = 2;
    
    SPB_TRANSFER_LIST_AND_ENTRIES(transfers) sequence;
    SPB_TRANSFER_LIST_INIT(&(sequence.List), transfers);

    // PreFAST cannot figure out the SPB_TRANSFER_LIST_ENTRY
    // "struct hack" size but using an index variable quiets 
    // the warning. This is a false positive from OACR.
    // 

    ULONG index = 0;
    sequence.List.Transfers[index] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionToDevice,
                                                                         0,
                                                                         InputBuffer,
                                                                         (ULONG)InputBufferLength);

    sequence.List.Transfers[index + 1] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionFromDevice,
                                                                             0,
                                                                             OutputBuffer,
                                                                             (ULONG)OutputBufferLength);


    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_RequestTarget_SendSynchronously(moduleContext->DmfModuleRequestTarget,
                                                   &sequence,
                                                   sizeof(sequence),
                                                   NULL,
                                                   0,
                                                   ContinuousRequestTarget_RequestType_Ioctl,
                                                   IOCTL_SPB_FULL_DUPLEX,
                                                   0,
                                                   &bytesWritten);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_SpbTarget_BufferWrite(
    _In_ DMFMODULE DmfModule,
    _In_reads_(NumberOfBytesToWrite) UCHAR* BufferToWrite,
    _In_ ULONG NumberOfBytesToWrite
    )
/*++
 
  Routine Description:

    This routine writes to the SPB controller.

  Arguments:

    DmfModule - This Module's Module handle.
    BufferToWrite- Data to write to the device.
    NumberOfBytesToWrite - Length of BufferToWrite.

  Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SpbTarget* moduleContext;
    size_t bytesWritten;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_RequestTarget_SendSynchronously(moduleContext->DmfModuleRequestTarget,
                                                   BufferToWrite,
                                                   NumberOfBytesToWrite,
                                                   NULL,
                                                   0,
                                                   ContinuousRequestTarget_RequestType_Write,
                                                   0,
                                                   0,
                                                   &bytesWritten);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_SpbTarget_BufferWriteRead(
    _In_ DMFMODULE DmfModule,
    _In_reads_(InputBufferLength) UCHAR* InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_(OutputBufferLength) UCHAR* OutputBuffer,
    _In_ ULONG OutputBufferLength
    )
/*++
 
  Routine Description:

    This routine sends a write-read sequence to the SPB controller. It reads a buffer from a particular address.

  Arguments:

    DmfModule - This Module's Module handle.
    InputBuffer - Address to read buffer from.
    InputBufferLength - Length of InputBuffer.
    OutputBuffer - Data read from device.
    OutputBufferLength - Length of OutputBuffer.

  Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_SpbTarget* moduleContext;
    NTSTATUS ntStatus;
    size_t bytesWritten;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Build SPB sequence.
    //
    const ULONG transfers = 2;
    
    SPB_TRANSFER_LIST_AND_ENTRIES(transfers) sequence;
    SPB_TRANSFER_LIST_INIT(&(sequence.List), transfers);

    // PreFAST cannot figure out the SPB_TRANSFER_LIST_ENTRY
    // "struct hack" size but using an index variable quiets 
    // the warning. This is a false positive from OACR.
    // 

    ULONG index = 0;
    sequence.List.Transfers[index] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionToDevice,
                                                                         0,
                                                                         InputBuffer,
                                                                         (ULONG)InputBufferLength);

    sequence.List.Transfers[index + 1] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionFromDevice,
                                                                             0,
                                                                             OutputBuffer,
                                                                             (ULONG)OutputBufferLength);


    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_RequestTarget_SendSynchronously(moduleContext->DmfModuleRequestTarget,
                                                   &sequence,
                                                   sizeof(sequence),
                                                   NULL,
                                                   0,
                                                   ContinuousRequestTarget_RequestType_Ioctl,
                                                   IOCTL_SPB_EXECUTE_SEQUENCE,
                                                   0,
                                                   &bytesWritten);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
SPBTARGET_SIMPLE_METHOD_TEMPLATE(DMF_SpbTarget_ConnectionLock, IOCTL_SPB_LOCK_CONNECTION)
#pragma code_seg()

#pragma code_seg("PAGE")
SPBTARGET_SIMPLE_METHOD_TEMPLATE(DMF_SpbTarget_ConnectionUnlock, IOCTL_SPB_UNLOCK_CONNECTION)
#pragma code_seg()

#pragma code_seg("PAGE")
SPBTARGET_SIMPLE_METHOD_TEMPLATE(DMF_SpbTarget_ControllerLock, IOCTL_SPB_LOCK_CONTROLLER)
#pragma code_seg()

#pragma code_seg("PAGE")
SPBTARGET_SIMPLE_METHOD_TEMPLATE(DMF_SpbTarget_ControllerUnlock, IOCTL_SPB_UNLOCK_CONTROLLER)
#pragma code_seg()

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_SpbTarget_InterruptAcquireLock(
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
    DMF_CONTEXT_SpbTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_SpbTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WdfInterruptAcquireLock(moduleContext->Interrupt);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_SpbTarget_InterruptReleaseLock(
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
    DMF_CONTEXT_SpbTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_SpbTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WdfInterruptReleaseLock(moduleContext->Interrupt);

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_SpbTarget_InterruptTryToAcquireLock(
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
    DMF_CONTEXT_SpbTarget* moduleContext;
    BOOLEAN returnValue;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_SpbTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    returnValue = WdfInterruptTryToAcquireLock(moduleContext->Interrupt);

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_SpbTarget_IsResourceAssigned(
    _In_ DMFMODULE DmfModule,
    _Out_opt_ BOOLEAN* SpbConnectionAssigned,
    _Out_opt_ BOOLEAN* InterruptAssigned
    )
/*++

Routine Description:

    GPIOs may or may not be present on some systems. This function returns the number of SPB
    resources assigned for drivers where it is not known the GPIOs exist.

Arguments:

    DmfModule - This Module's handle.
    SpbConnectionAssigned - Is SPB connection assigned to this Module instance.
    InterruptAssigned - Is Interrupt assigned to this Module instance.

Return Value:

    None

--*/
{
    DMF_CONTEXT_SpbTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_SpbTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (SpbConnectionAssigned != NULL)
    {
        *SpbConnectionAssigned = moduleContext->SpbConnectionAssigned;
    }

    if (InterruptAssigned != NULL)
    {
        *InterruptAssigned = moduleContext->InterruptAssigned;
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

// eof: Dmf_SpbTarget.c
//
