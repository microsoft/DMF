/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfGeneric.c

Abstract:

    DMF Implementation.
    This Module contains the default (generic) handlers for all DMF Module Callbacks.
    This allows DMF to perform validation, prevents the need for NULL pointer checking,
    and allows DMF to automatically support Module Callbacks as needed.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#include "DmfIncludeInternal.h"

#include "DmfGeneric.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DMF Module Callbacks (GENERIC)
//
// These functions are the default handlers for the DMF Module Callbacks.Some of these handlers
// are designed to execute when the DMF Module does not define them. Others are designed such that
// they will assert to indicate an invalid code path. Finally, the lock/unlock handlers almost always
// execute because the DMF Modules do not define them (although they can).
//
// One might consider these functions as the "Base Class" such that some of these functions are
// virtual functions and some are pure virtual functions.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Generic Callbacks. These handlers may be overridden by Client.
// ----------------------------------------------------------------
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
EVT_DMF_MODULE_Generic_OnDeviceNotificationOpen(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for EvtModuleOnDeviceNotificationOpen.
    It is a NOP.

Arguments:

    DmfModule - The Child Module from which this callback is called.

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p", DmfModule);

    // It is OK for this function to be called as NOP.
    //

    FuncExit(DMF_TRACE, "DmfModule=0x%p ntStatus=%!STATUS!", DmfModule, STATUS_SUCCESS);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_MODULE_Generic_OnDeviceNotificationPostOpen(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for EvtModuleOnDeviceNotificationPostOpen.
    It is a NOP.

Arguments:

    DmfModule - DMF Module

Return Value:

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p", DmfModule);

    // It is OK for this function to be called as NOP.
    //

    FuncExit(DMF_TRACE, "DmfModule=0x%p ntStatus=%!STATUS!", DmfModule, STATUS_SUCCESS);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_MODULE_Generic_OnDeviceNotificationPreClose(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for EvtModuleOnDeviceNotificationPreClose.
    It is a NOP.

Arguments:

    DmfModule - DMF Module

Return Value:

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p", DmfModule);

    // It is OK for this function to be called as NOP.
    //

    FuncExit(DMF_TRACE, "DmfModule=0x%p ntStatus=%!STATUS!", DmfModule, STATUS_SUCCESS);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_MODULE_Generic_OnDeviceNotificationClose(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for EvtModuleOnDeviceNotificationClose.
    It is a NOP.

Arguments:

    DmfModule - DMF Module

Return Value:

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p", DmfModule);

    // It is OK for this function to be called as NOP.
    //

    FuncExit(DMF_TRACE, "DmfModule=0x%p ntStatus=%!STATUS!", DmfModule, STATUS_SUCCESS);
}
#pragma code_seg()

VOID
DMF_Generic_Destroy(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for ModuleInstanceDestroy for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsCreatedOrClosed(dmfObject);

    FuncExit(DMF_TRACE, "DmfModule=0x%p ntStatus=%!STATUS!", DmfModule, STATUS_SUCCESS);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModulePrepareHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Generic callback for ModulePrepareHardware for a given DMF Module. This call is used for
    DMF Modules that indicate that the given DMF Module's Open Callback should be called
    during PrepareHardware.
    In cases, where the Open Callback must be explicitly called by the Client Driver, this callback
    maybe overridden.

    NOTE: This function is the inverse of DMF_Generic_ModuleReleaseHardware.

Arguments:

    DmfModule - The given DMF Module.
    ResourcesRaw - WDF Resource Raw parameter that is passed to the given
                   DMF Module callback.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.

Return Value:

   NTSTATUS of either the given DMF Module's Open Callback or STATUS_SUCCESS.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsCreatedOrOpened(dmfObject);

    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    // Always call ResourcesAssign callbacks unless DMF_MODULE_OPEN_OPTION_OPEN_Create is
    // specified because in that case there is no PrepareHardware to get resources.
    // This check is made to ensure that ASSERTs for Module validation work in ResourcesAssign.
    //
    if ((dmfObject->ModuleDescriptor.OpenOption != DMF_MODULE_OPEN_OPTION_OPEN_Create) &&
        (dmfObject->ModuleDescriptor.OpenOption != DMF_MODULE_OPEN_OPTION_NOTIFY_Create))
    {
        ntStatus = DMF_Internal_ResourcesAssign(DmfModule,
                                                ResourcesRaw,
                                                ResourcesTranslated);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Internal_ResourcesAssign ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    if (DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware == dmfObject->ModuleDescriptor.OpenOption)
    {
        // This Module is automatically opened in PrepareHardware.
        //
        ntStatus = DMF_Internal_Open(DmfModule);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleOpen ntStatus=%!STATUS!", ntStatus);
        }
        else
        {
            // Indicate when the Module was opened (for clean up operations).
            // Internal Open has set this value to Manual by default.
            //
            ASSERT(ModuleOpenedDuringType_Manual == dmfObject->ModuleOpenedDuring);
            dmfObject->ModuleOpenedDuring = ModuleOpenedDuringType_PrepareHardware;
        }
    }
    else if (DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware == dmfObject->ModuleDescriptor.OpenOption)
    {
        // This Module's Notification Registration is automatically opened in PrepareHardware.
        //
        ntStatus = DMF_Internal_NotificationRegister(DmfModule);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Module_NotificationRegister ntStatus=%!STATUS!", ntStatus);
        }
        else
        {
            //  Indicate when the Module was notification was registered for clean up operations.
            //
            ASSERT(ModuleOpenedDuringType_Invalid == dmfObject->ModuleNotificationRegisteredDuring);
            dmfObject->ModuleNotificationRegisteredDuring = ModuleOpenedDuringType_PrepareHardware;
        }
    }
    else if (dmfObject->ModuleDescriptor.OpenOption < DMF_MODULE_OPEN_OPTION_LAST)
    {
        // It means another valid option is selected and no further work is needed here.
        //
        ntStatus = STATUS_SUCCESS;
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid Code Path");
        ASSERT(FALSE);
        ntStatus = STATUS_INTERNAL_ERROR;
    }

Exit:

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Generic_ModuleReleaseHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Generic callback for ModuleReleaseHardware for a given DMF Module. This call is used for
    DMF Modules that indicate that the given DMF Module's Open Callback should be called
    during PrepareHardware. (In this case the given DMF Module's Close Callback should be
    called in a symmetrical manner to how it was opened.)
    In cases, where the Close Callback must be explicitly called by the Client Driver, this callback
    maybe overridden.

    NOTE: This function is the inverse of DMF_Generic_ModulePrepareHardware.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    // NOTE: Client Drivers have the option of closing modules at any time.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    // NOTE: This code is not totally symmetrical with DMF_Generic_ModulePrepareHardware because there is
    //       no corresponding DMF_Module_ResourcesAssign on the way down.
    //

    if (DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware == dmfObject->ModuleDescriptor.OpenOption)
    {
        // This Module is automatically closed in ReleaseHardware.
        // NOTE: ReleaseHardware is always called regardless of return status of PrepareHardare.
        //       Therefore, it is possible this Module may have been clean up if only some
        //       of the modules in the collection hare closed. So, check for that condition here.
        //
        if (dmfObject->ModuleOpenedDuring == ModuleOpenedDuringType_PrepareHardware)
        {
            DMF_Internal_Close(DmfModule);
        }
    }
    else if (DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware == dmfObject->ModuleDescriptor.OpenOption)
    {
        // This Module's Notification Registration is automatically closed in PrepareHardware.
        //
        DMF_Internal_NotificationUnregister(DmfModule);
    }
    else if (dmfObject->ModuleDescriptor.OpenOption < DMF_MODULE_OPEN_OPTION_LAST)
    {
        // It means another valid option is selected and no further work is needed here.
        //
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid Code Path");
        ASSERT(FALSE);
        ntStatus = STATUS_INTERNAL_ERROR;
    }

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Generic callback for ModuleD0Entry for a given DMF Module. This call is used for
    DMF Modules that indicate that the given DMF Module's Open Callback should be called
    during D0Entry.
    In cases, where the Open Callback must be explicitly called by the Client Driver, this callback
    maybe overridden.

    NOTE: This function is the inverse of DMF_Generic_ModuleD0Exit.

Arguments:

    DmfModule - The given DMF Module.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

   NTSTATUS of either the given DMF Module's Open Callback or STATUS_SUCCESS.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "dmfObject=0x%p PreviousState=%d", dmfObject, PreviousState);

    // NOTE: Modules can be closed in D0Exit.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    // NOTE: If the Module has a ResourceAssign handler, it will have been called by now.
    //

    if (DMF_MODULE_OPEN_OPTION_OPEN_D0Entry == dmfObject->ModuleDescriptor.OpenOption)
    {
        // This Module is automatically opened in D0Entry.
        //
        ntStatus = DMF_Internal_Open(DmfModule);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleOpen ntStatus=%!STATUS!", ntStatus);
        }
        else
        {
            // Indicate when the Module was opened (for clean up operations).
            // Internal Open has set this value to Manual by default.
            //
            ASSERT(ModuleOpenedDuringType_Manual == dmfObject->ModuleOpenedDuring);
            dmfObject->ModuleOpenedDuring = ModuleOpenedDuringType_D0Entry;
        }
    }
    else if (DMF_MODULE_OPEN_OPTION_NOTIFY_D0Entry == dmfObject->ModuleDescriptor.OpenOption)
    {
        // This Module's Notification Registration is automatically opened in D0Entry.
        //
        ntStatus = DMF_Internal_NotificationRegister(DmfModule);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Module_NotificationRegister ntStatus=%!STATUS!", ntStatus);
        }
        else
        {
            // Indicate when the Module was notification was registered for clean up operations.
            //
            ASSERT(ModuleOpenedDuringType_Invalid == dmfObject->ModuleNotificationRegisteredDuring);
            dmfObject->ModuleNotificationRegisteredDuring = ModuleOpenedDuringType_D0Entry;
        }
    }
    else if (dmfObject->ModuleDescriptor.OpenOption < DMF_MODULE_OPEN_OPTION_LAST)
    {
        // It means another valid option is selected and no further work is needed here.
        //
        ntStatus = STATUS_SUCCESS;
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid Code Path");
        ASSERT(FALSE);
        ntStatus = STATUS_INTERNAL_ERROR;
    }

    FuncExit(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleD0EntryPostInterruptsEnabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Generic callback for ModuleD0EntryPostInterruptsEnabled for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

   NTSTATUS of either the given DMF Module's Open Callback or STATUS_SUCCESS.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "dmfObject=0x%p PreviousState=%d", dmfObject, PreviousState);

    // NOTE: Modules can be closed in D0Exit.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Generic_ModuleD0ExitPreInterruptsDisabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Generic callback for ModuleD0ExitPreInterruptsDisabled for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;
    NTSTATUS ntStatus;

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s] TargetState=%d", DmfModule, dmfObject->ClientModuleInstanceName, TargetState);

    // NOTE: Client Drivers have the option of closing modules at any time.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Generic_ModuleD0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Generic callback for ModuleD0Exit for a given DMF Module. This call is used for
    DMF Modules that indicate that the given DMF Module's Open Callback should be called
    during D0Exit.
    In cases, where the Open Callback must be explicitly called by the Client Driver, this callback
    maybe overridden.

    NOTE: This function is the inverse of DMF_Generic_ModuleD0Entry.

Arguments:

    DmfModule - The given DMF Module.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;
    NTSTATUS ntStatus;

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s] TargetState=%d", DmfModule, dmfObject->ClientModuleInstanceName, TargetState);

    // NOTE: Client Drivers have the option of closing modules at any time.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    ntStatus = STATUS_SUCCESS;

    if (DMF_MODULE_OPEN_OPTION_OPEN_D0Entry == dmfObject->ModuleDescriptor.OpenOption)
    {
        // This Module is automatically closed in D0Exit.
        //
        DMF_Internal_Close(DmfModule);
    }
    else if (DMF_MODULE_OPEN_OPTION_NOTIFY_D0Entry == dmfObject->ModuleDescriptor.OpenOption)
    {
        // This Module's Notification Registration is automatically closed in D0Exit.
        //
        DMF_Internal_NotificationUnregister(DmfModule);
    }
    else if (dmfObject->ModuleDescriptor.OpenOption < DMF_MODULE_OPEN_OPTION_LAST)
    {
        // It just validates the code path..
        //
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid Code Path");
        ASSERT(FALSE);
        ntStatus = STATUS_INTERNAL_ERROR;
    }

    FuncExit(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Generic_ModuleQueueIoRead(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    )
/*++

Routine Description:

    Generic callback for ModuleQueueIoRead for a given DMF Module. If this call happens,
    it means that the given DMF Module did not handle the given IOCTL code.

Arguments:

    DmfModule - The given DMF Module.
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    Length - For fast access to the Request's Buffer Length.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Length);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    // Tell Client Driver this dispatch is still unhandled.
    //

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    return FALSE;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Generic_ModuleQueueIoWrite(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    )
/*++

Routine Description:

    Generic callback for ModuleQueueIoWrite for a given DMF Module. If this call happens,
    it means that the given DMF Module did not handle the given IOCTL code.

Arguments:

    DmfModule - The given DMF Module.
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    Length - For fast access to the Request's Buffer Length.

Return Value:

    FALSE because the given DMF Module did not handle the given IOCTL code.

--*/
{
    DMF_OBJECT* dmfObject;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Length);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    // Tell Client Driver this dispatch is still unhandled.
    //

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    return FALSE;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Generic_ModuleDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    Generic callback for ModuleDeviceIoControl for a given DMF Module. If this call happens,
    it means that the given DMF Module did not handle the given IOCTL code.

Arguments:

    DmfModule - The given DMF Module.
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    OutputBufferLength - For fast access to the Request's Output Buffer Length.
    InputBufferLength - For fast access to the Request's Input Buffer Length.
    IoControlCode - The given IOCTL code.

Return Value:

    FALSE because the given DMF Module did not handle the given IOCTL code.

--*/
{
    DMF_OBJECT* dmfObject;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(IoControlCode);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    // Tell Client Driver this dispatch is still unhandled.
    //

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] returnValue=0", DmfModule, dmfObject->ClientModuleInstanceName);

    return FALSE;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Generic_ModuleInternalDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    Generic callback for ModuleInternalDeviceIoControl for a given DMF Module. If this call happens,
    it means that the given DMF Module did not handle the given IOCTL code.

Arguments:

    DmfModule - The given DMF Module.
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    OutputBufferLength - For fast access to the Request's Output Buffer Length.
    InputBufferLength - For fast access to the Request's Input Buffer Length.
    IoControlCode - The given IOCTL code.

Return Value:

    FALSE because the given DMF Module did not handle the given IOCTL code.

--*/
{
    DMF_OBJECT* dmfObject;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(IoControlCode);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpened(dmfObject);

    // Tell Client Driver this dispatch is still unhandled.
    //

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] returnValue=0", DmfModule, dmfObject->ClientModuleInstanceName);

    return FALSE;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleSelfManagedIoCleanup(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for ModuleSelfManagedIoCleanup for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s] returnValue=0", dmfObject, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleSelfManagedIoFlush(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for ModuleSelfManagedIoFlush for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s] returnValue=0", dmfObject, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleSelfManagedIoInit(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for ModuleSelfManagedIoInit for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    ntStatus = STATUS_SUCCESS;

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s] returnValue=0", dmfObject, dmfObject->ClientModuleInstanceName);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleSelfManagedIoSuspend(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for ModuleSelfManagedIoSuspend for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    ntStatus = STATUS_SUCCESS;

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s] returnValue=0", dmfObject, dmfObject->ClientModuleInstanceName);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleSelfManagedIoRestart(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for ModuleSelfManagedIoRestart for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    ntStatus = STATUS_SUCCESS;

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s] returnValue=0", dmfObject, dmfObject->ClientModuleInstanceName);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleSurpriseRemoval(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for ModuleSurpriseRemoval for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s] returnValue=0", dmfObject, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleQueryRemove(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for ModuleQueryRemove for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    ntStatus = STATUS_SUCCESS;

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s] returnValue=0", dmfObject, dmfObject->ClientModuleInstanceName);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleQueryStop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for ModuleQueryStop for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    ntStatus = STATUS_SUCCESS;

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpened(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s] returnValue=0", dmfObject, dmfObject->ClientModuleInstanceName);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleRelationsQuery(
    _In_ DMFMODULE DmfModule,
    _In_ DEVICE_RELATION_TYPE RelationType
    )
/*++

Routine Description:

    Generic callback for ModuleRelationsQuery for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    RelationType - A DEVICE_RELATION_TYPE-typed enumerator value.
                   The DEVICE_RELATION_TYPE enumeration is defined in wdm.h.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(RelationType);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s] returnValue=0", dmfObject, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleUsageNotificationEx(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_SPECIAL_FILE_TYPE NotificationType,
    _In_ BOOLEAN IsInNotificationPath
    )
/*++

Routine Description:

    Generic callback for ModuleUsageNotificationEx for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;
    NTSTATUS ntStatus;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(NotificationType);
    UNREFERENCED_PARAMETER(IsInNotificationPath);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    ntStatus = STATUS_SUCCESS;

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpened(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s] returnValue=0", dmfObject, dmfObject->ClientModuleInstanceName);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleArmWakeFromS0(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for ModuleArmWakeFromS0 for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    ntStatus = STATUS_SUCCESS;

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpened(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s] returnValue=0", dmfObject, dmfObject->ClientModuleInstanceName);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleDisarmWakeFromS0(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for ModuleDisarmWakeFromS0 for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpened(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s]", dmfObject, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleWakeFromS0Triggered(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for ModuleWakeFromS0Triggered for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpened(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s]", dmfObject, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleArmWakeFromSxWithReason(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN DeviceWakeEnabled,
    _In_ BOOLEAN ChildrenArmedForWake
    )
/*++

Routine Description:

    Generic callback for ModuleArmWakeFromSxWithReason for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    DeviceWakeEnabled- A Boolean value that, if TRUE, indicates that the device's ability to wake the system is enabled.
    ChildrenArmedForWake - A Boolean value that, if TRUE, indicates that the ability of one or more child devices
                           to wake the system is enabled.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;
    NTSTATUS ntStatus;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DeviceWakeEnabled);
    UNREFERENCED_PARAMETER(ChildrenArmedForWake);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    ntStatus = STATUS_SUCCESS;

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpened(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s] returnValue=0", dmfObject, dmfObject->ClientModuleInstanceName);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleDisarmWakeFromSx(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for ModuleDisarmWakeFromSx for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpened(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s]", dmfObject, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleWakeFromSxTriggered(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for ModuleWakeFromSxTriggered for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpened(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s]", dmfObject, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Generic_ModuleFileCreate(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Generic callback for ModuleFileCreate for a given DMF Module. If this call happens,
    it means that the given DMF Module did not implement EvtDeviceFileCreate.

Arguments:

    DmfModule - The given DMF Module.
    Device - WDF device object.
    Request - WDF Request with IOCTL parameters.
    FileObject - WDF file object that describes a file that is being opened for the specified request

Return Value:

    FALSE always indicating this Module did not support the request.

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(FileObject);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    // It is possible for a Module to be created but not open if the Module uses a
    // notification to open but the notification has not happened yet.
    //
    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    // Tell Client Driver this dispatch is still unhandled.
    //

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s] returnValue=0", dmfObject, dmfObject->ClientModuleInstanceName);

    return FALSE;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Generic_ModuleFileCleanup(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Generic callback for ModuleFileCleanup for a given DMF Module. If this call happens,
    it means that the given DMF Module did not implement EvtFileCleanup.

Arguments:

    DmfModule - The given DMF Module.
    FileObject - WDF file object

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(FileObject);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    // Tell Client Driver this dispatch is still unhandled.
    //

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s] returnValue=0", dmfObject, dmfObject->ClientModuleInstanceName);

    return FALSE;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Generic_ModuleFileClose(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Generic callback for ModuleFileClose for a given DMF Module. If this call happens,
    it means that the given DMF Module did not implement EvtFileClose.

Arguments:

    DmfModule - The given DMF Module.
    FileObject - WDF file object

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(FileObject);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsCreatedOrOpenedOrClosed(dmfObject);

    // Tell Client Driver this dispatch is still unhandled.
    //

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s] returnValue=0", dmfObject, dmfObject->ClientModuleInstanceName);

    return FALSE;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Generic_ResourcesAssign(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Generic callback for ResourcesAssign for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    ResourcesRaw - WDF Resource Raw parameter that is passed to the given
                   DMF Module callback.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.

Return Value:

    STATUS_SUCCESS.

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    DMF_HandleValidate_IsCreatedOrIsNotify(dmfObject);

    // This is called during PrepareHardware when the flag is used.
    //

    FuncExit(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName, STATUS_SUCCESS);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_NotificationRegister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for NotificationRegister for a given DMF Module. This call can happen if the
    Client has not set the NotificationRegister callback. (Client may decide to open the Module
    for any reason, possibly unrelated to PnP, and may not need to support that call.)

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsCreatedOrClosed(dmfObject);

    FuncExit(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName, STATUS_SUCCESS);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_NotificationUnregister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for NotificationUnregister for a given DMF Module. This call can happen if the
    Client has not set the NotificationUnregister callback. (Client may decide to close the Module
    for any reason, possibly unrelated to PnP, and may not need to support this call.)

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    // NeedtoCallPreClose is set when the Module has not been closed.
    // DMF does not use ModuleState for decision making purposes, only
    // for debug and assertions. NeedToCallPreClose used to make a check
    // to determine if the Module has been closed instead. This flag is
    // cleared when the Module is closed.
    // TODO: Perhaps rename this variable to "HasModuleBeenClosed".
    //
    if (dmfObject->NeedToCallPreClose)
    {
        // The Module was successfully opened and now we are closing it.
        // No asynchronous notification will close the Module, so close it now
        // (as if this was the asynchronous notification).
        //
        // This eliminates the need to for the Module to handle this callback just
        // to close the Module in cases where OPEN_NOTIFY_* is used and the Client
        // does not need to actually register for a notification.
        //
        DMF_ModuleClose(DmfModule);
    }

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p dmfObject=0x%p [%s]", DmfModule, dmfObject, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsCreatedOrClosed(dmfObject);

    FuncExit(DMF_TRACE, "dmfObject=0x%p [%s]", dmfObject, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for Open for a given DMF Module. Many DMF Modules do not
    need to implement an Open Callback so it is legitimate for this call happen.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    // NOTE: This call only happens after the top level "Open" callback happens. That callback
    //       sets the Module State to "Opening" immediately after validating the handle. In cases
    //       where a Module has no Open Callback, this call is made, but at this point the state
    //       is now Opening. It is equivalent to what would happen if the Module were to
    //       validate the handle. (Clients do not need to validate handles because they have been
    //       validated by DMF prior to the call.
    //
    DMF_HandleValidate_IsOpening(dmfObject);

    // Some modules that have no Module Context do not need to handle Open.
    //

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, STATUS_SUCCESS);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback for Close for a given DMF Module. Many DMF Modules do not
    need to implement an Close Callback so it is legitimate for this call happen.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    // NOTE: This call only happens after the top level "Close" callback happens. That callback
    //       sets the Module State to "Closing" immediately after validating the handle. In cases
    //       where a Module has no Open Callback, this call is made, but at this point the state
    //       is now Closing. It is equivalent to what would happen if the Module were to
    //       validate the handle. (Clients do not need to validate handles because they have been
    //       validated by DMF prior to the call.
    //
    DMF_HandleValidate_IsClosing(dmfObject);

    // Some modules that have no Module Context do not need to handle Close.
    //

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, STATUS_SUCCESS);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ChildModulesAdd(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Generic callback for adding Child Modules to a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    DmfParentModuleAttributes - Pointer to the parent DMF_MODULE_ATTRIBUTES structure.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    UNREFERENCED_PARAMETER(DmfModuleInit);
    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    DMF_HandleValidate_IsCreated(dmfObject);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, STATUS_SUCCESS);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_AuxiliaryLock_Passive(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    )
/*++

Routine Description:

    Acquire a PASSIVE_LEVEL lock at the specified index on given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    AuxiliaryLockIndex - The index of the lock to acquire.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    // NOTE: No FuncEntry/Exit logging. It is too much and it is not necessary for this
    // simple function.
    //

    dmfObject = DMF_ModuleToObject(DmfModule);
    DMF_HandleValidate_IsAvailable(dmfObject);

    ASSERT(dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks <= DMF_MAXIMUM_AUXILIARY_LOCKS);
    ASSERT(AuxiliaryLockIndex < dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks + DMF_NUMBER_OF_DEFAULT_LOCKS);
    // This check is necessary for SAL.
    //
    if (AuxiliaryLockIndex < DMF_MAXIMUM_AUXILIARY_LOCKS + DMF_NUMBER_OF_DEFAULT_LOCKS)
    {
        WdfWaitLockAcquire(dmfObject->Synchronizations[AuxiliaryLockIndex].SynchronizationPassiveWaitLock,
                           NULL);
    }
    else
    {
        ASSERT(FALSE);
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_AuxiliaryUnlock_Passive(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    )
/*++

Routine Description:

    Acquire a PASSIVE_LEVEL lock at the specified index on given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    AuxiliaryLockIndex - The index of the lock to release.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    // NOTE: No FuncEntry/Exit logging. It is too much and it is not necessary for this
    // simple function.
    //

    dmfObject = DMF_ModuleToObject(DmfModule);
    DMF_HandleValidate_IsAvailable(dmfObject);

    ASSERT(dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks <= DMF_MAXIMUM_AUXILIARY_LOCKS);
    ASSERT(AuxiliaryLockIndex < dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks + DMF_NUMBER_OF_DEFAULT_LOCKS);
    // This check is necessary for SAL.
    //
    if (AuxiliaryLockIndex < DMF_MAXIMUM_AUXILIARY_LOCKS + DMF_NUMBER_OF_DEFAULT_LOCKS)
    {
        WdfWaitLockRelease(dmfObject->Synchronizations[AuxiliaryLockIndex].SynchronizationPassiveWaitLock);
    }
    else
    {
        ASSERT(FALSE);
    }
}
#pragma code_seg()

#pragma warning(suppress: 28167)
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Generic_AuxiliaryLock_Dispatch(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    )
/*++

Routine Description:

    Acquire a PASSIVE_LEVEL lock at the specified index on given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    AuxiliaryLockIndex - The index of the lock to acquire.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    // NOTE: No FuncEntry/Exit logging. It is too much and it is not necessary for this
    // simple function.
    //

    dmfObject = DMF_ModuleToObject(DmfModule);
    DMF_HandleValidate_IsAvailable(dmfObject);

    ASSERT(dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks <= DMF_MAXIMUM_AUXILIARY_LOCKS);
    ASSERT(AuxiliaryLockIndex < dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks + DMF_NUMBER_OF_DEFAULT_LOCKS);
    // This check is necessary for SAL.
    //
    if (AuxiliaryLockIndex < DMF_MAXIMUM_AUXILIARY_LOCKS + DMF_NUMBER_OF_DEFAULT_LOCKS)
    {
        WdfSpinLockAcquire(dmfObject->Synchronizations[AuxiliaryLockIndex].SynchronizationDispatchSpinLock);
    }
    else
    {
        ASSERT(FALSE);
    }
}

#pragma warning(suppress: 28167)
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Generic_AuxiliaryUnlock_Dispatch(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    )
/*++

Routine Description:

    Acquire a PASSIVE_LEVEL lock at the specified index on given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    AuxiliaryLockIndex - The index of the lock to release.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    // NOTE: No FuncEntry/Exit logging. It is too much and it is not necessary for this
    // simple function.
    //

    dmfObject = DMF_ModuleToObject(DmfModule);
    DMF_HandleValidate_IsAvailable(dmfObject);

    ASSERT(dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks <= DMF_MAXIMUM_AUXILIARY_LOCKS);
    ASSERT(AuxiliaryLockIndex < dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks + DMF_NUMBER_OF_DEFAULT_LOCKS);
    // This check is required for SAL.
    //
    if (AuxiliaryLockIndex < DMF_MAXIMUM_AUXILIARY_LOCKS + DMF_NUMBER_OF_DEFAULT_LOCKS)
    {
        WdfSpinLockRelease(dmfObject->Synchronizations[AuxiliaryLockIndex].SynchronizationDispatchSpinLock);
    }
    else
    {
        ASSERT(FALSE);
    }
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_Lock_Passive(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback to acquire a PASSIVE_LEVEL lock to a given DMF Module.
    Although a DMF Module may overwrite this call, it is unlikely. Therefore, it is
    very common that this code executes.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    PAGED_CODE();

    // NOTE: No FuncEntry/Exit logging. It is too much and it is not necessary for this
    // simple function.
    //

    DMF_Generic_AuxiliaryLock_Passive(DmfModule,
                                      DMF_DEFAULT_LOCK_INDEX);

}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_Unlock_Passive(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback to release a PASSIVE_LEVEL lock to a given DMF Module.
    Although a DMF Module may overwrite this call, it is unlikely. Therefore, it is
    very common that this code executes.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    PAGED_CODE();

    // NOTE: No FuncEntry/Exit logging. It is too much and it is not necessary for this
    // simple function.
    //

    DMF_Generic_AuxiliaryUnlock_Passive(DmfModule,
                                        DMF_DEFAULT_LOCK_INDEX);

}
#pragma code_seg()

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
// TODO: I am unable to make this warning to away legitimately.
//
#pragma warning(suppress: 28167)
#pragma warning(suppress: 28158)
DMF_Generic_Lock_Dispatch(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback to acquire a DISPATCH_LEVEL lock to a given DMF Module.
    Although a DMF Module may overwrite this call, it is unlikely. Therefore, it is
    very common that this code executes.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    // NOTE: No FuncEntry/Exit logging. It is too much and it is not necessary for this
    // simple function.
    //

    DMF_Generic_AuxiliaryLock_Dispatch(DmfModule,
                                       DMF_DEFAULT_LOCK_INDEX);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
// TODO: I am unable to make this warning to away legitimately.
//
#pragma warning(suppress: 28167)
DMF_Generic_Unlock_Dispatch(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Generic callback to release a DISPATCH_LEVEL lock to a given DMF Module.
    Although a DMF Module may overwrite this call, it is unlikely. Therefore, it is
    very common that this code executes.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    // NOTE: No FuncEntry/Exit logging. It is too much and it is not necessary for this
    // simple function.
    //

    DMF_Generic_AuxiliaryUnlock_Dispatch(DmfModule,
                                         DMF_DEFAULT_LOCK_INDEX);
}

// eof: DmfGeneric.c
//
