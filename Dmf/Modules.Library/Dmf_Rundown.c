/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Rundown.c

Abstract:
    
    This is used for rundown management when an object is being unregistered but its Methods may still 
    be called or running. It allows DMF to make sure the resource is available while Methods that are 
    already running continue running, but disallows new Methods from starting to run.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_Rundown.tmh"

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
    // Reference counter for DMF Object references.
    //
    volatile LONG ReferenceCount;
    
    // Flag indicating that the resource close is pending.
    // This is necessary to synchronize close with methods that might still be
    // using the resource.
    //
    BOOLEAN WaitingForRundown;

} DMF_CONTEXT_Rundown;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Rundown)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_NO_CONFIG(Rundown)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(DISPATCH_LEVEL)
LONG
Rundown_ReferenceAdd(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Increment the Module's Reference Count.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    The updated reference count.

--*/
{
    LONG returnValue;
    DMF_CONTEXT_Rundown* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p", DmfModule);

    // This routine must always be called in locked state.
    //
    ASSERT(DMF_ModuleIsLocked(DmfModule));

    returnValue = InterlockedIncrement(&moduleContext->ReferenceCount);

    FuncExit(DMF_TRACE, "DmfModule=0x%p returnValue=%d", DmfModule, returnValue);

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
LONG
Rundown_ReferenceDelete(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Decrement the Module's Reference Count.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    The updated reference count.

--*/
{
    LONG returnValue;
    DMF_CONTEXT_Rundown* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    FuncEntryArguments(DMF_TRACE, "DmfModule=0x%p", DmfModule);

    // This routine must always be called in locked state.
    //
    ASSERT(DMF_ModuleIsLocked(DmfModule));
    ASSERT(moduleContext->ReferenceCount > 0);

    returnValue = InterlockedDecrement(&moduleContext->ReferenceCount);

    FuncExit(DMF_TRACE, "DmfModule=0x%p returnValue=%d", DmfModule, returnValue);

    return returnValue;
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
_Must_inspect_result_
static
NTSTATUS
DMF_Rundown_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Rundown.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Rundown* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    ASSERT(0 == moduleContext->ReferenceCount);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_Rundown_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Rundown.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Rundown* moduleContext;
    BOOLEAN endForClient;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    endForClient = FALSE;

    DMF_ModuleLock(DmfModule);

    if ((! moduleContext->WaitingForRundown) &&
        (moduleContext->ReferenceCount >= 1))
    {
        // This path should be avoided. Client did not call DMF_Rundown_EndAndWait().
        //
        ASSERT(FALSE);
        endForClient = TRUE;
    }
    else
    {
        // This is the normal path that should execute.
        //
        ASSERT(0 == moduleContext->ReferenceCount);
    }

    DMF_ModuleUnlock(DmfModule);

    if (endForClient)
    {
        // Module cleans up for misbehaving Client, but this path should be avoided.
        //
        DMF_Rundown_EndAndWait(DmfModule);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Rundown_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Rundown.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Rundown;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Rundown;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Rundown);
    dmfCallbacksDmf_Rundown.DeviceOpen = DMF_Rundown_Open;
    dmfCallbacksDmf_Rundown.DeviceClose = DMF_Rundown_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Rundown,
                                            Rundown,
                                            DMF_CONTEXT_Rundown,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Rundown.CallbacksDmf = &dmfCallbacksDmf_Rundown;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Rundown,
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

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Rundown_Dereference(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Releases the Module acquired in DMF_Rundown_Reference.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Rundown* moduleContext;

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Rundown);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    Rundown_ReferenceDelete(DmfModule);
    // The reference count should never reach zero if Client is using this
    // Module's Methods correctly.
    // Start sets REference to 1.
    // Reference will always increment to 2 minimum.
    //
    ASSERT(moduleContext->ReferenceCount >= 1);

    DMF_ModuleUnlock(DmfModule);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Rundown_EndAndWait(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Waits for the Module's reference count to reach zero. This is used for rundown management 
    when an object is being unregistered but its Methods may still be called or running. 
    It allows DMF to make sure the resource is available while Methods that are 
    already running continue running, but disallows new Methods from starting to run.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Rundown* moduleContext;
    LONG referenceCount;
    // This value is chosen to give running thread time to execute,
    // but short enough to allow fast response.
    //
    const ULONG referenceCountPollingIntervalMs = 100;

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Rundown);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    // Set WaitingForRundown to TRUE, to avoid Module Method from acquiring
    // a reference to the Module infinitely and blocking the Module from closing.
    //
    moduleContext->WaitingForRundown = TRUE;
    referenceCount = moduleContext->ReferenceCount;

    // Ensure Client called DMF_Rundown_Start() before calling this Method.
    //
    ASSERT(referenceCount >= 1);

    DMF_ModuleUnlock(DmfModule);

    while (referenceCount > 0)
    {
        DMF_ModuleLock(DmfModule);

        if (referenceCount == 1)
        {
            // Module Method is not running. Prevent any Module Method from starting because call Acquire will fail.
            //
            moduleContext->ReferenceCount = 0;
            // For modules which open on notification callback, ReferenceCount = 0 means the Module is now closed.
            //
            moduleContext->WaitingForRundown = FALSE;
        }
        referenceCount = moduleContext->ReferenceCount;

        DMF_ModuleUnlock(DmfModule);

        if (referenceCount == 0)
        {
            break;
        }

        // Reference count > 1 means a Module Method is running.
        // Wait for Reference count to run down to 0.
        //
        DMF_Utility_DelayMilliseconds(referenceCountPollingIntervalMs);
        TraceInformation(DMF_TRACE, "DmfModule=0x%p Waiting to close", DmfModule);
    }

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Rundown_Reference(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Can be wrapped around a resource to make sure it exists until Rundown_Dereference
    method is called.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Rundown* moduleContext;

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Rundown);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    // Client must call DMF_Rundown_Start() before calling this Method.
    //
    ASSERT(moduleContext->ReferenceCount >= 1);

    // Increase reference only if Module is open (ReferenceCount >= 1) and if the Module close is not pending.
    // This is to stop new Module method callers from repeatedly accessing the Module when it should be closing.
    //
    if ((moduleContext->ReferenceCount >= 1) &&
        (! moduleContext->WaitingForRundown))
    {
        // Increase reference count to ensure that Module will not be closed while a Module method is running.
        //
        Rundown_ReferenceAdd(DmfModule);
        ntStatus = STATUS_SUCCESS;
    }
    else
    {
        // Tell caller that this Module has not started and that Module Method should not do anything.
        //
        ntStatus = STATUS_INVALID_DEVICE_STATE;
    }

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Rundown_Start(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description: 

    Used to set initial reference count at the start of the Rundown lifetime to 1.
    Needs to be called by the Client before the Rundown Reference and Dereference use. 

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Rundown* moduleContext;

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Rundown);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    // This is a reference that will be dereferenced in DMF_Rundown_EndAndWait().
    //
    moduleContext->WaitingForRundown = FALSE;
    moduleContext->ReferenceCount = 1;

    DMF_ModuleUnlock(DmfModule);

    FuncExitVoid(DMF_TRACE);
}

// eof: Dmf_Rundown.c
//
