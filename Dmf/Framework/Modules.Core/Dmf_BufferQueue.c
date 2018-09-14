/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_BufferQueue.c

Abstract:

    Creates a producer buffer pool and a consumer buffer pool and exposes primitives to
    use those pools.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModules.Core.h"
#include "DmfModules.Core.Trace.h"

#include "Dmf_BufferQueue.tmh"

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
    // DMFMODULE to Producer BufferPool.
    //
    DMFMODULE DmfModuleBufferPoolProducer;
    // DMFMODULE to Consumer BufferPool.
    //
    DMFMODULE DmfModuleBufferPoolConsumer;
} DMF_CONTEXT_BufferQueue;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(BufferQueue)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(BufferQueue)

// Memory Pool Tag.
//
#define MemoryTag 'oMQB'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Wdf Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_BufferQueue_ChildModulesAdd(
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
    DMF_CONFIG_BufferPool moduleConfigProducer;
    DMF_CONFIG_BufferPool moduleConfigConsumer;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_BufferQueue* moduleConfig;
    DMF_CONTEXT_BufferQueue* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_BufferQueue);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // BufferPoolProducer
    // ------------------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigProducer,
                                              &moduleAttributes);
    moduleConfigProducer.BufferPoolMode = BufferPool_Mode_Source;
    moduleConfigProducer.Mode.SourceSettings = moduleConfig->SourceSettings;
    moduleAttributes.ClientModuleInstanceName = "BufferPoolProducer";
    moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPoolProducer);

    // BufferPoolConsumer
    // ------------------
    //
    DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigConsumer,
                                              &moduleAttributes);
    moduleConfigConsumer.BufferPoolMode = BufferPool_Mode_Sink;
    moduleAttributes.ClientModuleInstanceName = "BufferPoolConsumer";
    moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferPoolConsumer);

    FuncExitVoid(DMF_TRACE_BufferQueue);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_BufferQueue;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_BufferQueue;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferQueue_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type BufferQueue.

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

    FuncEntry(DMF_TRACE_BufferQueue);

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_BufferQueue);
    DmfCallbacksDmf_BufferQueue.ChildModulesAdd = DMF_BufferQueue_ChildModulesAdd;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_BufferQueue,
                                            BufferQueue,
                                            DMF_CONTEXT_BufferQueue,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    DmfModuleDescriptor_BufferQueue.CallbacksDmf = &DmfCallbacksDmf_BufferQueue;
    DmfModuleDescriptor_BufferQueue.ModuleConfigSize = sizeof(DMF_CONFIG_BufferQueue);

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_BufferQueue,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_BufferQueue, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE_BufferQueue, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG
DMF_BufferQueue_Count(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Return the number of entries currently in the list.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BufferQueue* moduleContext;
    ULONG numberOfEntriesInList;

    FuncEntry(DMF_TRACE_BufferQueue);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    numberOfEntriesInList = DMF_BufferPool_Count(moduleContext->DmfModuleBufferPoolConsumer);

    FuncExit(DMF_TRACE_BufferQueue, "numberOfEntriesInList=%d", numberOfEntriesInList);

    return numberOfEntriesInList;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferQueue_Dequeue(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_ VOID** ClientBufferContext
    )
/*++

Routine Description:

    Removes the next buffer in the consumer list (head of the list) if there is a buffer.
    Then, returns the Client Buffer and its associated Client Buffer Context.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The Client Buffer.
    ClientBufferContext - Client context associated with the buffer.

Return Value:

    STATUS_SUCCESS if a buffer is removed from the list.
    STATUS_UNSUCCESSFUL if the list is empty.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_BufferQueue* moduleContext;

    FuncEntry(DMF_TRACE_BufferQueue);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPoolConsumer,
                                  ClientBuffer,
                                  ClientBufferContext);

    FuncExit(DMF_TRACE_BufferQueue, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferQueue_DequeueWithMemoryDescriptor(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_ PWDF_MEMORY_DESCRIPTOR MemoryDescriptor,
    _Out_ VOID** ClientBufferContext
    )
/*++

Routine Description:

    Removes the next buffer in the list (head of the list) if there is a buffer.
    Then, returns the Client Buffer and its associated Memory Descriptor and ClientBufferContext.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The Client Buffer.
    MemoryDescriptor - WDF Memory Descriptor associated with removed buffer.
    ClientBufferContext - Client context associated with the buffer.

Return Value:

    STATUS_SUCCESS if a buffer is removed from the list.
    STATUS_UNSUCCESSFUL if the list is empty.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_BufferQueue* moduleContext;

    FuncEntry(DMF_TRACE_BufferQueue);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_BufferPool_GetWithMemoryDescriptor(moduleContext->DmfModuleBufferPoolConsumer,
                                                      ClientBuffer,
                                                      MemoryDescriptor,
                                                      ClientBufferContext);

    FuncExit(DMF_TRACE_BufferQueue, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Enqueue(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    )
/*++

Routine Description:

    Adds a Client Buffer to the consumer list.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The buffer to add to the list.
                   NOTE: This must be a properly formed buffer that was created by this Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BufferQueue* moduleContext;

    FuncEntry(DMF_TRACE_BufferQueue);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolConsumer,
                       ClientBuffer);

    FuncExitVoid(DMF_TRACE_BufferQueue);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Enumerate(
    _In_ DMFMODULE DmfModule,
    _In_ EVT_DMF_BufferPool_Enumeration EntryEnumerationCallback,
    _In_opt_ VOID* ClientDriverCallbackContext,
    _Out_opt_ VOID** ClientBuffer,
    _Out_opt_ VOID** ClientBufferContext
    )
/*++

Routine Description:

    Enumerate all the buffers in the consumer buffer list, calling a Client Driver's callback function
    for each buffer. If the Client wishes, the buffer can be removed from the list.
    NOTE: Module lock is held during this call.

Arguments:

    DmfModule - This Module's handle.
    EntryEnumerationCallback - Caller's enumeration function called for each buffer in the list.
    ClientDriverCallbackContext - Context passed for this call.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BufferQueue* moduleContext;

    FuncEntry(DMF_TRACE_BufferQueue);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_BufferPool_Enumerate(moduleContext->DmfModuleBufferPoolConsumer,
                             EntryEnumerationCallback,
                             ClientDriverCallbackContext,
                             ClientBuffer,
                             ClientBufferContext);

    FuncExitVoid(DMF_TRACE_BufferQueue);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferQueue_Fetch(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_ VOID** ClientBufferContext
    )
/*++

Routine Description:

    Removes the next buffer in the producer list (head of the list) if there is a buffer.
    Then, returns the Client Buffer and its associated Client Buffer Context.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The Client Buffer.
    ClientBufferContext - Client context associated with the buffer.

Return Value:

    STATUS_SUCCESS if a buffer is removed from the list.
    STATUS_UNSUCCESSFUL if the list is empty.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_BufferQueue* moduleContext;

    FuncEntry(DMF_TRACE_BufferQueue);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPoolProducer,
                                  ClientBuffer,
                                  ClientBufferContext);

    FuncExit(DMF_TRACE_BufferQueue, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Flush(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Remove all buffers from consumer buffer pool and place them in producer pool.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BufferQueue* moduleContext;
    VOID* buffer;
    VOID* bufferContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE_BufferQueue);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;
    while (NT_SUCCESS(ntStatus))
    {
        ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPoolConsumer,
                                      &buffer,
                                      &bufferContext);
        if (NT_SUCCESS(ntStatus))
        {
            DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolProducer,
                               buffer);
        }
    }

    FuncExitVoid(DMF_TRACE_BufferQueue);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferQueue_Reuse(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    )
/*++

Routine Description:

    Adds a Client Buffer to the producer list.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The buffer to add to the list.
                   NOTE: This must be a properly formed buffer that was created by this Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BufferQueue* moduleContext;

    FuncEntry(DMF_TRACE_BufferQueue);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_BufferQueue);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolProducer,
                       ClientBuffer);

    FuncExitVoid(DMF_TRACE_BufferQueue);
}

// eof: Dmf_BufferQueue.c
//
