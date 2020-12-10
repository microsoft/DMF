/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_NotifyUserWithRequestMultiple.c

Abstract:

    This Module provides every Client with a unique NotifyUserWithRequest Module for
    buffer consistency.

Environment:

    Kernel-mode
    User-mode

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_NotifyUserWithRequestMultiple.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_NotifyUserWithRequestMultiple
{
    // If ModeType is ReplayLastMessageToNewClients, this stores latest buffer.
    //
    VOID* CachedBuffer;
    // Used to handle cases where no cached buffer is present.
    //
    BOOLEAN BufferAvailable;
    // List containing Clients to be added to ListHead.
    //
    LIST_ENTRY PendingAddListHead;
    // List containing Clients to be removed from ListHead.
    //
    LIST_ENTRY PendingRemoveListHead;
    // List containing Clients that will receive broadcast
    // data.
    //
    LIST_ENTRY ListHead;
    // Handle to DMF Doorbell Module.
    //
    DMFMODULE DmfModuleDoorbell;
    // Handle to DMF BufferQueue Module.
    //
    DMFMODULE DmfModuleBufferQueueProcessing;
    // Handle to DMF BufferQueue Module for FileObject mapping.
    //
    DMFMODULE DmfBufferQueueFileContextPool;
    // Size of BufferQueue's buffer.
    //
    ULONG BufferQueueBufferSize;
    // Keep track of any failed requests to Broadcast method.
    //
    ULONG FailureCountDuringBroadcast;
    // Keep track of FileCreate failing.
    //
    ULONG FailureCountDuringFileCreate;
    // Keep track of RequestProcess failing.
    //
    ULONG FailureCountDuringRequestProcess;
} DMF_CONTEXT_NotifyUserWithRequestMultiple;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(NotifyUserWithRequestMultiple)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(NotifyUserWithRequestMultiple)

// Memory Pool Tag.
//
#define MemoryTag 'MRUN' // NURM

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Default buffer count for DmfModuleBufferQueueProcessing.
//
#define BUFFER_QUEUE_PROCESSING_COUNT 15

// Default buffer count for DmfModuleBufferQueueFileObject.
//
#define BUFFER_QUEUE_FILE_OBJECT_COUNT 8

// Context passed to BufferQueue.
//
typedef struct
{
    // NtStatus to be passed to DataProcess().
    //
    NTSTATUS NtStatus;
    // Callback Context to be passed to DataProcess().
    //
    UCHAR DataBuffer[1];
} NotifyUserWithRequestMultiple_BufferQueueBufferType;

// This context is associated with FileObject and added to BufferQueueFileContextPool.
// Since a client could be using multiple instance of this module, a WDFFILEOBJECT
// may have multiple instance of this context, one for each instance of 
// DMF_NotifyUserWithRequestMultiple module.
//
typedef struct _FILE_OBJECT_CONTEXT
{
    // Handle to NotifyUserWithRequest Module.
    //
    DMFMODULE DmfModuleNotifyUserWithRequest;
    // List structure to be added to PendingAddListHead in Context.
    //
    LIST_ENTRY PendingListEntryAdd;
    // List structure to be added to PendingRemoveListHead in Context.
    //
    LIST_ENTRY PendingListEntryRemove;
    // List entry to be added to ListHead in Context.
    //
    LIST_ENTRY ProcessingListEntry;
    // Handle to the FileObject of this User.
    //
    WDFFILEOBJECT FileObject;
    // Keeps track of the membership of this User in the main list.
    //
    BOOLEAN AddedToBroadcastList;
} FILE_OBJECT_CONTEXT;

// This is a structure that is used to enumerate all the dynamically allocated File contexts. 
// It is initialized by the caller and passed to Dmf_BufferQueue_Enumerate.
//
typedef struct _ENUMERATION_CONTEXT
{
    // If set to TRUE, Buffer will be removed from buffer pool if found during enumeration.
    // This is to be set by the caller before calling Dmf_BufferQueue_Enumerate.
    //
    BOOLEAN RemoveBuffer;
    // This is input search criteria for the enumeration. This is set by the caller before calling 
    // Dmf_BufferQueue_Enumerate.
    //
    WDFFILEOBJECT FileObjectToFind;
    // The following member must be initialized to NULL by the caller before calling Dmf_BufferQueue_Enumerate
    // The enumeration callback sets this member if an entry matching FileObject is found. 
    //
    FILE_OBJECT_CONTEXT* FileObjectContext;
} ENUMERATION_CONTEXT;

_Function_class_(EVT_DMF_BufferPool_Enumeration)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BufferPool_EnumerationDispositionType
NotifyUserWithRequestMultiple_FindFileContext(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext,
    _In_opt_ VOID* ClientDriverCallbackContext
    )
/*++

Routine Description:

    Enumeration callback to check if a given FileObject is in the pool.
    Returns FileObjectContext associated with the FileObject
    if it is found. Can also remove entries if specified in 
    ClientDriverCallbackContext.

Arguments:

    DmfModule -  DMFMODULE.
    ClientBuffer - The given input buffer.
    ClientBufferContext - Context associated with the given input buffer.
    ClientDriverCallbackContext - Context for this enumeration.

Return Value:

    BufferPool_EnumerationDispositionType - Lets BufferQueue_Enumerate know
        what actions to take with a buffer processed by this callback.

--*/
{
    ENUMERATION_CONTEXT* callbackContext;
    FILE_OBJECT_CONTEXT* fileObjectContext;
    BufferPool_EnumerationDispositionType returnValue;

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ClientBufferContext);

    fileObjectContext = (FILE_OBJECT_CONTEXT*)ClientBuffer;

    callbackContext = (ENUMERATION_CONTEXT*)ClientDriverCallbackContext;
    // 'Dereferencing NULL pointer. 'callbackContext'
    //
    #pragma warning(suppress:28182)
    __analysis_assume(ClientDriverCallbackContext != NULL);

    returnValue = BufferPool_EnumerationDisposition_ContinueEnumeration;

    if (fileObjectContext->FileObject == callbackContext->FileObjectToFind)
    {
        callbackContext->FileObjectContext = fileObjectContext;
        if (callbackContext->RemoveBuffer)
        {
            // This happens during FileClose callback.
            //
            returnValue = BufferPool_EnumerationDisposition_RemoveAndStopEnumeration;
        }
        else
        {
            // This happens during RequestProcess Method.
            //
            returnValue = BufferPool_EnumerationDisposition_StopEnumeration;
        }
    }

    FuncExit(DMF_TRACE, "Enumeration Disposition=%d", returnValue);

    return returnValue;
}

NTSTATUS
NotifyUserWithRequestMultiple_AllocateDynamicFileObjectContext(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject,
    _Out_ FILE_OBJECT_CONTEXT** FileObjectContext
    )
/*++

Routine Description:

    Creates a NotifyUserWithRequest Module and initializes and returns
    FileObjectContext.

Arguments:

    DmfModule -  DMFMODULE.
    FileObject - WDF file object that describes a file that is associated with a request.
    FileContext - Context associated with the given FileObject.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_NotifyUserWithRequestMultiple* moduleContext;
    DMF_CONFIG_NotifyUserWithRequestMultiple* moduleConfig;
    NTSTATUS ntStatus;
    UCHAR* clientBuffer;
    VOID* clientBufferContext;
    FILE_OBJECT_CONTEXT* fileObjectContext;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    
    PAGED_CODE();
    
    FuncEntry(DMF_TRACE);
    
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    device = DMF_ParentDeviceGet(DmfModule);

    *FileObjectContext = NULL;

    // Create DMF Module NotifyUserWithRequest
    // ---------------------------------------
    //
    DMFMODULE dmfModuleNotifyUserWithRequest; 
    DMF_CONFIG_NotifyUserWithRequest moduleConfigNotifyUserWithRequest;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = DmfModule;

    DMF_CONFIG_NotifyUserWithRequest_AND_ATTRIBUTES_INIT(&moduleConfigNotifyUserWithRequest,
                                                         &moduleAttributes);

    moduleConfigNotifyUserWithRequest.MaximumNumberOfPendingRequests = moduleConfig->MaximumNumberOfPendingRequests;
    moduleConfigNotifyUserWithRequest.MaximumNumberOfPendingDataBuffers = moduleConfig->MaximumNumberOfPendingDataBuffers;
    moduleConfigNotifyUserWithRequest.SizeOfDataBuffer = moduleConfig->SizeOfDataBuffer;
    ntStatus = DMF_NotifyUserWithRequest_Create(device,
                                                &moduleAttributes,
                                                &attributes,
                                                &dmfModuleNotifyUserWithRequest);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_NotifyUserWithRequest_Create fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Initialize BufferQueueFileObject.
    //
    ntStatus = DMF_BufferQueue_Fetch(moduleContext->DmfBufferQueueFileContextPool,
                                     (VOID**)&clientBuffer,
                                     &clientBufferContext);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BufferQueue_Fetch fails: ntStatus=%!STATUS!", ntStatus);
        // Destroy the Dmf NotifyUserWithRequest Module.
        //
        WdfObjectDelete(dmfModuleNotifyUserWithRequest);
        goto Exit;
    }

    // Map the Client buffer for ease of access.
    //
    fileObjectContext = (FILE_OBJECT_CONTEXT*)clientBuffer;
    // Initialize Client buffer.
    //
    RtlZeroMemory(fileObjectContext,
                  sizeof(FILE_OBJECT_CONTEXT));
    fileObjectContext->DmfModuleNotifyUserWithRequest = dmfModuleNotifyUserWithRequest;
    fileObjectContext->FileObject = FileObject;
    fileObjectContext->AddedToBroadcastList = FALSE;
    InitializeListHead(&fileObjectContext->ProcessingListEntry);
    InitializeListHead(&fileObjectContext->PendingListEntryAdd);
    InitializeListHead(&fileObjectContext->PendingListEntryRemove);

    // Add to pending work list.
    //
    DMF_BufferQueue_Enqueue(moduleContext->DmfBufferQueueFileContextPool,
                            clientBuffer);

    // Set fileObjectContext to be returned.
    //
    *FileObjectContext = fileObjectContext;

Exit:

    FuncExit(DMF_TRACE,"ntStatus=%!STATUS!",ntStatus);

    return ntStatus;
}

VOID
NotifyUserWithRequestMultiple_DeleteDynamicFileObjectContext(
    _In_ DMFMODULE DmfModule,
    _In_ FILE_OBJECT_CONTEXT* FileContext
    )
/*++

Routine Description:

    Removes FileContext from the DmfBufferQueueFileContextPool consumer
    list and puts it back in producer list. Deletes the NotifyUserWithRequest
    Module.

Arguments:

    DmfModule -  DMFMODULE.
    FileContext - Context associated with the given FileObject.

Return Value:

    None.

--*/
{
    DMF_CONTEXT_NotifyUserWithRequestMultiple* moduleContext;
    ENUMERATION_CONTEXT callbackContext;
    FILE_OBJECT_CONTEXT* fileObjectContext;
    
    PAGED_CODE();
    
    FuncEntry(DMF_TRACE);
    
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Destroy the Dmf NotifyUserWithRequest Module.
    //
    WdfObjectDelete(FileContext->DmfModuleNotifyUserWithRequest);
    FileContext->DmfModuleNotifyUserWithRequest = NULL;
    
    // Set up context for BufferQueue enumerate.
    //
    RtlZeroMemory(&callbackContext,
                  sizeof(ENUMERATION_CONTEXT));
    callbackContext.FileObjectToFind = FileContext->FileObject;
    callbackContext.RemoveBuffer = TRUE;
    
    // Find and remove this context from the BufferQueue.
    //
    DMF_BufferQueue_Enumerate(moduleContext->DmfBufferQueueFileContextPool,
                              NotifyUserWithRequestMultiple_FindFileContext,
                              (VOID *)&callbackContext,
                              (VOID **)&fileObjectContext,
                              NULL);

    DmfAssert(callbackContext.FileObjectContext != NULL);
    // Put this buffer back into producer list.
    //
    DMF_BufferQueue_Reuse(moduleContext->DmfBufferQueueFileContextPool,
                          callbackContext.FileObjectContext);

    FuncExitVoid(DMF_TRACE);

}

FILE_OBJECT_CONTEXT*
NotifyUserWithRequestMultiple_GetDynamicFileObjectContext(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Creates a NotifyUserWithRequest Module and initializes and returns
    FileObjectContext.

Arguments:

    DmfModule -  DMFMODULE.
    FileObject - WDF file object that describes a file that is associated with a request.

Return Value:

    FILE_OBJECT_CONTEXT* - Context associated with the FileObject. Can be NULL if context
        is not found.

--*/
{
    DMF_CONTEXT_NotifyUserWithRequestMultiple* moduleContext;
    ENUMERATION_CONTEXT callbackContext;
    
    PAGED_CODE();
    
    FuncEntry(DMF_TRACE);
    
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    
    // Set up context for BufferQueue enumerate.
    //
    RtlZeroMemory(&callbackContext,
                  sizeof(ENUMERATION_CONTEXT));
    callbackContext.FileObjectToFind = FileObject;
    callbackContext.RemoveBuffer = FALSE;
    
    // Find this context from the BufferQueue.
    //
    DMF_BufferQueue_Enumerate(moduleContext->DmfBufferQueueFileContextPool,
                              NotifyUserWithRequestMultiple_FindFileContext,
                              (VOID *)&callbackContext,
                              NULL,
                              NULL);

    FuncExit(DMF_TRACE,"FileObjectContext=%p",callbackContext.FileObjectContext);

    return callbackContext.FileObjectContext;
}

_Function_class_(EVT_DMF_Doorbell_ClientCallback)
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
NotifyUserWithRequestMultiple_DoorbellCallback(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Processes PendingAdd, PendingRemove and ListHead lists.

Arguments:

    DmfModule - Handle to DMF Doorbell Module.

Return Value:

    None.

--*/
{
    DMFMODULE dmfModuleNotifyUserWithRequestMultiple;
    DMF_CONTEXT_NotifyUserWithRequestMultiple* moduleContext;
    DMF_CONFIG_NotifyUserWithRequestMultiple* moduleConfig;
    FILE_OBJECT_CONTEXT* fileObjectContext;
    FILE_OBJECT_CONTEXT* fileObjectContextNext;
    UCHAR* clientBuffer;
    NTSTATUS ntStatus;
    VOID* clientBufferContext;
    LIST_ENTRY listToAdd;
    LIST_ENTRY listToRemove;

    FuncEntry(DMF_TRACE);

    dmfModuleNotifyUserWithRequestMultiple = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleNotifyUserWithRequestMultiple);
    moduleConfig = DMF_CONFIG_GET(dmfModuleNotifyUserWithRequestMultiple);

    // Transfer PendingAdd and PendingRemove list heads to local list heads.
    //
    DMF_ModuleLock(dmfModuleNotifyUserWithRequestMultiple);
    DMF_Utility_TransferList(&listToAdd,
                             &moduleContext->PendingAddListHead);
    DMF_Utility_TransferList(&listToRemove,
                             &moduleContext->PendingRemoveListHead);
    DMF_ModuleUnlock(dmfModuleNotifyUserWithRequestMultiple);

    // 1. Add new Clients to the ListHead from PendingAdd list.
    // --------------------------------------------------------
    //
    // Iterate through listToAdd until head is reached.
    //
    DMF_Utility_FOR_ALL_IN_LIST_SAFE(FILE_OBJECT_CONTEXT,
                                     &listToAdd,
                                     PendingListEntryAdd,
                                     fileObjectContext,
                                     fileObjectContextNext)
    {

        // Remove from PendingAdd list.
        //
        RemoveEntryList(&fileObjectContext->PendingListEntryAdd);
        InitializeListHead(&fileObjectContext->PendingListEntryAdd);

        // If an Arrival Callback is registered, invoke it. Based on the resulting
        // NTSTATUS this FileObject's context can be freed.
        //
        if (moduleConfig->EvtClientArrivalCallback != NULL)
        {
            // Callback registered by Client for Data/Request processing
            // upon Client arrival.
            //
            ntStatus = moduleConfig->EvtClientArrivalCallback(DmfModule,
                                                              fileObjectContext->FileObject);
            if (!NT_SUCCESS(ntStatus))
            {
                // Client chose to not add this User by sending status != success.
                //
                TraceEvents(TRACE_LEVEL_INFORMATION,
                            DMF_TRACE,
                            "Client failed ArrivalCallback: FileObject=%p ntStatus=%!STATUS!",
                            fileObjectContext->FileObject,
                            ntStatus);
                ntStatus = STATUS_SUCCESS;

                // Uninitialize and remove from DmfBufferQueueFileContextPool.
                //
                NotifyUserWithRequestMultiple_DeleteDynamicFileObjectContext(dmfModuleNotifyUserWithRequestMultiple,
                                                                             fileObjectContext);
                // No further operations in the Add routine will take place.
                //
                continue;
            }
        }

        // If mode is set to ReplayLastMessageToNewClients, fill this User's buffer with latest cached data.
        //
        if ((moduleConfig->ModeType.Modes.ReplayLastMessageToNewClients == 1) &&
            (moduleContext->BufferAvailable == TRUE))
        {
            NotifyUserWithRequestMultiple_BufferQueueBufferType* bufferQueueContext;

            // Map the Cached buffer for ease of access.
            //
            bufferQueueContext = (NotifyUserWithRequestMultiple_BufferQueueBufferType*)moduleContext->CachedBuffer;
            // Process data to service first request from Client.
            //
            DMF_NotifyUserWithRequest_DataProcess(fileObjectContext->DmfModuleNotifyUserWithRequest,
                                                  moduleConfig->CompletionCallback,
                                                  bufferQueueContext->DataBuffer,
                                                  bufferQueueContext->NtStatus);
        }

        // Add this User to the ListHead.
        //
        InsertTailList(&moduleContext->ListHead,
                       &fileObjectContext->ProcessingListEntry);

        // Update the User's fileContext to reflect this.
        //
        fileObjectContext->AddedToBroadcastList = TRUE;

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Client Added. FileObject=%p",fileObjectContext->FileObject);
    }

    // 2. Remove Clients in PendingRemove list from ListHead.
    // ------------------------------------------------------
    //
    // Iterate through listToRemove until head is reached.
    //
    DMF_Utility_FOR_ALL_IN_LIST_SAFE(FILE_OBJECT_CONTEXT,
                                     &listToRemove,
                                     PendingListEntryRemove,
                                     fileObjectContext,
                                     fileObjectContextNext)
    {
        if (fileObjectContext->AddedToBroadcastList)
        {
            // Callback registered by Client for Data/Request processing
            // upon Client removal.
            //
            if (moduleConfig->EvtClientDepartureCallback != NULL)
            {
                moduleConfig->EvtClientDepartureCallback(DmfModule,
                                                         fileObjectContext->FileObject);
            }

            // Remove from main list.
            //
            RemoveEntryList(&fileObjectContext->ProcessingListEntry);
            InitializeListHead(&fileObjectContext->ProcessingListEntry);

            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Client Removed. FileObject=%p",fileObjectContext->FileObject);
        }

        // Remove from listToRemove.
        //
        RemoveEntryList(&fileObjectContext->PendingListEntryRemove);
        InitializeListHead(&fileObjectContext->PendingListEntryRemove);

        // Dereference FileObject.
        //
        WdfObjectDereference(fileObjectContext->FileObject);

        // Uninitialize and remove from DmfBufferQueueFileContextPool.
        //
        NotifyUserWithRequestMultiple_DeleteDynamicFileObjectContext(dmfModuleNotifyUserWithRequestMultiple,
                                                                     fileObjectContext);
    }

    // 3. Broadcast data to the Clients in the ListHead list.
    // ------------------------------------------------------
    //
    // Dequeue the next buffer. Repeat until no buffer is available.
    //
    ntStatus = DMF_BufferQueue_Dequeue(moduleContext->DmfModuleBufferQueueProcessing,
                                       (VOID**)&clientBuffer,
                                       &clientBufferContext);
    while (NT_SUCCESS(ntStatus))
    {
        // Keep an updated copy of Client's Buffer if the mode is set to
        // ReplayLastMessageToNewClients.
        //
        if (moduleConfig->ModeType.Modes.ReplayLastMessageToNewClients == 1)
        {
            moduleContext->BufferAvailable = TRUE;
            RtlCopyMemory(moduleContext->CachedBuffer,
                          clientBuffer,
                          moduleContext->BufferQueueBufferSize);
        }

        // Iterate through ListHead until head is reached.
        //
        DMF_Utility_FOR_ALL_IN_LIST(FILE_OBJECT_CONTEXT,
                                    &moduleContext->ListHead,
                                    ProcessingListEntry,
                                    fileObjectContext)
        {
            NotifyUserWithRequestMultiple_BufferQueueBufferType* bufferQueueContext;

            // Map the Client buffer for ease of access.
            //
            bufferQueueContext = (NotifyUserWithRequestMultiple_BufferQueueBufferType*)clientBuffer;

            // Send data to this Client's NotifyUserWithRequest.
            //
            DMF_NotifyUserWithRequest_DataProcess(fileObjectContext->DmfModuleNotifyUserWithRequest,
                                                  moduleConfig->CompletionCallback,
                                                  bufferQueueContext->DataBuffer,
                                                  bufferQueueContext->NtStatus);
        }

        // Add the used client buffer back to empty buffer list.
        //
        DMF_BufferQueue_Reuse(moduleContext->DmfModuleBufferQueueProcessing,
                              clientBuffer);

        // Dequeue the next buffer.
        //
        ntStatus = DMF_BufferQueue_Dequeue(moduleContext->DmfModuleBufferQueueProcessing,
                                           (VOID**)&clientBuffer,
                                           &clientBufferContext);
    }
    // Setting ntStatus to success because unsuccessful status is expected and okay. 
    //
    ntStatus = STATUS_SUCCESS;

    FuncExitVoid(DMF_TRACE);

}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_ModuleFileCreate)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
BOOLEAN
DMF_NotifyUserWithRequestMultiple_ModuleFileCreate(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    NotifyUserWithRequestMultiple callback for ModuleFileCreate for a given DMF Module.
    Used to provide every caller their own context and NotifyUserWithRequest Modules.

Arguments:

    DmfModule - This Module's handle.
    Device - WDF device object.
    Request - WDF Request with IOCTL parameters.
    FileObject - WDF file object that describes a file that is being opened for the specified request.

Return Value:

    Return FALSE to indicate that this Module does not support File Create.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_NotifyUserWithRequestMultiple* moduleContext;
    BOOLEAN returnValue;
    FILE_OBJECT_CONTEXT* fileObjectContext;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Device);

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    returnValue = FALSE;
    fileObjectContext = NULL;

    ntStatus = NotifyUserWithRequestMultiple_AllocateDynamicFileObjectContext(DmfModule,
                                                                              FileObject,
                                                                              &fileObjectContext);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "NotifyUserWithRequestMultiple_AllocateDynamicFileObject fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    // Add the Client in PendingAddListHead so it can be transferred
    // to the main list.
    //
    DMF_ModuleLock(DmfModule);
    InsertTailList(&moduleContext->PendingAddListHead,
                   &fileObjectContext->PendingListEntryAdd);
    DMF_ModuleUnlock(DmfModule);

    // Ring the doorbell.
    //
    DMF_Doorbell_Ring(moduleContext->DmfModuleDoorbell);

Exit:

    if (!NT_SUCCESS(ntStatus))
    {
        moduleContext->FailureCountDuringFileCreate++;
    }

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ModuleFileClose)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
BOOLEAN
DMF_NotifyUserWithRequestMultiple_ModuleFileClose(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    NotifyUserWithRequestMultiple callback for ModuleFileClose for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    FileObject - WDF file object

Return Value:

    BOOLEAN

--*/
{
    BOOLEAN returnValue;
    DMF_CONTEXT_NotifyUserWithRequestMultiple* moduleContext;
    FILE_OBJECT_CONTEXT* fileContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    
    fileContext = NotifyUserWithRequestMultiple_GetDynamicFileObjectContext(DmfModule,
                                                                            FileObject);
    if (fileContext == NULL)
    {
        // This can happen if there was a failure during FileCreate. 
        //
        goto Exit;
    }
    
    // Add the Client in to PendingRemoveListHead so it can be removed
    // from the main list.
    //
    DMF_ModuleLock(DmfModule);
    // Increase FileObject's reference so it is not freed before being
    // processed by Doorbell callback.
    //
    WdfObjectReferenceWithTag(FileObject,
                              (VOID*)DmfModule);
    InsertTailList(&moduleContext->PendingRemoveListHead,
                   &fileContext->PendingListEntryRemove);
    DMF_ModuleUnlock(DmfModule);

    // Ring the doorbell.
    //
    DMF_Doorbell_Ring(moduleContext->DmfModuleDoorbell);

Exit: 
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
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_NotifyUserWithRequestMultiple_ChildModulesAdd(
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
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONTEXT_NotifyUserWithRequestMultiple* moduleContext;
    DMF_CONFIG_NotifyUserWithRequestMultiple* moduleConfig;
    DMF_CONFIG_Doorbell doorbellConfig;
    DMF_CONFIG_BufferQueue bufferQueueConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Doorbell
    // --------
    //
    DMF_CONFIG_Doorbell_AND_ATTRIBUTES_INIT(&doorbellConfig,
                                            &moduleAttributes);
    doorbellConfig.WorkItemCallback = NotifyUserWithRequestMultiple_DoorbellCallback;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleDoorbell);

    // BufferQueue
    // -----------
    //
    DMF_CONFIG_BufferQueue_AND_ATTRIBUTES_INIT(&bufferQueueConfig,
                                               &moduleAttributes);
    bufferQueueConfig.SourceSettings.EnableLookAside = TRUE;
    bufferQueueConfig.SourceSettings.BufferCount = BUFFER_QUEUE_PROCESSING_COUNT;
    bufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
    bufferQueueConfig.SourceSettings.BufferContextSize = 0;
    bufferQueueConfig.SourceSettings.BufferSize = moduleConfig->SizeOfDataBuffer + sizeof(NTSTATUS);
    moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferQueueProcessing);

    // BufferQueue
    // -----------
    //
    DMF_CONFIG_BufferQueue_AND_ATTRIBUTES_INIT(&bufferQueueConfig,
                                               &moduleAttributes);
    bufferQueueConfig.SourceSettings.EnableLookAside = TRUE;
    bufferQueueConfig.SourceSettings.BufferCount = BUFFER_QUEUE_FILE_OBJECT_COUNT;
    bufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
    bufferQueueConfig.SourceSettings.BufferContextSize = 0;
    bufferQueueConfig.SourceSettings.BufferSize = sizeof(FILE_OBJECT_CONTEXT);
    moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfBufferQueueFileContextPool);

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
DMF_NotifyUserWithRequestMultiple_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type NotifyUserWithRequestMultiple.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_NotifyUserWithRequestMultiple;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_NotifyUserWithRequestMultiple;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_NotifyUserWithRequestMultiple;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_NotifyUserWithRequestMultiple);
    dmfCallbacksDmf_NotifyUserWithRequestMultiple.ChildModulesAdd = DMF_NotifyUserWithRequestMultiple_ChildModulesAdd;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_NotifyUserWithRequestMultiple);
    dmfCallbacksWdf_NotifyUserWithRequestMultiple.ModuleFileCreate = DMF_NotifyUserWithRequestMultiple_ModuleFileCreate;
    dmfCallbacksWdf_NotifyUserWithRequestMultiple.ModuleFileClose = DMF_NotifyUserWithRequestMultiple_ModuleFileClose;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_NotifyUserWithRequestMultiple,
                                            NotifyUserWithRequestMultiple,
                                            DMF_CONTEXT_NotifyUserWithRequestMultiple,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_NotifyUserWithRequestMultiple.CallbacksDmf = &dmfCallbacksDmf_NotifyUserWithRequestMultiple;
    dmfModuleDescriptor_NotifyUserWithRequestMultiple.CallbacksWdf = &dmfCallbacksWdf_NotifyUserWithRequestMultiple;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_NotifyUserWithRequestMultiple,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DMF_ModuleCreate fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    // Allocate resources for the lifetime of this Module instance.
    // ------------------------------------------------------------
    //
    DMF_CONTEXT_NotifyUserWithRequestMultiple* moduleContext;
    DMF_CONFIG_NotifyUserWithRequestMultiple* moduleConfig;
    WDFMEMORY CachedBufferMemory;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    moduleContext = DMF_CONTEXT_GET(*DmfModule);
    moduleConfig = DMF_CONFIG_GET(*DmfModule);

    // Initialize Context.
    //
    moduleContext->BufferAvailable = FALSE;
    moduleContext->CachedBuffer = NULL;
    moduleContext->FailureCountDuringBroadcast = 0;
    moduleContext->FailureCountDuringFileCreate = 0;
    InitializeListHead(&moduleContext->ListHead);
    InitializeListHead(&moduleContext->PendingAddListHead);
    InitializeListHead(&moduleContext->PendingRemoveListHead);
    
    // Every buffer contains a ClientContext and NtStatus.
    //
    moduleContext->BufferQueueBufferSize = moduleConfig->SizeOfDataBuffer + sizeof(NTSTATUS);

    // If Client has specified ReplayLastMessageToNewClients, allocate buffers.
    //
    if (moduleConfig->ModeType.Modes.ReplayLastMessageToNewClients == 1)
    {
        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        objectAttributes.ParentObject = *DmfModule;
        ntStatus = WdfMemoryCreate(&objectAttributes,
                                   NonPagedPoolNx,
                                   MemoryTag,
                                   moduleContext->BufferQueueBufferSize,
                                   &CachedBufferMemory,
                                   (VOID**)&moduleContext->CachedBuffer);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "WdfMemoryCreate for CachedBuffer fails: ntStatus=%!STATUS!",  
                        ntStatus);
            goto Exit;
        }
    }

    // ------------------------------------------------------------
    //

Exit:

    FuncExit(DMF_TRACE,
             "ntStatus=%!STATUS!",
             ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_NotifyUserWithRequestMultiple_DataBroadcast(
    _In_ DMFMODULE DmfModule,
    _In_reads_(DataBufferSize) VOID* DataBuffer,
    _In_ size_t DataBufferSize,
    _In_ NTSTATUS NtStatus
    )
/*++

Routine Description:

    Broadcasts data to all NotifyUserWithRequest Modules corresponding to the number of Client connections.

Arguments:

    DmfModule - This Module's handle.
    DataBuffer - Data to be passed to CompletionCallback.
    DataBufferSize - Size of Data buffer.
    NtStatus - Status associated with the User-mode event.

Return Value:

    NTSTATUS

++*/
{
    DMF_CONTEXT_NotifyUserWithRequestMultiple* moduleContext;
    DMF_CONFIG_NotifyUserWithRequestMultiple* moduleConfig;
    NotifyUserWithRequestMultiple_BufferQueueBufferType* bufferQueueContext;
    NTSTATUS ntStatus;
    UCHAR* clientBuffer;
    VOID* clientBufferContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithRequestMultiple);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (DataBufferSize != (size_t)moduleConfig->SizeOfDataBuffer)
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_UNSUCCESSFUL;
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Passed DataBufferSize does not match with SizeOfDataBuffer: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    // Fetch buffer.
    //
    ntStatus = DMF_BufferQueue_Fetch(moduleContext->DmfModuleBufferQueueProcessing,
                                     (VOID**)&clientBuffer,
                                     &clientBufferContext);
    if (! NT_SUCCESS(ntStatus))
    {
        moduleContext->FailureCountDuringBroadcast++;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BufferQueue_Fetch fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Map the Client buffer for ease of access.
    //
    bufferQueueContext = (NotifyUserWithRequestMultiple_BufferQueueBufferType*)clientBuffer;
    // Copy NtStatus.
    //
    bufferQueueContext->NtStatus = NtStatus;
    // Copy Client context.
    //
    RtlCopyMemory(bufferQueueContext->DataBuffer,
                  DataBuffer,
                  moduleConfig->SizeOfDataBuffer);

    // Add to pending work list.
    //
    DMF_BufferQueue_Enqueue(moduleContext->DmfModuleBufferQueueProcessing,
                            clientBuffer);

    // Ring the doorbell.
    //
    DMF_Doorbell_Ring(moduleContext->DmfModuleDoorbell);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);
    
    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_NotifyUserWithRequestMultiple_RequestProcess(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    This Method routes the Request to NotifyUserWithRequest_RequestProcess in the Client's dynamically created
    NotifyUserWithRequest Module.

Arguments:

    DmfModule - This Module's handle.
    Request - The request to add to this object's queue.

Return Value:

    Status of the operation.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_NotifyUserWithRequestMultiple* moduleContext;
    WDFFILEOBJECT fileObject;
    FILE_OBJECT_CONTEXT* fileObjectContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithRequestMultiple);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    fileObject = WdfRequestGetFileObject(Request);

    fileObjectContext = NotifyUserWithRequestMultiple_GetDynamicFileObjectContext(DmfModule,
                                                                                  fileObject);
    if (fileObjectContext == NULL)
    {
        moduleContext->FailureCountDuringRequestProcess++; 
        ntStatus = STATUS_NOT_FOUND;
        goto Exit;
    }

    // Process this Request.
    //
    ntStatus = DMF_NotifyUserWithRequest_RequestProcess(fileObjectContext->DmfModuleNotifyUserWithRequest,
                                                        Request);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DMF_NotifyUserWithRequest_RequestProcess fails: ENQUEUE DmfModule=0x%p Request=0x%p ntStatus=%!STATUS!",
                    DmfModule,
                    Request,
                    ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// eof: Dmf_NotifyUserWithRequestMultiple.c
//
