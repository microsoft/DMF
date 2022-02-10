/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_SpbTarget.c

Abstract:

    Supports SPB targets.

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
#include "Dmf_SpbTarget.tmh"
#endif

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
typedef struct _DMF_CONTEXT_SpbTarget
{
    // Resources assigned.
    //
    BOOLEAN SpbConnectionAssigned;

    // SPB Line Index that is instantiated in this object.
    //
    ULONG SpbTargetLineIndex;

    // Resource information for SPB device.
    //
    CM_PARTIAL_RESOURCE_DESCRIPTOR SpbTargetConnection;

    // Interrupt object.
    //
    WDFINTERRUPT Interrupt;

    // SPB controller target
    //
    WDFIOTARGET SpbController;

    // Support for building and sending WDFREQUESTS.
    //
    DMFMODULE DmfModuleRequestTarget;

    // InterruptResource.
    //
    DMFMODULE DmfModuleInterruptResource;

    // Optional Callback from ISR (with Interrupt Spin Lock held).
    //
    EVT_DMF_InterruptResource_InterruptIsr* EvtSpbTargetInterruptIsr;
    // Optional Callback at DPC_LEVEL Level.
    //
    EVT_DMF_InterruptResource_InterruptDpc* EvtSpbTargetInterruptDpc;
    // Optional Callback at PASSIVE_LEVEL Level.
    //
    EVT_DMF_InterruptResource_InterruptPassive* EvtSpbTargetInterruptPassive;
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

_Must_inspect_result_
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

_Must_inspect_result_
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

_Must_inspect_result_
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

EVT_DMF_InterruptResource_InterruptIsr SpbTarget_InterruptIsr;

_Function_class_(EVT_DMF_InterruptResource_InterruptIsr)
_IRQL_requires_same_
BOOLEAN
SpbTarget_InterruptIsr(
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
    DMFMODULE DmfModuleSpbTarget;
    DMF_CONTEXT_SpbTarget* moduleContext;
    BOOLEAN returnValue;

    DmfModuleSpbTarget = DMF_ParentModuleGet(DmfModuleInterruptResource);
    moduleContext = DMF_CONTEXT_GET(DmfModuleSpbTarget);

    DmfAssert(moduleContext->EvtSpbTargetInterruptIsr != NULL);
    returnValue = moduleContext->EvtSpbTargetInterruptIsr(DmfModuleSpbTarget,
                                                          MessageId,
                                                          QueuedWorkItem);

    return returnValue;
}

EVT_DMF_InterruptResource_InterruptDpc SpbTarget_InterruptDpc;

_Function_class_(EVT_DMF_InterruptResource_InterruptDpc)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
SpbTarget_InterruptDpc(
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
    DMFMODULE DmfModuleSpbTarget;
    DMF_CONTEXT_SpbTarget* moduleContext;

    DmfModuleSpbTarget = DMF_ParentModuleGet(DmfModuleInterruptResource);
    moduleContext = DMF_CONTEXT_GET(DmfModuleSpbTarget);

    DmfAssert(moduleContext->EvtSpbTargetInterruptIsr != NULL);
    moduleContext->EvtSpbTargetInterruptDpc(DmfModuleSpbTarget,
                                            QueuedWorkItem);
}

EVT_DMF_InterruptResource_InterruptPassive SpbTarget_InterruptPassive;

_Function_class_(EVT_DMF_InterruptResource_InterruptPassive)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
SpbTarget_InterruptPassive(
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
    DMFMODULE DmfModuleSpbTarget;
    DMF_CONTEXT_SpbTarget* moduleContext;

    DmfModuleSpbTarget = DMF_ParentModuleGet(DmfModuleInterruptResource);
    moduleContext = DMF_CONTEXT_GET(DmfModuleSpbTarget);

    DmfAssert(moduleContext->EvtSpbTargetInterruptPassive != NULL);
    moduleContext->EvtSpbTargetInterruptPassive(DmfModuleSpbTarget);
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
    DMF_CONFIG_SpbTarget* moduleConfig;
    DMF_CONTEXT_SpbTarget* moduleContext;
    DMF_CONFIG_InterruptResource configInterruptResource;

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
        moduleContext->EvtSpbTargetInterruptIsr = moduleConfig->InterruptResource.EvtInterruptResourceInterruptIsr;
        configInterruptResource.EvtInterruptResourceInterruptIsr = SpbTarget_InterruptIsr;
    }
    if (moduleConfig->InterruptResource.EvtInterruptResourceInterruptDpc != NULL)
    {
        moduleContext->EvtSpbTargetInterruptDpc = moduleConfig->InterruptResource.EvtInterruptResourceInterruptDpc;
        configInterruptResource.EvtInterruptResourceInterruptDpc = SpbTarget_InterruptDpc;
    }
    if (moduleConfig->InterruptResource.EvtInterruptResourceInterruptPassive != NULL)
    {
        moduleContext->EvtSpbTargetInterruptPassive = moduleConfig->InterruptResource.EvtInterruptResourceInterruptPassive;
        configInterruptResource.EvtInterruptResourceInterruptPassive = SpbTarget_InterruptPassive;
    }
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleInterruptResource);

    // RequestTarget
    // -------------
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

#pragma code_seg("PAGE")
_Function_class_(DMF_ResourcesAssign)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
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
    DMF_CONFIG_SpbTarget* moduleConfig;
    ULONG spbConnectionIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(ResourcesRaw);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    moduleContext->SpbConnectionAssigned = FALSE;

    // Check the number of resources for the button device.
    //
    resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);

    // Parse the resources. This Module cares about SPB resources.
    //
    spbConnectionIndex = 0;
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
                if (moduleConfig->SpbConnectionIndex == spbConnectionIndex)
                {
                    // Store the index of the SPB line that is instantiated.
                    // (For debug purposes only.)
                    //
                    moduleContext->SpbTargetLineIndex = spbConnectionIndex;

                    // Assign the information needed to open the target.
                    //
                    moduleContext->SpbTargetConnection = *descriptor;

                    moduleContext->SpbConnectionAssigned = TRUE;

                    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Assign: SpbTargetLineIndex=%d",
                                moduleContext->SpbTargetLineIndex);
                }

                spbConnectionIndex++;

                TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "CmResourceTypeConnection 0x%08X:%08X",
                            descriptor->u.Connection.IdHighPart,
                            descriptor->u.Connection.IdLowPart);
            }
        }
        else
        {
            // All other resources are ignored by this Module.
            //
        }
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "SpbConnectionAssigned=%d SpbConnectionMandatory=%d", 
                moduleContext->SpbConnectionAssigned, 
                moduleConfig->SpbConnectionMandatory);

    //  Validate SPB connection with the Client Driver's requirements.
    //
    if (moduleConfig->SpbConnectionMandatory && 
        (! moduleContext->SpbConnectionAssigned))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Spb Connection not assigned");
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
        DmfAssert(FALSE);
        goto Exit;
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
_Function_class_(DMF_Close)
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
        moduleContext->Interrupt = NULL;
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

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_SpbTarget_TransportAddressRead(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload
    )
/*++

Routine Description:

    This routine sends a write-read sequence to the SPB controller. It reads a buffer from a particular address.

Arguments:

    DmfInterface - Interface Module handle.
    Payload - Data to write and read

Return Value:

    ntStatus

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE BusTransportModule;

    BusTransportModule = DMF_InterfaceTransportModuleGet(DmfInterface);

    ntStatus = DMF_SpbTarget_BufferWriteRead(BusTransportModule,
                                             Payload->AddressRead.Address,
                                             Payload->AddressRead.AddressLength,
                                             Payload->AddressRead.Buffer,
                                             Payload->AddressRead.BufferLength);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_SpbTarget_BufferWriteRead fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_SpbTarget_TransportAddressReadEx(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload,
    _In_ ULONG RequestTimeoutMilliseconds
    )
/*++

Routine Description:

    Write and read data from Spb.

Arguments:

    DmfInterface - Interface Module handle.
    Payload - Data to write and read
    RequestTimeoutMilliseconds - Timeout in milliseconds.

Return Value:

    ntStatus

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE BusTransportModule;

    BusTransportModule = DMF_InterfaceTransportModuleGet(DmfInterface);

    ntStatus = DMF_SpbTarget_BufferWriteReadEx(BusTransportModule,
                                               Payload->AddressRead.Address,
                                               Payload->AddressRead.AddressLength,
                                               Payload->AddressRead.Buffer,
                                               Payload->AddressRead.BufferLength,
                                               RequestTimeoutMilliseconds);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_SpbTarget_BufferWriteReadEx fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_SpbTarget_TransportBufferWriteEx(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload,
    _In_ ULONG RequestTimeoutMilliseconds
    )
/*++

Routine Description:

    This routine writes to the SPB controller. It also has a timeout for the Request sent down the device stack.

Arguments:

    DmfInterface - Interface Module handle.
    Payload - Data to write.
    RequestTimeoutMilliseconds - Timeout in milliseconds.

Return Value:

    ntStatus

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE BusTransportModule;

    BusTransportModule = DMF_InterfaceTransportModuleGet(DmfInterface);

    ntStatus = DMF_SpbTarget_BufferWriteEx(BusTransportModule,
                                           Payload->BufferWrite.Buffer,
                                           Payload->BufferWrite.BufferLength,
                                           RequestTimeoutMilliseconds);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_SpbTarget_BufferWrite fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_SpbTarget_TransportBufferWrite(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload
    )
/*++

Routine Description:

    This routine writes to the SPB controller.

Arguments:

    DmfInterface - Interface Module handle.
    Payload - Data to write.

Return Value:

    ntStatus

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE BusTransportModule;

    BusTransportModule = DMF_InterfaceTransportModuleGet(DmfInterface);

    ntStatus = DMF_SpbTarget_BufferWrite(BusTransportModule,
                                         Payload->BufferWrite.Buffer,
                                         Payload->BufferWrite.BufferLength);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_SpbTarget_BufferWrite fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_SpbBusTarget_TransportBind(
    _In_ DMFINTERFACE DmfInterface,
    _In_ DMF_INTERFACE_PROTOCOL_BusTarget_BIND_DATA* ProtocolBindData,
    _Inout_opt_ DMF_INTERFACE_TRANSPORT_BusTarget_BIND_DATA* TransportBindData
    )
/*++

Routine Description:

    Does nothing

Arguments:

    DmfInterface - Interface Module handle.
    ProtocolBindData
    TransportBindData

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);
    UNREFERENCED_PARAMETER(ProtocolBindData);
    UNREFERENCED_PARAMETER(TransportBindData);

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_SpbBusTarget_TransportUnbind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Does nothing

Arguments:

    DmfInterface - Interface Module handle.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_SpbTarget_TransportPostBind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Does nothing

Arguments:

    DmfInterface - Interface Module handle.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_SpbTarget_TransportPreUnbind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Does nothing

Arguments:

    DmfInterface - Interface Module handle.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);
}

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_SpbTarget;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_SpbTarget;
    DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA BusTargetDeclarationData;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_SpbTarget);
    dmfCallbacksDmf_SpbTarget.ChildModulesAdd = DMF_SpbTarget_ChildModulesAdd;
    dmfCallbacksDmf_SpbTarget.DeviceResourcesAssign = DMF_SpbTarget_ResourcesAssign;
    dmfCallbacksDmf_SpbTarget.DeviceOpen = DMF_SpbTarget_Open;
    dmfCallbacksDmf_SpbTarget.DeviceClose = DMF_SpbTarget_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_SpbTarget,
                                            SpbTarget,
                                            DMF_CONTEXT_SpbTarget,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_SpbTarget.CallbacksDmf = &dmfCallbacksDmf_SpbTarget;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_SpbTarget,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    DMF_INTERFACE_TRANSPORT_BusTarget_DESCRIPTOR_INIT(&BusTargetDeclarationData,
                                                      DMF_SpbTarget_TransportPostBind,
                                                      DMF_SpbTarget_TransportPreUnbind,
                                                      DMF_SpbBusTarget_TransportBind,
                                                      DMF_SpbBusTarget_TransportUnbind,
                                                      NULL,
                                                      DMF_SpbTarget_TransportAddressRead,
                                                      DMF_SpbTarget_TransportBufferWrite,
                                                      NULL,
                                                      DMF_SpbTarget_TransportAddressReadEx,
                                                      DMF_SpbTarget_TransportBufferWriteEx);

    // Add the interface to the Transport Module.
    //
    ntStatus = DMF_ModuleInterfaceDescriptorAdd(*DmfModule,
                                                (DMF_INTERFACE_DESCRIPTOR*)&BusTargetDeclarationData);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleInterfaceDescriptorAdd fails: ntStatus=%!STATUS!", ntStatus);
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
_Must_inspect_result_                                                                                               \
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
_Must_inspect_result_
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
_Must_inspect_result_
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
_Must_inspect_result_
NTSTATUS
DMF_SpbTarget_BufferWriteEx(
    _In_ DMFMODULE DmfModule,
    _In_reads_(NumberOfBytesToWrite) UCHAR* BufferToWrite,
    _In_ ULONG NumberOfBytesToWrite,
    _In_ ULONG RequestTimeoutMilliseconds
    )
/*++
 
  Routine Description:

    This routine writes to the SPB controller. It also has a timeout for the request sent down the device stack.

  Arguments:

    DmfModule - This Module's Module handle.
    BufferToWrite- Data to write to the device.
    NumberOfBytesToWrite - Length of BufferToWrite.
    RequestTimeoutMilliseconds - Timeout in milliseconds.

  Return Value:

    NTSTATUS

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
                                                   RequestTimeoutMilliseconds,
                                                   &bytesWritten);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SpbTarget_BufferWriteReadEx(
    _In_ DMFMODULE DmfModule,
    _In_reads_(InputBufferLength) UCHAR* InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_(OutputBufferLength) UCHAR* OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _In_ ULONG RequestTimeoutMilliseconds
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
    RequestTimeoutMilliseconds - Timeout in milliseconds.

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
                                                   RequestTimeoutMilliseconds,
                                                   &bytesWritten);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
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

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 SpbTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_InterruptResource_InterruptAcquireLock(moduleContext->DmfModuleInterruptResource);

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

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 SpbTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_InterruptResource_InterruptReleaseLock(moduleContext->DmfModuleInterruptResource);

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

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 SpbTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    returnValue = DMF_InterruptResource_InterruptTryToAcquireLock(moduleContext->DmfModuleInterruptResource);

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

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 SpbTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (SpbConnectionAssigned != NULL)
    {
        *SpbConnectionAssigned = moduleContext->SpbConnectionAssigned;
    }

    if (InterruptAssigned != NULL)
    {
        DMF_InterruptResource_IsResourceAssigned(moduleContext->DmfModuleInterruptResource,
                                                 InterruptAssigned);
    }

    FuncExitVoid(DMF_TRACE);
}

// eof: Dmf_SpbTarget.c
//
