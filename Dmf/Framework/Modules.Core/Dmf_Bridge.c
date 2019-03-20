/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Bridge.c

Abstract:

    Chains the WDF callbacks to DMF and the Client Driver.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Core.h"
#include "DmfModules.Core.Trace.h"

#include "Dmf_Bridge.tmh"

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
    PFN_WDF_OBJECT_CONTEXT_CLEANUP EvtDeviceContextCleanup;
    PFN_WDF_DEVICE_PREPARE_HARDWARE EvtDevicePrepareHardware;
    PFN_WDF_DEVICE_RELEASE_HARDWARE EvtDeviceReleaseHardware;
    PFN_WDF_DEVICE_D0_ENTRY EvtDeviceD0Entry;
    PFN_WDF_DEVICE_D0_ENTRY_POST_INTERRUPTS_ENABLED EvtDeviceD0EntryPostInterruptsEnabled;
    PFN_WDF_DEVICE_D0_EXIT_PRE_INTERRUPTS_DISABLED EvtDeviceD0ExitPreInterruptsDisabled;
    PFN_WDF_DEVICE_D0_EXIT EvtDeviceD0Exit;
    PFN_WDF_IO_QUEUE_IO_READ EvtQueueIoRead;
    PFN_WDF_IO_QUEUE_IO_WRITE EvtQueueIoWrite;
    PFN_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtDeviceIoControl;
#if !defined(DMF_USER_MODE)
    PFN_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtInternalDeviceIoControl;
#endif // !defined(DMF_USER_MODE)
    PFN_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP EvtDeviceSelfManagedIoCleanup;
    PFN_WDF_DEVICE_SELF_MANAGED_IO_FLUSH EvtDeviceSelfManagedIoFlush;
    PFN_WDF_DEVICE_SELF_MANAGED_IO_INIT EvtDeviceSelfManagedIoInit;
    PFN_WDF_DEVICE_SELF_MANAGED_IO_SUSPEND EvtDeviceSelfManagedIoSuspend;
    PFN_WDF_DEVICE_SELF_MANAGED_IO_RESTART EvtDeviceSelfManagedIoRestart;
    PFN_WDF_DEVICE_SURPRISE_REMOVAL EvtDeviceSurpriseRemoval;
    PFN_WDF_DEVICE_QUERY_REMOVE EvtDeviceQueryRemove;
    PFN_WDF_DEVICE_QUERY_STOP EvtDeviceQueryStop;
    PFN_WDF_DEVICE_RELATIONS_QUERY EvtDeviceRelationsQuery;
    PFN_WDF_DEVICE_USAGE_NOTIFICATION_EX EvtDeviceUsageNotificationEx;
    PFN_WDF_DEVICE_ARM_WAKE_FROM_S0 EvtDeviceArmWakeFromS0;
    PFN_WDF_DEVICE_DISARM_WAKE_FROM_S0 EvtDeviceDisarmWakeFromS0;
    PFN_WDF_DEVICE_WAKE_FROM_S0_TRIGGERED EvtDeviceWakeFromS0Triggered;
    PFN_WDF_DEVICE_ARM_WAKE_FROM_SX_WITH_REASON EvtDeviceArmWakeFromSxWithReason;
    PFN_WDF_DEVICE_DISARM_WAKE_FROM_SX EvtDeviceDisarmWakeFromSx;
    PFN_WDF_DEVICE_WAKE_FROM_SX_TRIGGERED EvtDeviceWakeFromSxTriggered;
    PFN_WDF_DEVICE_FILE_CREATE EvtFileCreate;
    PFN_WDF_FILE_CLEANUP EvtFileCleanup;
    PFN_WDF_FILE_CLOSE EvtFileClose;
} DMF_CONTEXT_Bridge;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Bridge)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(Bridge)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

VOID
Bridge_DefaultEvtDeviceContextCleanup(
    _In_ WDFOBJECT DeviceObject
    )
/*++

Routine Description:

    Default implementation for EvtDeviceContextCleanup.

Arguments:

    DeviceObject - Handle to a WDF Device object.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DeviceObject);

    FuncEntry(DMF_TRACE);

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Bridge_DefaultEvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Default implementation for EvtDevicePrepareHardware.

Arguments:

    Device - Handle to a WDF Device object.
    ResourcesRaw - WDF Resource Raw parameter that is passed to the given
                   DMF Module callback.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Bridge_DefaultEvtDeviceReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Default implementation for EvtDeviceReleaseHardware.

Arguments:

    Device - Handle to a WDF Device object.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}
#pragma code_seg()

NTSTATUS
Bridge_DefaultEvtDeviceD0Entry(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Default implementation for EvtDeviceD0Entry.

Arguments:

    Device - Handle to a WDF Device object.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(PreviousState);

    FuncEntry(DMF_TRACE);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}

NTSTATUS
Bridge_DefaultEvtDeviceD0EntryPostInterruptsEnabled(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Default implementation for EvtDeviceD0EntryPostInterruptsEnabled.

Arguments:

    Device - Handle to a WDF Device object.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(PreviousState);

    FuncEntry(DMF_TRACE);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}

NTSTATUS
Bridge_DefaultEvtDeviceD0ExitPreInterruptsDisabled(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Default implementation for EvtDeviceD0ExitPreInterrruptsDisabled.

Arguments:

    Device - Handle to a WDF Device object.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(TargetState);

    FuncEntry(DMF_TRACE);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}

NTSTATUS
Bridge_DefaultEvtDeviceD0Exit(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Default implementation for EvtDeviceD0Exit.

Arguments:

    Device - Handle to a WDF Device object.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(TargetState);

    FuncEntry(DMF_TRACE);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Bridge_DefaultEvtDeviceSelfManagedIoCleanup(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Default implementation for EvtDeviceSelfManagedIoCleanup.

Arguments:

    Device - Handle to a WDF Device object.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(Device);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Bridge_DefaultEvtDeviceSelfManagedIoFlush(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Default implementation for EvtDeviceSelfManagedIoFlush.

Arguments:

    Device - Handle to a WDF Device object.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(Device);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Bridge_DefaultEvtDeviceSelfManagedIoInit(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Default implementation for EvtDeviceSelfManagedIoInit.

Arguments:

    Device - Handle to a WDF Device object.

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(Device);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Bridge_DefaultEvtDeviceSelfManagedIoSuspend(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Default implementation for EvtDeviceSelfManagedIoSuspend.

Arguments:

    Device - Handle to a WDF Device object.

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(Device);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Bridge_DefaultEvtDeviceSelfManagedIoRestart(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Default implementation for EvtDeviceSelfManagedIoRestart.

Arguments:

    Device - Handle to a WDF Device object.

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(Device);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Bridge_DefaultEvtDeviceSurpriseRemoval(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Default implementation for EvtDeviceSurpriseRemoval.

Arguments:

    Device - Handle to a WDF Device object.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(Device);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Bridge_DefaultEvtDeviceQueryRemove(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Default implementation for EvtDeviceQueryRemove.

Arguments:

    Device - Handle to a WDF Device object.

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(Device);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Bridge_DefaultEvtDeviceQueryStop(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Default implementation for EvtDeviceQueryStop.

Arguments:

    Device - Handle to a WDF Device object.

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(Device);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Bridge_DefaultEvtDeviceRelationsQuery(
    _In_ WDFDEVICE Device,
    _In_ DEVICE_RELATION_TYPE RelationType
    )
/*++

Routine Description:

    Default implementation for EvtDeviceRelationsQuery.

Arguments:

    Device - Handle to a WDF Device object.
    RelationType - Device relation type.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(RelationType);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Bridge_DefaultEvtDeviceUsageNotificationEx(
    _In_ WDFDEVICE Device,
    _In_ WDF_SPECIAL_FILE_TYPE NotificationType,
    _In_ BOOLEAN IsInNotificationPath
    )
/*++

Routine Description:

    Default implementation for EvtDeviceUsageNotificationEx.

Arguments:

    Device - Handle to a WDF Device object.

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(NotificationType);
    UNREFERENCED_PARAMETER(IsInNotificationPath);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Bridge_DefaultEvtDeviceArmWakeFromS0(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Default implementation for EvtDeviceArmWakeFromS0.

Arguments:

    Device - Handle to a WDF Device object.

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(Device);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Bridge_DefaultEvtDeviceDisarmWakeFromS0(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Default implementation for EvtDeviceDisarmWakeFromS0.

Arguments:

    Device - Handle to a WDF Device object.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(Device);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Bridge_DefaultEvtDeviceWakeFromS0Triggered(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Default implementation for EvtDeviceWakeFromS0Triggered.

Arguments:

    Device - Handle to a WDF Device object.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(Device);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Bridge_DefaultEvtDeviceArmWakeFromSxWithReason(
    _In_ WDFDEVICE Device,
    _In_ BOOLEAN DeviceWakeEnabled,
    _In_ BOOLEAN ChildrenArmedForWake
    )
/*++

Routine Description:

    Default implementation for EvtDeviceArmWakeFromSxWithReason.

Arguments:

    Device - Handle to a WDF Device object.
    DeviceWakeEnabled - TRUE if Device is enabled for Wake.
    ChildrenArmedForWake - TRUE if Device's children are armed for Wake.

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(DeviceWakeEnabled);
    UNREFERENCED_PARAMETER(ChildrenArmedForWake);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Bridge_DefaultEvtDeviceDisarmWakeFromSx(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Default implementation for EvtDeviceDisarmWakeFromSx.

Arguments:

    Device - Handle to a WDF Device object.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(Device);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Bridge_DefaultEvtDeviceWakeFromSxTriggered(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Default implementation for EvtDeviceWakeFromSxTriggered.

Arguments:

    Device - Handle to a WDF Device object.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(Device);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Bridge_ModulePrepareHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Bridge callback for ModulePrepareHardware.

Arguments:

    DmfModule - This Module's handle.
    ResourcesRaw - WDF Resource Raw parameter that is passed to the given
                   DMF Module callback.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.

Return Value:

    If no errors, STATUS_SUCCESS or another status value for which NT_SUCCESS(status) equals TRUE
    Otherwise, a status value for which NT_SUCCESS(status) equals FALSE

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDevicePrepareHardware != NULL);
    ntStatus = moduleContext->EvtDevicePrepareHardware(device,
                                                       ResourcesRaw,
                                                       ResourcesTranslated);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtDevicePrepareHardware fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_Bridge_ModuleReleaseHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Bridge callback for ModuleReleaseHardware.

Arguments:

    DmfModule - This Module's handle.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.

Return Value:

    If no errors, STATUS_SUCCESS or another status value for which NT_SUCCESS(status) equals TRUE
    Otherwise, a status value for which NT_SUCCESS(status) equals FALSE

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceReleaseHardware != NULL);
    ntStatus = moduleContext->EvtDeviceReleaseHardware(device,
                                                       ResourcesTranslated);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtDeviceReleaseHardware fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Bridge_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Bridge callback for ModuleD0Entry.

Arguments:

    DmfModule - This Module's handle.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

    If no errors, STATUS_SUCCESS or another status value for which NT_SUCCESS(ntStatus) equals TRUE.
    Otherwise, a status value for which NT_SUCCESS(ntStatus) equals FALSE.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(moduleContext->EvtDeviceD0Entry != NULL);
    device = DMF_ParentDeviceGet(DmfModule);

    ntStatus = moduleContext->EvtDeviceD0Entry(device,
                                               PreviousState);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtDeviceD0Entry fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Bridge_ModuleD0EntryPostInterruptsEnabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Bridge callback for ModuleD0EntryPostInterruptsEnabled.

Arguments:

    DmfModule - This Module's handle.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

    If no errors, STATUS_SUCCESS or another status value for which NT_SUCCESS(ntStatus) equals TRUE.
    Otherwise, a status value for which NT_SUCCESS(ntStatus) equals FALSE.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(moduleContext->EvtDeviceD0Entry != NULL);
    device = DMF_ParentDeviceGet(DmfModule);

    ntStatus = moduleContext->EvtDeviceD0EntryPostInterruptsEnabled(device,
                                                                    PreviousState);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtDeviceD0EntryPostInterruptsEnabled fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
NTSTATUS
DMF_Bridge_ModuleD0ExitPreInterruptsDisabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Bridge callback for ModuleD0ExitPreInterruptsDiabled.

Arguments:

    DmfModule - This Module's handle.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    If no errors, STATUS_SUCCESS or another status value for which NT_SUCCESS(ntStatus) equals TRUE.
    Otherwise, a status value for which NT_SUCCESS(ntStatus) equals FALSE.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceD0Exit != NULL);
    ntStatus = moduleContext->EvtDeviceD0ExitPreInterruptsDisabled(device,
                                                                   TargetState);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtDeviceD0ExitPreInterruptsDiabled fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
NTSTATUS
DMF_Bridge_ModuleD0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Bridge callback for ModuleD0Exit.

Arguments:

    DmfModule - This Module's handle.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    If no errors, STATUS_SUCCESS or another status value for which NT_SUCCESS(ntStatus) equals TRUE.
    Otherwise, a status value for which NT_SUCCESS(ntStatus) equals FALSE.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceD0Exit != NULL);
    ntStatus = moduleContext->EvtDeviceD0Exit(device,
                                              TargetState);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtDeviceD0Exit fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
BOOLEAN
DMF_Bridge_ModuleQueueIoRead(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    )
/*++

Routine Description:

    Bridge callback for ModuleQueueIoRead.

Arguments:

    DmfModule - This Module's handle.
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    Length - For fast access to the Request's Buffer Length.

Return Value:

    Return FALSE if this Module does not support (know) the IOCTL.

    NOTE: DMF will call this Modules' Callbacks after all other Modules used by client.
          So, if the Client supports QueueIoRead for this Module, it has to complete the request.

--*/
{
    BOOLEAN returnValue;
    DMF_CONTEXT_Bridge* moduleContext;

    FuncEntry(DMF_TRACE);

    returnValue = FALSE;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Since EvtQueueIoRead returns VOID, the Request state is unknown after EvtQueueIoRead returns.
    // So if Client implements EvtQueueIoRead, it has to handle the Request when called.
    // DMF_ModuleCollectionQueueIoRead returns a Boolean to specify if Request was handled or not
    // and the caller needs to further handle Request appropriately.
    // Thus if client does not implement EvtQueueIoRead,
    // we just return FALSE instead of providing a default implementation.
    //
    if (moduleContext->EvtQueueIoRead != NULL)
    {
        moduleContext->EvtQueueIoRead(Queue,
                                      Request,
                                      Length);
        returnValue = TRUE;
    }

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
BOOLEAN
DMF_Bridge_ModuleQueueIoWrite(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    )
/*++

Routine Description:

    Bridge callback for ModuleQueueIoWrite.

Arguments:

    DmfModule - This Module's handle.
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    Length - For fast access to the Request's Buffer Length.

Return Value:
Return FALSE if this Module does not support (know) the IOCTL.

    NOTE: DMF will call this Modules' Callbacks after all other Modules used by client.
          So, if the Client supports QueueIoWrite for this Module, it has to complete the request.

--*/
{
    BOOLEAN returnValue;
    DMF_CONTEXT_Bridge* moduleContext;

    FuncEntry(DMF_TRACE);

    returnValue = FALSE;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Since EvtQueueIoWrite returns VOID, the Request state is unknown after EvtQueueIoWrite returns.
    // So if Client implements EvtQueueIoWrite, it has to handle the Request when called.
    // DMF_ModuleCollectionQueueIoWrite returns a Boolean to specify if Request was handled or not
    // and the caller needs to further handle Request appropriately.
    // Thus if client does not implement EvtQueueIoWrite,
    // we just return FALSE instead of providing a default implementation.
    //
    if (moduleContext->EvtQueueIoWrite != NULL)
    {
        moduleContext->EvtQueueIoWrite(Queue,
                                       Request,
                                       Length);
        returnValue = TRUE;
    }

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
BOOLEAN
DMF_Bridge_ModuleDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    Bridge callback for ModuleDeviceIoControl.

Arguments:

    DmfModule - This Module's handle.
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    OutputBufferLength - For fast access to the Request's Output Buffer Length.
    InputBufferLength - For fast access to the Request's Input Buffer Length.
    IoControlCode - The given IOCTL code.

Return Value:

    Return FALSE if this Module does not support (know) the IOCTL.

    NOTE: DMF will call this Modules' Callbacks after all other Modules used by client.
          So, if the Client supports DeviceIoControl for this Module, it has to complete the request.

--*/
{
    BOOLEAN returnValue;
    DMF_CONTEXT_Bridge* moduleContext;

    FuncEntry(DMF_TRACE);

    returnValue = FALSE;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Since EvtDeviceIoControl returns VOID, the Request state is unknown after EvtDeviceIoControl returns.
    // So if Client implements EvtDeviceIoControl, it has to handle the Request when called.
    // DMF_ModuleCollectionDeviceIoControl returns a Boolean to specify if Request was handled or not
    // and the caller needs to further handle Request appropriately.
    // Thus if client does not implement EvtDeviceIoControl,
    // we just return FALSE instead of providing a default implementation.
    //
    if (moduleContext->EvtDeviceIoControl != NULL)
    {
        moduleContext->EvtDeviceIoControl(Queue,
                                          Request,
                                          OutputBufferLength,
                                          InputBufferLength,
                                          IoControlCode);
        returnValue = TRUE;
    }

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

#if !defined(DMF_USER_MODE)

_IRQL_requires_max_(DISPATCH_LEVEL)
static
BOOLEAN
DMF_Bridge_ModuleInternalDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    Bridge callback for ModuleInternalDeviceIoControl.

Arguments:

    DmfModule - This Module's handle.
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    OutputBufferLength - For fast access to the Request's Output Buffer Length.
    InputBufferLength - For fast access to the Request's Input Buffer Length.
    IoControlCode - The given IOCTL code.

Return Value:

    Return FALSE if this Module does not support (know) the IOCTL.

    NOTE: DMF will call this Modules' Callbacks after all other Modules used by client.
          So, if the Client supports DeviceIoControl for this Module, it has to complete the request.
--*/
{
    BOOLEAN returnValue;
    DMF_CONTEXT_Bridge* moduleContext;

    FuncEntry(DMF_TRACE);

    returnValue = FALSE;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Since EvtInternalDeviceIoControl returns VOID, the Request state is unknown after EvtInternalDeviceIoControl returns.
    // So if Client implements EvtInternalDeviceIoControl, it has to handle the Request when called.
    // DMF_ModuleCollectionInternalDeviceIoControl returns a Boolean to specify if Request was handled or not
    // and the caller needs to further handle Request appropriately.
    // Thus if client does not implement EvtInternalDeviceIoControl,
    // we just return FALSE instead of providing a default implementation.
    //
    if (moduleContext->EvtInternalDeviceIoControl != NULL)
    {
        moduleContext->EvtInternalDeviceIoControl(Queue,
                                                  Request,
                                                  OutputBufferLength,
                                                  InputBufferLength,
                                                  IoControlCode);
        returnValue = TRUE;
    }

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

#endif // !defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_Bridge_ModuleSelfManagedIoCleanup(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Bridge callback for ModuleSelfManagedIoCleanup.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceSelfManagedIoCleanup != NULL);
    moduleContext->EvtDeviceSelfManagedIoCleanup(device);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_Bridge_ModuleSelfManagedIoFlush(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Bridge callback for ModuleSelfManagedIoFlush.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceSelfManagedIoFlush != NULL);
    moduleContext->EvtDeviceSelfManagedIoFlush(device);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_Bridge_ModuleSelfManagedIoInit(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Bridge callback for ModuleSelfManagedIoInit.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceSelfManagedIoInit != NULL);
    ntStatus = moduleContext->EvtDeviceSelfManagedIoInit(device);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtDeviceSelfManagedIoInit fails: ntStatus=%!STATUS!", ntStatus);
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
NTSTATUS
DMF_Bridge_ModuleSelfManagedIoSuspend(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Bridge callback for ModuleSelfManagedIoSuspend.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceSelfManagedIoSuspend != NULL);
    ntStatus = moduleContext->EvtDeviceSelfManagedIoSuspend(device);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtDeviceSelfManagedIoSuspend fails: ntStatus=%!STATUS!", ntStatus);
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
NTSTATUS
DMF_Bridge_ModuleSelfManagedIoRestart(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Bridge callback for ModuleSelfManagedIoRestart.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceSelfManagedIoRestart != NULL);
    ntStatus = moduleContext->EvtDeviceSelfManagedIoRestart(device);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtDeviceSelfManagedIoRestart fails: ntStatus=%!STATUS!", ntStatus);
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
DMF_Bridge_ModuleSurpriseRemoval(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Bridge callback for ModuleSurpriseRemoval.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceSurpriseRemoval != NULL);
    moduleContext->EvtDeviceSurpriseRemoval(device);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_Bridge_ModuleQueryRemove(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Bridge callback for ModuleQueryRemove.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceQueryRemove != NULL);
    ntStatus = moduleContext->EvtDeviceQueryRemove(device);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtDeviceQueryRemove fails: ntStatus=%!STATUS!", ntStatus);
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
NTSTATUS
DMF_Bridge_ModuleQueryStop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Bridge callback for ModuleQueryStop.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceQueryStop != NULL);
    ntStatus = moduleContext->EvtDeviceQueryStop(device);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtDeviceQueryStop fails: ntStatus=%!STATUS!", ntStatus);
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
DMF_Bridge_ModuleRelationsQuery(
    _In_ DMFMODULE DmfModule,
    _In_ DEVICE_RELATION_TYPE RelationType
    )
/*++

Routine Description:

    Bridge callback for ModuleRelationsQuery.

Arguments:

    DmfModule - This Module's handle.
    RelationType - A DEVICE_RELATION_TYPE-typed enumerator value.
                   The DEVICE_RELATION_TYPE enumeration is defined in wdm.h.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceRelationsQuery != NULL);
    moduleContext->EvtDeviceRelationsQuery(device,
                                           RelationType);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_Bridge_ModuleUsageNotificationEx(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_SPECIAL_FILE_TYPE NotificationType,
    _In_ BOOLEAN IsInNotificationPath
    )
/*++

Routine Description:

    Bridge callback for EvtDeviceUsageNotificationEx.

Arguments:

    DmfModule - This Module's handle.
    NotificationType - Identifies the type of special file that the system is storing on the specified device.
    IsInNotificationPath - TRUE indicates that the system has starting using the special file,
                           FALSE indicates that the system has finished using the special file.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceUsageNotificationEx != NULL);
    ntStatus = moduleContext->EvtDeviceUsageNotificationEx(device,
                                                           NotificationType,
                                                           IsInNotificationPath);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtDeviceUsageNotificationEx fails: ntStatus=%!STATUS!", ntStatus);
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
NTSTATUS
DMF_Bridge_ModuleArmWakeFromS0(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Bridge callback for ModuleArmWakeFromS0.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceArmWakeFromS0 != NULL);
    ntStatus = moduleContext->EvtDeviceArmWakeFromS0(device);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtDeviceArmWakeFromS0 fails: ntStatus=%!STATUS!", ntStatus);
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
DMF_Bridge_ModuleDisarmWakeFromS0(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Bridge callback for ModuleDisarmWakeFromS0.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceDisarmWakeFromS0 != NULL);
    moduleContext->EvtDeviceDisarmWakeFromS0(device);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_Bridge_ModuleWakeFromS0Triggered(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Bridge callback for ModuleWakeFromS0Triggered.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceWakeFromS0Triggered != NULL);
    moduleContext->EvtDeviceWakeFromS0Triggered(device);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_Bridge_ModuleArmWakeFromSxWithReason(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN DeviceWakeEnabled,
    _In_ BOOLEAN ChildrenArmedForWake
    )
/*++

Routine Description:

    Bridge callback for ModuleArmWakeFromSxWithReason.

Arguments:

    DmfModule - This Module's handle.
    DeviceWakeEnabled - TRUE indicates that the device's ability to wake the system is enabled
    ChildrenArmedForWake - TRUE indicates that the ability of one or more child devices to wake the system is enabled.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceArmWakeFromSxWithReason != NULL);
    ntStatus = moduleContext->EvtDeviceArmWakeFromSxWithReason(device,
                                                               DeviceWakeEnabled,
                                                               ChildrenArmedForWake);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtDeviceArmWakeFromSxWithReason fails: ntStatus=%!STATUS!", ntStatus);
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
DMF_Bridge_ModuleDisarmWakeFromSx(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Bridge callback for ModuleDisarmWakeFromSx.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceDisarmWakeFromSx != NULL);
    moduleContext->EvtDeviceDisarmWakeFromSx(device);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_Bridge_ModuleWakeFromSxTriggered(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Bridge callback for ModuleWakeFromSxTriggered.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(moduleContext->EvtDeviceWakeFromSxTriggered != NULL);
    moduleContext->EvtDeviceWakeFromSxTriggered(device);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
BOOLEAN
DMF_Bridge_ModuleFileCreate(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Bridge callback for ModuleFileCreate.

Arguments:

    DmfModule - This Module's handle.
    Device - WDF device object.
    Request - WDF Request with IOCTL parameters.
    FileObject - WDF file object that describes a file that is being opened for the specified request

Return Value:

    Return FALSE if this Module does not support (know) the IOCTL.

    NOTE: DMF will call this Modules' Callbacks after all other Modules used by client.
          So, if the Client supports QueueIoRead for this Module, it has to complete the request.

--*/
{
    BOOLEAN returnValue;
    DMF_CONTEXT_Bridge* moduleContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    returnValue = FALSE;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Since EvtFileCreate returns VOID, the Request state is unknown after EvtFileCreate returns.
    // So if Client implements EvtFileCreate, it has to handle the Request when called.
    // DMF_ModuleCollectionQueueIoRead returns a Boolean to specify if Request was handled or not
    // and the caller needs to further handle Request appropriately.
    // Thus if client does not implement EvtFileCreate,
    // we just return FALSE instead of providing a default implementation.
    //
    if (moduleContext->EvtFileCreate != NULL)
    {
        moduleContext->EvtFileCreate(Device,
                                     Request,
                                     FileObject);

        returnValue = TRUE;
    }

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
BOOLEAN
DMF_Bridge_ModuleFileCleanup(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Bridge callback for ModuleFileCleanup.

Arguments:

    DmfModule - This Module's handle.
    FileObject - WDF file object

Return Value:

    Return FALSE if this client does not support this.

--*/
{
    BOOLEAN returnValue;
    DMF_CONTEXT_Bridge* moduleContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    returnValue = FALSE;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Since EvtFileCleanup returns VOID, the Request state is unknown after EvtFileCleanup returns.
    // So if Client implements EvtFileCleanup, it has to handle the Request when called.
    // DMF_ModuleCollectionQueueIoRead returns a Boolean to specify if Request was handled or not
    // and the caller needs to further handle Request appropriately.
    // Thus if client does not implement EvtFileCleanup,
    // we just return FALSE instead of providing a default implementation.
    //
    if (moduleContext->EvtFileCleanup != NULL)
    {
        moduleContext->EvtFileCleanup(FileObject);

        returnValue = TRUE;
    }

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
BOOLEAN
DMF_Bridge_ModuleFileClose(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Bridge callback for ModuleFileClose.

Arguments:

    DmfModule - This Module's handle.
    FileObject - WDF file object

Return Value:

    Return FALSE if this client does not support this.

--*/
{
    BOOLEAN returnValue;
    DMF_CONTEXT_Bridge* moduleContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    returnValue = FALSE;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Since EvtFileClose returns VOID, the Request state is unknown after EvtFileClose returns.
    // So if Client implements EvtFileClose, it has to handle the Request when called.
    // DMF_ModuleCollectionQueueIoRead returns a Boolean to specify if Request was handled or not
    // and the caller needs to further handle Request appropriately.
    // Thus if client does not implement EvtFileClose,
    // we just return FALSE instead of providing a default implementation.
    //
    if (moduleContext->EvtFileClose != NULL)
    {
        moduleContext->EvtFileClose(FileObject);

        returnValue = TRUE;
    }

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
static
VOID
DMF_Bridge_Destroy(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Destroy an instance of a Module of type Bridge.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_Bridge* moduleContext;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    // If Client implements DeviceContextCleanup callback,
    // it will be called here before the Module is destroyed. 
    // NOTE: EvtDeviceContextCleanup is set in Open. But Open may not be
    //       called before Destroy.
    //
    if (moduleContext->EvtDeviceContextCleanup != NULL)
    {
        moduleContext->EvtDeviceContextCleanup(device);
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Bridge_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Bridge.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Bridge* moduleContext;
    DMF_CONFIG_Bridge* moduleConfig;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    moduleContext->EvtDeviceContextCleanup = Bridge_DefaultEvtDeviceContextCleanup;
    moduleContext->EvtDevicePrepareHardware = Bridge_DefaultEvtDevicePrepareHardware;
    moduleContext->EvtDeviceReleaseHardware = Bridge_DefaultEvtDeviceReleaseHardware;
    moduleContext->EvtDeviceD0Entry = Bridge_DefaultEvtDeviceD0Entry;
    moduleContext->EvtDeviceD0EntryPostInterruptsEnabled = Bridge_DefaultEvtDeviceD0EntryPostInterruptsEnabled;
    moduleContext->EvtDeviceD0ExitPreInterruptsDisabled = Bridge_DefaultEvtDeviceD0ExitPreInterruptsDisabled;
    moduleContext->EvtDeviceD0Exit = Bridge_DefaultEvtDeviceD0Exit;
    moduleContext->EvtDeviceSelfManagedIoCleanup = Bridge_DefaultEvtDeviceSelfManagedIoCleanup;
    moduleContext->EvtDeviceSelfManagedIoFlush = Bridge_DefaultEvtDeviceSelfManagedIoFlush;
    moduleContext->EvtDeviceSelfManagedIoInit = Bridge_DefaultEvtDeviceSelfManagedIoInit;
    moduleContext->EvtDeviceSelfManagedIoSuspend = Bridge_DefaultEvtDeviceSelfManagedIoSuspend;
    moduleContext->EvtDeviceSelfManagedIoRestart = Bridge_DefaultEvtDeviceSelfManagedIoRestart;
    moduleContext->EvtDeviceSurpriseRemoval = Bridge_DefaultEvtDeviceSurpriseRemoval;
    moduleContext->EvtDeviceQueryRemove = Bridge_DefaultEvtDeviceQueryRemove;
    moduleContext->EvtDeviceQueryStop = Bridge_DefaultEvtDeviceQueryStop;
    moduleContext->EvtDeviceRelationsQuery = Bridge_DefaultEvtDeviceRelationsQuery;
    moduleContext->EvtDeviceUsageNotificationEx = Bridge_DefaultEvtDeviceUsageNotificationEx;
    moduleContext->EvtDeviceArmWakeFromS0 = Bridge_DefaultEvtDeviceArmWakeFromS0;
    moduleContext->EvtDeviceDisarmWakeFromS0 = Bridge_DefaultEvtDeviceDisarmWakeFromS0;
    moduleContext->EvtDeviceWakeFromS0Triggered = Bridge_DefaultEvtDeviceWakeFromS0Triggered;
    moduleContext->EvtDeviceArmWakeFromSxWithReason = Bridge_DefaultEvtDeviceArmWakeFromSxWithReason;
    moduleContext->EvtDeviceDisarmWakeFromSx = Bridge_DefaultEvtDeviceDisarmWakeFromSx;
    moduleContext->EvtDeviceWakeFromSxTriggered = Bridge_DefaultEvtDeviceWakeFromSxTriggered;
    // We do not assign default implementations for Callbacks below
    // since default implementation returns VOID, and
    // we need to return a boolean to specify if its handled or not.
    //
    moduleContext->EvtQueueIoRead = NULL;
    moduleContext->EvtQueueIoWrite = NULL;
    moduleContext->EvtDeviceIoControl = NULL;
#if !defined(DMF_USER_MODE)
    moduleContext->EvtInternalDeviceIoControl = NULL;
#endif // !defined(DMF_USER_MODE)
    moduleContext->EvtFileCreate = NULL;
    moduleContext->EvtFileCleanup = NULL;
    moduleContext->EvtFileClose = NULL;

    if (moduleConfig->EvtDeviceContextCleanup != NULL)
    {
        moduleContext->EvtDeviceContextCleanup = moduleConfig->EvtDeviceContextCleanup;
    }
    if (moduleConfig->EvtDevicePrepareHardware != NULL)
    {
        moduleContext->EvtDevicePrepareHardware = moduleConfig->EvtDevicePrepareHardware;
    }
    if (moduleConfig->EvtDeviceReleaseHardware != NULL)
    {
        moduleContext->EvtDeviceReleaseHardware = moduleConfig->EvtDeviceReleaseHardware;
    }
    if (moduleConfig->EvtDeviceD0Entry != NULL)
    {
        moduleContext->EvtDeviceD0Entry = moduleConfig->EvtDeviceD0Entry;
    }
    if (moduleConfig->EvtDeviceD0EntryPostInterruptsEnabled != NULL)
    {
        moduleContext->EvtDeviceD0EntryPostInterruptsEnabled = moduleConfig->EvtDeviceD0EntryPostInterruptsEnabled;
    }
    if (moduleConfig->EvtDeviceD0ExitPreInterruptsDisabled != NULL)
    {
        moduleContext->EvtDeviceD0ExitPreInterruptsDisabled = moduleConfig->EvtDeviceD0ExitPreInterruptsDisabled;
    }
    if (moduleConfig->EvtDeviceD0Exit != NULL)
    {
        moduleContext->EvtDeviceD0Exit = moduleConfig->EvtDeviceD0Exit;
    }
    if (moduleConfig->EvtQueueIoRead != NULL)
    {
        moduleContext->EvtQueueIoRead = moduleConfig->EvtQueueIoRead;
    }
    if (moduleConfig->EvtQueueIoWrite != NULL)
    {
        moduleContext->EvtQueueIoWrite = moduleConfig->EvtQueueIoWrite;
    }
    if (moduleConfig->EvtDeviceIoControl != NULL)
    {
        moduleContext->EvtDeviceIoControl = moduleConfig->EvtDeviceIoControl;
    }
#if !defined(DMF_USER_MODE)
    if (moduleConfig->EvtInternalDeviceIoControl != NULL)
    {
        moduleContext->EvtInternalDeviceIoControl = moduleConfig->EvtInternalDeviceIoControl;
    }
#endif // !defined(DMF_USER_MODE)
    if (moduleConfig->EvtDeviceSelfManagedIoCleanup != NULL)
    {
        moduleContext->EvtDeviceSelfManagedIoCleanup = moduleConfig->EvtDeviceSelfManagedIoCleanup;
    }
    if (moduleConfig->EvtDeviceSelfManagedIoFlush != NULL)
    {
        moduleContext->EvtDeviceSelfManagedIoFlush = moduleConfig->EvtDeviceSelfManagedIoFlush;
    }
    if (moduleConfig->EvtDeviceSelfManagedIoInit != NULL)
    {
        moduleContext->EvtDeviceSelfManagedIoInit = moduleConfig->EvtDeviceSelfManagedIoInit;
    }
    if (moduleConfig->EvtDeviceSelfManagedIoSuspend != NULL)
    {
        moduleContext->EvtDeviceSelfManagedIoSuspend = moduleConfig->EvtDeviceSelfManagedIoSuspend;
    }
    if (moduleConfig->EvtDeviceSelfManagedIoRestart != NULL)
    {
        moduleContext->EvtDeviceSelfManagedIoRestart = moduleConfig->EvtDeviceSelfManagedIoRestart;
    }
    if (moduleConfig->EvtDeviceSurpriseRemoval != NULL)
    {
        moduleContext->EvtDeviceSurpriseRemoval = moduleConfig->EvtDeviceSurpriseRemoval;
    }
    if (moduleConfig->EvtDeviceQueryRemove != NULL)
    {
        moduleContext->EvtDeviceQueryRemove = moduleConfig->EvtDeviceQueryRemove;
    }
    if (moduleConfig->EvtDeviceQueryStop != NULL)
    {
        moduleContext->EvtDeviceQueryStop = moduleConfig->EvtDeviceQueryStop;
    }
    if (moduleConfig->EvtDeviceRelationsQuery != NULL)
    {
        moduleContext->EvtDeviceRelationsQuery = moduleConfig->EvtDeviceRelationsQuery;
    }
    if (moduleConfig->EvtDeviceUsageNotificationEx != NULL)
    {
        moduleContext->EvtDeviceUsageNotificationEx = moduleConfig->EvtDeviceUsageNotificationEx;
    }
    if (moduleConfig->EvtDeviceArmWakeFromS0 != NULL)
    {
        moduleContext->EvtDeviceArmWakeFromS0 = moduleConfig->EvtDeviceArmWakeFromS0;
    }
    if (moduleConfig->EvtDeviceDisarmWakeFromS0 != NULL)
    {
        moduleContext->EvtDeviceDisarmWakeFromS0 = moduleConfig->EvtDeviceDisarmWakeFromS0;
    }
    if (moduleConfig->EvtDeviceWakeFromS0Triggered != NULL)
    {
        moduleContext->EvtDeviceWakeFromS0Triggered = moduleConfig->EvtDeviceWakeFromS0Triggered;
    }
    if (moduleConfig->EvtDeviceArmWakeFromSxWithReason != NULL)
    {
        moduleContext->EvtDeviceArmWakeFromSxWithReason = moduleConfig->EvtDeviceArmWakeFromSxWithReason;
    }
    if (moduleConfig->EvtDeviceDisarmWakeFromSx != NULL)
    {
        moduleContext->EvtDeviceDisarmWakeFromSx = moduleConfig->EvtDeviceDisarmWakeFromSx;
    }
    if (moduleConfig->EvtDeviceWakeFromSxTriggered != NULL)
    {
        moduleContext->EvtDeviceWakeFromSxTriggered = moduleConfig->EvtDeviceWakeFromSxTriggered;
    }
    if (moduleConfig->EvtFileCreate != NULL)
    {
        moduleContext->EvtFileCreate = moduleConfig->EvtFileCreate;
    }
    if (moduleConfig->EvtFileCleanup != NULL)
    {
        moduleContext->EvtFileCleanup = moduleConfig->EvtFileCleanup;
    }
    if (moduleConfig->EvtFileClose != NULL)
    {
        moduleContext->EvtFileClose = moduleConfig->EvtFileClose;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()


///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_Bridge;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_Bridge;
static DMF_CALLBACKS_WDF DmfCallbacksWdf_Bridge;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Bridge_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Bridge.

Arguments:

    Device - Client Driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_Bridge);
    DmfCallbacksDmf_Bridge.ModuleInstanceDestroy = DMF_Bridge_Destroy;
    DmfCallbacksDmf_Bridge.DeviceOpen = DMF_Bridge_Open;

    DMF_CALLBACKS_WDF_INIT(&DmfCallbacksWdf_Bridge);
    DmfCallbacksWdf_Bridge.ModulePrepareHardware = DMF_Bridge_ModulePrepareHardware;
    DmfCallbacksWdf_Bridge.ModuleReleaseHardware = DMF_Bridge_ModuleReleaseHardware;
    DmfCallbacksWdf_Bridge.ModuleD0Entry = DMF_Bridge_ModuleD0Entry;
    DmfCallbacksWdf_Bridge.ModuleD0EntryPostInterruptsEnabled = DMF_Bridge_ModuleD0EntryPostInterruptsEnabled;
    DmfCallbacksWdf_Bridge.ModuleD0ExitPreInterruptsDisabled = DMF_Bridge_ModuleD0ExitPreInterruptsDisabled;
    DmfCallbacksWdf_Bridge.ModuleD0Exit = DMF_Bridge_ModuleD0Exit;
    DmfCallbacksWdf_Bridge.ModuleQueueIoRead = DMF_Bridge_ModuleQueueIoRead;
    DmfCallbacksWdf_Bridge.ModuleQueueIoWrite = DMF_Bridge_ModuleQueueIoWrite;
    DmfCallbacksWdf_Bridge.ModuleDeviceIoControl = DMF_Bridge_ModuleDeviceIoControl;
#if !defined(DMF_USER_MODE)
    DmfCallbacksWdf_Bridge.ModuleInternalDeviceIoControl = DMF_Bridge_ModuleInternalDeviceIoControl;
#endif // !defined(DMF_USER_MODE)
    DmfCallbacksWdf_Bridge.ModuleSelfManagedIoCleanup = DMF_Bridge_ModuleSelfManagedIoCleanup;
    DmfCallbacksWdf_Bridge.ModuleSelfManagedIoFlush = DMF_Bridge_ModuleSelfManagedIoFlush;
    DmfCallbacksWdf_Bridge.ModuleSelfManagedIoInit = DMF_Bridge_ModuleSelfManagedIoInit;
    DmfCallbacksWdf_Bridge.ModuleSelfManagedIoSuspend = DMF_Bridge_ModuleSelfManagedIoSuspend;
    DmfCallbacksWdf_Bridge.ModuleSelfManagedIoRestart = DMF_Bridge_ModuleSelfManagedIoRestart;
    DmfCallbacksWdf_Bridge.ModuleSurpriseRemoval = DMF_Bridge_ModuleSurpriseRemoval;
    DmfCallbacksWdf_Bridge.ModuleQueryRemove = DMF_Bridge_ModuleQueryRemove;
    DmfCallbacksWdf_Bridge.ModuleQueryStop = DMF_Bridge_ModuleQueryStop;
    DmfCallbacksWdf_Bridge.ModuleRelationsQuery = DMF_Bridge_ModuleRelationsQuery;
    DmfCallbacksWdf_Bridge.ModuleUsageNotificationEx = DMF_Bridge_ModuleUsageNotificationEx;
    DmfCallbacksWdf_Bridge.ModuleArmWakeFromS0 = DMF_Bridge_ModuleArmWakeFromS0;
    DmfCallbacksWdf_Bridge.ModuleDisarmWakeFromS0 = DMF_Bridge_ModuleDisarmWakeFromS0;
    DmfCallbacksWdf_Bridge.ModuleWakeFromS0Triggered = DMF_Bridge_ModuleWakeFromS0Triggered;
    DmfCallbacksWdf_Bridge.ModuleArmWakeFromSxWithReason = DMF_Bridge_ModuleArmWakeFromSxWithReason;
    DmfCallbacksWdf_Bridge.ModuleDisarmWakeFromSx = DMF_Bridge_ModuleDisarmWakeFromSx;
    DmfCallbacksWdf_Bridge.ModuleWakeFromSxTriggered = DMF_Bridge_ModuleWakeFromSxTriggered;
    DmfCallbacksWdf_Bridge.ModuleFileCreate = DMF_Bridge_ModuleFileCreate;
    DmfCallbacksWdf_Bridge.ModuleFileCleanup = DMF_Bridge_ModuleFileCleanup;
    DmfCallbacksWdf_Bridge.ModuleFileClose = DMF_Bridge_ModuleFileClose;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_Bridge,
                                            Bridge,
                                            DMF_CONTEXT_Bridge,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    DmfModuleDescriptor_Bridge.CallbacksDmf = &DmfCallbacksDmf_Bridge;
    DmfModuleDescriptor_Bridge.CallbacksWdf = &DmfCallbacksWdf_Bridge;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_Bridge,
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

// eof: Dmf_Bridge.c
//
