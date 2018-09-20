/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_SmbiosWmi.c

Abstract:

    Store SMBIOS Table information read via WMI.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_SmbiosWmi.tmh"

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
    UCHAR* SmbiosTable;
    ULONG SmbioTableSize;
} DMF_CONTEXT_SmbiosWmi;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(SmbiosWmi)

// This Module has no Config. 
//
DMF_MODULE_DECLARE_NO_CONFIG(SmbiosWmi)

// Memory Pool Tag.
//
#define MemoryTag 'WvBS'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma pack(push, 1)
#pragma warning(disable:4200)

typedef struct RawSMBIOSData
{
    UCHAR    Used20CallingMethod;
    UCHAR    SMBIOSMajorVersion;
    UCHAR    SMBIOSMinorVersion;
    UCHAR    DmiRevision;
    ULONG32  Length;
    UCHAR    SMBIOSTableData[0];
} *PRAW_SMBIOS_HEADER, RAW_SMBIOS_HEADER;

typedef struct SMBIOSTableHeader
{
    UCHAR  Type;
    UCHAR  Length;
    USHORT Handle;
} *PSMBIOS_TABLE_HEADER, SMBIOS_TABLE_HEADER;

typedef struct SMBIOSTable1
{
    UCHAR  Type;
    UCHAR  Length;
    USHORT Handle;
    UCHAR Manufacturer;
    UCHAR ProductName;
    UCHAR Version;
    UCHAR SerialNumber;
    UCHAR UUID[16];
    UCHAR WakeUpType;
    UCHAR SKUNumber;
    UCHAR Family;
} *PSMBIOS_TABLE_1, SMBIOS_TABLE_1;

#pragma pack(pop)

#define OffsetToPointer(Base, Offset) ((UCHAR*)((UCHAR*)(Base) + (Offset)))

#pragma code_seg("PAGE")
_Check_return_
NTSTATUS
SmbiosWmi_Read(
    _Inout_opt_ UCHAR* TargetBuffer,
    _Inout_ ULONG* TargetBufferSize
    )
/*++

Routine Description:

    Read the SMBIOS data block via WMI.

Arguments:

    TargetBuffer - The read data is returned here.
    TargetBufferSize - The size of TargetBuffer in bytes.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    VOID* dataBlockObject;
    ULONG bufferSize;
    GUID smbiosGUID = SMBIOS_DATA_GUID;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // Use WMI to get access to SMBIOS Table.
    //
    ntStatus = IoWMIOpenBlock(&smbiosGUID,
                              WMIGUID_QUERY,
                              &dataBlockObject);
    if (! NT_SUCCESS(ntStatus))
    {
        dataBlockObject = NULL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "IoWMIOpenBlock ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Always query for size of buffer. 
    // STATUS_BUFFER_TOO_SMALL is expected.
    //
    bufferSize = 0;
    ntStatus = IoWMIQueryAllData(dataBlockObject,
                                 &bufferSize,
                                 NULL);
    if (ntStatus != STATUS_BUFFER_TOO_SMALL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "IoWMIQueryAllData ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Caller is making a query of the buffer size.
    //
    if (NULL == TargetBuffer)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Size=%d", bufferSize);
        *TargetBufferSize = bufferSize;
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    // It is validation.
    //
    if (*TargetBufferSize < bufferSize)
    {
        ASSERT(FALSE);
        *TargetBufferSize = bufferSize;
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Buffer Too Small (%d,%d) ntStatus=%!STATUS!", bufferSize, *TargetBufferSize, ntStatus);
        goto Exit;
    }

    // Read all the data to the Target Buffer.
    //
    ntStatus = IoWMIQueryAllData(dataBlockObject,
                                 &bufferSize,
                                 TargetBuffer);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "IoWMIQueryAllData failed %!STATUS!", ntStatus);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "SMBIOS Tables Read successfully.");
    ntStatus = STATUS_SUCCESS;

Exit:

    if (dataBlockObject != NULL)
    {
        ObDereferenceObject(dataBlockObject);
        dataBlockObject = NULL;
    }

    FuncExit(DMF_TRACE, "%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

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
_Must_inspect_result_
static
NTSTATUS
DMF_SmbiosWmi_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type SmbiosWmi.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SmbiosWmi* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Get the size of the table.
    //
    moduleContext->SmbioTableSize = 0;
    ntStatus = SmbiosWmi_Read(NULL,
                              &moduleContext->SmbioTableSize);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Allocate space for the table.
    //
    moduleContext->SmbiosTable = (UCHAR*)ExAllocatePoolWithTag(NonPagedPoolNx,
                                                               moduleContext->SmbioTableSize,
                                                               MemoryTag);
    if (moduleContext->SmbiosTable != NULL)
    {
        // Read the table.
        //
        ntStatus = SmbiosWmi_Read(moduleContext->SmbiosTable,
                                  &moduleContext->SmbioTableSize);
    }

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_SmbiosWmi_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Template.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_SmbiosWmi* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->SmbiosTable != NULL)
    {
        ExFreePoolWithTag(moduleContext->SmbiosTable,
                          MemoryTag);
        moduleContext->SmbiosTable = NULL;
    }
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_SmbiosWmi;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_SmbiosWmi;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SmbiosWmi_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type SmbioWmi.

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

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_SmbiosWmi);
    DmfCallbacksDmf_SmbiosWmi.DeviceOpen = DMF_SmbiosWmi_Open;
    DmfCallbacksDmf_SmbiosWmi.DeviceClose = DMF_SmbiosWmi_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_SmbiosWmi,
                                            SmbiosWmi,
                                            DMF_CONTEXT_SmbiosWmi,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    DmfModuleDescriptor_SmbiosWmi.CallbacksDmf = &DmfCallbacksDmf_SmbiosWmi;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_SmbiosWmi,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_Check_return_
NTSTATUS
DMF_SmbiosWmi_TableCopy(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* TargetBuffer,
    _In_ ULONG TargetBufferSize
    )
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SmbiosWmi* moduleContext;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_SmbiosWmi);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (TargetBufferSize < moduleContext->SmbioTableSize)
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    RtlCopyMemory(TargetBuffer,
                  moduleContext->SmbiosTable,
                  moduleContext->SmbioTableSize);

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

VOID
DMF_SmbiosWmi_TableInformationGet(
    _In_ DMFMODULE DmfModule,
    _Out_ UCHAR** TargetBuffer,
    _Out_ ULONG* TargetBufferSize
    )
{
    DMF_CONTEXT_SmbiosWmi* moduleContext;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_SmbiosWmi);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    *TargetBuffer = moduleContext->SmbiosTable;
    *TargetBufferSize = moduleContext->SmbioTableSize;
}

// eof: Dmf_SmbiosWmi.c
//
