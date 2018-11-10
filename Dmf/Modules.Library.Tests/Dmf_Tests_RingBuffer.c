/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_RingBuffer.c

Abstract:

    Functional tests for Dmf_RingBuffer Module

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

// This Module has no Context.
//
DMF_MODULE_DECLARE_NO_CONTEXT(Tests_RingBuffer)

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
        ASSERT(FALSE);                                                    \
        goto Exit;                                                        \
    }                                                                     
                                                                          
#define READ_AND_VERIFY(Value)                                            \
    ntStatus = DMF_RingBuffer_Read(dmfModuleRingBuffer,                   \
                                   (UCHAR*)&data,                         \
                                   sizeof(data));                         \
    if (!NT_SUCCESS(ntStatus) ||                                          \
        data != Value)                                                    \
    {                                                                     \
        ASSERT(FALSE);                                                    \
        goto Exit;                                                        \
    }                                                                     
                                                                          
#define READ_MUST_FAIL()                                                  \
    ntStatus = DMF_RingBuffer_Read(dmfModuleRingBuffer,                   \
                                   (UCHAR*)&data,                         \
                                   sizeof(data));                         \
    if (NT_SUCCESS(ntStatus))                                             \
    {                                                                     \
        ASSERT(FALSE);                                                    \
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
    ASSERT(enumContext.ItemsFound == (NumberOfItems));                    \
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
    ASSERT(enumContext.ItemsFound == 1);                                  \
}

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
    ASSERT(enumContext != NULL);

    ASSERT(sizeof(ULONG) == BufferSize);
    data = *(ULONG*)Buffer;
    ASSERT(enumContext->ValueExpected == data);
    
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
VOID
Tests_RingBuffer_RunTests(
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

    // Time per iteration increases every iteration. After 256, it begins to take a very long time.
    //
    const ULONG maximumNumberOfItems = 256;

    PAGED_CODE();

    dmfModuleRingBuffer = NULL;

    // Make a maximum count of 256.
    //
    if (MaximumItemCount > maximumNumberOfItems)
    {
        MaximumItemCount = maximumNumberOfItems;
    }

    for (itemCountIndex = 1; itemCountIndex < MaximumItemCount; itemCountIndex++)
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
            ASSERT(FALSE);
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
        for (ULONG itemIndex = 0; itemIndex < itemCountIndex; itemIndex++)
        {
            WRITE_MUST_SUCCEED(itemIndex);
        }
        DMF_RingBuffer_Reorder(dmfModuleRingBuffer,
                               TRUE);
        ENUM_AND_VERIFY(0, 
                        itemCountIndex);
        for (ULONG itemIndex = 0; itemIndex < itemCountIndex; itemIndex++)
        {
            FIND_AND_VERIFY(itemIndex);
            READ_AND_VERIFY(itemIndex);
        }

        // Overfill the buffer, reorder, enumerate and read back.
        //
        for (ULONG overFillExtra = 0; overFillExtra < itemCountIndex * 8; overFillExtra++)
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

        for (ULONG partialFillSize = 0; partialFillSize < itemCountIndex; partialFillSize++)
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
            for (ULONG itemIndex = 0; itemIndex < (itemCountIndex - partialFillSize); itemIndex++)
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
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
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
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    device = DMF_ParentDeviceGet(DmfModule);

    Tests_RingBuffer_RunTests(device, 
                              ITEM_COUNT_MAX);

    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_Tests_RingBuffer;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_Tests_RingBuffer;

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

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_Tests_RingBuffer);
    DmfCallbacksDmf_Tests_RingBuffer.DeviceOpen = Tests_RingBuffer_Open;

    DMF_MODULE_DESCRIPTOR_INIT(DmfModuleDescriptor_Tests_RingBuffer,
                               Tests_RingBuffer,
                               DMF_MODULE_OPTIONS_PASSIVE,
                               DMF_MODULE_OPEN_OPTION_OPEN_Create);

    DmfModuleDescriptor_Tests_RingBuffer.CallbacksDmf = &DmfCallbacksDmf_Tests_RingBuffer;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_Tests_RingBuffer,
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
