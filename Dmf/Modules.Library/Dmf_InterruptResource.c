/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_InterruptResource.c

Abstract:

    Allows Clients to register for interrupt callbacks.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_InterruptResource.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_InterruptResource
{
    // Resources assigned.
    //
    BOOLEAN InterruptAssigned;

    // Interrupt Line Index that is instantiated in this object.
    //
    ULONG InterruptResourceLineIndex;
    // Interrupt Index that is instantiated in this object.
    //
    ULONG InterruptResourceInterruptIndex;

    // Resource information of the interrupt.
    //
    CM_PARTIAL_RESOURCE_DESCRIPTOR InterruptResourceConnection;

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
} DMF_CONTEXT_InterruptResource;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(InterruptResource)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(InterruptResource)

// Memory Pool Tag.
//
#define MemoryTag 'RtnI'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

EVT_WDF_WORKITEM InterruptResource_Workitem;

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
InterruptResource_Workitem(
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
    DMF_CONTEXT_InterruptResource* moduleContext;
    DMF_CONFIG_InterruptResource* moduleConfig;
    ULONG numberOfTimesWorkitemMustExecute;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    dmfModule = (DMFMODULE)WdfWorkItemGetParentObject(Workitem);

    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    DmfAssert(moduleConfig->EvtInterruptResourceInterruptPassive != NULL);

    // It is possible attempts to enqueue fail, so make sure to execute exactly the number
    // of times ISR attempted to enqueue.
    //
    WdfInterruptAcquireLock(moduleContext->Interrupt);
    numberOfTimesWorkitemMustExecute = moduleContext->NumberOfTimesWorkitemMustExecute;
    moduleContext->NumberOfTimesWorkitemMustExecute = 0;
    WdfInterruptReleaseLock(moduleContext->Interrupt);

    while (numberOfTimesWorkitemMustExecute > 0)
    {
        moduleConfig->EvtInterruptResourceInterruptPassive(dmfModule);
        numberOfTimesWorkitemMustExecute--;
    }

    FuncExitNoReturn(DMF_TRACE);
}
#pragma code_seg()

EVT_WDF_INTERRUPT_WORKITEM InterruptResource_PasiveLevelCallback;

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID
InterruptResource_PasiveLevelCallback(
    _In_ WDFINTERRUPT Interrupt,
    _In_ WDFOBJECT WdfDevice
    )
/*++
Routine Description:

    Passive Level callback for a passive level interrupt.

Arguments:

    Interrupt - handle to a WDF interrupt object.
    WdfDevice - handle to a WDF device object.

Return Value:

    None

--*/
{
    DMFMODULE* dmfModuleAddress;
    DMF_CONFIG_InterruptResource* moduleConfig;

    UNREFERENCED_PARAMETER(WdfDevice);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    dmfModuleAddress = WdfObjectGet_DMFMODULE(Interrupt);

    // This is just a sanity check.
    //
    DmfAssert(WdfDevice == DMF_ParentDeviceGet(*dmfModuleAddress));

    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

    // Call the optional PASSIVE_LEVEL Client Driver callback.
    //
    DmfAssert(moduleConfig->EvtInterruptResourceInterruptPassive != NULL);

    moduleConfig->EvtInterruptResourceInterruptPassive(*dmfModuleAddress);

    FuncExitNoReturn(DMF_TRACE);
}
#pragma code_seg()

EVT_WDF_INTERRUPT_DPC InterruptResource_DpcForIsr;

_Use_decl_annotations_
VOID
InterruptResource_DpcForIsr(
    _In_ WDFINTERRUPT Interrupt,
    _In_ WDFOBJECT WdfDevice
    )
/*++
Routine Description:

    DPC callback for a interrupt.

Arguments:

    Interrupt - handle to a WDF interrupt object.
    WdfDevice - handle to a WDF device object.

Return Value:

    None

--*/
{
    DMFMODULE* dmfModuleAddress;
    DMF_CONFIG_InterruptResource* moduleConfig;
    DMF_CONTEXT_InterruptResource* moduleContext;
    InterruptResource_QueuedWorkItem_Type queuedWorkItem;
    ULONG numberOfTimesDpcMustExecute;

    UNREFERENCED_PARAMETER(WdfDevice);

    FuncEntry(DMF_TRACE);

    // The Interrupt's Module Context area has the DMF Module.
    //
    dmfModuleAddress = WdfObjectGet_DMFMODULE(Interrupt);

    // This is just a sanity check.
    //
    DmfAssert(WdfDevice == DMF_ParentDeviceGet(*dmfModuleAddress));

    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

    // Call the DISPATCH_LEVEL Client callback.
    //
    DmfAssert(moduleConfig->EvtInterruptResourceInterruptDpc != NULL);
    DmfAssert(! moduleConfig->PassiveHandling);

    // It is possible attempts to enqueue fail, so make sure to execute exactly the number
    // of times ISR attempted to enqueue.
    //
    WdfInterruptAcquireLock(moduleContext->Interrupt);
    numberOfTimesDpcMustExecute = moduleContext->NumberOfTimesDpcMustExecute;
    moduleContext->NumberOfTimesDpcMustExecute = 0;
    WdfInterruptReleaseLock(moduleContext->Interrupt);

    while (numberOfTimesDpcMustExecute > 0)
    {
        moduleConfig->EvtInterruptResourceInterruptDpc(*dmfModuleAddress,
                                                       &queuedWorkItem);
        if (InterruptResource_QueuedWorkItem_WorkItem == queuedWorkItem)
        {
            moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
            DmfAssert(moduleContext != NULL);

            DmfAssert(moduleContext->Workitem != NULL);
            WdfInterruptAcquireLock(moduleContext->Interrupt);
            moduleContext->NumberOfTimesWorkitemMustExecute++;
            WdfInterruptReleaseLock(moduleContext->Interrupt);

            WdfWorkItemEnqueue(moduleContext->Workitem);
        }
        numberOfTimesDpcMustExecute--;
    }

    FuncExitNoReturn(DMF_TRACE);
}

EVT_WDF_INTERRUPT_ISR InterruptResource_Isr;

_Use_decl_annotations_
BOOLEAN
InterruptResource_Isr(
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
    DMF_CONTEXT_InterruptResource* moduleContext;
    DMF_CONFIG_InterruptResource* moduleConfig;
    BOOLEAN interruptHandled;
    BOOLEAN enqueued;

    UNREFERENCED_PARAMETER(MessageId);

    FuncEntry(DMF_TRACE);

    // The Interrupt's Module context area has the DMF Module.
    //
    dmfModuleAddress = WdfObjectGet_DMFMODULE(Interrupt);

    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);

    DmfAssert((moduleConfig->EvtInterruptResourceInterruptDpc != NULL) || 
              (moduleConfig->EvtInterruptResourceInterruptPassive != NULL)  ||
              (moduleConfig->EvtInterruptResourceInterruptIsr != NULL));

    // Option 1: Caller wants to do work in ISR at DIRQL (and optionally, at DPC or PASSIVE
    //           levels.)
    // Option 2: Caller wants to do work at DPC level (and optionally at PASSIVE level).
    // Option 3: Caller wants to do work only at PASSIVE_LEVEL.
    //
    if (moduleConfig->EvtInterruptResourceInterruptIsr != NULL)
    {
        InterruptResource_QueuedWorkItem_Type queuedWorkItem;

        interruptHandled = moduleConfig->EvtInterruptResourceInterruptIsr(*dmfModuleAddress,
                                                                          MessageId,
                                                                          &queuedWorkItem);
        if (interruptHandled)
        {
            if (InterruptResource_QueuedWorkItem_Dpc == queuedWorkItem)
            {
                DmfAssert(moduleConfig->EvtInterruptResourceInterruptDpc != NULL);
                // Interrupt SpinLock is held.
                //
                moduleContext->NumberOfTimesDpcMustExecute++;
                enqueued = WdfInterruptQueueDpcForIsr(Interrupt);
            }
            else if (InterruptResource_QueuedWorkItem_WorkItem == queuedWorkItem)
            {
                DmfAssert(moduleConfig->EvtInterruptResourceInterruptPassive != NULL);
                // Interrupt SpinLock is held.
                //
                moduleContext->NumberOfTimesWorkitemMustExecute++;
                enqueued = WdfInterruptQueueWorkItemForIsr(Interrupt);
            }
        }
    }
    // Call the optional DPC_LEVEL Client Driver callback.
    //
    else if (moduleConfig->EvtInterruptResourceInterruptDpc != NULL)
    {
        // Interrupt SpinLock is held.
        //
        moduleContext->NumberOfTimesDpcMustExecute++;
        enqueued = WdfInterruptQueueDpcForIsr(Interrupt);
        interruptHandled = TRUE;
    }
    // If no DPC, launch the optional Client Driver PASSIVE_LEVEL callback.
    //
    else if (moduleConfig->EvtInterruptResourceInterruptPassive != NULL)
    {
        // Interrupt SpinLock is held.
        //
        moduleContext->NumberOfTimesWorkitemMustExecute++;
        enqueued = WdfInterruptQueueWorkItemForIsr(Interrupt);
        interruptHandled = TRUE;
    }
    else
    {
        DmfAssert(FALSE);
        interruptHandled = TRUE;
    }

    FuncExitNoReturn(DMF_TRACE);

    return interruptHandled;
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
_Function_class_(DMF_ResourcesAssign)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_InterruptResource_ResourcesAssign(
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
    DMF_CONTEXT_InterruptResource* moduleContext;
    ULONG resourceCount;
    ULONG resourceIndex;
    NTSTATUS ntStatus;
    ULONG interruptResourceIndex;
    WDF_INTERRUPT_CONFIG interruptConfig;
    WDF_OBJECT_ATTRIBUTES interruptAttributes;
    WDF_OBJECT_ATTRIBUTES workitemAttributes;
    WDF_WORKITEM_CONFIG  workitemConfig;
    WDFDEVICE device;
    DMF_CONFIG_InterruptResource* moduleConfig;
    ULONG interruptIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(ResourcesRaw != NULL);
    DmfAssert(ResourcesTranslated != NULL);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    DmfAssert((FALSE == moduleConfig->InterruptMandatory) ||
              (moduleConfig->EvtInterruptResourceInterruptDpc != NULL) ||
              (moduleConfig->EvtInterruptResourceInterruptPassive != NULL) ||
              (moduleConfig->EvtInterruptResourceInterruptIsr != NULL));

    device = DMF_ParentDeviceGet(DmfModule);

    moduleContext->InterruptAssigned = FALSE;
    interruptResourceIndex = 0;

    // Check the number of resources for the button device.
    //
    resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);

    // Parse the resources. This Module cares about Interrupt resources.
    //
    interruptIndex = 0;
    for (resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++)
    {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;

        descriptor = WdfCmResourceListGetDescriptor(ResourcesTranslated,
                                                    resourceIndex);

        if (CmResourceTypeInterrupt == descriptor->Type)
        {
            if (moduleConfig->InterruptIndex == interruptIndex)
            {
                // Store the index of the interrupt that is instantiated.
                // (For debug purposes only.)
                //
                moduleContext->InterruptResourceInterruptIndex = interruptIndex;

                // Save the actual resource index that is used later to initialize the interrupt.
                //
                interruptResourceIndex = resourceIndex;

                moduleContext->InterruptAssigned = TRUE;

                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Assign: InterruptResourceInterruptIndex=%d interruptResourceIndex=%d",
                            moduleContext->InterruptResourceInterruptIndex,
                            interruptResourceIndex);
            }

            interruptIndex++;

            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "CmResourceTypeInterrupt 0x%08X 0x%IX 0x%08X",
                        descriptor->u.Interrupt.Vector,
                        descriptor->u.Interrupt.Affinity,
                        descriptor->u.Interrupt.Level);
        }
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "InterruptAssigned=%d InterruptMandatory=%d", moduleContext->InterruptAssigned, moduleConfig->InterruptMandatory);

    //  Validate interrupt with the Client Driver's requirements.
    //
    if (moduleConfig->InterruptMandatory && 
        (! moduleContext->InterruptAssigned))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Interrupt resource not assigned");
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
        DmfAssert(FALSE);
        goto Exit;
    }

    // Initialize the interrupt, if necessary.
    // If the Client does not register any handlers, do not connect to the interrupt.
    // This is necessary for the case where the Client just uses a GPIO but does not 
    // need the interrupt resource that is present.
    //
    if ((moduleContext->InterruptAssigned) &&
        ((moduleConfig->EvtInterruptResourceInterruptIsr != NULL) ||
         (moduleConfig->EvtInterruptResourceInterruptPassive != NULL) ||
         (moduleConfig->EvtInterruptResourceInterruptDpc != NULL)))
    {
        WDF_INTERRUPT_CONFIG_INIT(&interruptConfig,
                                  InterruptResource_Isr,
                                  NULL);

        interruptConfig.PassiveHandling = moduleConfig->PassiveHandling;
        interruptConfig.CanWakeDevice = moduleConfig->CanWakeDevice;

        // Configure either a DPC or a Workitem.
        //
        if (moduleConfig->EvtInterruptResourceInterruptDpc != NULL)
        {
            interruptConfig.EvtInterruptDpc = InterruptResource_DpcForIsr;
        }
        else if (moduleConfig->EvtInterruptResourceInterruptPassive != NULL)
        {
            interruptConfig.EvtInterruptWorkItem = InterruptResource_PasiveLevelCallback;
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

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Interrupt Created");

        // NOTE: It is not possible to get the parent of a WDFINTERRUPT.
        // Therefore, it is necessary to save the DmfModule in its context area.
        //
        DMF_ModuleInContextSave(moduleContext->Interrupt,
                                DmfModule);

        // If both DPC and Passive level callback, then ISR queues DPC 
        // and then DPC queues passive level workitem. (It means the Client
        // wants to do work both at DPC and PASSIVE levels.)
        //
        if ((moduleConfig->EvtInterruptResourceInterruptDpc != NULL) &&
            (moduleConfig->EvtInterruptResourceInterruptPassive != NULL))
        {
            WDF_WORKITEM_CONFIG_INIT(&workitemConfig,
                                     InterruptResource_Workitem);
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
            DmfAssert(moduleContext->Workitem == NULL);
        }
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_InterruptResource_Open(
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
    DMF_CONTEXT_InterruptResource* moduleContext;
    DMF_CONFIG_InterruptResource* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_InterruptResource_Close(
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
    DMF_CONTEXT_InterruptResource* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Do not delete moduleContext->Interrupt. It is prohibited per
    // Verifier.
    //

    // Make sure all pending work is complete.
    //
    if (moduleContext->Workitem != NULL)
    {
        WdfWorkItemFlush(moduleContext->Workitem);
    }

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
DMF_InterruptResource_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type InterruptResource.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_InterruptResource;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_InterruptResource;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_InterruptResource);
    dmfCallbacksDmf_InterruptResource.DeviceResourcesAssign = DMF_InterruptResource_ResourcesAssign;
    dmfCallbacksDmf_InterruptResource.DeviceOpen = DMF_InterruptResource_Open;
    dmfCallbacksDmf_InterruptResource.DeviceClose = DMF_InterruptResource_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_InterruptResource,
                                            InterruptResource,
                                            DMF_CONTEXT_InterruptResource,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_InterruptResource.CallbacksDmf = &dmfCallbacksDmf_InterruptResource;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_InterruptResource,
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

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_InterruptResource_InterruptAcquireLock(
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
    DMF_CONTEXT_InterruptResource* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 InterruptResource);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WdfInterruptAcquireLock(moduleContext->Interrupt);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_InterruptResource_InterruptReleaseLock(
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
    DMF_CONTEXT_InterruptResource* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 InterruptResource);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WdfInterruptReleaseLock(moduleContext->Interrupt);

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_InterruptResource_InterruptTryToAcquireLock(
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
    DMF_CONTEXT_InterruptResource* moduleContext;
    BOOLEAN returnValue;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 InterruptResource);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    returnValue = WdfInterruptTryToAcquireLock(moduleContext->Interrupt);

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_InterruptResource_IsResourceAssigned(
    _In_ DMFMODULE DmfModule,
    _Out_opt_ BOOLEAN* InterruptAssigned
    )
/*++

Routine Description:

    Desired interrupt resources may or may not be present on some systems. This function returns the 
    number of interrupt resources assigned for drivers where it is not known the interrupt resources exist.

Arguments:

    DmfModule - This Module's handle.
    InterruptAssigned - Is Interrupt assigned to this Module instance.

Return Value:

    None

--*/
{
    DMF_CONTEXT_InterruptResource* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 InterruptResource);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (InterruptAssigned != NULL)
    {
        *InterruptAssigned = moduleContext->InterruptAssigned;
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

// eof: Dmf_InterruptResource.c
//
