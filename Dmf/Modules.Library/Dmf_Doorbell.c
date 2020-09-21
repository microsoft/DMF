/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Doorbell.c

Abstract:

    Supports client to synchronize the registered callback by ensuring
    to have one Workitem enqueue/running at a time which invokes the client's
    registered callback by tracking the doorbell ring.
    It doesn't matter how many times the client rings the doorbell,
    this Module will invoke the client's registered callbacks once before
    it clears the tracking. And double check the tracking for any doorbell ring
    before completes its Work Item, to ensure not to miss any doorbell ring.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//

#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_Doorbell.tmh"

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
    // This flag tracks the Doorbell ring.
    // If it is TRUE:
    //      The request has been submitted and it is not processed.
    // If it is FALSE:
    //      No pending request.
    //
    BOOLEAN TrackDoorbellRing;

    // This flag tracks the Scheduled workitem.
    // It is used to ensure that the workitem is scheduled only one time
    // even though the Client has rung the doorbell more than one time.
    // This ensures that multiple workitems do not run in parallel.
    //
    BOOLEAN WorkItemScehduled;

    // Workitem.
    //
    WDFWORKITEM Workitem;
} DMF_CONTEXT_Doorbell;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Doorbell)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(Doorbell)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Declarations
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

EVT_WDF_WORKITEM Doorbell_WorkitemHandler;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_Function_class_(EVT_WDF_WORKITEM)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Doorbell_WorkitemHandler(
    _In_ WDFWORKITEM Workitem
    )
/*++

Routine Description:

    Workitem handler.

Arguments:

    Workitem - WDFORKITEM which gives access to necessary context including this
               Module's DMF Module.

Return Value:

    VOID

--*/
{
    DMFMODULE dmfModule;
    DMF_CONFIG_Doorbell* moduleConfig;
    DMF_CONTEXT_Doorbell* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    dmfModule = (DMFMODULE)WdfWorkItemGetParentObject(Workitem);
    moduleConfig = DMF_CONFIG_GET(dmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    DMF_ModuleLock(dmfModule);

    while (1)
    {

        moduleContext->TrackDoorbellRing = FALSE;

        DMF_ModuleUnlock(dmfModule);

        // dmfModule is the DMF_Doorbell Module handle.
        //
        moduleConfig->WorkItemCallback(dmfModule);

        DMF_ModuleLock(dmfModule);

        if (moduleContext->TrackDoorbellRing)
        {
            // Doorbell got rung again after we dropped the lock.
            //
            continue;
        }

        // Here, current instance of the workitem will no longer
        // handle any more doorbells, thus set WorkItemScehduled to FALSE.
        //
        moduleContext->WorkItemScehduled = FALSE;
        DMF_ModuleUnlock(dmfModule);
        break;

    }

    FuncExitVoid(DMF_TRACE);
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
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Doorbell_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type RequestTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Doorbell *moduleContext;
    WDF_WORKITEM_CONFIG workItemConfiguration;
    WDF_OBJECT_ATTRIBUTES workItemAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Module Opening");

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Create the Passive Level Workitem.
    //
    WDF_WORKITEM_CONFIG_INIT(&workItemConfiguration, Doorbell_WorkitemHandler);
    workItemConfiguration.AutomaticSerialization = WdfFalse;
    
    WDF_OBJECT_ATTRIBUTES_INIT(&workItemAttributes);
    workItemAttributes.ParentObject = DmfModule;

    ntStatus = WdfWorkItemCreate(&workItemConfiguration,
                                 &workItemAttributes,
                                 &moduleContext->Workitem);

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfWorkItemCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

Exit:
    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_Doorbell_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type RequestTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    VOID

--*/
{
    DMF_CONTEXT_Doorbell *moduleContext;
    
    PAGED_CODE();
    
    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Module Closing");

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Wait for pending work to finish.
    //
    WdfWorkItemFlush(moduleContext->Workitem);
    WdfObjectDelete(moduleContext->Workitem);
    moduleContext->Workitem = NULL;

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
DMF_Doorbell_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Doorbell.

Arguments:

    Device - Client's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Doorbell;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Doorbell;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Doorbell);
    dmfCallbacksDmf_Doorbell.DeviceOpen = DMF_Doorbell_Open;
    dmfCallbacksDmf_Doorbell.DeviceClose = DMF_Doorbell_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Doorbell,
                                            Doorbell,
                                            DMF_CONTEXT_Doorbell,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Doorbell.CallbacksDmf = &dmfCallbacksDmf_Doorbell;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Doorbell,
                                DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);
    
    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Doorbell_Flush(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Flush WotkItem Single.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    VOID

--*/
{
    DMF_CONTEXT_Doorbell* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Wait for pending work to finish.
    //
    WdfWorkItemFlush(moduleContext->Workitem);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Doorbell_Ring(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Rings the Doorbell and the work item is enqueued if it is not already.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    VOID

--*/
{
    DMF_CONTEXT_Doorbell* moduleContext;
    BOOLEAN enqueueWorkitem = FALSE;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    if (moduleContext->WorkItemScehduled == FALSE)
    {
        enqueueWorkitem = TRUE;
    }

    moduleContext->TrackDoorbellRing = TRUE;
    moduleContext->WorkItemScehduled = TRUE;

    DMF_ModuleUnlock(DmfModule);

    if (enqueueWorkitem)
    {
        WdfWorkItemEnqueue(moduleContext->Workitem);
    }

    FuncExitVoid(DMF_TRACE);
}

// eof: Dmf_Doorbell.c
//