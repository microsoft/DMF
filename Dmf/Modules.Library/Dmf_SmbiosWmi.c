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
#include "DmfModule.h"
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
    // Memory Handle to raw SMBIOS table.
    //
    WDFMEMORY MemorySmbiosTable;

    // Pointer to the raw SMBIOS table.
    //
    UCHAR* SmbiosTableData;
    // Size of the raw SMBIOS table.
    //
    ULONG SmbiosTableDataSize;

    // Data for all supported translated tables.
    // TODO: Add more tables.
    //
    SmbiosWmi_TableType01* SmbiosTable01;
    size_t SmbiosTable01Size;
    WDFMEMORY MemorySmbiosTable01;

    // These two fields are included for legacy applications:
    // Do not use these fields.
    //

    // Pointer to the raw SMBIOS table (including WMI header).
    //
    UCHAR* SmbiosTableDataInWmiContainer;
    // Size of the raw SMBIOS table (including WMI header).
    //
    ULONG SmbiosTableDataSizeIncludesWmiContainer;
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

// Structures' information is available here:
// http://www.dmtf.org/standards/smbios
//

typedef struct
{
    UCHAR Used20CallingMethod;
    UCHAR SMBIOSMajorVersion;
    UCHAR SMBIOSMinorVersion;
    UCHAR DmiRevision;
    ULONG32 Length;
    UCHAR SMBIOSTableData[1];
} RAW_SMBIOS_HEADER;

typedef struct
{
    UCHAR Type;
    UCHAR Length;
    USHORT Handle;
    UCHAR TableData[1];
} SMBIOS_TABLE_HEADER;

typedef struct
{
    UCHAR Type;
    UCHAR Length;
    USHORT Handle;
    UCHAR Manufacturer;
    UCHAR ProductName;
    UCHAR Version;
    UCHAR SerialNumber;
    UCHAR UUID[16];
    UCHAR WakeUpType;
    UCHAR SKUNumber;
    UCHAR Family;
} RAW_SMBIOS_TABLE_01;

#define OffsetToPointer(Base, Offset) ((UCHAR*)((UCHAR*)(Base) + (Offset)))

#define SMBIOS_TABLE_01                     0x01
#define SMBIOS_TABLE_127                    0x7f

CHAR*
SmbiosViaWmi_StringAssign(
    _In_ CHAR StringNumber,
    _Inout_ UCHAR* *StringData,
    _In_ UCHAR* EndPointer
    )
/*++

Routine Description:

    Assign a given string from the pool of strings in the SMBIOS data buffer.
    This function returns the address of the assigned string and increments
    the pointer to the string pool to skip the returned string.

Arguments:

    StringNumber - The given string identifier.
    StringData - The address of the current string data in the string pool.
    EndPointer - The end of the SMBIOS data buffer.

Return Value:

    The address of the current string in the string pool if the given string
    is not zero. Otherwise, NULL is returned indicating that the current string
    in the string pool should be skipped.

--*/
{
    UCHAR* returnValue;

    if (StringNumber == 0)
    { 
        returnValue = NULL;
        goto Exit;
    }

    returnValue = *StringData;
       
    // Move StringData to next string.
    //
    while (((*StringData) < EndPointer) && 
           (!((0 == **StringData))))
    {
        (*StringData)++;
    }
       
    // If double NULL then it is the last string.
    //
    if ((*(*StringData + 1) != 0))
    {
        (*StringData)++;
    }

Exit:

    return (CHAR*)returnValue;
}

NTSTATUS
SmbioWmi_TablesSet(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Sets the data for each of the supported tables in the Module Context.
    NOTE: Not all tables are currently supported.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS there is enough room for at least one table.

--*/
{
    DMF_CONTEXT_SmbiosWmi* moduleContext;
    SMBIOS_TABLE_HEADER* smbiosTableHeader;
    NTSTATUS ntStatus;
    UCHAR* dataPointer;
    UCHAR* endPointer;
    UCHAR* stringData;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    ntStatus = STATUS_UNSUCCESSFUL;
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Process the table entries.
    // NOTE: This function is common to both WMI and non-WMI tables.
    //

    dataPointer = (PUCHAR)moduleContext->SmbiosTableData;
    endPointer = (PUCHAR)OffsetToPointer(dataPointer, 
                                         moduleContext->SmbiosTableDataSize);
    
    while (dataPointer < endPointer)
    {
        smbiosTableHeader = (SMBIOS_TABLE_HEADER*)dataPointer;

        if ((dataPointer + smbiosTableHeader->Length) > endPointer)
        {
            goto Exit;
        }

        // TODO: Add support for more Table Types.
        //
        switch (smbiosTableHeader->Type)
        {
            case SMBIOS_TABLE_01:
            {
                RAW_SMBIOS_TABLE_01* rawSmbiosTable01;

                rawSmbiosTable01 = (RAW_SMBIOS_TABLE_01*)dataPointer;
                stringData = (UCHAR*)(dataPointer + rawSmbiosTable01->Length);

                WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
                objectAttributes.ParentObject = DmfModule;

                ntStatus = WdfMemoryCreate(&objectAttributes,
                                           NonPagedPoolNx,
                                           MemoryTag,
                                           sizeof(SmbiosWmi_TableType01),
                                           &moduleContext->MemorySmbiosTable01,
                                           (PVOID*)&moduleContext->SmbiosTable01);
                if (!NT_SUCCESS(ntStatus))
                {
                    goto Exit;
                }

                moduleContext->SmbiosTable01Size = sizeof(SmbiosWmi_TableType01);

                moduleContext->SmbiosTable01->Manufacturer = SmbiosViaWmi_StringAssign(rawSmbiosTable01->Manufacturer, 
                                                                                        &stringData, 
                                                                                        endPointer);
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "SmbiosTable01.Manufacturer=[%s]", moduleContext->SmbiosTable01->Manufacturer);

                moduleContext->SmbiosTable01->ProductName = SmbiosViaWmi_StringAssign(rawSmbiosTable01->ProductName, 
                                                                                       &stringData, 
                                                                                       endPointer);
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "SmbiosTable01.ProductName=[%s]", moduleContext->SmbiosTable01->ProductName);

                moduleContext->SmbiosTable01->Version = SmbiosViaWmi_StringAssign(rawSmbiosTable01->Version, 
                                                                                   &stringData, 
                                                                                   endPointer);
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "SmbiosTable01.Version=[%s]", moduleContext->SmbiosTable01->Version);

                moduleContext->SmbiosTable01->SerialNumber = SmbiosViaWmi_StringAssign(rawSmbiosTable01->SerialNumber, 
                                                                                      &stringData, 
                                                                                      endPointer);
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "SmbiosTable01.SerialNumber=[%s]", moduleContext->SmbiosTable01->SerialNumber);

                for (ULONG uuidIndex = 0; uuidIndex < ARRAYSIZE(moduleContext->SmbiosTable01->Uuid); uuidIndex++)
                {
                    moduleContext->SmbiosTable01->Uuid[uuidIndex] = rawSmbiosTable01->UUID[uuidIndex];
                }
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, 
                            "SmbiosTable01.Uuid={0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X}", 
                            moduleContext->SmbiosTable01->Uuid[0],
                            moduleContext->SmbiosTable01->Uuid[1],
                            moduleContext->SmbiosTable01->Uuid[2],
                            moduleContext->SmbiosTable01->Uuid[3],
                            moduleContext->SmbiosTable01->Uuid[4],
                            moduleContext->SmbiosTable01->Uuid[5],
                            moduleContext->SmbiosTable01->Uuid[6],
                            moduleContext->SmbiosTable01->Uuid[7],
                            moduleContext->SmbiosTable01->Uuid[8],
                            moduleContext->SmbiosTable01->Uuid[9],
                            moduleContext->SmbiosTable01->Uuid[10],
                            moduleContext->SmbiosTable01->Uuid[11],
                            moduleContext->SmbiosTable01->Uuid[12],
                            moduleContext->SmbiosTable01->Uuid[13],
                            moduleContext->SmbiosTable01->Uuid[14],
                            moduleContext->SmbiosTable01->Uuid[15]);

                moduleContext->SmbiosTable01->WakeUpType = rawSmbiosTable01->WakeUpType;
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "SmbiosTable01.WakeUpType=[%d]", moduleContext->SmbiosTable01->WakeUpType);

                moduleContext->SmbiosTable01->SKUNumber = SmbiosViaWmi_StringAssign(rawSmbiosTable01->SKUNumber, 
                                                                                     &stringData, 
                                                                                     endPointer);
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "SmbiosTable01.SKUNumber=[%s]", moduleContext->SmbiosTable01->SKUNumber);

                moduleContext->SmbiosTable01->Family = SmbiosViaWmi_StringAssign(rawSmbiosTable01->Family, 
                                                                                &stringData, 
                                                                                endPointer);
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "SmbiosTable01.Family=[%s]", moduleContext->SmbiosTable01->Family);
                break;
            }
            // This case handle the scenario where the SMBIOS buffer is larger than the
            // data in the buffer. The end of the buffer, in this case, is indicated by "table 127".
            //
            case SMBIOS_TABLE_127:
            {
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Found End-Of-Table");
                ntStatus = STATUS_SUCCESS;
                goto Exit;
            }
            default:
            {
                // Skip over other tables.
                // TODO: Add support for other tables.
                //
                break;
            }
        }

        // Skip over unknown/unused data 
        //
        dataPointer += smbiosTableHeader->Length;
        while ((dataPointer < endPointer) &&
               (!((0 == *dataPointer) && 
                ((*(dataPointer + 1)) == 0))) )
        {
            dataPointer++;
        }

        // Skip over double null at end of entry.
        //
        dataPointer += 2;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

#if !defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_Check_return_
NTSTATUS
SmbiosWmi_Read(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Read the SMBIOS data block via WMI.

    NOTE: used by Kernel-mode only.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    VOID* dataBlockObject;
    ULONG bufferSize;
    GUID smbiosGUID;
    WNODE_ALL_DATA* nodeAllData;
    RAW_SMBIOS_HEADER* rawSmbiosHeader;
    ULONG smbiosLength;
    DMF_CONTEXT_SmbiosWmi* moduleContext;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    smbiosGUID = SMBIOS_DATA_GUID;

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

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;

    // Allocate space for the table.
    //
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               bufferSize,
                               &moduleContext->MemorySmbiosTable,
                               (PVOID*)&moduleContext->SmbiosTableDataInWmiContainer);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Read all the data to the Target Buffer.
    //
    ntStatus = IoWMIQueryAllData(dataBlockObject,
                                 &bufferSize,
                                 moduleContext->SmbiosTableDataInWmiContainer);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "IoWMIQueryAllData failed %!STATUS!", ntStatus);
        goto Exit;
    }

    // Container of the SMBIOS data.
    //
    nodeAllData = (PWNODE_ALL_DATA)moduleContext->SmbiosTableDataInWmiContainer;

    // Do a quick sanity check on pointer index.
    //
    if ((nodeAllData->OffsetInstanceDataAndLength[0].OffsetInstanceData) > bufferSize)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "TableSize=%d Expected=%d", 
                    nodeAllData->OffsetInstanceDataAndLength[0].OffsetInstanceData,
                    bufferSize);
        goto Exit;
    }
    
    // The SBMIOS Header Data is located here.
    //
    rawSmbiosHeader = (RAW_SMBIOS_HEADER*)OffsetToPointer(nodeAllData,
                                                          nodeAllData->OffsetInstanceDataAndLength[0].OffsetInstanceData);

    smbiosLength = nodeAllData->OffsetInstanceDataAndLength[0].LengthInstanceData;

    if (smbiosLength < sizeof(RAW_SMBIOS_HEADER))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "smbiosLength=%d Expected[sizeof(RAW_SMBIOS_HEADER)]=%d", 
                    smbiosLength,
                    sizeof(RAW_SMBIOS_HEADER));
        goto Exit;
    }

    // Process the table entries
    //
    moduleContext->SmbiosTableData = (PUCHAR)rawSmbiosHeader->SMBIOSTableData;
    moduleContext->SmbiosTableDataSize = smbiosLength;
    // For legacy support.
    //
    moduleContext->SmbiosTableDataSizeIncludesWmiContainer = bufferSize;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "SMBIOS Tables Read successfully: SmbiosTableDataSize=%u", moduleContext->SmbiosTableDataSize);
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

#else

// From MSDN.
//
#define SMBIOSWMI_FIRMWARE_TABLE_IDENTIFIER_SMBIOS  'RSMB'

#pragma code_seg("PAGE")
_Check_return_
NTSTATUS
SmbiosWmi_Read(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Read the SMBIOS data block via User-mode calls (non-WMI).

Arguments:

    TargetBuffer - The read data is returned here.
    TargetBufferSize - The size of TargetBuffer in bytes.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    UINT result;
    ULONG neededBufferSize;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    DMF_CONTEXT_SmbiosWmi* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ntStatus = STATUS_UNSUCCESSFUL;

    // Send zero to query for size.
    //
    neededBufferSize = 0;

    result = GetSystemFirmwareTable(SMBIOSWMI_FIRMWARE_TABLE_IDENTIFIER_SMBIOS,
                                    0x0000,
                                    NULL,
                                    neededBufferSize);
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "GetSystemFirmwareTable() result=%d", result);
    if (0 == result)
    {
        // An unrecoverable error happened.
        //
        goto Exit;
    }

    // This is the amount of data needed to store full raw SMBIOS table.
    //
    neededBufferSize = result;

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;

    // Allocate space for the table.
    //
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               neededBufferSize,
                               &moduleContext->MemorySmbiosTable,
                               (PVOID*)&moduleContext->SmbiosTableData);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    result = GetSystemFirmwareTable(SMBIOSWMI_FIRMWARE_TABLE_IDENTIFIER_SMBIOS,
                                    0x0000,
                                    (PVOID*)moduleContext->SmbiosTableData,
                                    neededBufferSize);
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "GetSystemFirmwareTable() result=%d *TargetBufferSize=%d", result, neededBufferSize);
    if (0 == result)
    {
        // An unrecoverable error happened.
        //
        goto Exit;
    }

    // It means the table was read successfully.
    // 
    moduleContext->SmbiosTableDataSize = neededBufferSize;
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "SMBIOS Tables Read successfully: SmbiosTableDataSize=%u", moduleContext->SmbiosTableDataSize);
    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#endif

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

    // Read the table.
    //
    ntStatus = SmbiosWmi_Read(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Parse the raw table to get component tables. Save them in the Module Context for
    // later use by Methods.
    //
    ntStatus = SmbioWmi_TablesSet(DmfModule);

Exit:

    return ntStatus;
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

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_SmbiosWmi,
                                            SmbiosWmi,
                                            DMF_CONTEXT_SmbiosWmi,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

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

NTSTATUS
DMF_SmbiosWmi_TableType01Get(
    _In_ DMFMODULE DmfModule,
    _Out_ SmbiosWmi_TableType01* SmbiosTable01Buffer,
    _Inout_ size_t* SmbiosTable01BufferSize
    )
/*++

Routine Description:

    Copy the SMBIOS Table 01 data to a given buffer.
    NOTE: The pointers in the structure point to memory that is private to the Module.

Arguments:

    DmfModule - This Module's handle.
    SmbiosTable01 - The given buffer.
    SmbiosTable01BufferSize - Client's target buffer size and/or size of data needed by Client.

Return Value:

    STATUS_SUCCESS if the table is present.
    STATUS_UNSUCCESSFUL if the table is not present.
    STATUS_BUFFER_TOO_SMALL if the Client's buffer is not large enough.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SmbiosWmi* moduleContext;

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_SmbiosWmi);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (NULL == moduleContext->SmbiosTable01)
    {
        // It means this table was not present.
        //
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    if (*SmbiosTable01BufferSize < sizeof(SmbiosWmi_TableType01))
    {
        // It means caller did not send a big enough buffer.
        // NOTE: Although the same buffer is always passed for this table, in order to 
        //       keep all APIs consistent with possible APIs that return variable size buffers
        //       this parameter is passed to this Method.
        //
        *SmbiosTable01BufferSize = sizeof(SmbiosWmi_TableType01);
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    // Copy the translated data to the caller's buffer.
    //
    RtlCopyMemory(SmbiosTable01Buffer,
                  moduleContext->SmbiosTable01,
                  sizeof(SmbiosWmi_TableType01));

    // Indicate the size of data written in caller's buffer.
    //
    *SmbiosTable01BufferSize = sizeof(SmbiosWmi_TableType01);

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

#if ! defined(DMF_USER_MODE)

_Check_return_
NTSTATUS
DMF_SmbiosWmi_TableCopy(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* TargetBuffer,
    _In_ ULONG TargetBufferSize
    )
/*++

Routine Description:

    Copies the SMBIOS data INCLUDING ITS WMI CONTAINER to a Client buffer.

    IMPORTANT: This Method is only included for legacy use which expects the WMI header.
               New code should use DMF_SmbiosWmi_TableCopyEx() instead.

    NOTE: This Method is only provided in Kernel-mode.

Arguments:

    DmfModule - This Module's handle.
    TargetBuffer - Client's target buffer where SMBIOS data including WMI header is written.
    TargetBufferSize - Size of TargetBuffer in bytes.

Return Value:

    STATUS_SUCCESS if the SMBIOS data is copied to Client's buffer.
    STATUS_BUFFER_TOO_SMALL if the Client target buffer is too small.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SmbiosWmi* moduleContext;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_SmbiosWmi);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (TargetBufferSize < moduleContext->SmbiosTableDataSizeIncludesWmiContainer)
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    RtlCopyMemory(TargetBuffer,
                  moduleContext->SmbiosTableDataInWmiContainer,
                  moduleContext->SmbiosTableDataSizeIncludesWmiContainer);

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

#endif

_Check_return_
NTSTATUS
DMF_SmbiosWmi_TableCopyEx(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* TargetBuffer,
    _In_ size_t* TargetBufferSize
    )
/*++

Routine Description:

    Copy the full SMBIOS data to a given Client buffer. This data is the raw SMBIOS data without
    a WMI header.

    IMPORTANT: All drivers (Kernel/User) should use this function instead of DMF_SmbiosWmi_TableCopy().

Arguments:

    DmfModule - This Module's handle.
    TargetBuffer - The given Client buffer.
    TargetBufferSize - The size of the given Client buffer.

Return Value:

    STATUS_SUCCESS - The table is present.
    STATUS_UNSUCCESSFUL - The table is not present.
    STATUS_BUFFER_TOO_SMALL - The Client's buffer is not large enough. In this case, the needed buffer size is 
                              written to TargetBufferSize.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SmbiosWmi* moduleContext;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_SmbiosWmi);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (*TargetBufferSize < moduleContext->SmbiosTableDataSize)
    {
        *TargetBufferSize = moduleContext->SmbiosTableDataSize;
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    RtlCopyMemory(TargetBuffer,
                  moduleContext->SmbiosTableData,
                  moduleContext->SmbiosTableDataSize);

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

#if !defined(DMF_USER_MODE)

VOID
DMF_SmbiosWmi_TableInformationGet(
    _In_ DMFMODULE DmfModule,
    _Out_ UCHAR** TargetBuffer,
    _Out_ ULONG* TargetBufferSize
    )
/*++

Routine Description:

    Gives the Client access to the internal buffer containing SBMIOS data including its WMI container.

    IMPORTANT: This Method is only included for legacy use which expects the WMI header.
               New code should use DMF_SmbiosWmi_TableInformationGetEx() instead.

    IMPORTANT: Clients should only read from this buffer. (It is provided primarily for Clients that need to
               access this data when writing to the crash dump file.)

    NOTE: This Method is only provided in Kernel-mode.

Arguments:

    DmfModule - This Module's handle.
    TargetBuffer - The address of this Module's SMBIOS data including WMI header.
    TargetBufferSize - The size of the buffer pointed to by TargetBuffer.

Return Value:

    None

--*/
{
    DMF_CONTEXT_SmbiosWmi* moduleContext;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_SmbiosWmi);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    *TargetBuffer = moduleContext->SmbiosTableDataInWmiContainer;
    *TargetBufferSize = moduleContext->SmbiosTableDataSizeIncludesWmiContainer;
}

#endif

VOID
DMF_SmbiosWmi_TableInformationGetEx(
    _In_ DMFMODULE DmfModule,
    _Out_ UCHAR** TargetBuffer,
    _Out_ size_t* TargetBufferSize
    )
/*++

Routine Description:

    Gives the Client access to the internal buffer containing SBMIOS data including its WMI container.

    IMPORTANT: Clients should only read from this buffer. (It is provided primarily for Clients that need to
               access this data when writing to the crash dump file.)

Arguments:

    DmfModule - This Module's handle.
    TargetBuffer - The address of this Module's SMBIOS data including WMI header.
    TargetBufferSize - The size of the buffer pointed to by TargetBuffer.

Return Value:

    None

--*/
{
    DMF_CONTEXT_SmbiosWmi* moduleContext;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_SmbiosWmi);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    *TargetBuffer = moduleContext->SmbiosTableData;
    *TargetBufferSize = moduleContext->SmbiosTableDataSize;
}

// eof: Dmf_SmbiosWmi.c
//
