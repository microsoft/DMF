 /*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Stack.c

Abstract:

    Implements a Stack data structure.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_Stack.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_Stack
{
    // BufferQueue that Module uses to implement a stack.
    //
    DMFMODULE DmfModuleBufferQueue;
} DMF_CONTEXT_Stack;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Stack)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(Stack)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Stack_ChildModulesAdd(
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
    DMF_CONFIG_Stack* moduleConfig;
    DMF_CONTEXT_Stack* moduleContext;
    DMF_CONFIG_BufferQueue moduleBufferQueueConfigList;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // DmfModuleBufferQueue
    // --------------------
    //
    DMF_CONFIG_BufferQueue_AND_ATTRIBUTES_INIT(&moduleBufferQueueConfigList,
                                               &moduleAttributes);
    moduleBufferQueueConfigList.SourceSettings.BufferCount = moduleConfig->StackDepth;
    moduleBufferQueueConfigList.SourceSettings.BufferSize = moduleConfig->StackElementSize;
    moduleBufferQueueConfigList.SourceSettings.EnableLookAside = TRUE;
    moduleBufferQueueConfigList.SourceSettings.BufferContextSize = 0;
    moduleBufferQueueConfigList.SourceSettings.PoolType = NonPagedPoolNx;
    moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleBufferQueue);

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
DMF_Stack_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Stack.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Stack;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Stack;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Stack);
    dmfCallbacksDmf_Stack.ChildModulesAdd = DMF_Stack_ChildModulesAdd;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Stack,
                                            Stack,
                                            DMF_CONTEXT_Stack,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Stack.CallbacksDmf = &dmfCallbacksDmf_Stack;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Stack,
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
_Must_inspect_result_
ULONG
DMF_Stack_Depth(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Return the number of entries currently in the stack.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    Number of entries in the stack.

--*/
{
    DMF_CONTEXT_Stack* moduleContext;
    ULONG numberOfEntriesInList;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Stack);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    numberOfEntriesInList = DMF_BufferQueue_Count(moduleContext->DmfModuleBufferQueue);

    FuncExit(DMF_TRACE, "numberOfEntriesInList=%d", numberOfEntriesInList);

    return numberOfEntriesInList;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Stack_Flush(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Empties the stack.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Stack* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Stack);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Move all the buffers from consumer list to producer list.
    //
    DMF_BufferQueue_Flush(moduleContext->DmfModuleBufferQueue);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Stack_Pop(
    _In_ DMFMODULE DmfModule,
    _Out_writes_bytes_(ClientBufferSize) VOID* ClientBuffer,
    _In_ size_t ClientBufferSize
    )
/*++

Routine Description:

    Pops next buffer in the list (head of the list) if there is a buffer.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The Client Buffer.
    ClientBufferSize - Size of Client buffer for verification.

Return Value:

    STATUS_SUCCESS if a buffer is removed from the list.
    STATUS_UNSUCCESSFUL if the list is empty.

--*/
{
    DMF_CONTEXT_Stack* moduleContext;
    DMF_CONFIG_Stack* moduleConfig;
    NTSTATUS ntStatus;
    UCHAR* stackBuffer;

    UNREFERENCED_PARAMETER(ClientBufferSize);

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Stack);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    DmfAssert(ClientBuffer != NULL);
    DmfAssert(ClientBufferSize == moduleConfig->StackElementSize);

    // Dequeue buffer.
    //
    ntStatus = DMF_BufferQueue_Dequeue(moduleContext->DmfModuleBufferQueue,
                                       (VOID**)&stackBuffer,
                                       NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BufferQueue_Dequeue fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Copy dequeued buffer to the client buffer.
    //

    // 'Possibly incorrect single element annotation on buffer'
    //
    __analysis_assume(ClientBufferSize == moduleConfig->StackElementSize);
    #pragma warning(suppress:26007)
    RtlCopyMemory(ClientBuffer,
                  stackBuffer,
                  moduleConfig->StackElementSize);

    // Add the used buffer back to empty buffer list.
    //
    DMF_BufferQueue_Reuse(moduleContext->DmfModuleBufferQueue,
                          stackBuffer);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Stack_Push(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    )
/*++

Routine Description:

    Push the Client buffer to the top of the stack.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The buffer to add to the list.
                   NOTE: Buffer should be the same size as declared in the config of this Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_Stack* moduleContext;
    DMF_CONFIG_Stack* moduleConfig;
    NTSTATUS ntStatus;
    UCHAR* stackBuffer;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Stack);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    DmfAssert(ClientBuffer != NULL);

    // Fetch buffer.
    //
    ntStatus = DMF_BufferQueue_Fetch(moduleContext->DmfModuleBufferQueue,
                                     (VOID**)&stackBuffer,
                                     NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BufferQueue_Fetch fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // 'Possibly incorrect single element annotation on buffer'
    //
    #pragma warning(suppress:26007)

    // Copy client buffer to the fetched buffer.
    //
    RtlCopyMemory(stackBuffer,
                  ClientBuffer,
                  moduleConfig->StackElementSize);

    // Push to the head of the consumer list.
    //
    DMF_BufferQueue_EnqueueAtHead(moduleContext->DmfModuleBufferQueue,
                                  stackBuffer);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// eof: Dmf_Stack.c
//
