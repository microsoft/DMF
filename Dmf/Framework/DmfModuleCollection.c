/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfModuleCollection.c

Abstract:

    DMF Implementation:

    This file contains support for DMF Module Collection.
    A DMF Module Collection is a list of top level Modules that are instantiated
    directly by the Client Driver.
    One of the tasks is to dispatch WDF Callbacks to all the Modules in the Collection 
    and their children. Depending on the callback, sometimes the parent receives the callback
    first and then the children. In other cases the inverse occurs.

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// Test Options:
// -------------
// NOTE: These are not to be used in production code.
//

#if defined(DEBUG)

// Inject partial successful initialization of Modules in a Module Collection.
//
// #define USE_DMF_INJECT_FAULT_PARTIAL_OPEN

#endif // defined(DEBUG)

#include "DmfIncludeInternal.h"

#include "DmfModuleCollection.tmh"

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_PrepareHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_ReleaseHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesTranslated
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Module_D0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Module_D0EntryPostInterruptsEnabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_D0ExitPreInterruptsDisabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_D0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Module_QueueIoRead(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Module_QueueIoWrite(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Module_DeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Module_InternalDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Module_SelfManagedIoCleanup(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Module_SelfManagedIoFlush(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_Module_SelfManagedIoInit(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_Module_SelfManagedIoSuspend(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_Module_SelfManagedIoRestart(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Module_SurpriseRemoval(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_QueryRemove(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_QueryStop(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Module_RelationsQuery(
    _In_ DMFMODULE DmfModule,
    _In_ DEVICE_RELATION_TYPE RelationType
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Module_UsageNotificationEx(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_SPECIAL_FILE_TYPE NotificationType,
    _In_ BOOLEAN IsInNotificationPath
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_ArmWakeFromS0(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Module_DisarmWakeFromS0(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Module_WakeFromS0Triggered(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_ArmWakeFromSxWithReason(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN DeviceWakeEnabled,
    _In_ BOOLEAN ChildrenArmedForWake
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Module_WakeFromSxTriggered(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Module_DisarmWakeFromSx(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Module_FileCreate(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Module_FileCleanup(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Module_FileClose(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    );

NTSTATUS
DMF_Module_NotificationRegister(
    _In_ DMFMODULE DmfModule
    );

VOID
DMF_Module_NotificationUnregister(
    _In_ DMFMODULE DmfModule
    );

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Module Collection Creation/Destruction
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
VOID
DMF_ModuleCollectionHandleSet(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Set the Module Collection handle in the given DMF Object as well as its Child Modules.

Arguments:

    DmfObject - The given DMF Object.

Return Value:

    None

--*/
{
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationIndex;

    ASSERT(DmfObject != NULL);
    ASSERT(DmfObject->ModuleCollection != NULL);

    PAGED_CODE();

    // Dispatch callback to Child DMF Modules first.
    //
    childDmfObject = DmfChildObjectFirstGet(DmfObject,
                                            &childObjectIterationIndex);
    while (childDmfObject != NULL)
    {
        // Assign the Child's Module Collection handle from the parent. In this way the Child can
        // access the Feature modules easily.
        //
        // NOTE: It is necessary to make sure the ModuleCollectionHandle is set to avoid numerous
        //       if checks other places. Also, this handle may be used for other purposes in
        //       the future.
        //
        ASSERT(NULL == childDmfObject->ModuleCollection);
        childDmfObject->ModuleCollection = DmfObject->ModuleCollection;
        ASSERT(childDmfObject->ModuleCollection != NULL);
        DMF_ModuleCollectionHandleSet(childDmfObject);
        childDmfObject = DmfChildObjectNextGet(&childObjectIterationIndex);
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
DMF_OBJECT*
DMF_ModuleCollectionFeatureHandleGet(
    _In_ DMF_MODULE_COLLECTION* ModuleCollectionHandle,
    _In_ DmfFeatureType DmfFeature
    )
/*++

Routine Description:

    Returns the DMF_OBJECT of a specified DMF Feature from the Module Collection.

Arguments:

    ModuleCollectionHandle - The DMF Module Collection Handle.
    DmfFeature - Specifies which DMF Feature.

Return Value:

    DMF_OBJECT* - The DMF Object of the specified feature if found, NULL otherwise.

--*/
{
    LONG driverModuleIndex;
    DMF_OBJECT* dmfObject;

    PAGED_CODE();

    dmfObject = NULL;

    for (driverModuleIndex = 0; driverModuleIndex < ModuleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        dmfObject = ModuleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];

        ASSERT(dmfObject != NULL);

        switch (DmfFeature)
        {
            case DmfFeature_BranchTrack:
            {
                if (strcmp(dmfObject->ModuleName, DMFFEATURE_NAME_BranchTrack) == 0)
                {
                    return dmfObject;
                }
                break;
            }
            case DmfFeature_LiveKernelDump:
            {
                if (strcmp(dmfObject->ModuleName, DMFFEATURE_NAME_LiveKernelDump) == 0)
                {
                    return dmfObject;
                }
                break;
            }
            default:
            {
                ASSERT(FALSE);
                break;
            }
        }
    }

    return dmfObject;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionHandlePropagate(
    _Inout_ DMF_MODULE_COLLECTION* ModuleCollectionHandle,
    _In_ LONG NumberOfEntries
    )
/*++

Routine Description:

    Set the Module Collection handle into the tree of instantiated Modules.

Arguments:

    ModuleCollectionHandle - Module Collection that contains the Modules that need DMF Feature initialization.
    NumberOfEntries - Number of entries in the table.

Return Value:

    None

--*/
{
    LONG driverModuleIndex;

    PAGED_CODE();

    for (driverModuleIndex = 0; driverModuleIndex < NumberOfEntries; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject = ModuleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];

        ASSERT(dmfObject != NULL);
        DMF_ModuleCollectionHandleSet(dmfObject);
    }
}
#pragma code_seg()

static
VOID
DMF_ModuleCollectionCleanup(
    _In_ DMF_MODULE_COLLECTION* ModuleCollectionHandle,
    _In_ ModuleOpenedDuringType ModuleOpenedDuring
    )
/*++

Routine Description:

    Close all Modules in a Module Collection that have been opened during a given
    callback. This function is called during failure to cleanup prior to exit.

Arguments:

    ModuleCollectionHandle - The list of the Client Driver's instantiated Modules.
    ModuleOpenedDuring - The current callback for which modules should be closed.

Return Value:

    None

--*/
{
    LONG driverModuleIndex;

    FuncEntryArguments(DMF_TRACE, "ModuleCollectionHandle=0x%p ModuleOpenedDuring=%d", ModuleCollectionHandle, ModuleOpenedDuring);

    // NumberOfClientDriverDmfModules may be zero in cases where all elements of structure
    // have not been allocated (usually due to fault injection).
    //
    for (driverModuleIndex = 0; driverModuleIndex < ModuleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;

        dmfObject = ModuleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        DMFMODULE dmfModule = DMF_ObjectToModule(dmfObject);
        if (dmfObject->ModuleOpenedDuring == ModuleOpenedDuring)
        {
            // The Module needs to be cleaned up (closed).
            //
            TraceInformation(DMF_TRACE, "Cleanup (close) ModuleCollectionHandle=0x%p dmfObject=0x%p", ModuleCollectionHandle, dmfObject);
            DMF_ModuleClose(dmfModule);
        }
        else if (dmfObject->ModuleNotificationRegisteredDuring == ModuleOpenedDuring)
        {
            // The Module needs to be cleaned up (notification unregistered).
            //
            TraceInformation(DMF_TRACE, "Cleanup (notification) ModuleCollectionHandle=0x%p dmfObject=0x%p", ModuleCollectionHandle, dmfObject);
            DMF_Module_NotificationUnregister(dmfModule);
        }
    }

    FuncExit(DMF_TRACE, "ModuleCollectionHandle=0x%p", ModuleCollectionHandle);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionDestroy(
    _In_ WDFOBJECT Object
    )
/*++

Routine Description:

    Given a WDFOBJECT which is a DMFCOLLECTION, this function destroys all the
    Modules that are associated with the DMFCOLLECTION. This function is designed
    to be called directly by WDF (it is a WDF clean up callback).

Arguments:

    Object - The given WDFOBJECT.

Return Value:

    None

--*/
{
    LONG driverModuleIndex;
    DMF_OBJECT* dmfObject;
    DMFCOLLECTION dmfCollection;

    dmfCollection = (DMFCOLLECTION)Object;

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", dmfCollection);

    if (NULL == dmfCollection)
    {
        // This is a normal code path in case driver initialization completely fails.
        // ReleaseHardware is always called.
        //
        goto Exit;
    }

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(dmfCollection);

    // In the case of non-PnP drivers where DMF_Invoke_DeviceCallbacksCreate() has been called in DriverEntry()
    // the Client Driver has no chance to call the corresponding DMF_Invoke_DeviceCallbacksDestroy() since this
    // callback happens before the Client Driver's Cleanup callback. Because Modules must be Closed before they
    // are destroyed this condition is detected and the corresponding function is called here.
    //
    if (moduleCollectionHandle->ManualDestroyCallbackIsPending)
    {
        moduleCollectionHandle->ManualDestroyCallbackIsPending = FALSE;
        // '_Param_(2)' could be '0':  this does not adhere to the specification for the function 'DMF_Invoke_DeviceCallbacksDestroy'
        //
        #pragma warning(suppress:6387)
        DMF_Invoke_DeviceCallbacksDestroy(moduleCollectionHandle->ClientDevice,
                                          NULL,
                                          WdfPowerDeviceD0);
    }

    // Close any modules that were automatically opened after creation.
    //
    for (driverModuleIndex = (moduleCollectionHandle->NumberOfClientDriverDmfModules - 1);
         driverModuleIndex >= 0;
         driverModuleIndex--)
    {
        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        DMFMODULE dmfModule = DMF_ObjectToModule(dmfObject);

        DMF_Module_CloseOrUnregisterNotificationOnDestroy(dmfModule);
    }

    // Destroy every Module in the collection.
    //
    for (driverModuleIndex = 0; driverModuleIndex < moduleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        DMF_ModuleTreeDestroy(dmfModule);
        moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex] = NULL;
    }

    if (moduleCollectionHandle->ClientDriverDmfModules != NULL)
    {
        ASSERT(moduleCollectionHandle->ClientDriverDmfModulesMemory != NULL);
        WdfObjectDelete(moduleCollectionHandle->ClientDriverDmfModulesMemory);
        moduleCollectionHandle->ClientDriverDmfModules = NULL;
        moduleCollectionHandle->NumberOfClientDriverDmfModules = 0;
        moduleCollectionHandle->ClientDriverDmfModulesMemory = NULL;
    }

    // (This function is called from the object's delete callback. It means that ModuleCollectionHandleMemory
    // has already been deleted and must not be deleted again. It is not possible to call this function
    // directly. It must always be called from the object's delete callback.)
    //
    moduleCollectionHandle = NULL;

Exit:
    ;

    FuncExitVoid(DMF_TRACE);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Module Collection Dispatch Function Helpers
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Types for Module Collection functions that have common signatures so that the common coding pattern
// does not need to be duplicated.
//
typedef NTSTATUS (*ModuleCollectionHandleDispatchFunctionNtStatusType)(_In_ DMFMODULE DmfModule);
typedef VOID (*ModuleCollectionHandleDispatchFunctionVoidType)(_In_ DMFMODULE DmfModule);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_ModuleCollectionDispatchNtStatus(
    _In_ DMF_MODULE_COLLECTION* ModuleCollectionHandle,
    _In_ ModuleCollectionHandleDispatchFunctionNtStatusType ModuleCollectionDispatchFunction
    )
/*++

Routine Description:

    Given a DMF_MODULE_COLLECTION and a callback function that takes a DMFMODULE and returns NTSTATUS,
    call the callback function for every Module associated with the given DMF_MODULE_COLLECTION.

Arguments:

    ModuleCollectionHandle - The list of the Client Driver's instantiated Modules.
    ModuleCollectionDispatchFunction - The given callback function to call.

Return Value:

    NTSTATUS

--*/
{
    LONG driverModuleIndex;
    NTSTATUS ntStatus;

    FuncEntryArguments(DMF_TRACE, "ModuleCollectionHandle=0x%p", ModuleCollectionHandle);

    ntStatus = STATUS_SUCCESS;

    for (driverModuleIndex = 0; driverModuleIndex < ModuleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = ModuleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        // NOTE: By design, this function will exit as soon as a Module returns an error.
        //
        ntStatus = ModuleCollectionDispatchFunction(dmfModule);
        if (! NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "ModuleCollectionHandle=0x%p ntStatus=%!STATUS!", ModuleCollectionHandle, ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ModuleCollectionDispatchVoid(
    _In_ DMF_MODULE_COLLECTION* ModuleCollectionHandle,
    _In_ ModuleCollectionHandleDispatchFunctionVoidType ModuleCollectionDispatchFunction
    )
/*++

Routine Description:

    Given a DMF_MODULE_COLLECTION and a callback function that takes a DMFMODULE and returns nothing,
    call the callback function for every Module associated with the given DMF_MODULE_COLLECTION.

Arguments:

    ModuleCollectionHandle - The list of the Client Driver's instantiated Modules.
    ModuleCollectionDispatchFunction - The given callback function to call.

Return Value:

    NTSTATUS

--*/
{
    LONG driverModuleIndex;

    FuncEntryArguments(DMF_TRACE, "ModuleCollectionHandle=0x%p", ModuleCollectionHandle);

    for (driverModuleIndex = 0; driverModuleIndex < ModuleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = ModuleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        ModuleCollectionDispatchFunction(dmfModule);
    }

    FuncExit(DMF_TRACE, "ModuleCollectionHandle=0x%p", ModuleCollectionHandle);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Module Collection Dispatch Functions
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModuleCollectionPrepareHardware(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDevicePrepareHardware to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The given DMFCOLLECTION. (Contains the list of Client Driver's instantiated Modules.)
    ResourcesRaw - Same as passed from WDF.
    ResourcesTranslated - Same as passed from WDF.

Return Value:

    STATUS_SUCCESS on success if every Module in the list succeeds, or the NTSTATUS error
    code from the Module that returns an error.

--*/
{
    NTSTATUS ntStatus;
    LONG driverModuleIndex;

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    ntStatus = STATUS_SUCCESS;

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModulePrepareHardwareImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModulePrepareHardware ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    for (driverModuleIndex = 0; driverModuleIndex < moduleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        ntStatus = DMF_Module_PrepareHardware(dmfModule,
                                              ResourcesRaw,
                                              ResourcesTranslated);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ModulePrepareHardware dmfObject=0x%p ntStatus=%!STATUS!", dmfObject, ntStatus);
            goto Exit;
        }
    }

Exit:

    if (! NT_SUCCESS(ntStatus))
    {
        // If any call to PrepareHardware handlers fails: we need to close any modules that were 
        // opened by THIS call.
        //
        DMF_ModuleCollectionCleanup(moduleCollectionHandle,
                                    ModuleOpenedDuringType_PrepareHardware);
    }

    FuncExit(DMF_TRACE, "DmfCollection=0x%p ntStatus=%!STATUS!", DmfCollection, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionReleaseHardware(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceReleaseHardware to
    every Module associated with DMFCOLLECTION.

    NOTE: DMF_ModuleCollectionPrepareHardwareCleanup is not necessary because ReleaseHardware 
          is always called regardless of exit status of PrepareHardware.

Arguments:

    DmfCollection - The given DMFCOLLECTION. (Contains the list of Client Driver's instantiated Modules.)
    ResourcesTranslated - Same as passed from WDF.

Return Value:

    STATUS_SUCCESS on success if every Module in the list succeeds, or the NTSTATUS error
    code from the Module that returns an error.

--*/
{
    NTSTATUS ntStatus;
    LONG driverModuleIndex;

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    ntStatus = STATUS_SUCCESS;

    if (NULL == DmfCollection)
    {
        // It is possible to zero if Prepare Hardware failed.
        // UnprepareHardware is always called.
        //
        goto Exit;
    }

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleReleaseHardwareImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleReleaseHardware ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (0 == moduleCollectionHandle->NumberOfClientDriverDmfModules)
    {
        // It is possible to zero if Prepare Hardware failed.
        // UnprepareHardware is always called.
        //
        goto Exit;
    }

    // Release Modules in reverse order in which they were created.
    //
    for (driverModuleIndex = moduleCollectionHandle->NumberOfClientDriverDmfModules - 1; driverModuleIndex >= 0; driverModuleIndex--)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        ntStatus = DMF_Module_ReleaseHardware(dmfModule,
                                              ResourcesTranslated);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ModulePrepareHardware dmfObject=0x%p ntStatus=%!STATUS!", dmfObject, ntStatus);
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p ntStatus=%!STATUS!", DmfCollection, ntStatus);

    return ntStatus;
}
#pragma code_seg()

// D0Entry/D0Exit code must not be pageable even though it runs at PASSIVE_LEVEL.
//
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModuleCollectionD0Entry(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceD0Entry to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The given DMFCOLLECTION. (Contains the list of Client Driver's instantiated Modules.)
    PreviousState - Same as passed from WDF.

Return Value:

    STATUS_SUCCESS on success if every Module in the list succeeds, or the NTSTATUS error
    code from the Module that returns an error.

--*/
{
    NTSTATUS ntStatus;
    LONG driverModuleIndex;

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p PreviousState=%d", DmfCollection, PreviousState);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    ntStatus = STATUS_SUCCESS;

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleD0EntryImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleD0Entry ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ASSERT(moduleCollectionHandle->NumberOfClientDriverDmfModules > 0);
    for (driverModuleIndex = 0; driverModuleIndex < moduleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        ntStatus = DMF_Module_D0Entry(dmfModule,
                                      PreviousState);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ModuleD0Entry dmfObject=0x%p ntStatus=%!STATUS!", dmfObject, ntStatus);
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p ntStatus=%!STATUS!", DmfCollection, ntStatus);

    return ntStatus;
}

// D0Entry/D0Exit code must not be pageable even though it runs at PASSIVE_LEVEL.
//
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModuleCollectionD0EntryPostInterruptsEnabled(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceD0EntryPostInterruptsEnabled to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The given DMFCOLLECTION. (Contains the list of Client Driver's instantiated Modules.)
    PreviousState - Same as passed from WDF.

Return Value:

    STATUS_SUCCESS on success if every Module in the list succeeds, or the NTSTATUS error
    code from the Module that returns an error.

--*/
{
    NTSTATUS ntStatus;
    LONG driverModuleIndex;

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p PreviousState=%d", DmfCollection, PreviousState);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    ntStatus = STATUS_SUCCESS;

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleD0EntryPostInterruptsEnabledImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleD0EntryPostInterruptsEnabled ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ASSERT(moduleCollectionHandle->NumberOfClientDriverDmfModules > 0);
    for (driverModuleIndex = 0; driverModuleIndex < moduleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        ntStatus = DMF_Module_D0EntryPostInterruptsEnabled(dmfModule,
                                                           PreviousState);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ModuleD0EntryPostInterruptsEnabled dmfObject=0x%p ntStatus=%!STATUS!", dmfObject, ntStatus);
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p ntStatus=%!STATUS!", DmfCollection, ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ModuleCollectionD0EntryCleanup(
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    The Client Driver may call this function in its Device EvtDeviceD0Entry callback if the call
    to DMF_ModuleCollectionD0Entry succeeded but later operations in that
    callback fail.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.

Return Value:

    VOID

--*/
{
    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    DMF_ModuleCollectionCleanup(moduleCollectionHandle,
                                ModuleOpenedDuringType_D0EntrySystemPowerUp);
    DMF_ModuleCollectionCleanup(moduleCollectionHandle,
                                ModuleOpenedDuringType_D0Entry);

    FuncExit(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionD0ExitPreInterruptsDisabled(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceD0EntryPreInterruptsDisabled to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The given DMFCOLLECTION. (Contains the list of Client Driver's instantiated Modules.)
    TargetState - Same as passed from WDF.

Return Value:

    STATUS_SUCCESS on success if every Module in the list succeeds, or the NTSTATUS error
    code from the Module that returns an error.

--*/
{
    NTSTATUS ntStatus;
    LONG driverModuleIndex;

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p TargetState=%d", DmfCollection, TargetState);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    ntStatus = STATUS_SUCCESS;

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleD0ExitPreInterruptsDisabledImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleD0ExitPreInterruptsDisabled ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Destroy Modules in reverse way they were created.
    //
    ASSERT(moduleCollectionHandle->NumberOfClientDriverDmfModules > 0);
    for (driverModuleIndex = moduleCollectionHandle->NumberOfClientDriverDmfModules - 1; driverModuleIndex >= 0; driverModuleIndex--)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        ntStatus = DMF_Module_D0ExitPreInterruptsDisabled(dmfModule,
                                                          TargetState);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ModuleD0ExitPreInterruptsDisabled dmfObject=0x%p ntStatus=%!STATUS!", dmfObject, ntStatus);
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p ntStatus=%!STATUS!", DmfCollection, ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionD0Exit(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceD0Exit to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The given DMFCOLLECTION. (Contains the list of Client Driver's instantiated Modules.)
    TargetState - Same as passed from WDF.

Return Value:

    STATUS_SUCCESS on success if every Module in the list succeeds, or the NTSTATUS error
    code from the Module that returns an error.

--*/
{
    NTSTATUS ntStatus;
    LONG driverModuleIndex;

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p TargetState=%d", DmfCollection, TargetState);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    ntStatus = STATUS_SUCCESS;

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleD0ExitImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleD0Exit ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Destroy Modules in reverse way they were created.
    //
    ASSERT(moduleCollectionHandle->NumberOfClientDriverDmfModules > 0);
    for (driverModuleIndex = moduleCollectionHandle->NumberOfClientDriverDmfModules - 1; driverModuleIndex >= 0; driverModuleIndex--)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        ntStatus = DMF_Module_D0Exit(dmfModule,
                                     TargetState);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ModuleD0Exit dmfObject=0x%p ntStatus=%!STATUS!", dmfObject, ntStatus);
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p ntStatus=%!STATUS!", DmfCollection, ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_ModuleCollectionQueueIoRead(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtQueueIoRead to
    every Module associated with DMFCOLLECTION. As soon as one of the Modules
    handles the IOCTL, this function returns to prevent any possible double return of a WDFREQUEST.

Arguments:

    DmfCollection - The given DMFCOLLECTION. (Contains the list of Client Driver's instantiated Modules.)
    Queue - Client Driver's Default Queue.
    Request - The WDFREQEUST with the IOCTL's parameters.
    Length - Buffer Length for the IOCTL.

Return Value:

    TRUE if any of the Modules handled the Request, FALSE otherwise.
    In the FALSE case the Client Driver is expected to handle the request.

--*/
{
    LONG driverModuleIndex;
    BOOLEAN handled;

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p Request=0x%p", DmfCollection, Request);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    handled = FALSE;

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleQueueIoReadImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleQueueIoRead handled=%d", handled);
        goto Exit;
    }

    ASSERT(moduleCollectionHandle->NumberOfClientDriverDmfModules > 0);
    for (driverModuleIndex = 0; driverModuleIndex < moduleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        handled = DMF_Module_QueueIoRead(dmfModule,
                                         Queue,
                                         Request,
                                         Length);
        if (handled)
        {
            // The Module handled the call...no need to continue dispatching.
            //
            break;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p handled=%d", DmfCollection, handled);

    return handled;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_ModuleCollectionQueueIoWrite(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtQueueIoWrite to
    every Module associated with DMFCOLLECTION. As soon as one of the Modules
    handles the IOCTL, this function returns to prevent any possible double return of a WDFREQUEST.

Arguments:

    DmfCollection - The given DMFCOLLECTION. (Contains the list of Client Driver's instantiated Modules.)
    Queue - Client Driver's Default Queue.
    Request - The WDFREQEUST with the IOCTL's parameters.
    Length - Buffer Length for the IOCTL.

Return Value:

    TRUE if any of the Modules handled the Request, FALSE otherwise.
    In the FALSE case the Client Driver is expected to handle the request.

--*/
{
    LONG driverModuleIndex;
    BOOLEAN handled;

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p Request=0x%p", DmfCollection, Request);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    handled = FALSE;

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleQueueIoWriteImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleQueueIoWrite handled=%d", handled);
        goto Exit;
    }

    ASSERT(moduleCollectionHandle->NumberOfClientDriverDmfModules > 0);
    for (driverModuleIndex = 0; driverModuleIndex < moduleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        handled = DMF_Module_QueueIoWrite(dmfModule,
                                          Queue,
                                          Request,
                                          Length);
        if (handled)
        {
            // The Module handled the call...no need to continue dispatching.
            //
            break;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p handled=%d", DmfCollection, handled);

    return handled;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_ModuleCollectionDeviceIoControl(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceIoControl to
    every Module associated with DMFCOLLECTION. As soon as one of the Modules
    handles the IOCTL, this function returns to prevent any possible double return of a WDFREQUEST.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.
    Queue - Client Driver's Default Queue.
    Request - The WDFREQEUST with the IOCTL's parameters.
    OutputBufferLength - Output Buffer Length for the IOCTL.
    InputBufferLength - Input Buffer Length for the IOCTL.
    IoControlCode - The IOCTL Code.

Return Value:

    TRUE if any of the Modules handled the Request, FALSE otherwise.
    In the FALSE case the Client Driver is expected to handle the request.

--*/
{
    LONG driverModuleIndex;
    BOOLEAN handled;

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p Request=0x%p", DmfCollection, Request);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    handled = FALSE;

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleDeviceIoControlImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleDeviceIoControl handled=%d", handled);
        goto Exit;
    }

    ASSERT(moduleCollectionHandle->NumberOfClientDriverDmfModules > 0);
    for (driverModuleIndex = 0; driverModuleIndex < moduleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        handled = DMF_Module_DeviceIoControl(dmfModule,
                                             Queue,
                                             Request,
                                             OutputBufferLength,
                                             InputBufferLength,
                                             IoControlCode);
        if (handled)
        {
            // The Module handled the call...no need to continue dispatching.
            //
            break;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p handled=%d", DmfCollection, handled);

    return handled;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_ModuleCollectionInternalDeviceIoControl(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtInternalDeviceIoControl to
    every Module associated with DMFCOLLECTION. As soon as one of the Modules
    handles the IOCTL, this function returns to prevent any possible double return of a WDFREQUEST.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.
    Queue - Client Driver's Default Queue.
    Request - The WDFREQEUST with the IOCTL's parameters.
    OutputBufferLength - Output Buffer Length for the IOCTL.
    InputBufferLength - Input Buffer Length for the IOCTL.
    IoControlCode - The IOCTL Code.

Return Value:

    TRUE if any of the Modules handled the Request, FALSE otherwise.
    In the FALSE case the Client Driver is expected to handle the request.

--*/
{
    LONG driverModuleIndex;
    BOOLEAN handled;

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p Request=0x%p", DmfCollection, Request);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    handled = FALSE;

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleInternalDeviceIoControlImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleInternalDeviceIoControl handled=%d", handled);
        goto Exit;
    }

    ASSERT(moduleCollectionHandle->NumberOfClientDriverDmfModules > 0);
    for (driverModuleIndex = 0; driverModuleIndex < moduleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        handled = DMF_Module_InternalDeviceIoControl(dmfModule,
                                                     Queue,
                                                     Request,
                                                     OutputBufferLength,
                                                     InputBufferLength,
                                                     IoControlCode);
        if (handled)
        {
            // The Module handled the call...no need to continue dispatching.
            //
            break;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p handled=%d", DmfCollection, handled);

    return handled;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionSelfManagedIoCleanup(
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceSelfManagedIoCleanup to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleSelfManagedIoCleanupImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleSelfManagedIoCleanup");
        goto Exit;
    }

    DMF_ModuleCollectionDispatchVoid(moduleCollectionHandle,
                                     DMF_Module_SelfManagedIoCleanup);

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionSelfManagedIoFlush(
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceSelfManagedIoFlush to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleSelfManagedIoFlushImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleSelfManagedIoFlush");
        goto Exit;
    }

    DMF_ModuleCollectionDispatchVoid(moduleCollectionHandle,
                                     DMF_Module_SelfManagedIoFlush);

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionSelfManagedIoInit(
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceSelfManagedIoInit to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.

Return Value:

    STATUS_SUCCESS on success if every Module in the list succeeds, or the NTSTATUS error
    code from the Module that returns an error.

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleSelfManagedIoInitImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleSelfManagedIoInit");
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    ntStatus = DMF_ModuleCollectionDispatchNtStatus(moduleCollectionHandle,
                                                    DMF_Module_SelfManagedIoInit);

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p ntStatus=%!STATUS!", DmfCollection, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionSelfManagedIoSuspend(
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceSelfManagedIoSuspend to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.

Return Value:

    STATUS_SUCCESS on success if every Module in the list succeeds, or the NTSTATUS error
    code from the Module that returns an error.

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleSelfManagedIoSuspendImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleSelfManagedIoSuspend");
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    ntStatus = DMF_ModuleCollectionDispatchNtStatus(moduleCollectionHandle,
                                                    DMF_Module_SelfManagedIoSuspend);

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p ntStatus=%!STATUS!", DmfCollection, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionSelfManagedIoRestart(
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceSelfManagedIoRestart to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.

Return Value:

    STATUS_SUCCESS on success if every Module in the list succeeds, or the NTSTATUS error
    code from the Module that returns an error.

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleSelfManagedIoRestartImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleSelfManagedIoRestart");
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    ntStatus = DMF_ModuleCollectionDispatchNtStatus(moduleCollectionHandle,
                                                    DMF_Module_SelfManagedIoRestart);

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p ntStatus=%!STATUS!", DmfCollection, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionSurpriseRemoval(
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceSurpriseRemoval to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleSurpriseRemovalImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleSurpriseRemoval");
        goto Exit;
    }

    DMF_ModuleCollectionDispatchVoid(moduleCollectionHandle,
                                     DMF_Module_SurpriseRemoval);

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionQueryRemove(
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceQueryRemove to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.

Return Value:

    STATUS_SUCCESS on success if every Module in the list succeeds, or the NTSTATUS error
    code from the Module that returns an error.

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleQueryRemoveImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleQueryRemove");
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    ntStatus = DMF_ModuleCollectionDispatchNtStatus(moduleCollectionHandle,
                                                    DMF_Module_QueryRemove);

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p ntStatus=%!STATUS!", DmfCollection, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionQueryStop(
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceQueryStop to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.

Return Value:

    STATUS_SUCCESS on success if every Module in the list succeeds, or the NTSTATUS error
    code from the Module that returns an error.

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleQueryStopImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleQueryStop");
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    ntStatus = DMF_ModuleCollectionDispatchNtStatus(moduleCollectionHandle,
                                                    DMF_Module_QueryStop);

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p ntStatus=%!STATUS!", DmfCollection, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionRelationsQuery(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ DEVICE_RELATION_TYPE RelationType
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceRelationsQuery to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.

Return Value:

    None

--*/
{
    LONG driverModuleIndex;

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleRelationsQueryImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleRelationsQuery");
        goto Exit;
    }

    for (driverModuleIndex = 0; driverModuleIndex < moduleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        DMF_Module_RelationsQuery(dmfModule,
                                  RelationType);
    }

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionUsageNotificationEx(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDF_SPECIAL_FILE_TYPE NotificationType,
    _In_ BOOLEAN IsInNotificationPath
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceRelationsQuery to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.
    NotificationType - Same as passed from WDF.
    IsInNotificationPath - Same as passed from WDF.

Return Value:

    STATUS_SUCCESS on success if every Module in the list succeeds, or the NTSTATUS error
    code from the Module that returns an error.

--*/
{
    LONG driverModuleIndex;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    ntStatus = STATUS_SUCCESS;

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleUsageNotificationExImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleUsageNotificationEx");
        goto Exit;
    }

    for (driverModuleIndex = 0; driverModuleIndex < moduleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        ntStatus = DMF_Module_UsageNotificationEx(dmfModule,
                                                  NotificationType,
                                                  IsInNotificationPath);
        if (! NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p ntStatus=%!STATUS!", DmfCollection, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionArmWakeFromS0(
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceArmWakeFromS0 to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.

Return Value:

    STATUS_SUCCESS on success if every Module in the list succeeds, or the NTSTATUS error
    code from the Module that returns an error.

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleArmWakeFromS0Implemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleArmWakeFromS0");
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    ntStatus = DMF_ModuleCollectionDispatchNtStatus(moduleCollectionHandle,
                                                    DMF_Module_ArmWakeFromS0);

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p ntStatus=%!STATUS!", DmfCollection, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionDisarmWakeFromS0(
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceDisarmWakeFromS0 to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleDisarmWakeFromS0Implemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleDisarmWakeFromS0");
        goto Exit;
    }

    DMF_ModuleCollectionDispatchVoid(moduleCollectionHandle,
                                     DMF_Module_DisarmWakeFromS0);

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionWakeFromS0Triggered(
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceWakeFromS0Triggered to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleWakeFromS0TriggeredImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleWakeFromS0Triggered");
        goto Exit;
    }

    DMF_ModuleCollectionDispatchVoid(moduleCollectionHandle,
                                     DMF_Module_WakeFromS0Triggered);

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionArmWakeFromSxWithReason(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ BOOLEAN DeviceWakeEnabled,
    _In_ BOOLEAN ChildrenArmedForWake
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceArmWakeFromSxWithReason to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.
    DeviceWakeEnabled - Same as passed from WDF.
    ChildrenArmedForWake - Same as passed from WDF.

Return Value:

    STATUS_SUCCESS on success if every Module in the list succeeds, or the NTSTATUS error
    code from the Module that returns an error.

--*/
{
    LONG driverModuleIndex;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    ntStatus = STATUS_SUCCESS;

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleArmWakeFromSxWithReasonImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleArmWakeFromSxWithReason");
        goto Exit;
    }

    for (driverModuleIndex = 0; driverModuleIndex < moduleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        ntStatus = DMF_Module_ArmWakeFromSxWithReason(dmfModule,
                                                      DeviceWakeEnabled,
                                                      ChildrenArmedForWake);
        if (! NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p ntStatus=%!STATUS!", DmfCollection, ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionDisarmWakeFromSx(
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceDisarmWakeFromSx to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleDisarmWakeFromSxImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleDisarmWakeFromSx");
        goto Exit;
    }

    DMF_ModuleCollectionDispatchVoid(moduleCollectionHandle,
                                     DMF_Module_DisarmWakeFromSx);

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionWakeFromSxTriggered(
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtDeviceWakeFromSxTriggered to
    every Module associated with DMFCOLLECTION.

Arguments:

    DmfCollection - The Given DMFCOLLECtION which contains the list of Client Driver's instantiated Modules.

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleWakeFromSxTriggeredImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleWakeFromSxTriggered");
        goto Exit;
    }

    DMF_ModuleCollectionDispatchVoid(moduleCollectionHandle,
                                     DMF_Module_WakeFromSxTriggered);

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_ModuleCollectionFileCreate(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtFileCreate to
    every Module associated with DMFCOLLECTION. As soon as one of the Modules
    handles the IOCTL, this function returns to prevent any possible double return of a WDFREQUEST.

Arguments:

    DmfCollection - The given DMFCOLLECTION. (Contains the list of Client Driver's instantiated Modules.)
    Device - WDF device object.
    Request - WDF Request with IOCTL parameters.
    FileObject - WDF file object that describes a file that is being opened for the specified request.

Return Value:

    TRUE if any of the Modules handled the Request, FALSE otherwise.
    In the FALSE case the Client Driver is expected to handle the request.

--*/
{
    LONG driverModuleIndex;
    BOOLEAN handled;

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p Request=0x%p", DmfCollection, Request);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    handled = FALSE;

    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleFileCreateImplemented)
    {
        // It means that none of the Modules in the Module Collection handled the request.
        //
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleFileCreate");
        goto Exit;
    }

    ASSERT(moduleCollectionHandle->NumberOfClientDriverDmfModules > 0);
    for (driverModuleIndex = 0; driverModuleIndex < moduleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        handled = DMF_Module_FileCreate(dmfModule,
                                        Device,
                                        Request,
                                        FileObject);
        if (handled)
        {
            // The Module handled the call...no need to continue dispatching.
            //
            break;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p handled=%d", DmfCollection, handled);

    return handled;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_ModuleCollectionFileCleanup(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtFileCleanup to
    every Module associated with DMFCOLLECTION. As soon as one of the Modules
    handles the IOCTL, this function returns to prevent any possible double return of a WDFREQUEST.

Arguments:

    DmfCollection - The given DMFCOLLECTION. (Contains the list of Client Driver's instantiated Modules.)
    FileObject - WDF file object that describes a file that is being opened for the specified request.

Return Value:

    TRUE if any of the Modules handled the Request, FALSE otherwise.
    In the FALSE case the Client Driver is expected to handle the request.

--*/
{
    LONG driverModuleIndex;
    BOOLEAN handled;

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    handled = FALSE;

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleFileCleanupImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleFileCleanup");
        goto Exit;
    }

    ASSERT(moduleCollectionHandle->NumberOfClientDriverDmfModules > 0);
    for (driverModuleIndex = 0; driverModuleIndex < moduleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        handled = DMF_Module_FileCleanup(dmfModule,
                                         FileObject);
        if (handled)
        {
            // The Module handled the call...no need to continue dispatching.
            //
            break;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p handled=%d", DmfCollection, handled);

    return handled;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_ModuleCollectionFileClose(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Given a DMFCOLLECTION, this function dispatches EvtFileClose to
    every Module associated with DMFCOLLECTION. As soon as one of the Modules
    handles the IOCTL, this function returns to prevent any possible double return of a WDFREQUEST.

Arguments:

    DmfCollection - The given DMFCOLLECTION. (Contains the list of Client Driver's instantiated Modules.)
    FileObject - WDF file object that describes a file that is being opened for the specified request.

Return Value:

    TRUE if any of the Modules handled the Request, FALSE otherwise.
    In the FALSE case the Client Driver is expected to handle the request.

--*/
{
    LONG driverModuleIndex;
    BOOLEAN handled;

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfCollection=0x%p", DmfCollection);

    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    handled = FALSE;

    // If no Module in the Collection supports this entry point,
    // then do not iterate through Collection and Child Modules since they will do nothing.
    // If at least one Module in the Collection supports this entry point,
    // then it is necessary to iterate though the Collection and Child Modules.
    // In that case, the Generic handlers will be called for Module that do not specifically support this entry point
    // (so that validation can happen).
    //
    if (! moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleFileCloseImplemented)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No Modules in Collection implement ModuleFileClose");
        goto Exit;
    }

    ASSERT(moduleCollectionHandle->NumberOfClientDriverDmfModules > 0);
    for (driverModuleIndex = 0; driverModuleIndex < moduleCollectionHandle->NumberOfClientDriverDmfModules; driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;

        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        dmfModule = DMF_ObjectToModule(dmfObject);
        handled = DMF_Module_FileClose(dmfModule,
                                       FileObject);
        if (handled)
        {
            // The Module handled the call...no need to continue dispatching.
            //
            break;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "DmfCollection=0x%p handled=%d", DmfCollection, handled);

    return handled;
}
#pragma code_seg()

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Module Collection Helper Functions
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionConfigListInitialize(
    _Inout_ DMF_MODULE_COLLECTION_CONFIG* ModuleCollectionConfig
    );

#pragma code_seg("PAGE")
VOID
DMF_ModuleCollectionConfigAddAttributes(
    _Inout_ DMF_MODULE_COLLECTION_CONFIG* ModuleCollectionConfig,
    _In_ DMF_MODULE_ATTRIBUTES* ModuleAttributes,
    _In_opt_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _In_opt_ DMFMODULE* ResultantDmfModule
)
/*++

Routine Description:

    Add a Module's initialized Config structure to the list of Config structures
    that is used later to create a Module Collection.

Arguments:

    ModuleCollectionConfig - Indicates how the Client wants to set up the Module Collection.
    ModuleAttributes - Module specific attributes for the Module to add to the list.
    ObjectAttributes - DMF specific attributes for the Module to add to the list.
    ResultantDmfModule - When the Module is created, its handle is written here so that Client can access the Module.

Return Value:

    NTSTATUS

--*/
{
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(ObjectAttributes);

    PAGED_CODE();

    if (!NT_SUCCESS(ModuleCollectionConfig->DmfPrivate.ErrorCodeNtStatus))
    {
        goto Exit;
    }

    if (NULL == ModuleCollectionConfig->DmfPrivate.ListOfConfigs)
    {
        // It is the first call. Relationship is as follows:
        // Client Driver's FDO's WDFDEVICE is the parent of the ListOfConfigs.
        // ListOfConfigs is the parent of all the memory allocated and added to ListOfConfigs.
        //
        ntStatus = DMF_ModuleCollectionConfigListInitialize(ModuleCollectionConfig);
        if (!NT_SUCCESS(ntStatus))
        {
            // Error code is set in the above function.
            //
            ASSERT(ntStatus == ModuleCollectionConfig->DmfPrivate.ErrorCodeNtStatus);
            goto Exit;
        }
    }

    // This is the address where Framework will give Client the DMFMODULE.
    // (It is OK if it is NULL because Client may not need the Module handle.)
    //
    ASSERT(NULL == ModuleAttributes->ResultantDmfModule);
    ModuleAttributes->ResultantDmfModule = ResultantDmfModule;

    // This flag is set before the Client callback. Set it on a per Module basis now.
    //
    ModuleAttributes->IsTransportModule = ModuleCollectionConfig->DmfPrivate.IsTransportModule;

    // Module is created as part of a collection. It is not a DynamicModule.
    //
    ModuleAttributes->DynamicModuleImmediate = FALSE;

    WDFMEMORY memoryConfigAndAttributes;
    VOID* memoryConfigAndAttributesBuffer;
    ULONG totalSizeOfAttributesAndCallbacksAndConfig;

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = ModuleCollectionConfig->DmfPrivate.ListOfConfigs;

    // The Buffer that is being created, will have 4 parts:
    // DMF_MODULE_ATTRIBUTES (Fixed Size)
    // WDF_OBJECT_ATTRIBUTES (Fixed Size)
    // DMF_MODULE_EVENT_CALLBACKS (Fixed Size)
    // DMF_#ModuleName#_Config (Variable Size).
    //
    totalSizeOfAttributesAndCallbacksAndConfig = sizeof(DMF_MODULE_ATTRIBUTES) +
                                                 sizeof(WDF_OBJECT_ATTRIBUTES) +
                                                 sizeof(DMF_MODULE_EVENT_CALLBACKS) +
                                                 ModuleAttributes->SizeOfModuleSpecificConfig;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               DMF_TAG,
                               totalSizeOfAttributesAndCallbacksAndConfig,
                               &memoryConfigAndAttributes,
                               &memoryConfigAndAttributesBuffer);
    if (!NT_SUCCESS(ntStatus))
    {
        ModuleCollectionConfig->DmfPrivate.ErrorCodeNtStatus = ntStatus;
        goto Exit;
    }

    DMF_MODULE_ATTRIBUTES* moduleAttributes;
    WDF_OBJECT_ATTRIBUTES* clientObjectAttributes;
    DMF_MODULE_EVENT_CALLBACKS* callbacks;
    VOID* moduleConfig;

    moduleAttributes = (DMF_MODULE_ATTRIBUTES*)memoryConfigAndAttributesBuffer;
    clientObjectAttributes = (WDF_OBJECT_ATTRIBUTES*)(moduleAttributes + 1);
    callbacks = (DMF_MODULE_EVENT_CALLBACKS*)(clientObjectAttributes + 1);
    moduleConfig = (VOID*)(callbacks + 1);

    RtlZeroMemory(memoryConfigAndAttributesBuffer,
                  totalSizeOfAttributesAndCallbacksAndConfig);

    // Copy the Attributes into the list entry.
    // ' warning C6386: Buffer overrun while writing to 'moduleAttributes':  the writable size is 'totalSizeOfAttributesAndConfig' bytes, but '64' bytes might be written.'.
    //
    #pragma warning(suppress:6386)
    RtlCopyMemory(moduleAttributes,
                  ModuleAttributes,
                  sizeof(DMF_MODULE_ATTRIBUTES));

    // Copy the ObjectAttributes passed by Client.
    //
    if (ObjectAttributes != WDF_NO_OBJECT_ATTRIBUTES)
    {
        RtlCopyMemory(clientObjectAttributes,
                      ObjectAttributes,
                      sizeof(WDF_OBJECT_ATTRIBUTES));
    }
    else
    {
        WDF_OBJECT_ATTRIBUTES_INIT(clientObjectAttributes);
    }

    // Copy the Callbacks if its set by Client.
    //
    if (moduleAttributes->ClientCallbacks != NULL)
    {
        RtlCopyMemory(callbacks,
                      ModuleAttributes->ClientCallbacks,
                      sizeof(DMF_MODULE_EVENT_CALLBACKS));
        // Change ClientCallbacks from Client's buffer (no longer valid after this call)
        // to Callbacks inside the newly allocated memory (which will be valid for later use).
        //
        moduleAttributes->ClientCallbacks = callbacks;
    }

    // Copy the Module Config into the list entry.
    //
    RtlCopyMemory(moduleConfig,
                  ModuleAttributes->ModuleConfigPointer,
                  ModuleAttributes->SizeOfModuleSpecificConfig);

    // If Module defines a Config, change ModuleConfigPointer from Client's buffer (no longer valid after this call)
    // to the Config inside the newly allocated memory (which will be valid for later use).
    //
    if (moduleAttributes->SizeOfModuleSpecificConfig > 0)
    {
        moduleAttributes->ModuleConfigPointer = moduleConfig;
    }
    else
    {
        ASSERT(moduleAttributes->ModuleConfigPointer == NULL);
    }

    // TODO: Use LIST_ENTRY structures.
    // ''ModuleCollectionConfig->DmfPrivate.ListOfConfigs' could be '0':'.
    //
    #pragma warning(suppress:6387)
    ntStatus = WdfCollectionAdd(ModuleCollectionConfig->DmfPrivate.ListOfConfigs,
                                (VOID*)memoryConfigAndAttributes);
    if (!NT_SUCCESS(ntStatus))
    {
        ModuleCollectionConfig->DmfPrivate.ErrorCodeNtStatus = ntStatus;
        // It deletes all the entires in the list also.
        // ''ModuleCollectionConfig->DmfPrivate.ListOfConfigs' could be '0':'
        //
        #pragma warning(suppress:6387)
        WdfObjectDelete(ModuleCollectionConfig->DmfPrivate.ListOfConfigs);
        ModuleCollectionConfig->DmfPrivate.ListOfConfigs = NULL;
        goto Exit;
    }

Exit:
    ;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionConfigListInitialize(
    _Inout_ DMF_MODULE_COLLECTION_CONFIG* ModuleCollectionConfig
    )
/*++

Routine Description:

    This function initializes the list of CONFIG structures that contains a copy of all the Client's CONFIG structures
    for each Module that the Client wants to instantiate. It is possible the Client has no Modules specified and just
    wants DMF Features to be instantiated.

Arguments:

    ModuleCollectionConfig - Indicates how the Client wants to set up the Module Collection.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE();

    // Client Driver's FDO's WDFDEVICE or Parent Module is the parent of the ListOfConfigs.
    // ListOfConfigs is the parent of all the memory allocated and added to ListOfConfigs.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    ASSERT(ModuleCollectionConfig->DmfPrivate.ClientDriverWdfDevice != NULL);
    if (ModuleCollectionConfig->DmfPrivate.ParentDmfModule != NULL)
    {
        objectAttributes.ParentObject = ModuleCollectionConfig->DmfPrivate.ParentDmfModule;
    }
    else
    {
        objectAttributes.ParentObject = ModuleCollectionConfig->DmfPrivate.ClientDriverWdfDevice;
    }
    ntStatus = WdfCollectionCreate(&objectAttributes,
                                   &ModuleCollectionConfig->DmfPrivate.ListOfConfigs);
    if (! NT_SUCCESS(ntStatus))
    {
        ModuleCollectionConfig->DmfPrivate.ErrorCodeNtStatus = ntStatus;
        goto Exit;
    }

    // Feature modules must always be the first Module in the list because it must be
    // the last Module that is destroyed.
    // As soon as the list has been created, add Feature Module's Config if the
    // Client Driver wants to use DMF Features.
    //
    if (ModuleCollectionConfig->BranchTrackModuleConfig != NULL)
    {
        DMF_MODULE_ATTRIBUTES moduleAttributes;

        DMF_BranchTrack_ATTRIBUTES_INIT(&moduleAttributes);
        moduleAttributes.ModuleConfigPointer = ModuleCollectionConfig->BranchTrackModuleConfig;
        moduleAttributes.SizeOfModuleSpecificConfig = sizeof(DMF_CONFIG_BranchTrack);

        // Recursively call this function to add BranchTrack's Config.
        //
        DMF_ModuleCollectionConfigAddAttributes(ModuleCollectionConfig,
                                                &moduleAttributes,
                                                WDF_NO_OBJECT_ATTRIBUTES,
                                                NULL);
    }

#if !defined(DMF_USER_MODE)
    if (ModuleCollectionConfig->LiveKernelDumpModuleConfig != NULL)
    {
        DMF_MODULE_ATTRIBUTES moduleAttributes;

        DMF_LiveKernelDump_ATTRIBUTES_INIT(&moduleAttributes);
        moduleAttributes.ModuleConfigPointer = ModuleCollectionConfig->LiveKernelDumpModuleConfig;
        moduleAttributes.SizeOfModuleSpecificConfig = sizeof(DMF_CONFIG_LiveKernelDump);

        // Recursively call this function to add LiveKernelDump's Config.
        //
        DMF_ModuleCollectionConfigAddAttributes(ModuleCollectionConfig,
                                                &moduleAttributes,
                                                WDF_NO_OBJECT_ATTRIBUTES,
                                                NULL);
    }
#endif // !defined(DMF_USER_MODE)

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_ModuleCollection_ModuleValidate(
    _In_ DMF_OBJECT* ModuleHandle
    )
/*++

Routine Description:

    Given a Module handle, populate its DmfCallbacksWdfCheck structure.

Arguments:

    ModuleHandle - The given Module handle.

Return Value:

    None

--*/
{
    BOOLEAN returnValue;
    DMF_MODULE_COLLECTION* moduleCollectionHandle;
    DMF_CALLBACKS_WDF* wdfCallbacks;

    PAGED_CODE();

    moduleCollectionHandle = ModuleHandle->ModuleCollection;
    ASSERT(moduleCollectionHandle != NULL);

    wdfCallbacks = ModuleHandle->ModuleDescriptor.CallbacksWdf;
    ASSERT(wdfCallbacks != NULL);

    returnValue = TRUE;

    if (wdfCallbacks->ModulePrepareHardware != DMF_Generic_ModulePrepareHardware)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModulePrepareHardwareImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleReleaseHardware != DMF_Generic_ModuleReleaseHardware)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleReleaseHardwareImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleD0Entry != DMF_Generic_ModuleD0Entry)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleD0EntryImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleD0EntryPostInterruptsEnabled != DMF_Generic_ModuleD0EntryPostInterruptsEnabled)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleD0EntryPostInterruptsEnabledImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleD0ExitPreInterruptsDisabled != DMF_Generic_ModuleD0ExitPreInterruptsDisabled)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleD0ExitPreInterruptsDisabledImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleD0Exit != DMF_Generic_ModuleD0Exit)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleD0ExitImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleQueueIoRead != DMF_Generic_ModuleQueueIoRead)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleQueueIoReadImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleQueueIoWrite != DMF_Generic_ModuleQueueIoWrite)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleQueueIoWriteImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleDeviceIoControl != DMF_Generic_ModuleDeviceIoControl)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleDeviceIoControlImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleInternalDeviceIoControl != DMF_Generic_ModuleInternalDeviceIoControl)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleInternalDeviceIoControlImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleSelfManagedIoCleanup != DMF_Generic_ModuleSelfManagedIoCleanup)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleSelfManagedIoCleanupImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleSelfManagedIoFlush != DMF_Generic_ModuleSelfManagedIoFlush)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleSelfManagedIoFlushImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleSelfManagedIoInit != DMF_Generic_ModuleSelfManagedIoInit)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleSelfManagedIoInitImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleSelfManagedIoSuspend != DMF_Generic_ModuleSelfManagedIoSuspend)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleSelfManagedIoSuspendImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleSelfManagedIoRestart != DMF_Generic_ModuleSelfManagedIoRestart)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleSelfManagedIoRestartImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleSurpriseRemoval != DMF_Generic_ModuleSurpriseRemoval)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleSurpriseRemovalImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleQueryRemove != DMF_Generic_ModuleQueryRemove)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleQueryRemoveImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleQueryStop != DMF_Generic_ModuleQueryStop)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleQueryStopImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleRelationsQuery != DMF_Generic_ModuleRelationsQuery)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleRelationsQueryImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleUsageNotificationEx != DMF_Generic_ModuleUsageNotificationEx)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleUsageNotificationExImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleArmWakeFromS0 != DMF_Generic_ModuleArmWakeFromS0)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleArmWakeFromS0Implemented = TRUE;
    }
    if (wdfCallbacks->ModuleDisarmWakeFromS0 != DMF_Generic_ModuleDisarmWakeFromS0)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleDisarmWakeFromS0Implemented = TRUE;
    }
    if (wdfCallbacks->ModuleWakeFromS0Triggered != DMF_Generic_ModuleWakeFromS0Triggered)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleWakeFromS0TriggeredImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleArmWakeFromSxWithReason != DMF_Generic_ModuleArmWakeFromSxWithReason)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleArmWakeFromSxWithReasonImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleDisarmWakeFromSx != DMF_Generic_ModuleDisarmWakeFromSx)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleDisarmWakeFromSxImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleWakeFromSxTriggered != DMF_Generic_ModuleWakeFromSxTriggered)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleWakeFromSxTriggeredImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleFileCreate != DMF_Generic_ModuleFileCreate)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleFileCreateImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleFileCleanup != DMF_Generic_ModuleFileCleanup)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleFileCleanupImplemented = TRUE;
    }
    if (wdfCallbacks->ModuleFileClose != DMF_Generic_ModuleFileClose)
    {
        moduleCollectionHandle->DmfCallbacksWdfCheck.ModuleFileCloseImplemented = TRUE;
    }

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModuleCollectionCreate(
    _In_opt_ PDMFDEVICE_INIT DmfDeviceInit,
    _In_ DMF_MODULE_COLLECTION_CONFIG* ModuleCollectionConfig,
    _Out_ DMFCOLLECTION* DmfCollection
    )
/*++

Routine Description:

    Creates a DMFMODULECOLLECTION.

Arguments:

    DmfDeviceInit - Client Driver's PDMFDEVICE_INIT structure.
    ModuleCollectionConfig - Module Collection Config populated by the Client Driver.
    DmfCollecton - On success, the resultant DMFCOLLECTION. This handle is used by the DMF Container Driver
                   to send messages to all instantiated Modules.

Return Value:

    NTSTATUS

--*/
{
    LONG driverModuleIndex;
    DMF_MODULE_COLLECTION* moduleCollectionHandle;
    NTSTATUS ntStatus;
    LONG firstModuleToInstantiate;
    LONG numberOfClientModulesToCreate;
    WDFMEMORY moduleCollectionHandleMemory;
    WDF_OBJECT_ATTRIBUTES attributes;
    BOOLEAN dmfBridgeEnabled;
    DMF_CONFIG_Bridge* bridgeModuleConfig;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    BOOLEAN createChildModuleCollection;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);
    TraceInformation(DMF_TRACE, "%!FUNC!");

    ASSERT(ModuleCollectionConfig->DmfPrivate.ClientDriverWdfDevice != NULL);

    ntStatus = STATUS_UNSUCCESSFUL;
    moduleCollectionHandle = NULL;

    // Module Collection can be top level Collection (created by Client Driver for top level Modules)
    // or child Module Collection (created by Module for its child Modules).
    // Modules do not pass in DmfDeviceInit.
    // Dmf Bridge is only created for top level Collection. 
    //
    if (DmfDeviceInit != NULL)
    {
        createChildModuleCollection = FALSE;
        dmfBridgeEnabled = DMF_DmfDeviceInitIsBridgeEnabled(DmfDeviceInit);
        ASSERT(dmfBridgeEnabled == TRUE);
    }
    else
    {
        createChildModuleCollection = TRUE;
        dmfBridgeEnabled = FALSE;
    }

    // If any error occurred during table (list) creation, do not proceed and report
    // the error.
    //
    if (! NT_SUCCESS(ModuleCollectionConfig->DmfPrivate.ErrorCodeNtStatus))
    {
        ntStatus = ModuleCollectionConfig->DmfPrivate.ErrorCodeNtStatus;
        goto Exit;
    }

    if (NULL == ModuleCollectionConfig->DmfPrivate.ListOfConfigs)
    {
        // Client called create before add.
        // This is valid for the case where the Client uses no Modules, but supports BranchTrack
        // or has enabled Bridge.
        //
        if ((NULL == ModuleCollectionConfig->BranchTrackModuleConfig) &&
            (NULL == ModuleCollectionConfig->LiveKernelDumpModuleConfig) &&
            (! dmfBridgeEnabled))
        {
            ASSERT(FALSE);
            goto Exit;
        }

        // Initialize the list and add Feature Modules to it.
        //
        ntStatus = DMF_ModuleCollectionConfigListInitialize(ModuleCollectionConfig);
        if (! NT_SUCCESS(ntStatus))
        {
            ASSERT(ntStatus == ModuleCollectionConfig->DmfPrivate.ErrorCodeNtStatus);
            goto Exit;
        }
    }

    // Determine if BranchTrack should be enabled for this Module Collection.
    // There are 3 cases:
    // 1. BranchTrack is not supported. In this case, all Modules in the table should 
    //    be instantiated.
    // 2. BranchTrack is supported but not enabled. In this case, all Modules in the
    //    table, except for the first one at index 0, should be instantiated.
    // 3. BranchTrack is supported and enabled.  In this case, all Modules in the table should 
    //    be instantiated.
    //
    firstModuleToInstantiate = 0;
    ASSERT(! ModuleCollectionConfig->DmfPrivate.BranchTrackEnabled);
    ASSERT(ModuleCollectionConfig->DmfPrivate.ListOfConfigs != NULL);
    // 'ModuleCollectionConfig->DmfPrivate.ListOfConfigs' could be '0'.
    //
    #pragma warning(suppress:6387)
    numberOfClientModulesToCreate = WdfCollectionGetCount(ModuleCollectionConfig->DmfPrivate.ListOfConfigs);

    // If BranchTrackModuleConfig is not NULL, Client Driver supports BranchTrack.
    //
    if (ModuleCollectionConfig->BranchTrackModuleConfig != NULL)
    {
        // The Client Driver has enabled BranchTrack. Check if the User has enabled BranchTrack.
        //
        ULONG shouldBranchTrackBeInstantiatedAutomatically = DMF_ModuleBranchTrack_HasClientEnabledBranchTrack(ModuleCollectionConfig->DmfPrivate.ClientDriverWdfDevice);

        if (shouldBranchTrackBeInstantiatedAutomatically)
        {
            // Tell the rest of Module Collection code path that BranchTrack is enabled.
            //
            ModuleCollectionConfig->DmfPrivate.BranchTrackEnabled = TRUE;
        }
        else
        {
            // The table is capable of instantiating BranchTrack. But BranchTrack
            // will not be instantiated. Since BranchTrack is the first entry in
            // the table, tell the rest of the code to skip it.
            //
            firstModuleToInstantiate = 1;
            // Instantiate all the modules in the table EXCEPT FOR THE FIRST MODULE.
            //
            numberOfClientModulesToCreate--;
        }
    }

    // LiveKernelDump Module enabled if Config structure is set by Client.
    // This feature is supported in Kernel-mode only.
    //
#if !defined(DMF_USER_MODE)
    if (ModuleCollectionConfig->LiveKernelDumpModuleConfig != NULL)
    {
        ModuleCollectionConfig->DmfPrivate.LiveKernelDumpEnabled = TRUE;
    }
    else
    {
        ASSERT(! ModuleCollectionConfig->DmfPrivate.LiveKernelDumpEnabled);
    }
#else
    ASSERT(! ModuleCollectionConfig->DmfPrivate.LiveKernelDumpEnabled);
#endif // !defined(DMF_USER_MODE)

    if (! createChildModuleCollection)
    {
        // Add Bridge Module to the end of ModuleCollection's Config List.
        //
        bridgeModuleConfig = (DMF_CONFIG_Bridge*)DMF_DmfDeviceInitBridgeModuleConfigGet(DmfDeviceInit);
        ASSERT(bridgeModuleConfig != NULL);

        DMF_Bridge_ATTRIBUTES_INIT(&moduleAttributes);
        moduleAttributes.ModuleConfigPointer = bridgeModuleConfig;
        DMF_ModuleCollectionConfigAddAttributes(ModuleCollectionConfig,
                                                &moduleAttributes,
                                                WDF_NO_OBJECT_ATTRIBUTES,
                                                NULL);

        numberOfClientModulesToCreate++;
    }

    // NOTE: Zero Modules are allowed for the case where Client only instantiates BranchTrack but, BranchTrack
    //       is not enabled.
    //
    if (numberOfClientModulesToCreate == 0)
    {
        // It is OK if the Client supports at least one Feature.
        //
        if ((NULL == ModuleCollectionConfig->BranchTrackModuleConfig &&
             NULL == ModuleCollectionConfig->LiveKernelDumpModuleConfig))
        {
            ntStatus = STATUS_INVALID_PARAMETER;
            goto Exit;
        }
    }

    // Create the Module Collection Handle memory.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    if (ModuleCollectionConfig->DmfPrivate.ParentDmfModule != NULL)
    {
        attributes.ParentObject = ModuleCollectionConfig->DmfPrivate.ParentDmfModule;
        // NOTE: Child Module Collection will be deleted after Child Modules are created. 
        // Child Modules will be destroyed when top level Collection is destroyed, or 
        // when Client destroys dynamically created Module. 
        //
    }
    else
    {
        attributes.ParentObject = ModuleCollectionConfig->DmfPrivate.ClientDriverWdfDevice;
        // NOTE: It must be Cleanup, not Destroy because Destroy is too late.
        //
        attributes.EvtCleanupCallback = DMF_ModuleCollectionDestroy;
    }
    ntStatus = WdfMemoryCreate(&attributes,
                               NonPagedPoolNx,
                               DMF_TAG,
                               sizeof(DMF_MODULE_COLLECTION),
                               &moduleCollectionHandleMemory,
                               (VOID**)&moduleCollectionHandle);
    if (! NT_SUCCESS(ntStatus))
    {
        moduleCollectionHandleMemory = NULL;
        moduleCollectionHandle = NULL;
        goto Exit;
    }

    RtlZeroMemory(moduleCollectionHandle,
                  sizeof(DMF_MODULE_COLLECTION));

    // Save of this handle for later deallocation.
    //
    moduleCollectionHandle->ModuleCollectionHandleMemory = moduleCollectionHandleMemory;

    // Assign DMFCOLLECTION_TYPE as custom type to ModuleCollectionHandleMemory,
    // so we can validate that if a Module is created as part of DMFCOLLECTION, its Parent is actually a DMFCOLLECTION.
    //
    ntStatus = WdfObjectAddCustomType((DMFCOLLECTION)(moduleCollectionHandle->ModuleCollectionHandleMemory),
                                      DMFCOLLECTION_TYPE);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfObjectAddCustomType fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Create space for the Module Collection's DMF Module Handles.
    //
    if (numberOfClientModulesToCreate > 0)
    {
        // Allocate space for the list of pointers to the Client Driver's instantiated DMF Modules.
        //
        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        // Assign DMFCOLLECTION as ParentObject of DMFMODULE.
        //
        attributes.ParentObject = (DMFCOLLECTION)(moduleCollectionHandle->ModuleCollectionHandleMemory);
        ntStatus = WdfMemoryCreate(&attributes,
                                   NonPagedPoolNx,
                                   DMF_TAG,
                                   sizeof(DMF_OBJECT*) * numberOfClientModulesToCreate,
                                   &moduleCollectionHandle->ClientDriverDmfModulesMemory,
                                   (VOID**)&moduleCollectionHandle->ClientDriverDmfModules);
        if (! NT_SUCCESS(ntStatus))
        {
            // Clean up happens on exit.
            //
            goto Exit;
        }

        RtlZeroMemory(moduleCollectionHandle->ClientDriverDmfModules,
                      sizeof(DMF_OBJECT*) * numberOfClientModulesToCreate);
    }

    // Create all the Modules in the Module Collection.
    //
    for (driverModuleIndex = firstModuleToInstantiate;
         driverModuleIndex < numberOfClientModulesToCreate + firstModuleToInstantiate;
         driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;
        DMFMODULE dmfModule;
        DMF_MODULE_ATTRIBUTES* moduleAttributesPointer;
        WDFMEMORY moduleConfigAndAttributesMemory;
        VOID* moduleConfigAndAttributes;
        WDF_OBJECT_ATTRIBUTES* moduleObjectAttributesPointer;

        // This buffer contains the Module Attributes followed by the Module Config.
        // 'ModuleCollectionConfig->DmfPrivate.ListOfConfigs' could be '0'.
        //
        ASSERT(ModuleCollectionConfig->DmfPrivate.ListOfConfigs != NULL);
        #pragma warning(suppress:6387)
        moduleConfigAndAttributesMemory = (WDFMEMORY)WdfCollectionGetItem(ModuleCollectionConfig->DmfPrivate.ListOfConfigs,
                                                                          driverModuleIndex);
        // The Buffer that is being retrieved, will have 4 parts in this order:
        // DMF_MODULE_ATTRIBUTES (Fixed Size)
        // WDF_OBJECT_ATTRIBUTES (Fixed Size)
        // DMF_MODULE_EVENT_CALLBACKS (Fixed Size)
        // DMF_#ModuleName#_Config (Variable Size)
        //
        moduleConfigAndAttributes = (DMF_MODULE_ATTRIBUTES*)WdfMemoryGetBuffer(moduleConfigAndAttributesMemory,
                                                                               NULL);
        moduleAttributesPointer = (DMF_MODULE_ATTRIBUTES*)moduleConfigAndAttributes;
        moduleObjectAttributesPointer = (WDF_OBJECT_ATTRIBUTES*)(moduleAttributesPointer + 1);

        // Create an instance of the DMF Module on behalf of the Client Driver.
        //
        ASSERT(moduleAttributesPointer->InstanceCreator != NULL);
        ASSERT(moduleAttributesPointer->ClientModuleInstanceName != NULL);

        if (ModuleCollectionConfig->DmfPrivate.ParentDmfModule != NULL)
        {
            moduleObjectAttributesPointer->ParentObject = ModuleCollectionConfig->DmfPrivate.ParentDmfModule;
            moduleAttributesPointer->DynamicModule = DMF_IsModuleDynamic(ModuleCollectionConfig->DmfPrivate.ParentDmfModule);
        }
        else
        {
            moduleObjectAttributesPointer->ParentObject = ModuleCollectionConfig->DmfPrivate.ClientDriverWdfDevice;
            moduleAttributesPointer->DynamicModule = FALSE;
        }
        ntStatus = moduleAttributesPointer->InstanceCreator(ModuleCollectionConfig->DmfPrivate.ClientDriverWdfDevice,
                                                            moduleAttributesPointer,
                                                            moduleObjectAttributesPointer,
                                                            &dmfModule);
        if (! NT_SUCCESS(ntStatus))
        {
            dmfModule = NULL;

            // Clean up will happen below.
            //
            goto Exit;
        }

        dmfObject = DMF_ModuleToObject(dmfModule);

        // If the Module is not instantiated as a Transport Module, and the Client wants the 
        // Module Handle, give it to the Client.
        //
        if (moduleAttributesPointer->ResultantDmfModule != NULL)
        {
            if (!moduleAttributesPointer->IsTransportModule)
            {
                // The Client Driver requests the DmfModule. Give the Client Driver the newly created Module's handle.
                //
                *moduleAttributesPointer->ResultantDmfModule = dmfModule;
            }
        }
        else
        {
            // In some cases (we hope many), the Client Driver will not need the DmfObject because all the work
            // will be done at the Module Level. This is the cleanest case. This will be a powerful feature when
            // further dispatches are created. In this case, the Client Driver causes all the work to be done
            // using only the Module Collection Handle.
            //
        }

        // Save the DMF Module Handle in the collection list.
        //
        ASSERT(moduleCollectionHandle->NumberOfClientDriverDmfModules < numberOfClientModulesToCreate);
        // 'warning C6386: Buffer overrun while writing to 'moduleCollectionHandle->ClientDriverDmfModules''
        //
        #pragma warning(suppress:6386)
        moduleCollectionHandle->ClientDriverDmfModules[moduleCollectionHandle->NumberOfClientDriverDmfModules] = dmfObject;

        // Do not store the Module Collection handle for child Modules. 
        // Top level Collection will be propagated to all Modules after Collection is created.
        //
        if (! createChildModuleCollection)
        {
            // Save the Parent Module Collection handle in the DMF Object.
            //
            ASSERT(NULL == dmfObject->ModuleCollection);
            dmfObject->ModuleCollection = moduleCollectionHandle;
            ASSERT(dmfObject->ModuleCollection != NULL);
            // Check if the Module just added to the Collection follows all the 
            // rules of DMFCOLLECTION object.
            //
            if (! DMF_ModuleCollection_ModuleValidate(dmfObject))
            {
                ntStatus = STATUS_UNSUCCESSFUL;
                goto Exit;
            }
        }

        // Count the number of modules successfully instantiated.
        //
        moduleCollectionHandle->NumberOfClientDriverDmfModules++;
    }

    ntStatus = STATUS_SUCCESS;

    if (! createChildModuleCollection)
    {
        // Set the Module Collection Handle into all the DMF Modules in the
        // instantiated Module tree.
        //
        DMF_ModuleCollectionHandlePropagate(moduleCollectionHandle,
                                            moduleCollectionHandle->NumberOfClientDriverDmfModules);
    }

    if (ModuleCollectionConfig->DmfPrivate.BranchTrackEnabled)
    {
        moduleCollectionHandle->DmfObjectFeature[DmfFeature_BranchTrack] = DMF_ModuleCollectionFeatureHandleGet(moduleCollectionHandle,
                                                                                                                DmfFeature_BranchTrack);
        ASSERT(moduleCollectionHandle->DmfObjectFeature[DmfFeature_BranchTrack] != NULL);
    }
    else
    {
        ASSERT(NULL == moduleCollectionHandle->DmfObjectFeature[DmfFeature_BranchTrack]);
    }

    if (ModuleCollectionConfig->DmfPrivate.LiveKernelDumpEnabled)
    {
        moduleCollectionHandle->DmfObjectFeature[DmfFeature_LiveKernelDump] = DMF_ModuleCollectionFeatureHandleGet(moduleCollectionHandle,
                                                                                                                   DmfFeature_LiveKernelDump);
        ASSERT(moduleCollectionHandle->DmfObjectFeature[DmfFeature_LiveKernelDump] != NULL);
    }
    else
    {
        ASSERT(NULL == moduleCollectionHandle->DmfObjectFeature[DmfFeature_LiveKernelDump]);
    }

    ASSERT(moduleCollectionHandle->NumberOfClientDriverDmfModules == numberOfClientModulesToCreate);

Exit:

    if (! NT_SUCCESS(ntStatus))
    {
        if (moduleCollectionHandle != NULL)
        {
            // A Client Module has failed to open. Clean up.
            //
            DMF_ModuleCollectionCleanup(moduleCollectionHandle,
                                        ModuleOpenedDuringType_Create);
            // Destroy the collection this call created.
            // NOTE: DMF_ModuleCollectionDestroy is called via the destroy callback.
            //
            WdfObjectDelete(moduleCollectionHandle->ModuleCollectionHandleMemory);
            moduleCollectionHandle = NULL;
        }
        else
        {
            // Failure prior to creating Module Collection due to no memory.
        }
    }

    FuncExit(DMF_TRACE, "moduleCollectionHandle=0x%p ntStatus=%!STATUS!", moduleCollectionHandle, ntStatus);

    // The Client Driver stores this handle so that the it can be passed to the DMF Library's
    // Dispatchers. These Dispatchers will make sure each Module can do work as needed.
    //
    ASSERT(DmfCollection != NULL);
    if (NT_SUCCESS(ntStatus))
    {
        // Give the caller the DMFCOLLECTION so that Client can use it to dispatch WDF callbacks
        // to all the Modules in the Collection.
        //
        *DmfCollection = (DMFCOLLECTION)(moduleCollectionHandle->ModuleCollectionHandleMemory);
    }
    else
    {
        // For SAL.
        //
        *DmfCollection = NULL;
    }

    // Delete the "table" of Configurations because it is no longer necessary.
    //
    if (ModuleCollectionConfig->DmfPrivate.ListOfConfigs != NULL)
    {
        WdfObjectDelete(ModuleCollectionConfig->DmfPrivate.ListOfConfigs);
        ModuleCollectionConfig->DmfPrivate.ListOfConfigs = NULL;
    }

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModuleCollectionPostCreate(
    _In_ DMF_MODULE_COLLECTION_CONFIG* ModuleCollectionConfig,
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    Open or register for notification for OPEN_Create or NOTIFY_Create Modules.
    Initialize Feature Modules as necessary.

Arguments:

    ModuleCollectionConfig - Contains Module Collection creation parameters.
    DmfCollecton - The given DMFCOLLECTION.

Return Value:

    NTSTATUS

--*/
{
    LONG driverModuleIndex;
    DMF_MODULE_COLLECTION* moduleCollectionHandle;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);
    TraceInformation(DMF_TRACE, "%!FUNC!");

    ntStatus = STATUS_UNSUCCESSFUL;
    moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    // Go through the Modules in the Module Collection and open any that should be
    // opened after they have been created.
    //
    for (driverModuleIndex = 0;
         driverModuleIndex < moduleCollectionHandle->NumberOfClientDriverDmfModules;
         driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject;

        ASSERT(moduleCollectionHandle != NULL);
        // 'warning C6385: Reading invalid data from 'moduleCollectionHandle->ClientDriverDmfModules''
        //
        #pragma warning(suppress:6385)
        dmfObject = moduleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        ASSERT(dmfObject != NULL);
        DMFMODULE dmfModule = DMF_ObjectToModule(dmfObject);

        ntStatus = DMF_Module_OpenOrRegisterNotificationOnCreate(dmfModule);
#if defined(USE_DMF_INJECT_FAULT_PARTIAL_OPEN)
#if !defined(DEBUG)
#error You cannot inject faults in non-Debug builds.
#endif // !defined(DEBUG)
        // Inject fault. Open only half the modules, then return error.
        //
        if (driverModuleIndex >= (moduleCollectionHandle->NumberOfClientDriverDmfModules / 2))
        {
            ntStatus = STATUS_UNSUCCESSFUL;
        }
#endif // defined(USE_DMF_INJECT_FAULT_PARTIAL_OPEN)
        if (! NT_SUCCESS(ntStatus))
        {
            // Client Module has failed to open. Fail this call. Module Collection and its 
            // child objects will be deleted.
            //
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Module_OpenOrRegisterNotificationOnCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    // Initialize all BranchTrack tables for all Modules and Child Modules in the 
    // Module Collection. Do this before any Modules are opened because some
    // Modules may execute BranchTrack in Open or Notification callbacks.
    // NOTE: This must be done regardless of whether BranchTrack is enabled or not
    //       so that the ModuleCollectionHandle is written to all the Child Modules.
    //
    if (ModuleCollectionConfig->DmfPrivate.BranchTrackEnabled)
    {
        DMF_ModuleBranchTrack_ModuleCollectionInitialize(moduleCollectionHandle);
    }

#if !defined(DMF_USER_MODE)
    if (ModuleCollectionConfig->DmfPrivate.LiveKernelDumpEnabled)
    {
        // This feature is available only for Kernel-mode Drivers.
        //
        // Initialize all LiveKernelDump settings for all Modules and Child Modules in the 
        // Module Collection. Do this before any Modules are opened because some
        // Modules may use LiveKernelDump in Open or Notification callbacks.
        // NOTE: This must be done regardless of whether LiveKernelDump is enabled or not
        //       so that the ModuleCollectionHandle is written to all the Child Modules.
        //
        DMF_ModuleLiveKernelDump_ModuleCollectionInitialize(moduleCollectionHandle);
    }
#endif // !defined(DMF_USER_MODE)

Exit:

    return ntStatus;
}
#pragma code_seg()

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Client Driver API
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModulesCreate(
    _In_ WDFDEVICE Device,
    _In_ PDMFDEVICE_INIT* DmfDeviceInitPointer
    )
/*++

Routine Description:

    Check if DmfDeviceInit is valid.
    Initialize the Module Config. Create the default queue if its not created by client driver.
    Assign DMF_DEVICE_CONTEXT to Device.
    Call EvtDmfDeviceModuleAdd callback for client to Add required Modules to Collection.
    Call DMF_ModuleCollectionCreate to create the Collection of Modules.
    Store the Collection in DMF_DEVICE_CONTEXT.
    Free DmfDeviceInit.

Arguments:

    Device - WDFDEVICE object created by Client Driver.
    DmfDeviceInitPointer - Pointer to DmfDeviceInit.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_COLLECTION_CONFIG moduleCollectionConfig;
    DMF_EVENT_CALLBACKS* dmfEventCallbacks;
    BOOLEAN isDefaultQueueCreated;
    DMF_CONFIG_BranchTrack* branchTrackModuleConfig;
    DMF_CONFIG_LiveKernelDump* liveKernelDumpModuleConfig;
    DMFCOLLECTION dmfCollection;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    PDMFDEVICE_INIT dmfDeviceInit;
    BOOLEAN isControlDevice;
    BOOLEAN isFilterDriver;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);
    TraceInformation(DMF_TRACE, "%!FUNC!");

    ASSERT(Device != NULL);
    ASSERT(DmfDeviceInitPointer != NULL);

    dmfDeviceInit = *DmfDeviceInitPointer;
    liveKernelDumpModuleConfig = NULL;

    // Validate DmfDeviceInit.
    //
    if (! DMF_DmfDeviceInitValidate(dmfDeviceInit))
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfDeviceInit invalid: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    isDefaultQueueCreated = DMF_DmfDeviceInitIsDefaultQueueCreated(dmfDeviceInit);
    branchTrackModuleConfig = DMF_DmfDeviceInitBranchTrackModuleConfigGet(dmfDeviceInit);
#if !defined(DMF_USER_MODE)
    liveKernelDumpModuleConfig = DMF_DmfDeviceInitLiveKernelDumpModuleConfigGet(dmfDeviceInit);
#endif // !defined(DMF_USER_MODE)
    dmfEventCallbacks = DMF_DmfDeviceInitDmfEventCallbacksGet(dmfDeviceInit);
    isControlDevice = DMF_DmfDeviceInitIsControlDevice(dmfDeviceInit);
    isFilterDriver = DMF_DmfDeviceInitIsFilterDriver(dmfDeviceInit);

    // If Default queue is not created by the client, then create one here.
    // Module which implement IoQueue callbacks will need a default queue.
    //
    if (! isDefaultQueueCreated)
    {
        WDF_IO_QUEUE_CONFIG ioQueueConfig;
        WDFQUEUE queue;

        // Create the Device IO Control queue.
        //
        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
                                               WdfIoQueueDispatchParallel);
        DMF_ContainerQueueConfigCallbacksInit(&ioQueueConfig);

        ntStatus = WdfIoQueueCreate(Device,
                                    &ioQueueConfig,
                                    WDF_NO_OBJECT_ATTRIBUTES,
                                    &queue);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoQueueCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    // Add DMF_DEVICE_CONTEXT as context to Client's Device object.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes,
                                            DMF_DEVICE_CONTEXT);
    ntStatus = WdfObjectAllocateContext(Device,
                                        &deviceAttributes,
                                        (VOID**)&dmfDeviceContext);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfObjectAllocateContext fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    dmfDeviceContext->WdfDevice = Device;

    if (! isControlDevice)
    {
        dmfDeviceContext->WdfClientDriverDevice = Device;
        dmfDeviceContext->WdfControlDevice = NULL;
    }
    else
    {
        dmfDeviceContext->WdfControlDevice = Device;
        dmfDeviceContext->WdfClientDriverDevice = DMF_DmfControlDeviceInitClientDriverDeviceGet(dmfDeviceInit);
    }

    dmfDeviceContext->IsFilterDevice = isFilterDriver;

    // Prepare to create a Module Collection.
    //
    DMF_MODULE_COLLECTION_CONFIG_INIT(&moduleCollectionConfig,
                                      branchTrackModuleConfig,
                                      liveKernelDumpModuleConfig,
                                      Device);

    if (dmfEventCallbacks != NULL)
    {
        if (dmfEventCallbacks->EvtDmfDeviceModulesAdd != NULL)
        {
            dmfEventCallbacks->EvtDmfDeviceModulesAdd(Device,
                                                      (PDMFMODULE_INIT)&moduleCollectionConfig);
        }
    }

    // The attributes for all the Modules have been set. Create the Modules.
    //
    ntStatus = DMF_ModuleCollectionCreate(dmfDeviceInit,
                                          &moduleCollectionConfig,
                                          &dmfCollection);
    if (! NT_SUCCESS(ntStatus))
    {
        // TODO: Do we need to call DMF_DmfDeviceInitFree()?
        //
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCollectionCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Open or register for notification for OPEN_Create or NOTIFY_Create Modules.
    //
    ntStatus = DMF_ModuleCollectionPostCreate(&moduleCollectionConfig,
                                              dmfCollection);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCollectionPostCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Store the collection in Container Context.
    //
    dmfDeviceContext->DmfCollection = dmfCollection;
    dmfDeviceContext->ClientImplementsEvtWdfDriverDeviceAdd = DMF_DmfDeviceInitClientImplementsDeviceAdd(dmfDeviceInit);

    // Store information needed to automatically call DMF_Invoke_DeviceCallbacksDestroy() when the
    // Client is unable to do so (e.g., in the case of non-PnP drivers).
    //
    DMF_MODULE_COLLECTION* moduleCollectionHandle = DMF_CollectionToHandle(dmfCollection);
    moduleCollectionHandle->ClientDevice = Device;
    moduleCollectionHandle->ManualDestroyCallbackIsPending = FALSE;

Exit:

    dmfDeviceInit = NULL;
    DMF_DmfDeviceInitFree(DmfDeviceInitPointer);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfModuleAdd(
    _Inout_ PDMFMODULE_INIT DmfModuleInit,
    _In_ DMF_MODULE_ATTRIBUTES* ModuleAttributes,
    _In_opt_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _In_opt_ DMFMODULE* ResultantDmfModule
    )
/*++

Routine Description:

    The Client calls this function to add a Module's initialized Config structure to the list of Config structures
    that is used later to create a Module Collection.

Arguments:

    DmfModuleInit - Opaque to Client. Indicates how the Client wants to set up the Module Collection.
    ModuleAttributes - Module specific attributes for the Module to add to the list.
    ObjectAttributes - DMF specific attributes for the Module to add to the list.
    ResultantDmfModule - When the Module is created, its handle is written here so that Client can access the Module.

Return Value:

    None

--*/
{
    PAGED_CODE();

    DMF_ModuleCollectionConfigAddAttributes((DMF_MODULE_COLLECTION_CONFIG *)DmfModuleInit,
                                            ModuleAttributes,
                                            ObjectAttributes,
                                            ResultantDmfModule);
}
#pragma code_seg()

// eof: DmfModuleCollection.c
//
