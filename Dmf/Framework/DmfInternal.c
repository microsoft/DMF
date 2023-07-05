/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfInternal.c

Abstract:

    DMF Implementation:

    This Module contains implementation of Internal DMF Callbacks. Given a Module handle, these
    functions extract the address of the given Module's callback and call it. In DEBUG builds,
    some sanity checks are performed.

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#include "DmfIncludeInternal.h"

#if defined(DMF_INCLUDE_TMH)
#include "DmfInternal.tmh"
#endif

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_Destroy(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch ModuleInstanceDestroy to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    DmfAssert(DmfModule != NULL);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsCreatedOrClosed(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksDmf->ModuleInstanceDestroy != NULL);
    (dmfObject->ModuleDescriptor.CallbacksDmf->ModuleInstanceDestroy)(DmfModule);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModulePrepareHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Dispatch ModulePrepareHardware to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.
    ResourcesRaw - WDF Resource Raw parameter that is passed to the given
                   DMF Module callback.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.

Return Value:

   The given DMF Module's corresponding handler's NTSTATUS code.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    DmfAssert(DmfModule != NULL);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsCreatedOrOpened(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModulePrepareHardware != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModulePrepareHardware)(DmfModule,
                                                                                 ResourcesRaw,
                                                                                 ResourcesTranslated);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModuleReleaseHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Dispatch ModuleReleaseHardware to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    DmfAssert(DmfModule != NULL);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsAvailable(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleReleaseHardware != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleReleaseHardware)(DmfModule,
                                                                                 ResourcesTranslated);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Dispatch ModuleD0Entry to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

   The given DMF Module's corresponding handler's NTSTATUS code.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    DmfAssert(DmfModule != NULL);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsCreatedOrOpened(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0Entry != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0Entry)(DmfModule,
                                                                         PreviousState);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModuleD0EntryPostInterruptsEnabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Dispatch ModuleD0EntryPostInterruptsEnabled to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

   The given DMF Module's corresponding handler's NTSTATUS code.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    DmfAssert(DmfModule != NULL);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsCreatedOrOpened(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0EntryPostInterruptsEnabled != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0EntryPostInterruptsEnabled)(DmfModule,
                                                                                              PreviousState);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModuleD0ExitPreInterruptsDisabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Dispatch ModuleD0ExitPreInterruptsDisabled to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;
    NTSTATUS ntStatus;

    DmfAssert(DmfModule != NULL);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsAvailable(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0ExitPreInterruptsDisabled != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0ExitPreInterruptsDisabled)(DmfModule,
                                                                                             TargetState);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModuleD0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Dispatch ModuleD0Exit to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;
    NTSTATUS ntStatus;

    DmfAssert(DmfModule != NULL);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsAvailable(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0Exit != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0Exit)(DmfModule,
                                                                        TargetState);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Internal_ModuleQueueIoRead(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    )
/*++

Routine Description:

    Dispatch ModuleQueueIoRead to the given DMF Module's corresponding handler.

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
    BOOLEAN handled;

    DmfAssert(DmfModule != NULL);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s] Request=0x%p", DmfModule, dmfObject->ClientModuleInstanceName, Request);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueueIoRead != NULL);
    handled = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueueIoRead)(DmfModule,
                                                                            Queue,
                                                                            Request,
                                                                            Length);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] handled=%d", DmfModule, dmfObject->ClientModuleInstanceName, handled);

    return handled;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Internal_ModuleQueueIoWrite(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    )
/*++

Routine Description:

    Dispatch ModuleQueueIoWrite to the given DMF Module's corresponding handler.

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
    BOOLEAN handled;

    DmfAssert(DmfModule != NULL);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s] Request=0x%p", DmfModule, dmfObject->ClientModuleInstanceName, Request);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueueIoWrite != NULL);
    handled = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueueIoWrite)(DmfModule,
                                                                             Queue,
                                                                             Request,
                                                                             Length);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] handled=%d", DmfModule, dmfObject->ClientModuleInstanceName, handled);

    return handled;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Internal_ModuleDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    Dispatch ModuleDeviceIoControl to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    OutputBufferLength - For fast access to the Request's Output Buffer Length.
    InputBufferLength - For fast access to the Request's Input Buffer Length.
    IoControlCode - The given IOCTL code.

Return Value:

    TRUE if the given DMF Module handled the IOCTL (either success or fail); or
    FALSE if the given DMF Module does not handle the IOCTL.

--*/
{
    DMF_OBJECT* dmfObject;
    BOOLEAN handled;

    DmfAssert(DmfModule != NULL);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s] Request=0x%p", DmfModule, dmfObject->ClientModuleInstanceName, Request);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleDeviceIoControl != NULL);
    handled = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleDeviceIoControl)(DmfModule,
                                                                                Queue,
                                                                                Request,
                                                                                OutputBufferLength,
                                                                                InputBufferLength,
                                                                                IoControlCode);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] handled=%d", DmfModule, dmfObject->ClientModuleInstanceName, handled);

    return handled;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Internal_ModuleInternalDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    Dispatch ModuleInternalDeviceIoControl to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    OutputBufferLength - For fast access to the Request's Output Buffer Length.
    InputBufferLength - For fast access to the Request's Input Buffer Length.
    IoControlCode - The given IOCTL code.

Return Value:

    TRUE if the given DMF Module handled the IOCTL (either success or fail); or
    FALSE if the given DMF Module does not handle the IOCTL.

--*/
{
    DMF_OBJECT* dmfObject;
    BOOLEAN handled;

    DmfAssert(DmfModule != NULL);

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s] Request=0x%p", DmfModule, dmfObject->ClientModuleInstanceName, Request);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleInternalDeviceIoControl != NULL);
    handled = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleInternalDeviceIoControl)(DmfModule,
                                                                                        Queue,
                                                                                        Request,
                                                                                        OutputBufferLength,
                                                                                        InputBufferLength,
                                                                                        IoControlCode);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] handled=%d", DmfModule, dmfObject->ClientModuleInstanceName, handled);

    return handled;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleSelfManagedIoCleanup(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch ModuleSelfManagedIoCleanup to the given DMF Module's corresponding handler.

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

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoCleanup != NULL);
    (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoCleanup)(DmfModule);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleSelfManagedIoFlush(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch ModuleSelfManagedIoFlush to the given DMF Module's corresponding handler.

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

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoFlush != NULL);
    (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoFlush)(DmfModule);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModuleSelfManagedIoInit(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch ModuleSelfManagedIoInit to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoInit != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoInit)(DmfModule);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModuleSelfManagedIoSuspend(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch ModuleSelfManagedIoSuspend to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;
    NTSTATUS ntStatus;

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoSuspend != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoSuspend)(DmfModule);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModuleSelfManagedIoRestart(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch ModuleSelfManagedIoRestart to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;
    NTSTATUS ntStatus;

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoRestart != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoRestart)(DmfModule);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleSurpriseRemoval(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch ModuleSurpriseRemoval to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSurpriseRemoval != NULL);
    (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSurpriseRemoval)(DmfModule);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModuleQueryRemove(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch ModuleQueryRemove to the given DMF Module's corresponding handler.

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

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueryRemove != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueryRemove)(DmfModule);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModuleQueryStop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch ModuleQueryStop to the given DMF Module's corresponding handler.

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

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueryStop != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueryStop)(DmfModule);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleRelationsQuery(
    _In_ DMFMODULE DmfModule,
    _In_ DEVICE_RELATION_TYPE RelationType
    )
/*++

Routine Description:

    Dispatch ModuleRelationsQuery to the given DMF Module's corresponding handler.

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

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleRelationsQuery != NULL);
    (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleRelationsQuery)(DmfModule,
                                                                     RelationType);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModuleUsageNotificationEx(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_SPECIAL_FILE_TYPE NotificationType,
    _In_ BOOLEAN IsInNotificationPath
    )
/*++

Routine Description:

    Dispatch ModuleUsageNotificationEx to the given DMF Module's corresponding handler.

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

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleUsageNotificationEx != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleUsageNotificationEx)(DmfModule,
                                                                                     NotificationType,
                                                                                     IsInNotificationPath);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModuleArmWakeFromS0(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch ModuleArmWakeFromS0 to the given DMF Module's corresponding handler.

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

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleArmWakeFromS0 != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleArmWakeFromS0)(DmfModule);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleDisarmWakeFromS0(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch ModuleDisarmWakeFromS0 to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleDisarmWakeFromS0 != NULL);
    (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleDisarmWakeFromS0)(DmfModule);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleWakeFromS0Triggered(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch ModuleWakeFromS0Triggered to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleWakeFromS0Triggered != NULL);
    (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleWakeFromS0Triggered)(DmfModule);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModuleArmWakeFromSxWithReason(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN DeviceWakeEnabled,
    _In_ BOOLEAN ChildrenArmedForWake
    )
/*++

Routine Description:

    Dispatch ModuleArmWakeFromSX to the given DMF Module's corresponding handler.

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

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleArmWakeFromSxWithReason != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleArmWakeFromSxWithReason)(DmfModule,
                                                                                         DeviceWakeEnabled,
                                                                                         ChildrenArmedForWake);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleDisarmWakeFromSx(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch ModuleDisarmWakeFromSx to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleDisarmWakeFromSx != NULL);
    (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleDisarmWakeFromSx)(DmfModule);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleWakeFromSxTriggered(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch ModuleWakeFromSxTriggered to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleWakeFromSxTriggered != NULL);
    (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleWakeFromSxTriggered)(DmfModule);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Internal_ModuleFileCreate(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Dispatch ModuleFileCreate to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.
    Device - WDF device object.
    Request - WDF Request with IOCTL parameters.
    FileObject - WDF file object that describes a file that is being opened for the specified request

Return Value:

    TRUE if the given DMF Module handled the callback; or
    FALSE if the given DMF Module does not handle the callback.

--*/
{
    DMF_OBJECT* dmfObject;
    BOOLEAN handled;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s] Request=0x%p", DmfModule, dmfObject->ClientModuleInstanceName, Request);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleFileCreate != NULL);
    handled = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleFileCreate)(DmfModule,
                                                                           Device,
                                                                           Request,
                                                                           FileObject);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] handled=%d", DmfModule, dmfObject->ClientModuleInstanceName, handled);

    return handled;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Internal_ModuleFileCleanup(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Dispatch ModuleFileCleanup to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.
    FileObject - WDF file object

Return Value:

    TRUE if the given DMF Module handled the callback; or
    FALSE if the given DMF Module does not handle the callback.

--*/
{
    DMF_OBJECT* dmfObject;
    BOOLEAN handled;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleFileCleanup != NULL);
    handled = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleFileCleanup)(DmfModule,
                                                                            FileObject);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] handled=%d", DmfModule, dmfObject->ClientModuleInstanceName, handled);

    return handled;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Internal_ModuleFileClose(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Dispatch ModuleFileClose to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.
    FileObject - WDF file object

Return Value:

    TRUE if the given DMF Module handled the callback; or
    FALSE if the given DMF Module does not handle the callback.

--*/
{
    DMF_OBJECT* dmfObject;
    BOOLEAN handled;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_HandleValidate_IsOpen(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleFileClose != NULL);
    handled = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleFileClose)(DmfModule,
                                                                          FileObject);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] handled=%d", DmfModule, dmfObject->ClientModuleInstanceName, handled);

    return handled;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ResourcesAssign(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Dispatch ResourcesAssign to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.
    ResourcesRaw - WDF Resource Raw parameter that is passed to the given
                   DMF Module callback.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.

Return Value:

   The given DMF Module's corresponding handler's NTSTATUS code.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    // NOTE: In the case where there is no handler, allow "Opened".
    // NOTE: In the cases where Modules are Opened, Closed and Opened again, allow "Closed" state.
    //
    DMF_HandleValidate_IsAvailable(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksDmf->DeviceResourcesAssign != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksDmf->DeviceResourcesAssign)(DmfModule,
                                                                                 ResourcesRaw,
                                                                                 ResourcesTranslated);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_NotificationRegister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch NotificationRegister to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

   The given DMF Module's corresponding handler's NTSTATUS code.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    // NOTE: In the cases where Modules are Opened, Closed and Opened again, allow "Closed" state.
    //
    DMF_HandleValidate_IsAvailable(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksDmf->DeviceNotificationRegister != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksDmf->DeviceNotificationRegister)(DmfModule);
    // Module NotificationRegister should never fail unless the driver cannot be loaded.
    // When debugging it can be difficult to determine which Module failed to register for notifications.
    // When Module NotificationRegister fails, the driver just becomes disabled.
    // This breakpoint makes it easy to determine which Module fails.
    //
    DmfAssert(NT_SUCCESS(ntStatus));

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_NotificationUnregister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch NotificationUnregister to the given DMF Module's corresponding handler.

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

    DMF_HandleValidate_IsAvailable(dmfObject);

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksDmf->DeviceNotificationUnregister != NULL);
    (dmfObject->ModuleDescriptor.CallbacksDmf->DeviceNotificationUnregister)(DmfModule);

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch Open to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

   The given DMF Module's corresponding handler's NTSTATUS code.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    // Indicate the Module is open and cannot be deleted.
    //
    DMF_Portable_EventReset(&dmfObject->ModuleCanBeDeletedEvent);

    DMF_HandleValidate_Open(dmfObject);
    dmfObject->ModuleState = ModuleState_Opening;

    // Open the Module.
    //
    DmfAssert(dmfObject->ModuleDescriptor.CallbacksDmf->DeviceOpen != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksDmf->DeviceOpen)(DmfModule);
    if (NT_SUCCESS(ntStatus))
    {
        // The Module is open.
        //
        dmfObject->ModuleState = ModuleState_Opened;

        // Allow DMF_ModuleReference to succeed only after the Module is completely open. 
        //
        DmfAssert(dmfObject->ReferenceCount == 0);
        dmfObject->ReferenceCount = 1;

        // This may be overwritten by DMF if the Module is automatically opened.
        // Otherwise, it means the Client opened the Module.
        //
        DmfAssert(dmfObject->ModuleOpenedDuring == ModuleOpenedDuringType_Invalid);
        dmfObject->ModuleOpenedDuring = ModuleOpenedDuringType_Manual;

        // We will need to call PreClose when this Module is being closed.
        //
        dmfObject->NeedToCallPreClose = TRUE;

        // Allow client to call Module Methods if necessary.
        //
        DmfAssert(dmfObject->Callbacks.EvtModuleOnDeviceNotificationPostOpen != NULL);
        // Client notifications always get the Client Context. The Client decides what the
        // context means.
        //
        (dmfObject->Callbacks.EvtModuleOnDeviceNotificationPostOpen)(DmfModule);

        // Flag that indicates when Module can be closed.
        //
        dmfObject->ModuleClosed = FALSE;
    }
    else
    {
        // The Module is not open.
        //
        dmfObject->ModuleState = ModuleState_Created;
        // Module Open should never fail unless the driver cannot be loaded.
        // When debugging it can be difficult to determine which Module failed to open.
        // When Module open fails, the driver just becomes disabled.
        // This breakpoint makes it easy to determine which Module fails.
        //
        DmfAssert(FALSE);

        // Indicate the Module has can be deleted (because it was not opened).
        //
        DMF_Portable_EventSet(&dmfObject->ModuleCanBeDeletedEvent);
    }

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s] ntStatus=%!STATUS!", DmfModule, dmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_ModuleIsClosed(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Allows caller to know if the Module can be closed. Checks that status and clears it to ensure
    Client closes Module only a single time.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    TRUE if Module can be closed; FALSE, otherwise.

--*/
{
    DMF_OBJECT* dmfObject;
    BOOLEAN moduleClosed;

    dmfObject = DMF_ModuleToObject(DmfModule);

    GENERIC_SPINLOCK_CONTEXT lockContext;
    DMF_GenericSpinLockAcquire(&dmfObject->ReferenceCountLock,
                               &lockContext);

    moduleClosed = dmfObject->ModuleClosed;
    dmfObject->ModuleClosed = TRUE;

    DMF_GenericSpinLockRelease(&dmfObject->ReferenceCountLock,
                               lockContext);

    return moduleClosed;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dispatch Close to the given DMF Module's corresponding handler.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

   None.

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    // Prevent the Module from being closed twice.
    //
    if (DMF_ModuleIsClosed(DmfModule))
    {
        goto Exit;
    }

    DMF_HandleValidate_Close(dmfObject);

    if (TRUE == dmfObject->NeedToCallPreClose)
    {
        // The Module was successfully opened and now we are closing it.
        // Allow client to call Module Methods if necessary.
        //
        DmfAssert(dmfObject->Callbacks.EvtModuleOnDeviceNotificationPreClose != NULL);
        // Client notifications always get the Client Context. The Client decides what the
        // context means.
        //
        (dmfObject->Callbacks.EvtModuleOnDeviceNotificationPreClose)(DmfModule);
        dmfObject->NeedToCallPreClose = FALSE;
    }

    // Now that PreClose is done, wait for reference count to clear and close the Module.
    // This allows PreClose to access Module Methods if needed.
    //
    DMF_ModuleWaitForReferenceCountToClear(DmfModule);

    dmfObject->ModuleState = ModuleState_Closing;

    DmfAssert(dmfObject->ModuleDescriptor.CallbacksDmf->DeviceClose != NULL);
    (dmfObject->ModuleDescriptor.CallbacksDmf->DeviceClose)(DmfModule);

    dmfObject->ModuleState = ModuleState_Closed;

    DmfAssert(dmfObject->ModuleOpenedDuring < ModuleOpenedDuringType_Maximum);
    DmfAssert(dmfObject->ModuleOpenedDuring != ModuleOpenedDuringType_Invalid);
    dmfObject->ModuleOpenedDuring = ModuleOpenedDuringType_Invalid;

    // Indicate the Module has been closed and can be deleted.
    //
    DMF_Portable_EventSet(&dmfObject->ModuleCanBeDeletedEvent);

Exit:

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);
}

// eof: DmfInternal.c
//
