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
#include "DmfModule.h"
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

    // GPIO Line Index that is instantiated in this object.
    //
    ULONG GpioTargetLineIndex;

    // Underlying GPIO device connection.
    //
    WDFIOTARGET GpioTarget;

    // Resource information for GPIO device.
    //
    CM_PARTIAL_RESOURCE_DESCRIPTOR GpioTargetConnection;

    // InterruptResource.
    //
    DMFMODULE DmfModuleInterruptResource;

    // Optional Callback from ISR (with Interrupt Spin Lock held).
    //
    EVT_DMF_InterruptResource_InterruptIsr* EvtGpioTargetInterruptIsr;
    // Optional Callback at DPC_LEVEL Level.
    //
    EVT_DMF_InterruptResource_InterruptDpc* EvtGpioTargetInterruptDpc;
    // Optional Callback at PASSIVE_LEVEL Level.
    //
    EVT_DMF_InterruptResource_InterruptPassive* EvtGpioTargetInterruptPassive;
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

    FuncEntry(DMF_TRACE);

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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestCreate fails: ntStatus=%!STATUS!", ntStatus);
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreatePreallocated fails: ntStatus=%!STATUS!", ntStatus);
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetFormatRequestForIoctl fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_REQUEST_SEND_OPTIONS_INIT(&requestOptions,
                                  WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);

    if (! WdfRequestSend(request,
                         IoTarget,
                         &requestOptions))
    {
        ntStatus = WdfRequestGetStatus(request);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestSend fails: ntStatus=%!STATUS!", ntStatus);
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

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE);

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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestCreate fails: ntStatus=%!STATUS!", ntStatus);
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreatePreallocated fails: ntStatus=%!STATUS!", ntStatus);
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetFormatRequestForIoctl fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_REQUEST_SEND_OPTIONS_INIT(&requestOptions,
                                  WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);

    if (! WdfRequestSend(request,
                         IoTarget,
                         &requestOptions))
    {
        ntStatus = WdfRequestGetStatus(request);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestSend fails: ntStatus=%!STATUS!", ntStatus);
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
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "GPIO value read = 0x%x", data);
    }
    else
    {
        *PinValue = FALSE;
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "GPIO value read = 0x%x, ntStatus=%!STATUS!", data, ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

EVT_DMF_InterruptResource_InterruptIsr GpioTarget_InterruptIsr;

_Function_class_(EVT_DMF_InterruptResource_InterruptIsr)
_IRQL_requires_same_
BOOLEAN
GpioTarget_InterruptIsr(
    _In_ DMFMODULE DmfModuleInterruptResource,
    _In_ ULONG MessageId,
    _Out_ InterruptResource_QueuedWorkItem_Type* QueuedWorkItem
    )
/*++

Routine Description:

    Chain DIRQL_LEVEL interrupt callback from Child Module to Parent Module.
    (Callback Clients must always receive callbacks from immediate descendant.)

Arguments:

    DmfModuleInterruptResource - Child Module handle.
    MessageId - Interrupt message id.
    QueuedWorkItem - Indicates next action per callback client.

Return Value:

    TRUE indicates callback client recognizes interrupt.

--*/
{
    DMFMODULE DmfModuleGpioTarget;
    DMF_CONTEXT_GpioTarget* moduleContext;
    BOOLEAN returnValue;

    DmfModuleGpioTarget = DMF_ParentModuleGet(DmfModuleInterruptResource);
    moduleContext = DMF_CONTEXT_GET(DmfModuleGpioTarget);

    ASSERT(moduleContext->EvtGpioTargetInterruptIsr != NULL);
    returnValue = moduleContext->EvtGpioTargetInterruptIsr(DmfModuleGpioTarget,
                                                          MessageId,
                                                          QueuedWorkItem);

    return returnValue;
}

EVT_DMF_InterruptResource_InterruptDpc GpioTarget_InterruptDpc;

_Function_class_(EVT_DMF_InterruptResource_InterruptDpc)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
GpioTarget_InterruptDpc(
    _In_ DMFMODULE DmfModuleInterruptResource,
    _Out_ InterruptResource_QueuedWorkItem_Type* QueuedWorkItem
    )
/*++

Routine Description:

    Chain DISPATCH_LEVEL interrupt callback from Child Module to Parent Module.
    (Callback Clients must always receive callbacks from immediate descendant.)

Arguments:

    DmfModuleInterruptResource - Child Module handle.
    QueuedWorkItem - Indicates next action per callback client.

Return Value:

    None

--*/
{
    DMFMODULE DmfModuleGpioTarget;
    DMF_CONTEXT_GpioTarget* moduleContext;

    DmfModuleGpioTarget = DMF_ParentModuleGet(DmfModuleInterruptResource);
    moduleContext = DMF_CONTEXT_GET(DmfModuleGpioTarget);

    ASSERT(moduleContext->EvtGpioTargetInterruptIsr != NULL);
    moduleContext->EvtGpioTargetInterruptDpc(DmfModuleGpioTarget,
                                            QueuedWorkItem);
}

EVT_DMF_InterruptResource_InterruptPassive GpioTarget_InterruptPassive;

_Function_class_(EVT_DMF_InterruptResource_InterruptPassive)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
GpioTarget_InterruptPassive(
    _In_ DMFMODULE DmfModuleInterruptResource
    )
/*++

Routine Description:

    Chain PASSIVE_LEVEL interrupt callback from Child Module to Parent Module.
    (Callback Clients must always receive callbacks from immediate descendant.)

Arguments:

    DmfModuleInterruptResource - Child Module handle.

Return Value:

    None

--*/
{
    DMFMODULE DmfModuleGpioTarget;
    DMF_CONTEXT_GpioTarget* moduleContext;

    DmfModuleGpioTarget = DMF_ParentModuleGet(DmfModuleInterruptResource);
    moduleContext = DMF_CONTEXT_GET(DmfModuleGpioTarget);

    ASSERT(moduleContext->EvtGpioTargetInterruptPassive != NULL);
    moduleContext->EvtGpioTargetInterruptPassive(DmfModuleGpioTarget);
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
VOID
DMF_GpioTarget_ChildModulesAdd(
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
    DMF_CONFIG_GpioTarget* moduleConfig;
    DMF_CONTEXT_GpioTarget* moduleContext;
    DMF_CONFIG_InterruptResource configInterruptResource;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // InterruptResource
    // -----------------
    //
    DMF_CONFIG_InterruptResource_AND_ATTRIBUTES_INIT(&configInterruptResource,
                                                     &moduleAttributes);
    RtlCopyMemory(&configInterruptResource,
                  &moduleConfig->InterruptResource,
                  sizeof(DMF_CONFIG_InterruptResource));
    // Chain interrupt callbacks from this Module to Client.
    //
    if (moduleConfig->InterruptResource.EvtInterruptResourceInterruptIsr != NULL)
    {
        moduleContext->EvtGpioTargetInterruptIsr = moduleConfig->InterruptResource.EvtInterruptResourceInterruptIsr;
        configInterruptResource.EvtInterruptResourceInterruptIsr = GpioTarget_InterruptIsr;
    }
    if (moduleConfig->InterruptResource.EvtInterruptResourceInterruptDpc != NULL)
    {
        moduleContext->EvtGpioTargetInterruptDpc = moduleConfig->InterruptResource.EvtInterruptResourceInterruptDpc;
        configInterruptResource.EvtInterruptResourceInterruptDpc = GpioTarget_InterruptDpc;
    }
    if (moduleConfig->InterruptResource.EvtInterruptResourceInterruptPassive != NULL)
    {
        moduleContext->EvtGpioTargetInterruptPassive = moduleConfig->InterruptResource.EvtInterruptResourceInterruptPassive;
        configInterruptResource.EvtInterruptResourceInterruptPassive = GpioTarget_InterruptPassive;
    }
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleInterruptResource);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

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

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (! moduleContext->GpioConnectionAssigned)
    {
        // In some cases, the minimum number of resources is zero because the same driver
        // is used on different platforms. In that case, this Module still loads and opens
        // but it does nothing.
        //
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No GPIO Resources Found");
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    device = DMF_ParentDeviceGet(DmfModule);

    RtlInitEmptyUnicodeString(&resourcePathString,
                              resourcePathBuffer,
                              sizeof(resourcePathBuffer));

    ntStatus = RESOURCE_HUB_CREATE_PATH_FROM_ID(&resourcePathString,
                                                moduleContext->GpioTargetConnection.u.Connection.IdLowPart,
                                                moduleContext->GpioTargetConnection.u.Connection.IdHighPart);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RESOURCE_HUB_CREATE_PATH_FROM_ID fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;

    ntStatus = WdfIoTargetCreate(device,
                                 &objectAttributes,
                                 &moduleContext->GpioTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RESOURCE_HUB_CREATE_PATH_FROM_ID fails: ntStatus=%!STATUS!", ntStatus);
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetOpen fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->GpioTarget != NULL)
    {
        WdfIoTargetClose(moduleContext->GpioTarget);
        WdfObjectDelete(moduleContext->GpioTarget);
        moduleContext->GpioTarget = NULL;
    }

    FuncExitNoReturn(DMF_TRACE);
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
    DMF_CONFIG_GpioTarget* moduleConfig;
    ULONG gpioConnectionIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(ResourcesRaw);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    moduleContext->GpioConnectionAssigned = FALSE;

    // Check the number of resources for the button device.
    //
    resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);

    // Parse the resources. This Module cares about GPIO resources.
    //
    gpioConnectionIndex = 0;
    for (resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++)
    {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR resource;

        resource = WdfCmResourceListGetDescriptor(ResourcesTranslated,
                                                  resourceIndex);
        if (NULL == resource)
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No resources assigned");
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

                    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Assign: GpioTargetLineIndex=%d",
                                moduleContext->GpioTargetLineIndex);
                }

                gpioConnectionIndex++;

                TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "CmResourceTypeConnection 0x%08X:%08X",
                            resource->u.Connection.IdHighPart,
                            resource->u.Connection.IdLowPart);
            }
        }
        else
        {
            // All other resources are ignored by this Module.
            //
        }
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "GpioConnectionAssigned=%d GpioConnectionMandatory=%d", moduleContext->GpioConnectionAssigned, moduleConfig->GpioConnectionMandatory);

    //  Validate Gpio connection with the Client Driver's requirements.
    //
    if (moduleConfig->GpioConnectionMandatory && 
        (! moduleContext->GpioConnectionAssigned))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Gpio Connection not assigned");
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
        NT_ASSERT(FALSE);
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

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
    DmfCallbacksDmf_GpioTarget.ChildModulesAdd = DMF_GpioTarget_ChildModulesAdd;
    DmfCallbacksDmf_GpioTarget.DeviceOpen = DMF_GpioTarget_Open;
    DmfCallbacksDmf_GpioTarget.DeviceClose = DMF_GpioTarget_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_GpioTarget,
                                            GpioTarget,
                                            DMF_CONTEXT_GpioTarget,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    DmfModuleDescriptor_GpioTarget.CallbacksDmf = &DmfCallbacksDmf_GpioTarget;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_GpioTarget,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
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

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_GpioTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_InterruptResource_InterruptAcquireLock(moduleContext->DmfModuleInterruptResource);

    FuncExitVoid(DMF_TRACE);
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

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_GpioTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_InterruptResource_InterruptReleaseLock(moduleContext->DmfModuleInterruptResource);

    FuncExitVoid(DMF_TRACE);
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

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_GpioTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    returnValue = DMF_InterruptResource_InterruptTryToAcquireLock(moduleContext->DmfModuleInterruptResource);

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

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

    GPIOs may or may not be present on some systems. This function returns the number of Gpio
    resources assigned for drivers where it is not known the GPIOs exist.

Arguments:

    DmfModule - This Module's handle.
    GpioConnectionAssigned - Is Gpio connection assigned to this Module instance.
    InterruptAssigned - Is Interrupt assigned to this Module instance.

Return Value:

    None

--*/
{
    DMF_CONTEXT_GpioTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_GpioTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (GpioConnectionAssigned != NULL)
    {
        *GpioConnectionAssigned = moduleContext->GpioConnectionAssigned;
    }

    if (InterruptAssigned != NULL)
    {
        DMF_InterruptResource_IsResourceAssigned(moduleContext->DmfModuleInterruptResource,
                                                 InterruptAssigned);
    }

    FuncExitVoid(DMF_TRACE);
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

    FuncEntry(DMF_TRACE);

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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "GPIO Target is invalid. Please make sure GpioTargetIO is configured to read ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE);

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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "GPIO Target is invalid. Please make sure GpioTargetIO is configured. ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// eof: Dmf_GpioTarget.c
//
