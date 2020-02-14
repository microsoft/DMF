/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_ResourceHub.c

Abstract:

    Resource Hub support code.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_ResourceHub.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define RESHUB_USE_HELPER_ROUTINES
#include "reshub.h"
#include <Spb.h>

typedef struct _RESOURCEHUB_FILEOBJECT_CONTEXT
{
    USHORT SecondaryDeviceAddress;
    WDFMEMORY ConnectionProperties;
} RESOURCEHUB_FILEOBJECT_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RESOURCEHUB_FILEOBJECT_CONTEXT, ResourceHub_FileContextGet);

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Contains the WDF IO Target as well as all the structures needed for
// streaming requests.
//
typedef struct
{
    WDFIOTARGET ResourceHubTarget;
} DMF_CONTEXT_ResourceHub;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(ResourceHub)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(ResourceHub)

// Memory Pool Tag.
//
#define MemoryTag 'MHeR'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define INTERNAL_SERIAL_BUS_SIZE(Desc)     ((ULONG)(Desc)->Length +                                      \
                                           RTL_SIZEOF_THROUGH_FIELD(PNP_SERIAL_BUS_DESCRIPTOR, Length))

#include "pshpack1.h"

typedef struct _DIRECTFW_I2C_CONNECTION_DESCRIPTOR_SUBTYPE
{
    UINT32 ConnectionSpeed;
    USHORT SecondaryDeviceAddress;
    UCHAR  VendorDefinedData[ANYSIZE_ARRAY];
} DIRECTFW_I2C_CONNECTION_DESCRIPTOR_SUBTYPE;

#include "poppack.h"

#pragma code_seg("PAGE")
NTSTATUS
ResourceHub_ConnectResourceHubIoTarget(
    _Inout_ DMFMODULE DmfModule,
    _In_opt_ PLARGE_INTEGER Id,
    _Out_ WDFIOTARGET * RootHubIoTarget
    )
/*++

Routine Description:

    Opens an I/O target to the Resource Hub.

    If Id is left NULL, the target is opened to the Resource Hub directly, and can then
    be used to query for connection properties.

    If Id is specified, the target is opened to the RH with this Id as the
    filename. Internally the RH redirects the I/O target such that requests made
    against the target are sent to the device represented by that Id.

Arguments:

    DmfModule - This Module's handle.
    Id - Connection ID received as part of FileCreate callback or
         PrepareHardware resources.
    RootHubIoTarget - Opened I/O target to the Resource Hub, using a filename
                      constructed from the value Id.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES attributes;
    DECLARE_UNICODE_STRING_SIZE(resourceHubFileName, RESOURCE_HUB_PATH_SIZE);
    WDF_IO_TARGET_OPEN_PARAMS openParameters;

    PAGED_CODE();

    DmfAssert(DmfModule != NULL);
    DmfAssert(RootHubIoTarget != NULL);

    // Create an IO target to the controller driver via the resource hub.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = DmfModule;
    ntStatus = WdfIoTargetCreate(DMF_ParentDeviceGet(DmfModule),
                                 &attributes,
                                 RootHubIoTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetCreate fails: ntStatus=%!STATUS!", ntStatus);
        *RootHubIoTarget = NULL;
        goto Exit;
    }

    // Create controller driver string from descriptor information.
    //
    if (Id != NULL)
    {
        RESOURCE_HUB_CREATE_PATH_FROM_ID(&resourceHubFileName,
                                         Id->LowPart,
                                         Id->HighPart);
    }
    else
    {
        RtlInitUnicodeString(&resourceHubFileName,
                             RESOURCE_HUB_DEVICE_NAME);
    }

    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParameters,
                                                &resourceHubFileName,
                                                STANDARD_RIGHTS_ALL);

    // Open the controller driver / Resource Hub I/O target.
    //
    ntStatus = WdfIoTargetOpen(*RootHubIoTarget,
                               &openParameters);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetOpen fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    if (! NT_SUCCESS(ntStatus) &&
        (*RootHubIoTarget != NULL))
    {
        WdfObjectDelete(*RootHubIoTarget);
        *RootHubIoTarget = NULL;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
NTSTATUS
ResourceHub_RegisterForAcpiNotifications(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Function to register for ACPI notifications.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ResourceHub* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Create connection-less target to resource hub for querying connection 
    // properties.
    //
    ntStatus = ResourceHub_ConnectResourceHubIoTarget(DmfModule,
                                                      NULL,
                                                      &moduleContext->ResourceHubTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ResourceHub_ConnectResourceHubIoTarget fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

NTSTATUS
ResourceHub_ParseGenericSerialBusDescriptor(
    _In_  DMFMODULE DmfModule,
    _In_bytecount_(BiosDescriptorLength) VOID* BiosDescriptor,
    _In_ size_t BiosDescriptorLength,
    _Out_ ResourceHub_DIRECTFW_SERIAL_BUS_TYPE* SerialBusType,
    _Out_ __deref_bcount(*TypeSpecificDataLength) VOID* * TypeSpecificData,
    _Out_ ULONG* TypeSpecificDataLength,
    _Out_opt_ UCHAR** ResourcePath
    )
/*++

Routine Description:

    Parses a buffer containing a GenericSerialBus ACPI resource descriptor and
    returns the serial bus type as well as a pointer to any type specific data.

Arguments:

    DmfModule - This Module's handle.
    BiosDescriptor - Buffer containing the resource descriptor.
    BiosDescriptorLength - Length of buffer in bytes.
    SerialBusType - Type of the GenericSerialBus resource (I2C, SPI, etc)
    TypeSpecificData - Pointer to the offset in BiosDescriptor containing the
                       type specific data of the descriptor.
    TypeSpecificDataLength - Length of TypeSpecificData in bytes.
    ResourcePath - If not null, this string points ACPI BIOS name which this
                   resource descriptor consumes.
                   (Generally, the serial bus controller a device/OpRegion uses.)

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    PPNP_SERIAL_BUS_DESCRIPTOR serialBusDescriptor;
    UCHAR type;

    UNREFERENCED_PARAMETER(DmfModule);

    ntStatus = STATUS_SUCCESS;

    if (ResourcePath != NULL)
    {
        *ResourcePath = NULL;
    }

    // Before validating fields of descriptor, verify that buffer itself 
    // exists and is large enough that it could possibly be valid.
    //
    if (BiosDescriptor == NULL ||
        BiosDescriptorLength < sizeof(PNP_SERIAL_BUS_DESCRIPTOR))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "BiosDescriptorLength=%d sizeof(PNP_SERIAL_BUS_DESCRIPTOR)=%d",
                    (int)BiosDescriptorLength,
                    (int)sizeof(PNP_SERIAL_BUS_DESCRIPTOR));
        DmfAssert(FALSE);
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    // Verify that this descriptor is a GenericSerialBus type.
    //
    type = *(UCHAR*)BiosDescriptor;
    if (type != SERIAL_BUS_DESCRIPTOR)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "type=%d SERIAL_BUS_DESCRIPTOR=%d",
                    type,
                    SERIAL_BUS_DESCRIPTOR);
        DmfAssert(FALSE);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    serialBusDescriptor = (PPNP_SERIAL_BUS_DESCRIPTOR)BiosDescriptor;

    // Verify the Length field of the general Serial Bus Connection Descriptor,
    // as well as the subtype field are within bounds (which means > min
    // size and < size of buffer), and that there is still space for a resource
    // path.
    // 
    if ((INTERNAL_SERIAL_BUS_SIZE(serialBusDescriptor) <
         sizeof(PNP_SERIAL_BUS_DESCRIPTOR)) ||
         (INTERNAL_SERIAL_BUS_SIZE(serialBusDescriptor) >
          BiosDescriptorLength) ||
          (serialBusDescriptor->TypeDataLength >
        (BiosDescriptorLength - sizeof(PNP_SERIAL_BUS_DESCRIPTOR))))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid Code Path");
        DmfAssert(FALSE);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // Extract SerialBusType.
    //
    *SerialBusType = (ResourceHub_DIRECTFW_SERIAL_BUS_TYPE)serialBusDescriptor->SerialBusType;

    // Extract pointer to type specific data section and resource path.
    //
    *TypeSpecificData = (UCHAR*)serialBusDescriptor +
        RTL_SIZEOF_THROUGH_FIELD(PNP_SERIAL_BUS_DESCRIPTOR,
                                 TypeDataLength);
    *TypeSpecificDataLength = serialBusDescriptor->TypeDataLength;

    if (ResourcePath != NULL)
    {
        *ResourcePath = (UCHAR*)serialBusDescriptor +
            RTL_SIZEOF_THROUGH_FIELD(PNP_SERIAL_BUS_DESCRIPTOR,
                                     TypeDataLength) +
            serialBusDescriptor->TypeDataLength;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

NTSTATUS
ResourceHub_ParseI2CSerialBusDescriptorSubtype(
    _In_  DMFMODULE DmfModule,
    _In_bytecount_(TypeSpecificDataLength) VOID* TypeSpecificData,
    _In_ ULONG TypeSpecificDataLength,
    _Out_ PUSHORT SecondaryDeviceAddress
    )
/*++

Routine Description:

    Parse I2C Serial Bus Descriptor Subtype (retrieve secondary device Address).

Arguments:

    DmfModule - This Module's handle.
    TypeSpecificData - The descriptor to parse.
    TypeSpecificDataLength - The length of the descriptor in bytes.
    SecondaryDeviceAddress - Secondary device address that is extracted from descriptor.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DIRECTFW_I2C_CONNECTION_DESCRIPTOR_SUBTYPE* i2CSubDescriptor;

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    if (TypeSpecificDataLength <
        RTL_SIZEOF_THROUGH_FIELD(DIRECTFW_I2C_CONNECTION_DESCRIPTOR_SUBTYPE,
                                 SecondaryDeviceAddress))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "TypeSpecificDataLength=%d [%d]",
                    TypeSpecificDataLength,
                    RTL_SIZEOF_THROUGH_FIELD(DIRECTFW_I2C_CONNECTION_DESCRIPTOR_SUBTYPE, SecondaryDeviceAddress));
        DmfAssert(FALSE);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    i2CSubDescriptor = (DIRECTFW_I2C_CONNECTION_DESCRIPTOR_SUBTYPE*)TypeSpecificData;

    DmfAssert(SecondaryDeviceAddress != NULL);
    *SecondaryDeviceAddress = i2CSubDescriptor->SecondaryDeviceAddress;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

#pragma code_seg("PAGE")
NTSTATUS
ResourceHub_QueryConnectionProperties(
    _In_  DMFMODULE DmfModule,
    _In_  PLARGE_INTEGER Id,
    _In_  WDFOBJECT ConnectionPropertiesLifetimeReference,
    _Out_ WDFMEMORY* ConnectionProperties
    )
/*++

Routine Description:

    Queries the Resource Hub; returns a callee allocated WDFMEMORY, refcounted against
    this Device WDFMEMORY containing the ACPI resource descriptor for the
    connection id specific by Id.

Arguments:

    DmfModule - This Module's handle.
    Id - Connection ID received as part of FileCreate callback or
         PrepareHardware resources
    ConnectionPropertiesLifetimeReference - The lifetime of ConnectionProperties' WDFMEMORY is tied to the
                                            lifetime of this object, such that when this
                                            is deleted, the WDFMEMORY is automatically freed.
    ConnectionProperties - Callee allocated WDFMEMORY containing bytes of
                           resource descriptor for the connection id specified by Id. Caller must
                           free with WdfObjectDelete, or by deleting
                           ConnectionPropertiesLifetimeReference.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_ResourceHub* moduleContext;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_MEMORY_DESCRIPTOR inputMemoryDescriptor;
    WDF_MEMORY_DESCRIPTOR outputMemoryDescriptor;
    WDFMEMORY registrationMemory;
    WDFMEMORY resultsQueryMemory;
    WDFMEMORY resultsBufferMemory;
    PRH_QUERY_CONNECTION_PROPERTIES_INPUT_BUFFER registrationBuffer;
    PRH_QUERY_CONNECTION_PROPERTIES_OUTPUT_BUFFER resultsQuery;
    PRH_QUERY_CONNECTION_PROPERTIES_OUTPUT_BUFFER resultsBuffer;
    ULONG requiredInputBufferSize;
    ULONG requiredOutputBufferSize;
    ULONG_PTR ioctlBytesReturned;
    NTSTATUS ntStatus;
    VOID* connectionPropertiesPointer;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->ResourceHubTarget != NULL);

    registrationMemory = NULL;
    registrationBuffer = NULL;
    resultsQueryMemory = NULL;
    resultsQuery = NULL;
    resultsBufferMemory = NULL;
    resultsBuffer = NULL;
    DmfAssert(ConnectionProperties != NULL);
    *ConnectionProperties = NULL;

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = ConnectionPropertiesLifetimeReference;

    // Setup input buffer structure with parameters from the caller.
    //
    requiredInputBufferSize = sizeof(RH_QUERY_CONNECTION_PROPERTIES_INPUT_BUFFER);
    ntStatus = WdfMemoryCreate(&attributes,
                               PagedPool,
                               MemoryTag,
                               requiredInputBufferSize,
                               &registrationMemory,
                               (VOID* *)&registrationBuffer);
    if (! NT_SUCCESS(ntStatus) ||
        registrationMemory == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        registrationMemory = NULL;
        goto Exit;
    }

    RtlZeroMemory(registrationBuffer,
                  requiredInputBufferSize);
    registrationBuffer->Version = RH_QUERY_CONNECTION_PROPERTIES_INPUT_VERSION;
    registrationBuffer->QueryType = ConnectionIdType;
    registrationBuffer->u.ConnectionId.LowPart = Id->LowPart;
    registrationBuffer->u.ConnectionId.HighPart = Id->HighPart;
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemoryDescriptor,
                                      registrationBuffer,
                                      requiredInputBufferSize);

    // First determine how many bytes are needed.
    //
    requiredOutputBufferSize = sizeof(RH_QUERY_CONNECTION_PROPERTIES_OUTPUT_BUFFER);
    ntStatus = WdfMemoryCreate(&attributes,
                               PagedPool,
                               MemoryTag,
                               requiredOutputBufferSize,
                               &resultsQueryMemory,
                               (VOID* *)&resultsQuery);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        resultsQueryMemory = NULL;
        goto Exit;
    }

    RtlZeroMemory(resultsQuery,
                  requiredOutputBufferSize);
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputMemoryDescriptor,
                                      resultsQuery,
                                      requiredOutputBufferSize);
    ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->ResourceHubTarget,
                                                 NULL,
                                                 IOCTL_RH_QUERY_CONNECTION_PROPERTIES,
                                                 &inputMemoryDescriptor,
                                                 &outputMemoryDescriptor,
                                                 NULL,
                                                 &ioctlBytesReturned);
    if (ntStatus != STATUS_BUFFER_TOO_SMALL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetSendIoctlSynchronously fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    requiredOutputBufferSize = resultsQuery->PropertiesLength;

    // Then allocate the required amount of memory and call IOCTL for real.
    // 
    requiredOutputBufferSize = sizeof(RH_QUERY_CONNECTION_PROPERTIES_OUTPUT_BUFFER) +
                               requiredOutputBufferSize;
    __assume(requiredOutputBufferSize > 0);

    ntStatus = WdfMemoryCreate(&attributes,
                               PagedPool,
                               MemoryTag,
                               requiredOutputBufferSize,
                               &resultsBufferMemory,
                               (VOID* *)&resultsBuffer);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        resultsBufferMemory = NULL;
        goto Exit;
    }

    RtlZeroMemory(resultsBuffer,
                  requiredOutputBufferSize);
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputMemoryDescriptor,
                                      resultsBuffer,
                                      requiredOutputBufferSize);

    ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->ResourceHubTarget,
                                                 NULL,
                                                 IOCTL_RH_QUERY_CONNECTION_PROPERTIES,
                                                 &inputMemoryDescriptor,
                                                 &outputMemoryDescriptor,
                                                 NULL,
                                                 &ioctlBytesReturned);
    if (! NT_SUCCESS(ntStatus) ||
        (resultsBuffer->PropertiesLength == 0))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetSendIoctlSynchronously fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = WdfMemoryCreate(&attributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               resultsBuffer->PropertiesLength,
                               ConnectionProperties,
                               &connectionPropertiesPointer);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        *ConnectionProperties = NULL;
        goto Exit;
    }

    RtlCopyMemory(connectionPropertiesPointer,
                  resultsBuffer->ConnectionProperties,
                  resultsBuffer->PropertiesLength);

Exit:

    if (registrationMemory != NULL)
    {
        WdfObjectDelete(registrationMemory);
        registrationMemory = NULL;
    }
    if (resultsQueryMemory != NULL)
    {
        WdfObjectDelete(resultsQueryMemory);
        resultsQueryMemory = NULL;
    }
    if (resultsBufferMemory != NULL)
    {
        WdfObjectDelete(resultsBufferMemory);
        resultsBufferMemory = NULL;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

NTSTATUS
ResourceHub_ValidateTransferList(
    _In_  DMFMODULE DmfModule,
    _In_  VOID* TransferBuffer,
    _In_  size_t TransferBufferLength
    )
/*++

Routine Description:

    Validates that the buffer TransferBuffer contains a valid SPB_TRANSFER_LIST
    which utilizes the Simple buffer type and has either 1 or 2
    SPB_TRANSFER_LIST_ENTRYs. Essentially, validates that this buffer looks like
    it came from the ACPI I2CSerialBus OpRegion.

    The legitimate combinations of raw requests consist of:
        1. A single transfer list entry, to device, len >= 2 -> (cmd, ID)
        2. Two transfer list entries:
           a. to device, len >= 2   -> (cmd, ID, data)
           b. from device, len >= 1 -> (return data)

Arguments:

    DmfModule - This Module's handle.
    TransferBuffer - Buffer containing potential SPB_TRANSFER_LIST in need of
                     validation.
    TransferBufferLength - Length of TransferBuffer in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG transferIndex;
    PSPB_TRANSFER_LIST list;
    PSPB_TRANSFER_LIST_ENTRY listEntry;
    PSPB_TRANSFER_BUFFER buffer;
    PSPB_TRANSFER_BUFFER_LIST_ENTRY bufferEntry;
    SPB_TRANSFER_DIRECTION previousDataListEntryDirection;

    UNREFERENCED_PARAMETER(DmfModule);

    ntStatus = STATUS_SUCCESS;

    // Basic sanity checks of buffer.
    //
    if (TransferBufferLength < sizeof(SPB_TRANSFER_LIST))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "TransferBufferLength=%d sizeof(SPB_TRANSFER_LIST)=%d",
                    (int)TransferBufferLength,
                    (int)sizeof(SPB_TRANSFER_LIST));
        DmfAssert(FALSE);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    list = (PSPB_TRANSFER_LIST)TransferBuffer;

    if (list->Size != sizeof(SPB_TRANSFER_LIST))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "list->Size=%d sizeof(SPB_TRANSFER_LIST)=%d",
                    list->Size,
                    sizeof(SPB_TRANSFER_LIST));
        DmfAssert(FALSE);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // Communicate with ACPI OpRegions using 
    // AttribRawBytes type accesses, which generate read, write, or write-read
    // requests consisting of one or two SPB_TRANSFER_LIST_ENTRYs with buffers
    // of type SpbTransferBufferFormatSimple. More complex LIST_ENTRYs 
    // containing SpbTransferBufferFormatList or MDLs are not supported.
    //
    if (list->TransferCount != 1 && list->TransferCount != 2)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "list->TransferCount=%d", list->TransferCount);
        DmfAssert(FALSE);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    previousDataListEntryDirection = SpbTransferDirectionNone;

    for (transferIndex = 0; transferIndex < list->TransferCount; transferIndex++)
    {
        listEntry = &list->Transfers[transferIndex];
        buffer = &listEntry->Buffer;
        bufferEntry = &buffer->Simple;

        if (buffer->Format != SpbTransferBufferFormatSimple)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                        "buffer->Format=%d SpbTransferBufferFormatSimple=%d",
                        buffer->Format,
                        SpbTransferBufferFormatSimple);
            DmfAssert(FALSE);
            ntStatus = STATUS_INVALID_PARAMETER;
            goto Exit;
        }

        switch (transferIndex)
        {
            case 0:
            {
                // Protocol between OpRegion and PMU driver is:
                // < Command (1 byte) > < Rail ID (1 byte) >
                // Any request which contains less data for its first index is invalid
                //
                if (listEntry->Direction != SpbTransferDirectionToDevice)
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                                "listEntry->Direction=%d SpbTransferDirectionToDevice=%d",
                                listEntry->Direction,
                                SpbTransferDirectionToDevice);
                    DmfAssert(FALSE);
                    ntStatus = STATUS_INVALID_PARAMETER;
                    goto Exit;
                }
                // Fall through, because default: checks also apply.
                //
            }
            default:
            {
                // A BufferEntry with no bytes of data is always unexpected.
                //
                if (listEntry->Direction != SpbTransferDirectionToDevice &&
                    listEntry->Direction != SpbTransferDirectionFromDevice)
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                                "listEntry->Direction=%d SpbTransferDirectionToDevice=%d SpbTransferDirectionFromDevice=%d",
                                listEntry->Direction, SpbTransferDirectionToDevice,
                                SpbTransferDirectionFromDevice);
                    DmfAssert(FALSE);
                    ntStatus = STATUS_INVALID_PARAMETER;
                    goto Exit;
                }

                if (listEntry->Direction == previousDataListEntryDirection)
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "listEntry->Direction=%d previousDataListEntryDirection=%d", listEntry->Direction, previousDataListEntryDirection);
                    DmfAssert(FALSE);
                    ntStatus = STATUS_INVALID_PARAMETER;
                    goto Exit;
                }

                previousDataListEntryDirection = listEntry->Direction;
                break;
            }
        }
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
ResourceHub_IoctlClientCallback_SpbExecuteSequence(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoctlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    Performs SPB transactions based on IOCTL requests.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    Queue - The WDFQUEUE associated with Request.
    Request - Request data.
    IoctlCode - IOCTL to be used in the command.
    InputBuffer - Input data buffer.
    InputBufferSize - Input data buffer size.
    OutputBuffer - Output data buffer.
    OutputBufferSize - Output data buffer size.
    BytesReturned - Amount of data to be sent back.

Return Value:

    NTSTATUS

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_ResourceHub* moduleContext;
    DMF_CONFIG_ResourceHub* moduleConfig;
    RESOURCEHUB_FILEOBJECT_CONTEXT* resourceHubFileObjectContext;
    NTSTATUS ntStatus;
    WDFFILEOBJECT fileObject;
    PSPB_TRANSFER_LIST spbTransferList;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(IoctlCode);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_UNSUCCESSFUL;

    // This Module is the parent of the Child Module that is passed in.
    // (Module callbacks always receive the Child Module's handle.)
    // This is ResourceHub Module.
    //
    dmfModule = DMF_ParentModuleGet(DmfModule);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    moduleConfig = DMF_CONFIG_GET(dmfModule);

    *BytesReturned = 0;

    fileObject = WdfRequestGetFileObject(Request);

    resourceHubFileObjectContext = ResourceHub_FileContextGet(fileObject);

    ntStatus = ResourceHub_ValidateTransferList(dmfModule,
                                                InputBuffer,
                                                InputBufferSize);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ResourceHub_ValidateTransferList for IOCTL_SPB_EXECUTE_SEQUENCE fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // If validation succeeded, it is safe to cast the buffer to an SPB_TRANSFER_LIST.
    //
    spbTransferList = (PSPB_TRANSFER_LIST)InputBuffer;

    if (moduleConfig->EvtResourceHubDispatchTransferList != NULL)
    {
        ntStatus = moduleConfig->EvtResourceHubDispatchTransferList(dmfModule,
                                                                    spbTransferList,
                                                                    InputBufferSize,
                                                                    resourceHubFileObjectContext->SecondaryDeviceAddress,
                                                                    BytesReturned);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtResourceHubDispatchTransferList fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_ModuleFileCreate)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
BOOLEAN
DMF_ResourceHub_ModuleFileCreate(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    File Create callback. Use this callback to open a connection to ACPI.

Arguments:

    DmfModule - The given DMF Module.
    Device - WDF device object.
    Request - WDF Request with IOCTL parameters.
    FileObject - WDF file object that describes a file that is being opened for the specified request.

Return Value:

    TRUE if this Module handled the callback; false, otherwise.

--*/
{
    BOOLEAN handled;
    DMF_CONTEXT_ResourceHub* moduleContext;
    PUNICODE_STRING fileName;
    UNICODE_STRING filePart;
    LARGE_INTEGER id;
    NTSTATUS ntStatus;
    WDFMEMORY connectionProperties;
    VOID* connectionPropertiesBuffer;
    size_t connectionPropertiesLength;
    ResourceHub_DIRECTFW_SERIAL_BUS_TYPE serialBusType;
    VOID* typeSpecificData;
    ULONG typeSpecificDataLength;
    USHORT secondaryDeviceAddress;
    size_t filenameLength;
    WDF_OBJECT_ATTRIBUTES attributes;
    RESOURCEHUB_FILEOBJECT_CONTEXT* fileContext;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(Device);

    FuncEntry(DMF_TRACE);

    DmfAssert(Device == DMF_ParentDeviceGet(DmfModule));
    DmfAssert(Request != NULL);
    DmfAssert(FileObject != NULL);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                            RESOURCEHUB_FILEOBJECT_CONTEXT);
    ntStatus = WdfObjectAllocateContext(FileObject,
                                        &attributes,
                                        (VOID**)&fileContext);

    fileContext = ResourceHub_FileContextGet(FileObject);

    handled = FALSE;
    ntStatus = STATUS_SUCCESS;
    connectionProperties = NULL;
    filenameLength = 0;

    fileName = WdfFileObjectGetFileName(FileObject);
    if ((fileName != NULL) &&
        (fileName->Length != 0))
    {
        //If the string is null-terminated, Length does not include the trailing null character. 
        //So use MaximumLength field instead.
        //
        RtlInitEmptyUnicodeString(&filePart,
                                  fileName->Buffer,
                                  fileName->MaximumLength);

        // The file-name part received may begin with a leading backslash
        // in the form "\0000000012345678". If the first character is a
        // backslash, skip it.
        //
        filePart.Length = fileName->Length;
        if (filePart.Length >= sizeof(WCHAR) && filePart.Buffer[0] == L'\\')
        {
            ++filePart.Buffer;
            filePart.Length -= sizeof(WCHAR);
            filePart.MaximumLength -= sizeof(WCHAR);
        }

        if (filePart.Length < sizeof(WCHAR))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid fileName parameter");
            ntStatus = STATUS_INVALID_PARAMETER;
            goto Exit;
        }

        filePart.MaximumLength /= sizeof(WCHAR);

        ntStatus = RtlStringCchLengthW(filePart.Buffer,
                                       filePart.MaximumLength,
                                       &filenameLength);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid fileName parameter");
            goto Exit;
        }

        // Retrieve ACPI resource descriptor for this connection from Resource Hub.
        //
        RESOURCE_HUB_ID_FROM_FILE_NAME(filePart.Buffer,
                                       &id);
        ntStatus = ResourceHub_QueryConnectionProperties(DmfModule,
                                                         &id,
                                                         FileObject,
                                                         &connectionProperties);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ResourceHub_QueryConnectionProperties fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Only I2C GenericSerialBus descriptors are supported. Extract the secondaryDevice address.
        //
        connectionPropertiesBuffer = WdfMemoryGetBuffer(connectionProperties,
                                                        &connectionPropertiesLength);
        if (connectionPropertiesBuffer == NULL)
        {
            ntStatus = STATUS_UNSUCCESSFUL;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No resources returned from RH query");
            goto Exit;
        }

        ntStatus = ResourceHub_ParseGenericSerialBusDescriptor(DmfModule,
                                                               connectionPropertiesBuffer,
                                                               connectionPropertiesLength,
                                                               &serialBusType,
                                                               &typeSpecificData,
                                                               &typeSpecificDataLength,
                                                               NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ResourceHub_ParseGenericSerialBusDescriptor fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        if (serialBusType != ResourceHub_I2C)
        {
            ntStatus = STATUS_UNSUCCESSFUL;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "GenericSerialBus descriptor subtype not I2C: 0x%x ntStatus=%!STATUS!", serialBusType, ntStatus);
            goto Exit;
        }

        ntStatus = ResourceHub_ParseI2CSerialBusDescriptorSubtype(DmfModule,
                                                                  typeSpecificData,
                                                                  typeSpecificDataLength,
                                                                  &secondaryDeviceAddress);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ResourceHub_ParseI2CSerialBusDescriptorSubtype fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Success.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "secondaryDeviceAddress=0x%X request=0x%p", secondaryDeviceAddress, Request);

        fileContext->SecondaryDeviceAddress = secondaryDeviceAddress;

        WdfRequestComplete(Request,
                           ntStatus);
        handled = TRUE;
    }

Exit:

    if (connectionProperties != NULL)
    {
        WdfObjectDelete(connectionProperties);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return handled;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

IoctlHandler_IoctlRecord ResourceHub_IoctlSpecification[] =
{
    { IOCTL_SPB_EXECUTE_SEQUENCE, sizeof(SPB_TRANSFER_LIST), 0, ResourceHub_IoctlClientCallback_SpbExecuteSequence }
};

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ResourceHub_ChildModulesAdd(
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
    DMF_CONFIG_IoctlHandler ioctlHandlerModuleConfig;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);
    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // IoctlHandler
    // ------------
    //
    DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(&ioctlHandlerModuleConfig,
                                                &moduleAttributes);
    // NOTE: No GUID is necessary because device interface is not created.
    //
    ioctlHandlerModuleConfig.AccessModeFilter = IoctlHandler_AccessModeDefault;
    ioctlHandlerModuleConfig.EvtIoctlHandlerAccessModeFilter = NULL;
    ioctlHandlerModuleConfig.IoctlRecordCount = ARRAYSIZE(ResourceHub_IoctlSpecification);
    ioctlHandlerModuleConfig.IoctlRecords = ResourceHub_IoctlSpecification;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_ResourceHub_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type ResourceHub.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ResourceHub* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Create SPB Resource Hub target to receive messages sent by ACPI.
    //
    ntStatus = ResourceHub_RegisterForAcpiNotifications(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ResourceHub_RegisterForAcpiNotifications fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_ResourceHub_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type ResourceHub.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_ResourceHub* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

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
DMF_ResourceHub_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type ResourceHub.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_ResourceHub;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_ResourceHub;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_ResourceHub;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_ResourceHub);
    dmfCallbacksDmf_ResourceHub.DeviceOpen = DMF_ResourceHub_Open;
    dmfCallbacksDmf_ResourceHub.DeviceClose = DMF_ResourceHub_Close;
    dmfCallbacksDmf_ResourceHub.ChildModulesAdd = DMF_ResourceHub_ChildModulesAdd;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_ResourceHub);
    dmfCallbacksWdf_ResourceHub.ModuleFileCreate = DMF_ResourceHub_ModuleFileCreate;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_ResourceHub,
                                            ResourceHub,
                                            DMF_CONTEXT_ResourceHub,
                                            DMF_MODULE_OPTIONS_DISPATCH,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_ResourceHub.CallbacksDmf = &dmfCallbacksDmf_ResourceHub;
    dmfModuleDescriptor_ResourceHub.CallbacksWdf = &dmfCallbacksWdf_ResourceHub;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_ResourceHub,
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

// eof: Dmf_ResourceHub.c
//
