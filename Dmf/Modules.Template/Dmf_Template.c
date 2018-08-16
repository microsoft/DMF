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

    NOTE: This file contains all the function headers for all the DMF Callbacks. You should use these as a basis
          for your own code to save time. They should be copied and then updated to add specific details about the
          purpose of the function.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
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

    FuncEntry(DMF_TRACE_Template);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE_Template, "ntStatus=%!STATUS!", ntStatus);

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

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Template_ModulePrepareHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Template callback for ModulePrepareHardware for a given DMF Module.
    In cases, where the Open Callback must be explicitly called by the Client Driver, this callback
    maybe overridden. Use this callback when the device's assigned resources must be accessed.

    NOTE: This function is the inverse of DMF_Template_ModuleReleaseHardware.

Arguments:

    DmfModule - This Module's handle.
    ResourcesRaw - WDF Resource Raw parameter that is passed to the given
                   DMF Module callback.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    FuncEntry(DMF_TRACE_Template);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE_Template, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_Template_ModuleReleaseHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Template callback for ModuleReleaseHardware for a given DMF Module. (In this case the given Dmf
    Module's Close Callback should be called in a symmetrical manner to how it was opened.)
    In cases, where the Close Callback must be explicitly called by the Client Driver, this callback
    maybe overridden.

    NOTE: This function is the inverse of DMF_Template_ModulePrepareHardware.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    FuncEntry(DMF_TRACE_Template);

    FuncExitVoid(DMF_TRACE_Template);

    return STATUS_SUCCESS;
}
#pragma code_seg()

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

    FuncEntry(DMF_TRACE_Template);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE_Template, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE_Template);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE_Template, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE_Template);

    ntStatus = STATUS_SUCCESS;

    FuncExitVoid(DMF_TRACE_Template);

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

    FuncEntry(DMF_TRACE_Template);

    ntStatus = STATUS_SUCCESS;

    FuncExitVoid(DMF_TRACE_Template);

    return ntStatus;
}

BOOLEAN
DMF_Template_ModuleDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    Template callback for ModuleDeviceIoControl for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    OutputBufferLength - For fast access to the Request's Output Buffer Length.
    InputBufferLength - For fast access to the Request's Input Buffer Length.
    IoControlCode - The given IOCTL code.

Return Value:

    NOTE: Return FALSE if this Module does not support (know) the IOCTL.

--*/
{
    BOOLEAN returnValue;

    FuncEntry(DMF_TRACE_Template);

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(IoControlCode);

    FuncEntry(DMF_TRACE_Template);

    returnValue = FALSE;

    FuncExit(DMF_TRACE_Template, "returnValue=%d", returnValue);

    return returnValue;
}

BOOLEAN
DMF_Template_ModuleInternalDeviceIoControl(
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
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    OutputBufferLength - For fast access to the Request's Output Buffer Length.
    InputBufferLength - For fast access to the Request's Input Buffer Length.
    IoControlCode - The given IOCTL code.

Return Value:

    NOTE: Return FALSE if this Module does not support (know) the IOCTL.

--*/
{
    BOOLEAN returnValue;

    FuncEntry(DMF_TRACE_Template);

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(IoControlCode);

    returnValue = FALSE;

    FuncExit(DMF_TRACE_Template, "returnValue=%d", returnValue);

    return returnValue;
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
    FuncEntry(DMF_TRACE_Template);

    UNREFERENCED_PARAMETER(DmfModule);

    FuncExitVoid(DMF_TRACE_Template);
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

    FuncEntry(DMF_TRACE_Template);

    returnValue = FALSE;

    FuncExit(DMF_TRACE_Template, "returnValue=%d", returnValue);

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

    FuncEntry(DMF_TRACE_Template);

    returnValue = FALSE;

    FuncExit(DMF_TRACE_Template, "returnValue=%d", returnValue);

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

    FuncEntry(DMF_TRACE_Template);

    returnValue = FALSE;

    FuncExit(DMF_TRACE_Template, "returnValue=%d", returnValue);

    return returnValue;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_Template_Destroy(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Destroy an instance of a Module of type Template. This code can be deleted
    because DMF performs this function automatically. In some cases
    you may need this if special code executed during Create that needs inverse code
    to run here, or if it is necessary to validate conditions when this call is made.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    PAGED_CODE();

    DMF_ModuleDestroy(DmfModule);
    DmfModule = NULL;
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

    FuncEntry(DMF_TRACE_Template);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE_Template, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE_Template);

    FuncExitVoid(DMF_TRACE_Template);
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

    FuncEntry(DMF_TRACE_Template);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE_Template, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE_Template);

    FuncExitVoid(DMF_TRACE_Template);
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
    DMFMODULE dmfModule;
    NTSTATUS ntStatus;
    DMF_CONTEXT_Template* moduleContext;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_Template);

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_Template);
    DmfCallbacksDmf_Template.ModuleInstanceDestroy = DMF_Template_Destroy;
    DmfCallbacksDmf_Template.DeviceOpen = DMF_Template_Open;
    DmfCallbacksDmf_Template.DeviceClose = DMF_Template_Close;
    DmfCallbacksDmf_Template.DeviceNotificationRegister = DMF_Template_NotificationRegister;
    DmfCallbacksDmf_Template.DeviceNotificationUnregister = DMF_Template_NotificationUnregister;

    DMF_CALLBACKS_WDF_INIT(&DmfCallbacksWdf_Template);
    DmfCallbacksWdf_Template.ModulePrepareHardware = DMF_Template_ModulePrepareHardware;
    DmfCallbacksWdf_Template.ModuleReleaseHardware = DMF_Template_ModuleReleaseHardware;
    DmfCallbacksWdf_Template.ModuleD0Entry = DMF_Template_ModuleD0Entry;
    DmfCallbacksWdf_Template.ModuleD0Exit = DMF_Template_ModuleD0Exit;
    DmfCallbacksWdf_Template.ModuleDeviceIoControl = DMF_Template_ModuleDeviceIoControl;
    DmfCallbacksWdf_Template.ModuleInternalDeviceIoControl = DMF_Template_ModuleInternalDeviceIoControl;
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
    DmfModuleDescriptor_Template.ModuleTransportMethod = DMF_Template_TransportMethod;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_Template,
                                &dmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_Template, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // TODO: Create DMF Child Modules if necessary.
    // --------------------------------------------
    //

    moduleContext = DMF_CONTEXT_GET(dmfModule);
    device = DMF_AttachedDeviceGet(dmfModule);

    // dmfModule will be set as ParentObject for all child modules.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = dmfModule;

    // ...
    //

Exit:

    *DmfModule = dmfModule;

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
