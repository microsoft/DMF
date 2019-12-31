/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_RingBuffer.c

Abstract:

    Functional tests for Dmf_RingBuffer Module.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"

#include "Dmf_Tests_RingBuffer.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define ITEM_COUNT_MAX           (64)

typedef struct
{
    BOOLEAN ValueIncrement;
    ULONG ValueExpected;
    ULONG ItemsFound;
    ULONG ItemsTotal;
} ENUM_CONTEXT_Tests_RingBuffer, *PENUM_CONTEXT_Tests_RingBuffer;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Thread that executes tests.
    //
    DMFMODULE DmfModuleThread;
} DMF_CONTEXT_Tests_RingBuffer;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_RingBuffer)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_RingBuffer)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define WRITE_MUST_SUCCEED(Value)                                         \
    data = Value;                                                         \
    ntStatus = DMF_RingBuffer_Write(dmfModuleRingBuffer,                  \
                                    (UCHAR*)&data,                        \
                                    sizeof(data));                        \
    if (!NT_SUCCESS(ntStatus))                                            \
    {                                                                     \
        DmfAssert(FALSE);                                                 \
        goto Exit;                                                        \
    }                                                                     
                                                                          
#define READ_AND_VERIFY(Value)                                            \
    ntStatus = DMF_RingBuffer_Read(dmfModuleRingBuffer,                   \
                                   (UCHAR*)&data,                         \
                                   sizeof(data));                         \
    if (!NT_SUCCESS(ntStatus) ||                                          \
        data != Value)                                                    \
    {                                                                     \
        DmfAssert(FALSE);                                                 \
        goto Exit;                                                        \
    }                                                                     
                                                                          
#define READ_MUST_FAIL()                                                  \
    ntStatus = DMF_RingBuffer_Read(dmfModuleRingBuffer,                   \
                                   (UCHAR*)&data,                         \
                                   sizeof(data));                         \
    if (NT_SUCCESS(ntStatus))                                             \
    {                                                                     \
        DmfAssert(FALSE);                                                 \
        goto Exit;                                                        \
    }

#define ENUM_AND_VERIFY(FirstItem, NumberOfItems)                         \
{                                                                         \
    ENUM_CONTEXT_Tests_RingBuffer enumContext;                            \
                                                                          \
    enumContext.ValueIncrement = TRUE;                                    \
    enumContext.ValueExpected = (FirstItem);                              \
    enumContext.ItemsFound = 0;                                           \
    enumContext.ItemsTotal = NumberOfItems;                               \
    DMF_RingBuffer_Enumerate(dmfModuleRingBuffer,                         \
                             TRUE,                                        \
                             Tests_RingBuffer_Enumeration,                \
                             &enumContext);                               \
    DmfAssert(enumContext.ItemsFound == (NumberOfItems));                 \
}                                                                         

#define FIND_AND_VERIFY(Value)                                            \
{                                                                         \
    ENUM_CONTEXT_Tests_RingBuffer enumContext;                            \
                                                                          \
    enumContext.ValueIncrement = FALSE;                                   \
    enumContext.ValueExpected = Value;                                    \
    enumContext.ItemsFound = 0;                                           \
    enumContext.ItemsTotal = 0;                                           \
    DMF_RingBuffer_EnumerateToFindItem(dmfModuleRingBuffer,               \
                                       Tests_RingBuffer_Enumeration,      \
                                       &enumContext,                      \
                                       (UCHAR*)&Value,                    \
                                       sizeof(Value));                    \
    DmfAssert(enumContext.ItemsFound == 1);                               \
}

_Function_class_(EVT_DMF_RingBuffer_Enumeration)
BOOLEAN
Tests_RingBuffer_Enumeration(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _In_opt_ VOID* CallbackContext
    )
{
    PENUM_CONTEXT_Tests_RingBuffer enumContext;
    ULONG data;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(BufferSize);

    enumContext = (PENUM_CONTEXT_Tests_RingBuffer)CallbackContext;
    DmfAssert(enumContext != NULL);

    DmfAssert(sizeof(ULONG) == BufferSize);
    data = *(ULONG*)Buffer;
    DmfAssert(enumContext->ValueExpected == data);
    
    // 'Dereferencing NULL pointer. 'dataSource' contains the same NULL value as 'CallbackContext' did.'
    //
    #pragma warning(suppress:28182)
    enumContext->ItemsFound ++;

    // 'Dereferencing NULL pointer. 'dataSource' contains the same NULL value as 'CallbackContext' did.'
    //
    #pragma warning(suppress:28182)
    if (enumContext->ValueIncrement)
    {
        enumContext->ValueExpected ++;
    }

    return TRUE;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Tests_RingBuffer_RunTests(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device,
    _In_ ULONG MaximumItemCount
    )
{
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_RingBuffer moduleConfigRingBuffer;
    DMFMODULE dmfModuleRingBuffer;
    ULONG data;
    NTSTATUS ntStatus;
    ULONG itemCountIndex;
    DMF_CONTEXT_Tests_RingBuffer* moduleContext;

    // Time per iteration increases every iteration. After 256, it begins to take a very long time.
    //
    const ULONG maximumNumberOfItems = 256;

    PAGED_CODE();

    dmfModuleRingBuffer = NULL;
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ntStatus = STATUS_UNSUCCESSFUL;

    // Make a maximum count of 256.
    //
    if (MaximumItemCount > maximumNumberOfItems)
    {
        MaximumItemCount = maximumNumberOfItems;
    }

    for (itemCountIndex = 1; itemCountIndex < MaximumItemCount && (! DMF_Thread_IsStopPending(moduleContext->DmfModuleThread)); itemCountIndex++)
    {
        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        objectAttributes.ParentObject = Device;

        DMF_CONFIG_RingBuffer_AND_ATTRIBUTES_INIT(&moduleConfigRingBuffer,
                                                  &moduleAttributes);
        moduleConfigRingBuffer.ItemCount = itemCountIndex;
        moduleConfigRingBuffer.ItemSize = sizeof(ULONG);
        moduleConfigRingBuffer.Mode = RingBuffer_Mode_DeleteOldestIfFullOnWrite;
        ntStatus = DMF_RingBuffer_Create(Device,
                                         &moduleAttributes,
                                         &objectAttributes,
                                         &dmfModuleRingBuffer);
        if (!NT_SUCCESS(ntStatus))
        {
            // It can fail when driver is being removed.
            //
            goto Exit;
        }

        // Reorder a newly created, empty Ring Buffer.
        //
        DMF_RingBuffer_Reorder(dmfModuleRingBuffer,
                               TRUE);

        // Reorder a buffer with one element.
        //
        WRITE_MUST_SUCCEED(itemCountIndex);
        DMF_RingBuffer_Reorder(dmfModuleRingBuffer,
                               TRUE);
        READ_AND_VERIFY(itemCountIndex);

        // Fill the buffer, reorder, enumerate and read back.
        //
        for (ULONG itemIndex = 0; itemIndex < itemCountIndex && (! DMF_Thread_IsStopPending(moduleContext->DmfModuleThread)); itemIndex++)
        {
            WRITE_MUST_SUCCEED(itemIndex);
        }
        DMF_RingBuffer_Reorder(dmfModuleRingBuffer,
                               TRUE);
        ENUM_AND_VERIFY(0, 
                        itemCountIndex);
        for (ULONG itemIndex = 0; itemIndex < itemCountIndex && (! DMF_Thread_IsStopPending(moduleContext->DmfModuleThread)); itemIndex++)
        {
            FIND_AND_VERIFY(itemIndex);
            READ_AND_VERIFY(itemIndex);
        }

        // Overfill the buffer, reorder, enumerate and read back.
        //
        for (ULONG overFillExtra = 0; overFillExtra < (itemCountIndex * 8) && (! DMF_Thread_IsStopPending(moduleContext->DmfModuleThread)); overFillExtra++)
        {
            for (ULONG itemIndex = 0; itemIndex < itemCountIndex + overFillExtra; itemIndex++)
            {
                WRITE_MUST_SUCCEED(itemIndex);
            }
            DMF_RingBuffer_Reorder(dmfModuleRingBuffer,
                                   TRUE);
            ENUM_AND_VERIFY(overFillExtra, 
                            itemCountIndex);
            for (ULONG itemIndex = 0; itemIndex < itemCountIndex; itemIndex++)
            {
                ULONG currentValue;
                // Subtract one since loops go from 0 to (n-1).
                //
                currentValue = overFillExtra + itemIndex;
                FIND_AND_VERIFY(currentValue);
                READ_AND_VERIFY(currentValue);
            }
        }

        // Reorder empty Ring Buffer.
        //
        READ_MUST_FAIL();
        DMF_RingBuffer_Reorder(dmfModuleRingBuffer,
                               TRUE);
        READ_MUST_FAIL();

        for (ULONG partialFillSize = 0; partialFillSize < itemCountIndex && (! DMF_Thread_IsStopPending(moduleContext->DmfModuleThread)); partialFillSize++)
        {
            // Under fill the buffer, reorder, enumerate and read back.
            //
            for (ULONG itemIndex = 0; itemIndex < (itemCountIndex - partialFillSize); itemIndex++)
            {
                WRITE_MUST_SUCCEED(itemIndex);
            }
            DMF_RingBuffer_Reorder(dmfModuleRingBuffer,
                                   TRUE);
            ENUM_AND_VERIFY(0, 
                            itemCountIndex - partialFillSize);
            for (ULONG itemIndex = 0; itemIndex < (itemCountIndex - partialFillSize) && (! DMF_Thread_IsStopPending(moduleContext->DmfModuleThread)); itemIndex++)
            {
                FIND_AND_VERIFY(itemIndex);
                READ_AND_VERIFY(itemIndex);
            }

            // Reorder empty Ring Buffer.
            //
            READ_MUST_FAIL();
            DMF_RingBuffer_Reorder(dmfModuleRingBuffer,
                                   TRUE);
            READ_MUST_FAIL();
        }

        WdfObjectDelete(dmfModuleRingBuffer);
        dmfModuleRingBuffer = NULL;
    }

Exit:

    if (dmfModuleRingBuffer != NULL)
    {
        WdfObjectDelete(dmfModuleRingBuffer);
    }

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_RingBuffer_WorkThread(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Tests_RingBuffer* moduleContext;
    WDFDEVICE device;
    ULONG itemCountMax;
    NTSTATUS ntStatus;

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    moduleContext = DMF_CONTEXT_GET(dmfModule);
    device = DMF_ParentDeviceGet(dmfModule);

    itemCountMax = TestsUtility_GenerateRandomNumber(4, 
                                                     ITEM_COUNT_MAX);

    ntStatus = Tests_RingBuffer_RunTests(dmfModule,
                                         device, 
                                         itemCountMax);

    // Repeat the test, until stop is signaled or the function stopped because the
    // driver is stopping.
    //
    if ((! DMF_Thread_IsStopPending(DmfModuleThread)) &&
        (NT_SUCCESS(ntStatus)))
    {
        DMF_Thread_WorkReady(DmfModuleThread);
    }

    TestsUtility_YieldExecution();
}
#pragma code_seg()

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
Tests_RingBuffer_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Test_RingBuffer.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Tests_RingBuffer* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    // Start the thread.
    //
    ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThread);

    // Tell the thread it has work to do.
    //
    DMF_Thread_WorkReady(moduleContext->DmfModuleThread);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_RingBuffer_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Close an instance of a DMF Module of type Test_RingBuffer.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_CONTEXT_Tests_RingBuffer* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_Thread_Stop(moduleContext->DmfModuleThread);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_RingBuffer_ChildModulesAdd(
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
    DMF_CONTEXT_Tests_RingBuffer* moduleContext;
    DMF_CONFIG_Thread moduleConfigThread;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                          &moduleAttributes);
    moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
    moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_RingBuffer_WorkThread;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleThread);

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
DMF_Tests_RingBuffer_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Test_RingBuffer.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Tests_RingBuffer;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Tests_RingBuffer;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Tests_RingBuffer);
    dmfCallbacksDmf_Tests_RingBuffer.ChildModulesAdd = DMF_Tests_RingBuffer_ChildModulesAdd;
    dmfCallbacksDmf_Tests_RingBuffer.DeviceOpen = Tests_RingBuffer_Open;
    dmfCallbacksDmf_Tests_RingBuffer.DeviceClose = Tests_RingBuffer_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Tests_RingBuffer,
                                            Tests_RingBuffer,
                                            DMF_CONTEXT_Tests_RingBuffer,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Tests_RingBuffer.CallbacksDmf = &dmfCallbacksDmf_Tests_RingBuffer;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Tests_RingBuffer,
                                DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_Tests_RingBuffer.c
//
