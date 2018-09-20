/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Template.c

Abstract:

    Template that is used to create additional DMF Modules from scratch.

    To use this template:

    1. Save this file as a new file with the name of the new DMF Module to create. For example, Dmf_NewModule.c.
    2. Save the corresponding file, Dmf_Template.h, as Dmf_NewModule.h.
    3. Open the two new files and replace "Template" with the new name, e.g., "NewModule".
    4. Correct the Abstract (delete all of this, of course).
    5. Search for "TEMPLATE:" and make appropriate modifications as indicated.
    6. Update DmfModuleId.h to add DMF_ModuleId_NewModule.
    7. Delete all callbacks that the new Module does not need. (Most will probably not be needed.)

    NOTE: This file contains all the function headers for the most commonly needed DMF Callbacks. You should use 
          these as a basis for your own code to save time. They should be copied and then updated to add specific 
          details about the purpose of the function.

          EvtDevicePrepareHardware/EvtDeviceReleaseHardare/EvtDeviceIoControl/EvtDeviceIoInternalControl are not 
          included here because they are rarely used.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
// NOTE: If you are using a super set of the DMF Library, then use that Library's include file instead.
//       That include file will also include the Library include files of all Libraries the superset
//       depends on.
//
#include "DmfModules.Template.h"
#include "DmfModules.Template.Trace.h"

#include "Dmf_Template.tmh"

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
    // TEMPLATE: Put data needed to support this DMF Module.
    //
    ULONG Template;
} DMF_CONTEXT_Template;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Template)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(Template)

// Memory Pool Tag.
//
#define MemoryTag 'MpmT'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// TEMPLATE: Add PRIVATE functions used by this DMF Module. Make sure to tag them as "static" to
//           enforce privacy.
//

// 'unreferenced local function has been removed'
// TODO: Remove this is non-template code.
//
#pragma push()
#pragma warning(disable:4505)

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
Tempate_ModuleSupportFunction(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    A sample Module support function. It is just provided as a sample to show the naming convention.
    Support functions are:
        1. Always "static".
        2. Begin with the Module id, e.g., "Template".

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// 'unreferenced local function has been removed'
// TODO: Remove this is non-template code.
//
#pragma pop()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Wdf Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Template_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Template callback for ModuleD0Entry for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

   NTSTATUS of either the given DMF Module's Open Callback or STATUS_SUCCESS.

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(PreviousState);

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Template_ModuleD0EntryPostInterruptsEnabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Template callback for ModuleD0EntryPostInterruptsEnabled for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

   NTSTATUS of either the given DMF Module's Open Callback or STATUS_SUCCESS.

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(PreviousState);

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_Template_ModuleD0ExitPreInterruptsDisabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Template callback for ModuleD0ExitPreInterruptsDisabled for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(TargetState);

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    FuncExitVoid(DMF_TRACE);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_Template_ModuleD0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Template callback for ModuleD0Exit for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(TargetState);

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    FuncExitVoid(DMF_TRACE);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Template_SelfManagedIoCleanup(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Template callback for ModuleSelfManagedIoCleanup for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Template_SelfManagedIoFlush(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Template callback for ModuleSelfManagedIoFlush for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_Template_SelfManagedIoInit(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Template callback for ModuleSelfManagedIoInit for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_Template_SelfManagedIoSuspend(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Template callback for ModuleSelfManagedIoSuspend for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_Template_SelfManagedIoRestart(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Template callback for ModuleSelfManagedIoRestart for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Template_SurpriseRemoval(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Template callback for ModuleSurpriseRemoval for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfModule);

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
BOOLEAN
DMF_Template_ModuleFileCreate(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Template callback for ModuleFileCreate for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    Device - WDF device object.
    Request - WDF Request with IOCTL parameters.
    FileObject - WDF file object that describes a file that is being opened for the specified request

Return Value:

    Return FALSE to indicated that this Module does not support File Create.

--*/
{
    BOOLEAN returnValue;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(FileObject);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    returnValue = FALSE;

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
BOOLEAN
DMF_Template_ModuleFileCleanup(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Template callback for ModuleFileCleanup for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    FileObject - WDF file object

Return Value:

    None

--*/
{
    BOOLEAN returnValue;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(FileObject);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    returnValue = FALSE;

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
BOOLEAN
DMF_Template_ModuleFileClose(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Template callback for ModuleFileClose for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    FileObject - WDF file object

Return Value:

    None

--*/
{
    BOOLEAN returnValue;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(FileObject);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    returnValue = FALSE;

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Template_ChildModulesAdd(
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
    DMF_CONFIG_Template* moduleConfig;
    DMF_CONTEXT_Template* moduleContext;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);
    UNREFERENCED_PARAMETER(DmfModuleInit);

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // TODO: Add Child Module here as needed. See other Module for examples.
    //

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_Template_ResourcesAssign(
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
    DMF_CONTEXT_Template* moduleContext;
    ULONG resourceCount;
    ULONG resourceIndex;
    NTSTATUS ntStatus;
    DMF_CONFIG_Template* moduleConfig;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(ResourcesRaw);

    FuncEntry(DMF_TRACE);

    ASSERT(ResourcesRaw != NULL);
    ASSERT(ResourcesTranslated != NULL);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Check the number of resources for the button device.
    //
    resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);

    // Parse the resources. This Module cares about GPIO and Interrupt resources.
    //
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

        // TODO: Write code to find needed resources.
        //
    }

    // TODO: Validate and initialize resources.
    //

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
DMF_Template_NotificationRegister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This callback is called when the Module Open Flags indicate that the this Module
    is opened after an asynchronous notification has happened.
    (DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware or DMF_MODULE_OPEN_OPTION_NOTIFY_D0Entry)
    This callback registers the notification.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_Template_NotificationUnregister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This callback is called when the Module Open Flags indicate that the this Module
    is opened after an asynchronous notification has happened.
    (DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware or DMF_MODULE_OPEN_OPTION_NOTIFY_D0Entry)
    This callback unregisters the notification that was previously registered.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Template_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Template.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_Template_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Template.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// TEMPLATE: Update this table to override default DMF Module Handlers.
//           Make sure to update the size of the PRIVATE_CONTEXT and OPEN_CONTEXT.
// TEMPLATE: Replace "_Template_" with "_GENERIC_" for all callbacks that are not implemented
//           in this Module.
//
static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_Template;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_Template;
static DMF_CALLBACKS_WDF DmfCallbacksWdf_Template;
static DMF_ModuleTransportMethod DMF_Template_TransportMethod;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Template_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Template.

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

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_Template);
    DmfCallbacksDmf_Template.ChildModulesAdd = DMF_Template_ChildModulesAdd;
    DmfCallbacksDmf_Template.DeviceResourcesAssign = DMF_Template_ResourcesAssign;
    DmfCallbacksDmf_Template.DeviceOpen = DMF_Template_Open;
    DmfCallbacksDmf_Template.DeviceClose = DMF_Template_Close;
    DmfCallbacksDmf_Template.DeviceNotificationRegister = DMF_Template_NotificationRegister;
    DmfCallbacksDmf_Template.DeviceNotificationUnregister = DMF_Template_NotificationUnregister;

    DMF_CALLBACKS_WDF_INIT(&DmfCallbacksWdf_Template);
    DmfCallbacksWdf_Template.ModuleSelfManagedIoInit = DMF_Template_SelfManagedIoInit;
    DmfCallbacksWdf_Template.ModuleSelfManagedIoSuspend = DMF_Template_SelfManagedIoSuspend;
    DmfCallbacksWdf_Template.ModuleSelfManagedIoRestart = DMF_Template_SelfManagedIoRestart;
    DmfCallbacksWdf_Template.ModuleSelfManagedIoFlush = DMF_Template_SelfManagedIoFlush;
    DmfCallbacksWdf_Template.ModuleSelfManagedIoCleanup = DMF_Template_SelfManagedIoCleanup;
    DmfCallbacksWdf_Template.ModuleSurpriseRemoval = DMF_Template_SurpriseRemoval;
    DmfCallbacksWdf_Template.ModuleD0Entry = DMF_Template_ModuleD0Entry;
    DmfCallbacksWdf_Template.ModuleD0EntryPostInterruptsEnabled = DMF_Template_ModuleD0EntryPostInterruptsEnabled;
    DmfCallbacksWdf_Template.ModuleD0ExitPreInterruptsDisabled = DMF_Template_ModuleD0ExitPreInterruptsDisabled;
    DmfCallbacksWdf_Template.ModuleD0Exit = DMF_Template_ModuleD0Exit;
    DmfCallbacksWdf_Template.ModuleFileCreate = DMF_Template_ModuleFileCreate;
    DmfCallbacksWdf_Template.ModuleFileCleanup = DMF_Template_ModuleFileCleanup;
    DmfCallbacksWdf_Template.ModuleFileClose = DMF_Template_ModuleFileClose;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_Template,
                                            Template,
                                            DMF_CONTEXT_Template,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    DmfModuleDescriptor_Template.CallbacksDmf = &DmfCallbacksDmf_Template;
    DmfModuleDescriptor_Template.CallbacksWdf = &DmfCallbacksWdf_Template;
    DmfModuleDescriptor_Template.ModuleConfigSize = sizeof(DMF_CONFIG_Template);
    // NOTE: This is only used for Transport Modules.
    //
    DmfModuleDescriptor_Template.ModuleTransportMethod = DMF_Template_TransportMethod;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_Template,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// TEMPLATE: Add publicly accessible functions that allow Clients to access custom functionality of this Module.
//

// TEMPLATE: Sample Transport Method. Implement only when the Module is a Transport Module.
//

// NOTE: It is still possible to define Module Methods. Those can be called by the Transport Method.
//       Thus, it is possible to use any Module as a Transport Module as long as it has a Transport Method.
//
NTSTATUS
DMF_Template_TransportMethod(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG Message,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize
    )
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferSize);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);

    DMF_ObjectValidate(DmfModule);

    ntStatus = STATUS_SUCCESS;

    switch (Message)
    {
    case Template_TransportMessage_Foo:
    {
        break;
    }
    case Template_TransportMessage_Bar:
    {
        break;
    }
    default:
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        break;
    }
    }

    return ntStatus;
}

// eof: Dmf_Template.c
//
