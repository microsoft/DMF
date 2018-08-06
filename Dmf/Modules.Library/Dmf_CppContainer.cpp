/*++

    Copyright (c) 2015 Microsoft Corporation. All Rights Reserved.

Module Name:

    Dmf_CppContainer.cpp

Abstract:

    Template for User Mode driver Container Module.

Environment:

    User-mode Driver Framework

--*/

// The Dmf Library.
//
#include "DmfModule.h"

#define WPP_CONTROL_GUIDS                                                                                                                   \
    WPP_DEFINE_CONTROL_GUID(                                                                                                                \
        DMF_TraceGuid_CppContainer, (25AB6EA0,DMF_DriverId,DMF_ModuleId_CppContainer,A85C,2C29C1C3FA97),                                    \
        WPP_DEFINE_BIT(DMF_TRACE_CppContainer)                                                                                              \
        )                                                                                                                                   \
    WPP_DEFINE_CONTROL_GUID(                                                                                                                \
        DMF_TraceGuid_CCppContainedObject, (25AB6EA0,DMF_DriverId,DMF_ModuleId_CCppContainedObject,A85C,2C29C1C3FA97),                      \
        WPP_DEFINE_BIT(DMF_TRACE_CCppContainedObject)                                                                                       \
        )                                                                                                                                   \

// NOTE: Do not put INITGUID before this. You will be sad.
//
#include "..\..\User\General\CCppContainedObject.h"

#define INITGUID

//#include "Dmf_CppContainer_Public.h"

// WPP Tracing.
// NOTE: This must be the last include in this file.
//
#include "Dmf_CppContainer.tmh"

// Contains elements needed to send Requests to this driver.
//
typedef struct
{
    // TEMPLATE: Declare a POINTER to the object so it will be dynamically allocated. In this way,
    //           the constructors/destructors will be claled.
    //
    CCppContainedObject* CCppContainedObject;

    // TEMPLATE: Declare other structures as needed.
    //
} DMF_CONTEXT_CppContainer, *PDMF_CONTEXT_CppContainer;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(CppContainer)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(CppContainer)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Dmf Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// TEMPLATE: Here you put static methods that are needed to fullfill the requirements 
//           of the Module Entry Points.
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Wdf Module Entry Points
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Dmf Module Entry Points
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static
_Check_return_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_CppContainer_ModuleDeviceControl(
    _In_ DMFMODULE DmfModule,
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

    DmfModule - This module's Dmf Module.

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

    TRUE if this routine handled the request.

--*/
{
    BOOLEAN handled;
    PDMF_CONTEXT_CppContainer moduleContext;
    size_t bytesReturned;
    BOOLEAN requestHasBeenCompletedOrIsHeld;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    PAGED_CODE();

    FuncEntry(DMF_TRACE_CppContainer);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ASSERT(moduleContext != NULL);

    handled = FALSE;
    bytesReturned = 0;
    requestHasBeenCompletedOrIsHeld = FALSE;
    ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    // TEMPLATE: Call the object's DeviceIoControl method if there is one.
    //           Work can be done either in the container or contained object.
    //
    moduleContext->CCppContainedObject->DeviceIoControl();

    switch(IoControlCode) 
    {
        // TEMPLATE: Add IOCTLS as needed. If you don't have IOCTLs, then you don't need to define 
        //           this function.
        //
        case 0:
        default:
        {
            // Don't complete the request. It belongs to another Module.
            //
            requestHasBeenCompletedOrIsHeld = TRUE;
            ASSERT(! handled);
            break;
        }
    }

    if (! requestHasBeenCompletedOrIsHeld) 
    {
        // Only complete the request (1) it is handled by this module, (2) has not been completed above and 
        // (3) is not enqueued aobve.
        // 
        WdfRequestCompleteWithInformation(Request, 
                                          ntStatus, 
                                          bytesReturned);
    }

    FuncExitVoid(DMF_TRACE_CppContainer);

    return handled;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_CppContainer_Destroy(
    _In_ DMFMODULE DmfModule
    )
{
    PDMF_CONTEXT_CppContainer moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_CppContainer);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ASSERT(moduleContext != NULL);
    
    // TEMPLATE: Destroy any child modules created in the Module Create 
    //           callback.
    //

    // Now, destroy this module.
    //
    DMF_ModuleDestroy(DmfModule);
    DmfModule = NULL;

    FuncExitVoid(DMF_TRACE_CppContainer);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_CppContainer_PrepareHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
{
    NTSTATUS ntStatus;
    PDMF_CONTEXT_CppContainer moduleContext;

    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    FuncEntry(DMF_TRACE_CppContainer);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ASSERT(moduleContext != NULL);

    // Open this object here.
    //
    ntStatus = DMF_ModuleOpen(DmfModule);
    if ( ! NT_SUCCESS(ntStatus) )
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_CppContainer, "ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // TEMPLATE: Optionally do work, however, that works was probably done above in Open callback.
    //
    ntStatus = moduleContext->CCppContainedObject->PrepareHardware();

Exit:

    FuncExit(DMF_TRACE_CppContainer, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_CppContainer_ReleaseHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
{
    NTSTATUS ntStatus;
    PDMF_CONTEXT_CppContainer moduleContext;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    FuncEntry(DMF_TRACE_CppContainer);

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ASSERT(moduleContext != NULL);

    // TEMPLATE: Optionally do work, however, that works was probably done above in Close callback.
    //
    moduleContext->CCppContainedObject->ReleaseHardware();

    // Close down here.
    //
    DMF_ModuleClose(DmfModule);
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE_CppContainer, "CppContainer CLOSED");

    FuncExit(DMF_TRACE_CppContainer, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_ 
static
NTSTATUS 
DMF_CppContainer_Open(
    _In_ DMFMODULE DmfModule
    )
{
    NTSTATUS ntStatus;
    PDMF_CONTEXT_CppContainer moduleContext;
    WDFDEVICE device;
    HRESULT result;

    PAGED_CODE();
    
    FuncEntry(DMF_TRACE_CppContainer);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ASSERT(moduleContext != NULL);
    
    device = DMF_AttachedDeviceGet(DmfModule);

    // TEMPLATE: Open any child modules that the object PREVIOUSLY created.
    //

    // TEMPLATE: Create the contained object.
    //
    ASSERT(NULL == moduleContext->CCppContainedObject);
#pragma warning(suppress: 28197)
#pragma warning(suppress: 6014)
    moduleContext->CCppContainedObject = new CCppContainedObject();
    if (NULL == moduleContext->CCppContainedObject)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }
    
    // Initialize the contained object. It prepares the data structure
    // for further transactions.
    //
    result = moduleContext->CCppContainedObject->Initialize();
    if (result != S_OK)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_CppContainer, "CppContainer->Initialize result=%X", result);
        ntStatus = STATUS_INVALID_DEVICE_STATE;
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE_CppContainer, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID 
DMF_CppContainer_Close(
    _In_ DMFMODULE DmfModule
    )
{
    PDMF_CONTEXT_CppContainer moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_CppContainer);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ASSERT(moduleContext != NULL);

    // TEMPLATE: Do the oposite of what you did in the open handler.
    //

    // TEMPLATE: Call the contained object's destructor here.
    //
    if (moduleContext->CCppContainedObject != NULL)
    {
        // No need to call Uninitalize. It happens in the destructor.
        //
        delete moduleContext->CCppContainedObject;
        moduleContext->CCppContainedObject = NULL;
    }

    FuncExitVoid(DMF_TRACE_CppContainer);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_ 
static
NTSTATUS 
DMF_CppContainer_ModuleD0Entry(
    _In_ DMFMODULE DmfModule, 
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
{
    NTSTATUS ntStatus;
    PDMF_CONTEXT_CppContainer moduleContext;

    FuncEntry(DMF_TRACE_CppContainer);
    
    ntStatus = STATUS_SUCCESS;

    if (PreviousState == WdfPowerDeviceD3)
    {
        TraceInformation(DMF_TRACE_CppContainer, "Return from Hibernate PreviousState=%d", PreviousState);

        moduleContext = DMF_CONTEXT_GET(DmfModule);
        ASSERT(moduleContext != NULL);

        // TEMPLATE: Call the contained object's D0Entry code.
        //
        ntStatus = moduleContext->CCppContainedObject->D0Entry();
    }

    FuncExit(DMF_TRACE_CppContainer, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_CppContainer_ModuleD0Exit(
    _In_ DMFMODULE DmfModule, 
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
{
    NTSTATUS ntStatus;
    PDMF_CONTEXT_CppContainer moduleContext;

    FuncEntry(DMF_TRACE_CppContainer);
    
    if (TargetState == WdfPowerDeviceD3)
    {
        TraceInformation(DMF_TRACE_CppContainer, "Enter into Hibernate TargetState=%d", TargetState);

        moduleContext = DMF_CONTEXT_GET(DmfModule);
        ASSERT(moduleContext != NULL);

       // TEMPLATE: Call the contained object's D0Exit code.
       //
       moduleContext->CCppContainedObject->D0Exit();
    }

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE_CppContainer, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Dmf Module Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_CppContainer;
static DMF_ENTRYPOINTS_DMF DmfEntrypointsDmf_CppContainer;
static DMF_ENTRYPOINTS_WDF DmfEntrypointsWdf_CppContainer;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_ 
NTSTATUS
DMF_CppContainer_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a Dmf Module of type CppContainer.

Arguments:

    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where we return the created DMFMODULE handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_CppContainer);

    DMF_ENTRYPOINTS_DMF_INIT(&DmfEntrypointsDmf_CppContainer);
    DmfEntrypointsDmf_CppContainer.ModuleInstanceDestroy = DMF_CppContainer_Destroy;
    DmfEntrypointsDmf_CppContainer.DeviceOpen = DMF_CppContainer_Open;
    DmfEntrypointsDmf_CppContainer.DeviceClose = DMF_CppContainer_Close;

    DMF_ENTRYPOINTS_WDF_INIT(&DmfEntrypointsWdf_CppContainer);
    DmfEntrypointsWdf_CppContainer.ModulePrepareHardware = DMF_CppContainer_PrepareHardware;
    DmfEntrypointsWdf_CppContainer.ModuleReleaseHardware = DMF_CppContainer_ReleaseHardware;
    DmfEntrypointsWdf_CppContainer.ModuleD0Entry = DMF_CppContainer_ModuleD0Entry;
    DmfEntrypointsWdf_CppContainer.ModuleD0Exit = DMF_CppContainer_ModuleD0Exit;
    DmfEntrypointsWdf_CppContainer.ModuleDeviceIoControl = DMF_CppContainer_ModuleDeviceControl;

    DMF_MODULE_DESCRIPTOR_INIT(DmfModuleDescriptor_CppContainer,
                               CppContainer,
                               DMF_MODULE_OPTIONS_PASSIVE,
                               DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    DmfModuleDescriptor_CppContainer.EntrypointsDmf = &DmfEntrypointsDmf_CppContainer;
    DmfModuleDescriptor_CppContainer.EntrypointsWdf = &DmfEntrypointsWdf_CppContainer;
    DmfModuleDescriptor_CppContainer.ModuleConfigSize = sizeof(DMF_CONFIG_CppContainer);

    // ObjectAttributes must be initialized and
    // ParentObject attribute must be set to either WDFDEVICE or DMFMODULE.
    //
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(ObjectAttributes,
                                           DMF_CONTEXT_CppContainer);

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_CppContainer,
                                DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_CppContainer, "DMF_ModuleCreate failed, ntStatus=%!STATUS!", ntStatus);
    }
    
    // TEMPLATE: If this module needs to create child modules, do it here.
    //

    FuncExit(DMF_TRACE_CppContainer, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}

// eof: Dmf_CppContainer.c
//
