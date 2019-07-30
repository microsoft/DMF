/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfCall.c

Abstract:

    DMF Implementation:
    Functions in this file dispatch callbacks from WDF to every instantiated Module and
    its Child Modules. Also, several helper functions are included in this file.

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#include "DmfIncludeInternal.h"

#include "DmfCall.tmh"

// DMF dispatches all callbacks it receives from WDF to each Module in the Module Collection
// (Parent Module) as well as to each of its Child Modules. Depending on the callback, DMF 
// dispatches to either the Parent Module or Child Module first. Callbacks that occur during
// power up are sent to Child Modules first, then Parent Modules. Callbacks that occur during 
// power down are sent to the Parent Module first, then Child Modules. Note that this 
// dispatching order occurs recursively. Thus, Child Modules are always powered when Parent
// Modules send them commands. Inversely, Child Modules will not receive commands from
// Parent Modules when they are not powered.
// 
// This table summarizes the dispatching order for all supported WDF callbacks:
//  
// ---------------------------------------------------------------------------------------------
// |          Child First                     |              Parent First                      |
// ---------------------------------------------------------------------------------------------
// | DMF_Module_PrepareHardware               |      DMF_Module_ReleaseHardware                |
// | DMF_Module_D0Entry                       |      DMF_Module_D0Exit                         |
// | DMF_Module_D0EntryPostInterruptsEnabled  |      DMF_Module_D0ExitPreInterruptsDisabled    |
// | DMF_Module_SelfManagedIoInit             |      DMF_Module_QueueIoRead                    |
// | DMF_Module_SelfManagedIoRestart          |      DMF_Module_QueueIoWrite                   |
// | DMF_Module_RelationsQuery                |      DMF_Module_DeviceIoControl                |
// | DMF_Module_UsageNotificationEx           |      DMF_Module_InternalDeviceIoControl        |
// | DMF_Module_DisarmWakeFromS0              |      DMF_Module_SelfManagedIoCleanup           |
// | DMF_Module_WakeFromS0Triggered           |      DMF_Module_SelfManagedIoFlush             |
// | DMF_Module_DisarmWakeFromSx              |      DMF_Module_SelfManagedIoSuspend           |
// | DMF_Module_WakeFromSxTriggered           |      DMF_Module_ArmWakeFromS0                  |
// | DMF_Module_ResourcesAssign               |      DMF_Module_ArmWakeFromSxWithReason        |
// | DMF_Module_FileCreate                    |                                                |
// | DMF_Module_FileCleanup                   |                                                |
// | DMF_Module_FileClose                     |                                                |
// | DMF_Module_SurpriseRemoval               |                                                |
// | DMF_Module_QueryRemove                   |                                                |
// | DMF_Module_QueryStop                     |                                                |
// ----------------------------------------------------------------------------------------------
//
// In Child First callbacks, Child Modules will be iterated from first to last (using DmfChildObjectIterateForward).
// In Parent First callbacks, Child Modules will be iterated from last to first (using DmfChildObjectIterateBackward).
//

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Module Callback Child Module Helper Functions
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//

DMF_OBJECT*
DmfChildObjectFirstGet(
    _In_ DMF_OBJECT* ParentObject,
    _Out_ CHILD_OBJECT_INTERATION_CONTEXT* ChildObjectInterationContext
    )
/*++

Routine Description:

    Given a Parent Object and a Child Object Iteration Context, return the Parent Object's first 
    Child Object. Then, initialize the Child Object Iteration Context for subsequent iterations.

Arguments:

    ParentObject - The given Parent Object.
    ChildObjectInterationContext - The given Child Object Iteration Context

Return Value:

    The first Child Object or NULL if no Child Object is present.

--*/
{
    DMF_OBJECT* childDmfObject;
    LIST_ENTRY* firstChildListEntry;

    firstChildListEntry = ParentObject->ChildObjectList.Flink;
    if (firstChildListEntry == &ParentObject->ChildObjectList)
    {
        // There are no Children.
        //
        childDmfObject = NULL;
        ChildObjectInterationContext->NextChildObjectListEntry = NULL;
        ChildObjectInterationContext->PreviousChildObjectListEntry = NULL;
        ChildObjectInterationContext->ParentObject = NULL;
    }
    else
    {
        childDmfObject = CONTAINING_RECORD(firstChildListEntry,
                                           DMF_OBJECT,
                                           ChildListEntry);
        ChildObjectInterationContext->NextChildObjectListEntry = firstChildListEntry->Flink;
        ChildObjectInterationContext->PreviousChildObjectListEntry = firstChildListEntry->Blink;
        ChildObjectInterationContext->ParentObject = ParentObject;
    }

    return childDmfObject;
}

DMF_OBJECT*
DmfChildObjectNextGet(
    _Inout_ CHILD_OBJECT_INTERATION_CONTEXT* ChildObjectInterationContext
    )
/*++

Routine Description:

    Given a Child Object Iteration Context, return the next Child Object.

Arguments:

    ChildObjectInterationContext - The given Child Object Iteration Context

Return Value:

    The next Child Object or NULL if no next Child Object is present.

--*/
{
    DMF_OBJECT* childDmfObject;
    LIST_ENTRY* nextChildListEntry;

    ASSERT(ChildObjectInterationContext->ParentObject != NULL);
    ASSERT(ChildObjectInterationContext->NextChildObjectListEntry != NULL);

    nextChildListEntry = ChildObjectInterationContext->NextChildObjectListEntry;
    if (nextChildListEntry != &ChildObjectInterationContext->ParentObject->ChildObjectList)
    {
        childDmfObject = CONTAINING_RECORD(nextChildListEntry,
                                           DMF_OBJECT,
                                           ChildListEntry);
        ChildObjectInterationContext->NextChildObjectListEntry = nextChildListEntry->Flink;
        ChildObjectInterationContext->PreviousChildObjectListEntry = nextChildListEntry->Blink;
    }
    else
    {
        // There are no more Children.
        //
        childDmfObject = NULL;
        ChildObjectInterationContext->NextChildObjectListEntry = NULL;
        ChildObjectInterationContext->PreviousChildObjectListEntry = NULL;
        ChildObjectInterationContext->ParentObject = NULL;
    }

    return childDmfObject;
}

DMF_OBJECT*
DmfChildObjectLastGet(
    _In_ DMF_OBJECT* ParentObject,
    _Out_ CHILD_OBJECT_INTERATION_CONTEXT* ChildObjectInterationContext
    )
/*++

Routine Description:

    Given a Parent Object and a Child Object Iteration Context, return the Parent Object's last 
    Child Object. Then, initialize the Child Object Iteration Context for subsequent iterations.

Arguments:

    ParentObject - The given Parent Object.
    ChildObjectInterationContext - The given Child Object Iteration Context

Return Value:

    The last Child Object or NULL if no Child Object is present.

--*/
{
    DMF_OBJECT* childDmfObject;
    LIST_ENTRY* lastChildListEntry;

    lastChildListEntry = ParentObject->ChildObjectList.Blink;
    if (lastChildListEntry == &ParentObject->ChildObjectList)
    {
        // There are no Children.
        //
        childDmfObject = NULL;
        ChildObjectInterationContext->NextChildObjectListEntry = NULL;
        ChildObjectInterationContext->PreviousChildObjectListEntry = NULL;
        ChildObjectInterationContext->ParentObject = NULL;
    }
    else
    {
        childDmfObject = CONTAINING_RECORD(ParentObject->ChildObjectList.Blink,
                                           DMF_OBJECT,
                                           ChildListEntry);
        ChildObjectInterationContext->NextChildObjectListEntry = lastChildListEntry->Flink;
        ChildObjectInterationContext->PreviousChildObjectListEntry = lastChildListEntry->Blink;
        ChildObjectInterationContext->ParentObject = ParentObject;
    }

    return childDmfObject;
}

DMF_OBJECT*
DmfChildObjectPreviousGet(
    _Inout_ CHILD_OBJECT_INTERATION_CONTEXT* ChildObjectInterationContext
    )
/*++

Routine Description:

    Given a Child Object Iteration Context, return the next Child Object.

Arguments:

    ChildObjectInterationContext - The given Child Object Iteration Context

Return Value:

    The next Child Object or NULL if no next Child Object is present.

--*/
{
    DMF_OBJECT* childDmfObject;
    LIST_ENTRY* previousChildListEntry;

    ASSERT(ChildObjectInterationContext->ParentObject != NULL);
    ASSERT(ChildObjectInterationContext->NextChildObjectListEntry != NULL);

    previousChildListEntry = ChildObjectInterationContext->PreviousChildObjectListEntry;
    if (previousChildListEntry != &ChildObjectInterationContext->ParentObject->ChildObjectList)
    {
        childDmfObject = CONTAINING_RECORD(previousChildListEntry,
                                           DMF_OBJECT,
                                           ChildListEntry);
        ChildObjectInterationContext->NextChildObjectListEntry = previousChildListEntry->Flink;
        ChildObjectInterationContext->PreviousChildObjectListEntry = previousChildListEntry->Blink;
    }
    else
    {
        // There are no more Children.
        //
        childDmfObject = NULL;
        ChildObjectInterationContext->NextChildObjectListEntry = NULL;
        ChildObjectInterationContext->PreviousChildObjectListEntry = NULL;
        ChildObjectInterationContext->ParentObject = NULL;
    }

    return childDmfObject;
}

// Types for Module Callback Invoke functions that have common signatures so that the common coding pattern
// does not need to be duplicated.
//
typedef NTSTATUS (*DMF_SingleParameterNtStatus)(_In_ DMFMODULE DmfModule);
typedef VOID (*DMF_SingleParameterVoid)(_In_ DMFMODULE DmfModule);
typedef DMF_OBJECT* (*DMF_ChildObjectInitialGet)(_In_ DMF_OBJECT* ParentObject,
                                              _Out_ CHILD_OBJECT_INTERATION_CONTEXT* ChildObjectInterationContext);
typedef DMF_OBJECT* (*DMF_ChildObjectIterationGet)(_Inout_ CHILD_OBJECT_INTERATION_CONTEXT* ChildObjectInterationContext);

typedef struct _DMF_ChildObjectGet
{
    DMF_ChildObjectInitialGet ChildObjectIntialGet;
    DMF_ChildObjectIterationGet ChildObjectIterationGet;
} DMF_ChildObjectGet;

DMF_ChildObjectGet DmfChildObjectIterateForward = { DmfChildObjectFirstGet, DmfChildObjectNextGet };
DMF_ChildObjectGet DmfChildObjectIterateBackward = { DmfChildObjectLastGet, DmfChildObjectPreviousGet };

NTSTATUS
DMF_ChildDispatchSingleParameterNtStatus(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_SingleParameterNtStatus ChildRecursiveFunction,
    _In_ DMF_ChildObjectGet* ChildObjectGet
    )
/*++

Routine Description:

    Given a DMF Module, this function calls the given WDF callback for each child Module.
    The given WDF Callback returns NTSTATUS and takes WdfDevice associated with the given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    ChildRecursiveFunction - The given WDF callback.
    ChildObjectGet - The given iteration callbacks.

Return Value:

    The given WDF callback's return value.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    ntStatus = STATUS_SUCCESS;
    parentDmfObject = DMF_ModuleToObject(DmfModule);

    childDmfObject = ChildObjectGet->ChildObjectIntialGet(parentDmfObject,
                                                          &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild;

        dmfModuleChild = DMF_ObjectToModule(childDmfObject);
        ntStatus = ChildRecursiveFunction(dmfModuleChild);
        if (! NT_SUCCESS(ntStatus))
        {
            break;
        }
        childDmfObject = ChildObjectGet->ChildObjectIterationGet(&childObjectIterationContext);
    }

    return ntStatus;
}

VOID
DMF_ChildDispatchSingleParameterVoid(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_SingleParameterVoid ChildRecursiveFunction,
    _In_ DMF_ChildObjectGet* ChildObjectGet
    )
/*++

Routine Description:

    Given a DMF Module, this function calls the given WDF callback for each child Module.
    The given WDF Callback returns VOID and takes WdfDevice associated with the given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    ChildRecursiveFunction - The given WDF callback.

Return Value:

    None

--*/
{
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    childDmfObject = ChildObjectGet->ChildObjectIntialGet(parentDmfObject,
                                                          &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild;

        dmfModuleChild = DMF_ObjectToModule(childDmfObject);
        ChildRecursiveFunction(dmfModuleChild);
        childDmfObject = ChildObjectGet->ChildObjectIterationGet(&childObjectIterationContext);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Helper Functions for Module Authors
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//

VOID
DMF_ModuleInContextSave(
    _In_ WDFOBJECT WdfObject,
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Given a WDF Object and a DMF Module, this function stores the given DMF Module 
    in the context of the given WDF Object.

    NOTE: Not all WDFOBJECT have access to Parent so this helper is necessary
          for those cases.

Arguments:

    WdfObject - The given WDF Object.
    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMFMODULE* dmfModuleAddress;

    dmfModuleAddress = WdfObjectGet_DMFMODULE(WdfObject);
    ASSERT(dmfModuleAddress != NULL);
    *dmfModuleAddress = DmfModule;
}

NTSTATUS
DMF_ModuleTransportCall(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG Message,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize
    )
/*++

Routine Description:

    Given a DMF Module, this function calls the Module's generic transport Method.

Arguments:

    DmfModule - The given DMF Module.
    Message - Handler specific message.
    InputBuffer - Buffer sent to the handler.
    InputBufferSize - Size of the input buffer.
    OutputBuffer - Buffer to return to the handler.
    OutputBufferSize - Size of the output buffer.

Return Value:

    NTSTATUS from the Module's Transport Method.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;
    DMFMODULE dmfModuleTransport;

    dmfModuleTransport = DMF_ModuleTransportGet(DmfModule);
    dmfObject = DMF_ModuleToObject(dmfModuleTransport);
    ASSERT(dmfObject->ModuleDescriptor.ModuleTransportMethod != NULL);
    ntStatus = dmfObject->ModuleDescriptor.ModuleTransportMethod(dmfModuleTransport,
                                                                 Message,
                                                                 InputBuffer,
                                                                 InputBufferSize,
                                                                 OutputBuffer,
                                                                 OutputBufferSize);

    return ntStatus;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Module Callback Invoke Function Helpers
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleTreeDestroy(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Given a DMF Module, this function destroys the Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);

    ASSERT(! dmfObject->DynamicModuleImmediate);

    // Dispatch callback to Child DMF Modules first.
    //
    DMF_ChildDispatchSingleParameterVoid(DmfModule,
                                         DMF_ModuleTreeDestroy,
                                         &DmfChildObjectIterateForward);

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(dmfObject->ModuleDescriptor.CallbacksDmf->ModuleInstanceDestroy != NULL);
    (dmfObject->ModuleDescriptor.CallbacksDmf->ModuleInstanceDestroy)(DmfModule);

    // The Module Callback always does this. Do it for the Module.
    //
    DMF_ModuleDestroy(DmfModule, 
                      TRUE);
}

_Function_class_(EVT_WDF_OBJECT_CONTEXT_CLEANUP)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfEvtDynamicModuleCleanupCallback(
    _In_ WDFOBJECT Object
    )
/*++

Routine Description:

    Clean up callback when a Dynamic DMFMODULE is deleted. This callback will close the Module
    and destroy its Child Modules. Then, it calls the Client's CleanUp callback, if any.

Arguments:

    Object - DMFMODULE to delete.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_OBJECT* dmfObject;
    PFN_WDF_OBJECT_CONTEXT_CLEANUP clientEvtCleanupCallback;

    // NOTE: DMFMODULE should always be deleted in PASSIVE_LEVEL.
    // (Even though it can technically be called at DISPATCH_LEVEL, Clients should
    // not allow this to happen. It means Parents of Modules should not be WDFMEMORY
    // objects using NonPaged Pool, nor a WDFREQUEST.)
    //
#if !defined(DMF_USER_MODE)
    ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif

    dmfModule = (DMFMODULE)Object;
    dmfObject = DMF_ModuleToObject(dmfModule);

    // Save off Client's callback so it can be called after object is destroyed.
    //
    clientEvtCleanupCallback = dmfObject->ClientEvtCleanupCallback;

    // Since  it is a Dynamic Module automatically close it before it is destroyed.
    // (Client has no access to the Close API.)
    //
    ASSERT(dmfObject->DynamicModuleImmediate);
    DMF_Module_CloseOrUnregisterNotificationOnDestroy(dmfModule);

    // Dispatch callback to Child DMF Modules first.
    // 'The current function is permitted to run at an IRQ level above the maximum permitted'
    //
    #pragma warning(suppress:28118)
    DMF_ChildDispatchSingleParameterVoid(dmfModule,
                                         DMF_ModuleTreeDestroy,
                                         &DmfChildObjectIterateBackward);

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(dmfObject->ModuleDescriptor.CallbacksDmf->ModuleInstanceDestroy != NULL);
    // 'The current function is permitted to run at an IRQ level above the maximum permitted'
    //
    #pragma warning(suppress:28118)
    (dmfObject->ModuleDescriptor.CallbacksDmf->ModuleInstanceDestroy)(dmfModule);

    // The Module Callback always does this. Do it for the Module.
    // NOTE: Don't delete the memory because it will be deleted by WDF.
    // 'The current function is permitted to run at an IRQ level above the maximum permitted'
    //
    #pragma warning(suppress:28118)
    DMF_ModuleDestroy(dmfModule, 
                      FALSE);

    // Next, allow Client to clean up.
    //
    if (clientEvtCleanupCallback != NULL)
    {
        clientEvtCleanupCallback(Object);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DMF calls these functions to execute Modules' WDF callbacks.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_PrepareHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Invoke the ModulePrepareHardware Callback for a given DMF Module.
    First, each of the Child DMF Modules' corresponding callbacks are called.
    Next, the given DMF Module's corresponding callback is called.

Arguments:

    DmfModule - The given DMF Module.
    ResourcesRaw - WDF Resource Raw parameter that is passed to the given
                   DMF Module callback.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.

Return Value:

    STATUS_SUCCESS if all children return STATUS_SUCCESS and the given DMF Module
    does not encounter an error.
    If there is an error, then NTSTATUS code of the first child Module that encounters the
    error, or the NTSTATUS code of the given DMF Module.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to Child DMF Modules first.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild;

        dmfModuleChild = DMF_ObjectToModule(childDmfObject);

        ntStatus = DMF_Module_PrepareHardware(dmfModuleChild,
                                              ResourcesRaw,
                                              ResourcesTranslated);
        if (! NT_SUCCESS(ntStatus))
        {
            // If Child Module cannot open, then don't open the Parent Module.
            // When Parent Module opens it is guaranteed to have all its children open.
            //
            goto Exit;
        }
        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModulePrepareHardware != NULL);
    ntStatus = (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModulePrepareHardware)(DmfModule,
                                                                                       ResourcesRaw,
                                                                                       ResourcesTranslated);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }


Exit:

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_ReleaseHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    This is a wrapper around DMF_Module_ReleaseHardware

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleReleaseHardware != NULL);
    ntStatus = (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleReleaseHardware)(DmfModule,
                                                                                       ResourcesTranslated);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Dispatch callback to Child DMF Modules next.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild;

        dmfModuleChild = DMF_ObjectToModule(childDmfObject);

        ntStatus = DMF_Module_ReleaseHardware(dmfModuleChild,
                                              ResourcesTranslated);
        if (! NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Module_D0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Invoke the ModuleD0Entry Callback for a given DMF Module.
    First, each of the Child DMF Modules' corresponding callbacks are called.
    Next, the given DMF Module's corresponding callback is called.
    (Power up children first. Then, power up parent.)

Arguments:

    DmfModule - The given DMF Module.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

    STATUS_SUCCESS if all children return STATUS_SUCCESS and the given DMF Module
    does not encounter an error.
    If there is an error, then NTSTATUS code of the first child Module that encounters the
    error, or the NTSTATUS code of the given DMF Module.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to Child DMF Modules first.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild;

        dmfModuleChild = DMF_ObjectToModule(childDmfObject);
        ntStatus = DMF_Module_D0Entry(dmfModuleChild,
                                      PreviousState);
        if (! NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0Entry != NULL);
    ntStatus = (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0Entry)(DmfModule,
                                                                               PreviousState);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Module_D0EntryPostInterruptsEnabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Invoke the ModuleD0EntryPostInterruptsEnabled Callback for a given DMF Module.
    First, each of the Child DMF Modules' corresponding callbacks are called.
    Next, the given DMF Module's corresponding callback is called.
    (Power up children first. Then, power up parent.)

Arguments:

    DmfModule - The given DMF Module.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

    STATUS_SUCCESS if all children return STATUS_SUCCESS and the given DMF Module
    does not encounter an error.
    If there is an error, then NTSTATUS code of the first child Module that encounters the
    error, or the NTSTATUS code of the given DMF Module.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to Child DMF Modules first.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild;

        dmfModuleChild = DMF_ObjectToModule(childDmfObject);
        ntStatus = DMF_Module_D0EntryPostInterruptsEnabled(dmfModuleChild,
                                                           PreviousState);
        if (! NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0EntryPostInterruptsEnabled != NULL);
    ntStatus = (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0EntryPostInterruptsEnabled)(DmfModule,
                                                                                                    PreviousState);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_D0ExitPreInterruptsDisabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Invoke the ModuleD0ExitPreInterruptsDisabled Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called.
    (Power down parent first. Then, power down children.)

Arguments:

    DmfModule - The given DMF Module.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;
    NTSTATUS ntStatus;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0ExitPreInterruptsDisabled != NULL);
    ntStatus = (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0ExitPreInterruptsDisabled)(DmfModule,
                                                                                                   TargetState);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Dispatch callback to Child DMF Modules next.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild;

        dmfModuleChild = DMF_ObjectToModule(childDmfObject);

        ntStatus = DMF_Module_D0ExitPreInterruptsDisabled(dmfModuleChild,
                                                          TargetState);
        if (! NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_D0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Invoke the ModuleD0Exit Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called.
    (Power down parent first. Then, power down children.)

Arguments:

    DmfModule - The given DMF Module.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;
    NTSTATUS ntStatus;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0Exit != NULL);
    ntStatus = (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleD0Exit)(DmfModule,
                                                                              TargetState);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Dispatch callback to Child DMF Modules next.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild;

        dmfModuleChild = DMF_ObjectToModule(childDmfObject);

        ntStatus = DMF_Module_D0Exit(dmfModuleChild,
                                     TargetState);
        if (! NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Module_QueueIoRead(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    )
/*++

Routine Description:

    Invoke the ModuleQueueIoRead Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    OutputBufferLength - For fast access to the Request's Buffer Length.

Return Value:

    TRUE if either the given DMF Module or it children handle the IOCTL
    (and it should not be handled by any other Module).
    FALSE, otherwise.

--*/
{
    BOOLEAN returnValue;
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueueIoRead != NULL);
    returnValue = (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueueIoRead)(DmfModule,
                                                                                      Queue,
                                                                                      Request,
                                                                                      Length);
    if (returnValue)
    {
        // It is handled...do not submit to children.
        //
        goto Exit;
    }

    // Dispatch callback to Child DMF Modules after trying parent Module and only if parent Module
    // did not handle the IOCTL.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild;

        dmfModuleChild = DMF_ObjectToModule(childDmfObject);
        returnValue = DMF_Module_QueueIoRead(dmfModuleChild,
                                             Queue,
                                             Request,
                                             Length);
        if (returnValue)
        {
            // It is handled...do not submit to siblings.
            //
            break;
        }

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

Exit:

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Module_QueueIoWrite(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    )
/*++

Routine Description:

    Invoke the ModuleQueueIoRead Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    OutputBufferLength - For fast access to the Request's Buffer Length.

Return Value:

    TRUE if either the given DMF Module or it children handle the IOCTL
    (and it should not be handled by any other Module).
    FALSE, otherwise.

--*/
{
    BOOLEAN returnValue;
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueueIoWrite != NULL);
    returnValue = (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueueIoWrite)(DmfModule,
                                                                                       Queue,
                                                                                       Request,
                                                                                       Length);
    if (returnValue)
    {
        // It is handled...do not submit to children.
        //
        goto Exit;
    }

    // Dispatch callback to Child DMF Modules after trying parent Module and only if parent Module
    // did not handle the IOCTL.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild;

        dmfModuleChild = DMF_ObjectToModule(childDmfObject);
        returnValue = DMF_Module_QueueIoWrite(dmfModuleChild,
                                              Queue,
                                              Request,
                                              Length);
        if (returnValue)
        {
            // It is handled...do not submit to siblings.
            //
            break;
        }

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

Exit:

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Module_DeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    Invoke the ModuleDeviceIoControl Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    OutputBufferLength - For fast access to the Request's Output Buffer Length.
    InputBufferLength - For fast access to the Request's Input Buffer Length.
    IoControlCode - The given IOCTL code.

Return Value:

    TRUE if either the given DMF Module or it children handle the IOCTL
    (and it should not be handled by any other Module).
    FALSE, otherwise.

--*/
{
    BOOLEAN returnValue;
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleDeviceIoControl != NULL);
    returnValue = (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleDeviceIoControl)(DmfModule,
                                                                                          Queue,
                                                                                          Request,
                                                                                          OutputBufferLength,
                                                                                          InputBufferLength,
                                                                                          IoControlCode);
    if (returnValue)
    {
        // It is handled...do not submit to children.
        //
        goto Exit;
    }

    // Dispatch callback to Child DMF Modules after trying parent Module and only if parent Module
    // did not handle the IOCTL.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild;

        dmfModuleChild = DMF_ObjectToModule(childDmfObject);
        returnValue = DMF_Module_DeviceIoControl(dmfModuleChild,
                                                 Queue,
                                                 Request,
                                                 OutputBufferLength,
                                                 InputBufferLength,
                                                 IoControlCode);
        if (returnValue)
        {
            // It is handled...do not submit to siblings.
            //
            break;
        }

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

Exit:

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Module_InternalDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    Invoke the ModuleInternalDeviceIoControl Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.
    Queue - Target WDF Queue for the IOCTL call.
    Request - WDF Request with IOCTL parameters.
    OutputBufferLength - For fast access to the Request's Output Buffer Length.
    InputBufferLength - For fast access to the Request's Input Buffer Length.
    IoControlCode - The given IOCTL code.

Return Value:

    TRUE if either the given DMF Module or it children handle the IOCTL
    (and it should not be handled by any other Module).
    FALSE, otherwise.

--*/
{
    BOOLEAN returnValue;
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleInternalDeviceIoControl != NULL);
    returnValue = (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleInternalDeviceIoControl)(DmfModule,
                                                                                                  Queue,
                                                                                                  Request,
                                                                                                  OutputBufferLength,
                                                                                                  InputBufferLength,
                                                                                                  IoControlCode);
    if (returnValue)
    {
        // It is handled...do not submit to children.
        //
        goto Exit;
    }

    // Dispatch callback to Child DMF Modules after trying parent Module and only if parent Module
    // did not handle the IOCTL.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild;

        dmfModuleChild = DMF_ObjectToModule(childDmfObject);
        returnValue = DMF_Module_InternalDeviceIoControl(dmfModuleChild,
                                                         Queue,
                                                         Request,
                                                         OutputBufferLength,
                                                         InputBufferLength,
                                                         IoControlCode);
        if (returnValue)
        {
            // It is handled...do not submit to siblings.
            //
            break;
        }

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

Exit:

    return returnValue;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Module_SelfManagedIoCleanup(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the ModuleSelfManagedIoCleanup Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoCleanup != NULL);
    (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoCleanup)(DmfModule);

    // Dispatch callback to Child DMF Modules next.
    //
    DMF_ChildDispatchSingleParameterVoid(DmfModule,
                                         DMF_Module_SelfManagedIoCleanup,
                                         &DmfChildObjectIterateBackward);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Module_SelfManagedIoFlush(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the ModuleSelfManagedIoFlush Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoFlush != NULL);
    (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoFlush)(DmfModule);

    // Dispatch callback to Child DMF Modules next.
    //
    DMF_ChildDispatchSingleParameterVoid(DmfModule,
                                         DMF_Module_SelfManagedIoFlush,
                                         &DmfChildObjectIterateBackward);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_SelfManagedIoInit(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the ModuleSelfManagedIoInit Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);

    // Dispatch callback to Child DMF Modules first.
    //
    ntStatus = DMF_ChildDispatchSingleParameterNtStatus(DmfModule,
                                                        DMF_Module_SelfManagedIoInit,
                                                        &DmfChildObjectIterateForward);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoInit != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoInit)(DmfModule);

Exit:

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_SelfManagedIoSuspend(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the ModuleSelfManagedIoSuspend Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoSuspend != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoSuspend)(DmfModule);

    // Dispatch callback to Child DMF Modules next.
    //
    ntStatus = DMF_ChildDispatchSingleParameterNtStatus(DmfModule,
                                                        DMF_Module_SelfManagedIoSuspend,
                                                        &DmfChildObjectIterateBackward);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_SelfManagedIoRestart(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the ModuleSelfManagedIoRestart Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);

    // Dispatch callback to Child DMF Modules first.
    //
    ntStatus = DMF_ChildDispatchSingleParameterNtStatus(DmfModule,
                                                        DMF_Module_SelfManagedIoRestart,
                                                        &DmfChildObjectIterateForward);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoRestart != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSelfManagedIoRestart)(DmfModule);

Exit:

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Module_SurpriseRemoval(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the ModuleSurpriseRemoval Callback for a given DMF Module.
    First, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).
    Next, the given DMF Module's corresponding callback is called.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSurpriseRemoval != NULL);
    (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleSurpriseRemoval)(DmfModule);

    // Dispatch callback to Child DMF Modules next.
    //
    DMF_ChildDispatchSingleParameterVoid(DmfModule,
                                         DMF_Module_SurpriseRemoval,
                                         &DmfChildObjectIterateBackward);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_QueryRemove(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the ModuleQueryRemove Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueryRemove != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueryRemove)(DmfModule);

    // Dispatch callback to Child DMF Modules next.
    //
    ntStatus = DMF_ChildDispatchSingleParameterNtStatus(DmfModule,
                                                        DMF_Module_QueryRemove,
                                                        &DmfChildObjectIterateBackward);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_QueryStop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the ModuleQueryStop Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueryStop != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleQueryStop)(DmfModule);

    // Dispatch callback to Child DMF Modules next.
    //
    ntStatus = DMF_ChildDispatchSingleParameterNtStatus(DmfModule,
                                                        DMF_Module_QueryStop,
                                                        &DmfChildObjectIterateBackward);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Module_RelationsQuery(
    _In_ DMFMODULE DmfModule,
    _In_ DEVICE_RELATION_TYPE RelationType
    )
/*++

Routine Description:

    Invoke the ModuleRelationsQuery Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.
    RelationType - A DEVICE_RELATION_TYPE-typed enumerator value.
                   The DEVICE_RELATION_TYPE enumeration is defined in wdm.h.

Return Value:

    None

--*/
{
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to Child DMF Modules first.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild = DMF_ObjectToModule(childDmfObject);

        DMF_Module_RelationsQuery(dmfModuleChild,
                                  RelationType);

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleRelationsQuery != NULL);
    (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleRelationsQuery)(DmfModule,
                                                                           RelationType);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Module_UsageNotificationEx(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_SPECIAL_FILE_TYPE NotificationType,
    _In_ BOOLEAN IsInNotificationPath
    )
/*++

Routine Description:

    Invoke the ModuleUsageNotificationEx Callback for a given DMF Module.
    First, each of the Child DMF Modules' corresponding callbacks are called.
    Next, the given DMF Module's corresponding callback is called.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to Child DMF Modules first.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild = DMF_ObjectToModule(childDmfObject);

        ntStatus = DMF_Module_UsageNotificationEx(dmfModuleChild,
                                                  NotificationType,
                                                  IsInNotificationPath);
        if (! NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleUsageNotificationEx != NULL);
    ntStatus = (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleUsageNotificationEx)(DmfModule,
                                                                                           NotificationType,
                                                                                           IsInNotificationPath);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_ArmWakeFromS0(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the ModuleArmWakeFromS0 Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleArmWakeFromS0 != NULL);
    ntStatus = (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleArmWakeFromS0)(DmfModule);

    // Dispatch callback to Child DMF Modules next.
    //
    ntStatus = DMF_ChildDispatchSingleParameterNtStatus(DmfModule,
                                                        DMF_Module_ArmWakeFromS0,
                                                        &DmfChildObjectIterateBackward);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Module_DisarmWakeFromS0(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the ModuleDisarmWakeFromS0 Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);

    // Dispatch callback to Child DMF Modules first.
    //
    DMF_ChildDispatchSingleParameterVoid(DmfModule,
                                         DMF_Module_DisarmWakeFromS0,
                                         &DmfChildObjectIterateForward);

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleDisarmWakeFromS0 != NULL);
    (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleDisarmWakeFromS0)(DmfModule);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Module_WakeFromS0Triggered(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the ModuleWakeFromS0Triggered Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);

    // Dispatch callback to Child DMF Modules first.
    //
    DMF_ChildDispatchSingleParameterVoid(DmfModule,
                                         DMF_Module_WakeFromS0Triggered,
                                         &DmfChildObjectIterateForward);

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleWakeFromS0Triggered != NULL);
    (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleWakeFromS0Triggered)(DmfModule);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Module_ArmWakeFromSxWithReason(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN DeviceWakeEnabled,
    _In_ BOOLEAN ChildrenArmedForWake
    )
/*++

Routine Description:

    Invoke the ModuleArmWakeFromSxWithReason Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.
    DeviceWakeEnabled- A Boolean value that, if TRUE, indicates that the device's ability to wake the system is enabled.
    ChildrenArmedForWake - A Boolean value that, if TRUE, indicates that the ability of one or more child devices
                           to wake the system is enabled.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleArmWakeFromSxWithReason != NULL);
    ntStatus = (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleArmWakeFromSxWithReason)(DmfModule,
                                                                                               DeviceWakeEnabled,
                                                                                               ChildrenArmedForWake);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Dispatch callback to Child DMF Modules next.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                           &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild = DMF_ObjectToModule(childDmfObject);

        ntStatus = DMF_Module_ArmWakeFromSxWithReason(dmfModuleChild,
                                                      DeviceWakeEnabled,
                                                      ChildrenArmedForWake);
        if (! NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Module_DisarmWakeFromSx(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the ModuleDisarmWakeFromSx Callback for a given DMF Module.
    First, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).
    Next, the given DMF Module's corresponding callback is called.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);

    // Dispatch callback to Child DMF Modules first.
    //
    DMF_ChildDispatchSingleParameterVoid(DmfModule,
                                         DMF_Module_DisarmWakeFromSx,
                                         &DmfChildObjectIterateForward);

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleDisarmWakeFromSx != NULL);
    (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleDisarmWakeFromSx)(DmfModule);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Module_WakeFromSxTriggered(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the ModuleWakeFromSxTriggered Callback for a given DMF Module.
    First, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).
    Next, the given DMF Module's corresponding callback is called.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);

    // Dispatch callback to Child DMF Modules first.
    //
    DMF_ChildDispatchSingleParameterVoid(DmfModule,
                                         DMF_Module_WakeFromSxTriggered,
                                         &DmfChildObjectIterateForward);

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(dmfObject->ModuleDescriptor.CallbacksWdf->ModuleWakeFromSxTriggered != NULL);
    (dmfObject->ModuleDescriptor.CallbacksWdf->ModuleWakeFromSxTriggered)(DmfModule);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Module_FileCreate(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Invoke the ModuleFileCreate Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.
    Device - WDF device object.
    Request - WDF Request with IOCTL parameters.
    FileObject - WDF file object that describes a file that is being opened for the specified request

Return Value:

    TRUE if either the given DMF Module or it children handle the IOCTL.
    FALSE, otherwise.

--*/
{
    BOOLEAN returnValue;
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleFileCreate != NULL);
    returnValue = (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleFileCreate)(DmfModule,
                                                                                     Device,
                                                                                     Request,
                                                                                     FileObject);
    if (returnValue)
    {
        // It is handled...do not submit to children.
        //
        goto Exit;
    }

    // Dispatch callback to Child DMF Modules after trying parent Module and only if parent Module
    // did not handle the IOCTL.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild = DMF_ObjectToModule(childDmfObject);

        returnValue = DMF_Module_FileCreate(dmfModuleChild,
                                            Device,
                                            Request,
                                            FileObject);
        if (returnValue)
        {
            // It is handled...do not submit to siblings.
            //
            break;
        }

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

Exit:

    return returnValue;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Module_FileCleanup(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Invoke the ModuleFileCleanup Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.
    FileObject - WDF file object

Return Value:

    TRUE if either the given DMF Module or it children handle the IOCTL.
    FALSE, otherwise.

--*/
{
    BOOLEAN returnValue;
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleFileCleanup != NULL);
    returnValue = (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleFileCleanup)(DmfModule,
                                                                                      FileObject);
    if (returnValue)
    {
        // It is handled...do not submit to children.
        //
        goto Exit;
    }

    // Dispatch callback to Child DMF Modules after trying parent Module and only if parent Module
    // did not handle the IOCTL.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild = DMF_ObjectToModule(childDmfObject);

        returnValue = DMF_Module_FileCleanup(dmfModuleChild,
                                             FileObject);
        if (returnValue)
        {
            // It is handled...do not submit to siblings.
            //
            break;
        }

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

Exit:

    return returnValue;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Module_FileClose(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Invoke the ModuleFileClose Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called
    (if the IOCTL was not handled by the given DMF Module).

Arguments:

    DmfModule - The given DMF Module.
    FileObject - WDF file object

Return Value:

    TRUE if either the given DMF Module or it children handle the IOCTL.
    FALSE, otherwise.

--*/
{
    BOOLEAN returnValue;
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleFileClose != NULL);
    returnValue = (parentDmfObject->ModuleDescriptor.CallbacksWdf->ModuleFileClose)(DmfModule,
                                                                                    FileObject);
    if (returnValue)
    {
        // It is handled...do not submit to children.
        //
        goto Exit;
    }

    // Dispatch callback to Child DMF Modules after trying parent Module and only if parent Module
    // did not handle the IOCTL.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild = DMF_ObjectToModule(childDmfObject);

        returnValue = DMF_Module_FileClose(dmfModuleChild,
                                           FileObject);
        if (returnValue)
        {
            // It is handled...do not submit to siblings.
            //
            break;
        }

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

Exit:

    return returnValue;
}

NTSTATUS
DMF_Module_NotificationRegister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the NotificationRegister Callback for a given DMF Module.
    First, each of the Child DMF Modules' corresponding callbacks are called.
    Next, the given DMF Module's corresponding callback is called.
    (Since the notification callbacks happen asynchronously, the order is not
    particularly important.)

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    STATUS_SUCCESS if all children return STATUS_SUCCESS and the given DMF Module
    does not encounter an error.
    If there is an error, then NTSTATUS code of the first child Module that encounters the
    error, or the NTSTATUS code of the given DMF Module.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* parentDmfObject;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to Child DMF Modules first.
    //
    ntStatus = DMF_ChildDispatchSingleParameterNtStatus(DmfModule,
                                                        DMF_Module_NotificationRegister,
                                                        &DmfChildObjectIterateForward);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(parentDmfObject->InternalCallbacksDmf.DeviceNotificationRegister != NULL);
    ntStatus = (parentDmfObject->InternalCallbacksDmf.DeviceNotificationRegister)(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // This may be overwritten by DMF if the Module's register for notification handler is called
    // automatically.
    // Otherwise, it means the Client registered for notification for this Module.
    //
    ASSERT(parentDmfObject->ModuleNotificationRegisteredDuring == ModuleOpenedDuringType_Invalid);
    parentDmfObject->ModuleNotificationRegisteredDuring = ModuleOpenedDuringType_Manual;

Exit:

    return ntStatus;
}

VOID
DMF_Module_NotificationUnregister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the NotificationUnregisger Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called.
    (Since the notification callbacks happen asynchronously, the order is not
    particularly important.)

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to the given Parent DMF Module first.
    //
    ASSERT(dmfObject->InternalCallbacksDmf.DeviceNotificationUnregister != NULL);
    (dmfObject->InternalCallbacksDmf.DeviceNotificationUnregister)(DmfModule);

    // Reset Module Opened since Module is not open now.
    //
    ASSERT(dmfObject->ModuleNotificationRegisteredDuring != ModuleOpenedDuringType_Invalid);
    ASSERT(dmfObject->ModuleNotificationRegisteredDuring < ModuleOpenedDuringType_Maximum);
    dmfObject->ModuleNotificationRegisteredDuring = ModuleOpenedDuringType_Invalid;

    // Dispatch callback to Child DMF Modules next.
    //
    DMF_ChildDispatchSingleParameterVoid(DmfModule,
                                         DMF_Module_NotificationUnregister,
                                         &DmfChildObjectIterateBackward);
}

NTSTATUS
DMF_ModuleOpen(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the Open Callback for a given DMF Module.
    NOTE: This call is provided for Clients that manually open the Module. In most cases,
          modules are automatically opened, so it is rare that this call be used.
          It might be used in cases where a DMF Module must be created/opened/closed/destroyed
          in a callback function

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    STATUS_SUCCESS if the given DMF Module does not encounter an error.
    If there is an error, then NTSTATUS code of the first child Module that encounters the
    error, or the NTSTATUS code of the given DMF Module.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    ntStatus = STATUS_SUCCESS;

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(dmfObject->InternalCallbacksDmf.DeviceOpen != NULL);
    ntStatus = (dmfObject->InternalCallbacksDmf.DeviceOpen)(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

Exit:

    return ntStatus;
}

NTSTATUS
DMF_Module_OpenOrRegisterNotificationOnCreate(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the Open or Notify Callback for a Parent and all recursive children during Module Create.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    STATUS_SUCCESS if all children return STATUS_SUCCESS and the given DMF Module
    does not encounter an error.
    If there is an error, then NTSTATUS code of the first child Module that encounters the
    error, or the NTSTATUS code of the given DMF Module.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    ntStatus = STATUS_SUCCESS;

    // Dispatch callback to Child DMF Modules first.
    //
    ntStatus = DMF_ChildDispatchSingleParameterNtStatus(DmfModule,
                                                        DMF_Module_OpenOrRegisterNotificationOnCreate,
                                                        &DmfChildObjectIterateForward);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    if (dmfObject->ModuleDescriptor.OpenOption == DMF_MODULE_OPEN_OPTION_OPEN_Create)
    {
        // Dispatch Open callback to the given Parent DMF Module next.
        //
        ASSERT(dmfObject->InternalCallbacksDmf.DeviceOpen != NULL);
        ntStatus = (dmfObject->InternalCallbacksDmf.DeviceOpen)(DmfModule);
        if (! NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }

        // Indicate when the Module was opened (for clean up operations).
        // Internal Open has set this value to Manual by default.
        //
        ASSERT(ModuleOpenedDuringType_Manual == dmfObject->ModuleOpenedDuring);
        dmfObject->ModuleOpenedDuring = ModuleOpenedDuringType_Create;
    }
    else if (dmfObject->ModuleDescriptor.OpenOption == DMF_MODULE_OPEN_OPTION_NOTIFY_Create)
    {
        // Dispatch NotificationRegister callback to the given Parent DMF Module next.
        //
        ASSERT(dmfObject->InternalCallbacksDmf.DeviceNotificationRegister != NULL);
        ntStatus = (dmfObject->InternalCallbacksDmf.DeviceNotificationRegister)(DmfModule);
        if (! NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }
    }

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModuleReference(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    If a Module is open, acquires a reference to the Module so it remains open until 
    DMF_ModuleDereference is called.  Use this in Module Methods when a Module is 
    opened in notification callback.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

   STATUS_SUCCESS - Module is open and reference has been acquired.
   STATUS_INVALID_DEVICE_STATE - Module is not open.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    DMF_ModuleLock(DmfModule);

    // Increase reference only if Module is open (ReferenceCount >= 1) and if the Module close is not pending.
    // This is to stop new Module method callers from repeatedly accessing the Module when it should be closing.
    //
    if ((dmfObject->ReferenceCount >= 1) &&
        (! dmfObject->IsClosePending))
    {
        // Increase reference count to ensure that Module will not be closed while a Module method is running.
        //
        DMF_ModuleReferenceAdd(DmfModule);
        ntStatus = STATUS_SUCCESS;
    }
    else
    {
        // Tell caller that this Module is not open and that Module Method should not do anything.
        //
        ntStatus = STATUS_INVALID_DEVICE_STATE;
    }

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ModuleDereference(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Releases the Module acquired in DMF_ModuleReference.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = NULL;

    DMF_ModuleLock(DmfModule);

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject->ReferenceCount >= 1);

    DMF_ModuleReferenceDelete(DmfModule);

    DMF_ModuleUnlock(DmfModule);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleWaitForReferenceCountToClear(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Waits for the Module's reference count to reach zero. This is used for rundown
    management when a Module is closing but its Methods may still be called or running.
    It allows DMF to make the Module is open while Methods that are already running
    continue running, but disallows new Methods from starting to run.

    TODO: Replace this logic with the Kernel-mode primitive that performs this logic.
          Note, however, that the primitive is not supported in User-mode. Therefore,
          in that case, this code is still needed.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;
    LONG referenceCount;
    // This value is chosen to give running thread time to execute,
    // but short enough to allow fast response.
    //
    const ULONG referenceCountPollingIntervalMs = 100;

    dmfObject = DMF_ModuleToObject(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);

    DMF_ModuleLock(DmfModule);

    // Set IsClosePending to TRUE, to avoid Module Method from acquiring
    // a reference to the Module infinitely and blocking the Module from closing.
    //
    dmfObject->IsClosePending = TRUE;
    referenceCount = dmfObject->ReferenceCount;

    DMF_ModuleUnlock(DmfModule);

    while (referenceCount > 0)
    {
        DMF_ModuleLock(DmfModule);

        if (referenceCount == 1)
        {
            // Module Method is not running. Prevent any Module Method from starting because call Acquire will fail.
            //
            dmfObject->ReferenceCount = 0;
            // For modules which open on notification callback, ReferenceCount = 0 means the Module is now closed.
            //
            dmfObject->IsClosePending = FALSE;
        }
        referenceCount = dmfObject->ReferenceCount;

        DMF_ModuleUnlock(DmfModule);

        if (referenceCount == 0)
        {
            break;
        }

        // Reference count > 1 means a Module Method is running.
        // Wait for Reference count to run down to 0.
        //
        DMF_Utility_DelayMilliseconds(referenceCountPollingIntervalMs);
        TraceInformation(DMF_TRACE, "DmfModule=0x%p [%s] Waiting to close", DmfModule, dmfObject->ClientModuleInstanceName);
    }

    FuncExit(DMF_TRACE, "DmfModule=0x%p [%s]", DmfModule, dmfObject->ClientModuleInstanceName);
}

VOID
DMF_ModuleClose(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the Close Callback for a given DMF Module.
    NOTE: This call is provided for Clients that manually open the Module. In most cases,
          modules are automatically opened, so it is rare that this call be used.
          It might be used in cases where a DMF Module must be created/opened/closed/destroyed
          in a callback function

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to this Module first.
    //
    ASSERT(dmfObject->InternalCallbacksDmf.DeviceClose != NULL);
    (dmfObject->InternalCallbacksDmf.DeviceClose)(DmfModule);
}

VOID
DMF_Module_CloseOrUnregisterNotificationOnDestroy(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the Close or NotificationUnregister Callback for a given DMF Module.
    First, the given DMF Module's corresponding callback is called.
    Next, each of the Child DMF Modules' corresponding callbacks are called.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);

    // Dispatch callback to this Module first.
    //
    if (dmfObject->ModuleDescriptor.OpenOption == DMF_MODULE_OPEN_OPTION_OPEN_Create)
    {
        if (dmfObject->ModuleOpenedDuring == ModuleOpenedDuringType_Create)
        {
            ASSERT(dmfObject->InternalCallbacksDmf.DeviceClose != NULL);
            (dmfObject->InternalCallbacksDmf.DeviceClose)(DmfModule);
        }
        else
        {
            // Module was cleaned up due to partially successful initialization.
            // (Some, but not all modules in Module Collection were opened.)
            //
        }
    }
    else if (dmfObject->ModuleDescriptor.OpenOption == DMF_MODULE_OPEN_OPTION_NOTIFY_Create)
    {
        ASSERT(dmfObject->InternalCallbacksDmf.DeviceNotificationUnregister != NULL);
        (dmfObject->InternalCallbacksDmf.DeviceNotificationUnregister)(DmfModule);
    }

    // Dispatch callback to Child DMF Modules next.
    //
    DMF_ChildDispatchSingleParameterVoid(DmfModule,
                                         DMF_Module_CloseOrUnregisterNotificationOnDestroy,
                                         &DmfChildObjectIterateBackward);
}

// Sometimes the Thread ID of the current thread is zero. In that case, use DMF_INVALID_HANDLE_VALUE.
//
#define DMF_INVALID_HANDLE_VALUE (HANDLE)(-1)

VOID
DMF_ModuleLock(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the Lock Callback for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    ASSERT(dmfObject != NULL);
    ASSERT(dmfObject->InternalCallbacksInternal.AuxiliaryLock != NULL);
    (dmfObject->InternalCallbacksInternal.AuxiliaryLock)(DmfModule,
                                                         DMF_DEFAULT_LOCK_INDEX);
    ASSERT(NULL == dmfObject->Synchronizations[DMF_DEFAULT_LOCK_INDEX].LockHeldByThread);
#if !defined(DMF_USER_MODE)
    dmfObject->Synchronizations[DMF_DEFAULT_LOCK_INDEX].LockHeldByThread = PsGetCurrentThreadId();
#else
    dmfObject->Synchronizations[DMF_DEFAULT_LOCK_INDEX].LockHeldByThread = GetCurrentThread();
#endif
    // Sometimes the Thread ID of the current thread is zero. In that case, use DMF_INVALID_HANDLE_VALUE.
    //
    if (dmfObject->Synchronizations[DMF_DEFAULT_LOCK_INDEX].LockHeldByThread == 0)
    {
        dmfObject->Synchronizations[DMF_DEFAULT_LOCK_INDEX].LockHeldByThread = DMF_INVALID_HANDLE_VALUE;
    }
}

VOID
DMF_ModuleUnlock(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Invoke the Unlock Callback for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    ASSERT(dmfObject != NULL);
    ASSERT(NULL != dmfObject->Synchronizations[DMF_DEFAULT_LOCK_INDEX].LockHeldByThread);
    dmfObject->Synchronizations[DMF_DEFAULT_LOCK_INDEX].LockHeldByThread = NULL;
    ASSERT(dmfObject->InternalCallbacksInternal.AuxiliaryUnlock != NULL);
    (dmfObject->InternalCallbacksInternal.AuxiliaryUnlock)(DmfModule,
                                                           DMF_DEFAULT_LOCK_INDEX);
}

#if defined(DEBUG)
BOOLEAN
DMF_ModuleIsLocked(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Returns the lock status of the given DMF Module.
    NOTE: This function is for debug purposes only.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    TRUE if the given DMF Module is currently locked; false, otherwise.

--*/
{
    DMF_OBJECT* dmfObject;
    BOOLEAN lockHeld;

    ASSERT(DmfModule != NULL);

    dmfObject = DMF_ModuleToObject(DmfModule);

    if (dmfObject->Synchronizations[DMF_DEFAULT_LOCK_INDEX].LockHeldByThread != NULL)
    {
        lockHeld = TRUE;
    }
    else
    {
        lockHeld = FALSE;
    }

    return lockHeld;
}

BOOLEAN
DMF_ModuleLockIsPassive(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Indicates if the Module lock is a passive level lock.
    NOTE: This function is for debug purposes only.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    TRUE if the given DMF Module lock is a passive level lock.

--*/
{
    DMF_OBJECT* dmfObject;
    BOOLEAN returnValue;

    ASSERT(DmfModule != NULL);

    dmfObject = DMF_ModuleToObject(DmfModule);

    if (dmfObject->ModuleDescriptor.ModuleOptions & DMF_MODULE_OPTIONS_PASSIVE)
    {
        returnValue = TRUE;
    }
    else
    {
        returnValue = FALSE;
    }

    return returnValue;
}

BOOLEAN
DMF_IsPoolTypePassiveLevel(
    _In_ POOL_TYPE PoolType
    )
/*++

Routine Description:

    Indicates if the given pool type is passive level.
    NOTE: This function is for debug purposes only.

Arguments:

    PoolType - The given pool type.

Return Value:

    TRUE if the given pool type is passive level.

--*/
{
    BOOLEAN returnValue;

    switch (PoolType)
    {
        case NonPagedPool:
        case NonPagedPoolMustSucceed:
        case NonPagedPoolCacheAligned:
        case NonPagedPoolCacheAlignedMustS:
        case NonPagedPoolSession:
        case NonPagedPoolMustSucceedSession:
        case NonPagedPoolCacheAlignedSession:
        case NonPagedPoolCacheAlignedMustSSession:
#if (!defined(_ARM_) && !defined(_ARM64_)) || (!defined(POOL_NX_OPTIN_AUTO))
        // If POOL_NX_OPTIN_AUTO is defined, it means that NonPagedPoolNx = NonPagedPool.
        // So, don't include this case as it is a duplicate. This is the case for ARM and ARM64.
        //
        // NOTE: The condition has the OR so that the check is compatible with EWDK that does
        //       not support POOL_NX_OPTIN_AUTO.
        //
        case NonPagedPoolNx:
        case NonPagedPoolNxCacheAligned:
#endif
        case NonPagedPoolSessionNx:
        {
            returnValue = FALSE;
            break;
        }

        default:
        {
            returnValue = TRUE;
            break;
        }
    }

    return returnValue;
}
#endif // defined(DEBUG)

VOID
DMF_ModuleAuxiliaryLock(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    )
/*++

Routine Description:

    Invoke the Lock Callback for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    AuxiliaryLockIndex - Index of the auxiliary lock object

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);
    ASSERT(dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks <= DMF_MAXIMUM_AUXILIARY_LOCKS);
    ASSERT(AuxiliaryLockIndex < dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks);
    ASSERT(dmfObject->InternalCallbacksInternal.AuxiliaryLock != NULL);

    // Device lock is at 0. Auxiliary locks start from 1.
    // AuxiliaryLockIndex is 0 based.
    //
    (dmfObject->InternalCallbacksInternal.AuxiliaryLock)(DmfModule,
                                                         AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS);

    // This check is required for SAL.
    //
    if (AuxiliaryLockIndex < DMF_MAXIMUM_AUXILIARY_LOCKS)
    {
        ASSERT(NULL == dmfObject->Synchronizations[AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS].LockHeldByThread);
#if !defined(DMF_USER_MODE)
        dmfObject->Synchronizations[AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS].LockHeldByThread = PsGetCurrentThreadId();
#else
        dmfObject->Synchronizations[AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS].LockHeldByThread = GetCurrentThread();
#endif
        // Sometimes the Thread ID of the current thread is zero. In that case, use DMF_INVALID_HANDLE_VALUE.
        //
        if (dmfObject->Synchronizations[AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS].LockHeldByThread == 0)
        {
            dmfObject->Synchronizations[AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS].LockHeldByThread = DMF_INVALID_HANDLE_VALUE;
        }
    }
    else
    {
        ASSERT(FALSE);
    }
}

VOID
DMF_ModuleAuxiliaryUnlock(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    )
/*++

Routine Description:

    Invoke the Unlock Callback for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    AuxiliaryLockIndex - Index of the auxiliary lock object

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);
    ASSERT(dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks <= DMF_MAXIMUM_AUXILIARY_LOCKS);
    ASSERT(AuxiliaryLockIndex < dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks);

    // This check is required for SAL.
    //
    if (AuxiliaryLockIndex < DMF_MAXIMUM_AUXILIARY_LOCKS)
    {
        // Device lock is at 0. Auxiliary locks starts from 1.
        // AuxiliaryLockIndex is 0 based.
        //
        ASSERT(NULL != dmfObject->Synchronizations[AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS].LockHeldByThread);

        dmfObject->Synchronizations[AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS].LockHeldByThread = NULL;

        ASSERT(dmfObject->InternalCallbacksInternal.AuxiliaryUnlock != NULL);
        (dmfObject->InternalCallbacksInternal.AuxiliaryUnlock)(DmfModule,
                                                               AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS);
    }
    else
    {
        ASSERT(FALSE);
    }
}

#if defined(DEBUG)
BOOLEAN
DMF_ModuleAuxiliarySynchnonizationIsLocked(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    )
/*++

Routine Description:

    Returns the lock status of the given DMF Module.
    NOTE: This function is for debug purposes only.

Arguments:

    DmfModule - The given DMF Module.
    AuxiliaryLockIndex - Index of the auxiliary lock object

Return Value:

    TRUE if the given DMF Module is currently locked; FALSE, otherwise.

--*/
{
    BOOLEAN lockHeld;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);
    ASSERT(dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks <= DMF_MAXIMUM_AUXILIARY_LOCKS);
    ASSERT(AuxiliaryLockIndex < dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks);

    // This check is required for SAL.
    //
    if (AuxiliaryLockIndex < DMF_MAXIMUM_AUXILIARY_LOCKS)
    {
        // Device lock is at 0. Auxiliary locks starts from 1.
        // AuxiliaryLockIndex is 0 based
        //
        if (dmfObject->Synchronizations[AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS].LockHeldByThread != NULL)
        {
            lockHeld = TRUE;
        }
        else
        {
            lockHeld = FALSE;
        }
    }
    else
    {
        ASSERT(FALSE);
        lockHeld = FALSE;
    }

    return lockHeld;
}
#endif // defined(DEBUG)

NTSTATUS
DMF_Module_ResourcesAssign(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_  WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Invoke the ResourcesAssign Callback for a given DMF Module.
    First, each of the Child DMF Modules' corresponding callbacks are called.
    (Order is not particularly important.)
    Next, the given DMF Module's corresponding callback is called.

    NOTE: This callback is provided so that Modules can easily indicate that the Module's
          Open callback should be called in either PrepareHardware or D0Entry, yet still
          acquire resources provided in PrepareHardware.

Arguments:

    DmfModule - The given DMF Module.
    ResourcesRaw - WDF Resource Raw parameter that is passed to the given
                   DMF Module callback.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.

Return Value:

    STATUS_SUCCESS if all children return STATUS_SUCCESS and the given DMF Module
    does not encounter an error.
    If there is an error, then NTSTATUS code of the first child Module that encounters the
    error, or the NTSTATUS code of the given DMF Module.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* parentDmfObject;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    parentDmfObject = DMF_ModuleToObject(DmfModule);

    // Dispatch callback to Child DMF Modules first.
    //
    childDmfObject = DmfChildObjectFirstGet(parentDmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMFMODULE dmfModuleChild = DMF_ObjectToModule(childDmfObject);

        ntStatus = DMF_Module_ResourcesAssign(dmfModuleChild,
                                              ResourcesRaw,
                                              ResourcesTranslated);
        if (! NT_SUCCESS(ntStatus))
        {
            break;
        }

        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

    // Dispatch callback to the given Parent DMF Module next.
    //
    ASSERT(parentDmfObject->InternalCallbacksDmf.DeviceResourcesAssign != NULL);
    ntStatus = (parentDmfObject->InternalCallbacksDmf.DeviceResourcesAssign)(DmfModule,
                                                                             ResourcesRaw,
                                                                             ResourcesTranslated);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

Exit:

    return ntStatus;
}

// eof: DmfCall.c
//
