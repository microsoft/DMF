/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfUtility.c

Abstract:

    DMF Implementation:

    This Module contains general utility functions that perform commonly needed tasks
    by Clients.

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#include "DmfIncludeInternal.h"

#if defined(DMF_INCLUDE_TMH)
#include "DmfUtility.tmh"
#endif

#if defined(DMF_USER_MODE)

#pragma warning (disable : 4100 4131)
int __cdecl
_purecall (
    VOID
    )
/*++

Routine Description:

    In case there is a problem with undefined virtual functions.
    NOTE: This function is included in NTOSKRNL.lib. If you need this function, just link with
          that library.

Arguments:

    None

Return Value:

    Zero.

--*/
{
    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid Code Path");
    DmfAssert(FALSE);

    FuncExitVoid(DMF_TRACE);

    return 0;
}

#endif

_Must_inspect_result_
NTSTATUS
DMF_Utility_UserModeAccessCreate(
    _In_ WDFDEVICE Device,
    _In_opt_ const GUID* DeviceInterfaceGuid,
    _In_opt_ WCHAR* SymbolicLinkName
    )
/*++

Routine Description:

    Create a device interface and/or symbolic link.

Arguments:

    Device: Client Driver's  WDFDEVICE
    DeviceInterfaceGuid: The Device Interface GUID of the interface to expose.
    SymbolicLinkName: Name of the symbolic link to create.

Return Value:

    STATUS_SUCCESS on success, or Error Status Code on failure.

--*/
{
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);
    TraceInformation(DMF_TRACE, "%!FUNC!");

    DmfAssert(Device != NULL);
    DmfAssert((DeviceInterfaceGuid != NULL) || (SymbolicLinkName != NULL));

    if (DeviceInterfaceGuid != NULL)
    {
        // Create a device interface so that applications can find and talk send requests to this driver.
        //
        ntStatus = WdfDeviceCreateDeviceInterface(Device,
                                                  DeviceInterfaceGuid,
                                                  NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceCreateDeviceInterface fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    if (SymbolicLinkName != NULL)
    {
        UNICODE_STRING symbolicLinkName;

        // This is for legacy code.
        //
        RtlInitUnicodeString(&symbolicLinkName,
                             SymbolicLinkName);
        ntStatus = WdfDeviceCreateSymbolicLink(Device,
                                               &symbolicLinkName);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceCreateSymbolicLink fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
BOOLEAN
DMF_Utility_IsEqualGUID(
    _In_ GUID* Guid1,
    _In_ GUID* Guid2
    )
/*++

Routine Description:

    Determines whether the two GUIds are equal.

Arguments:

    Guid1 - The first GUID.
    Guid2 - The second GUID.

Return Value:

    TRUE - If the GUIDs are equal.
    FALSE - Otherwise.

--*/
{
    BOOLEAN returnValue;
#if defined(__cplusplus)
    if (IsEqualGUID(*Guid1,
                    *Guid2))
#else
    if (IsEqualGUID(Guid1,
                    Guid2))
#endif // defined(__cplusplus)
    {
        returnValue = TRUE;
    }
    else
    {
        returnValue = FALSE;
    }

    return returnValue;
}

#if defined(DMF_USER_MODE)

VOID
DMF_Utility_DelayMilliseconds(
    _In_ ULONG Milliseconds
    )
/*++

Routine Description:

    Cause the current User-mode thread to Sleep for a certain time.

Arguments:

    Milliseconds: Time to sleep in milliseconds.

Return Value:

    None

--*/
{
    FuncEntryArguments(DMF_TRACE, "Milliseconds=%d", Milliseconds);

    Sleep(Milliseconds);

    FuncExitVoid(DMF_TRACE);
}

#else

VOID
DMF_Utility_DelayMilliseconds(
    _In_ ULONG Milliseconds
    )
/*++

Routine Description:

    Cause the current Kernel-mode thread to Sleep for a certain time.

Arguments:

    Milliseconds: Time to sleep in milliseconds.

Return Value:

    None

--*/
{
    LARGE_INTEGER intervalMs;

    FuncEntryArguments(DMF_TRACE, "Milliseconds=%d", Milliseconds);

    intervalMs.QuadPart = WDF_REL_TIMEOUT_IN_MS(Milliseconds);
    KeDelayExecutionThread(KernelMode,
                           FALSE,
                           &intervalMs);

    FuncExitVoid(DMF_TRACE);
}

#endif // defined(DMF_USER_MODE)

#if defined(DMF_KERNEL_MODE)

#pragma code_seg("PAGE")
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Utility_AclPropagateInDeviceStack(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Attempts to propagate our ACL's from the device to the FDO.

Arguments:

    Device -- WDFDEVICE object.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    PDEVICE_OBJECT wdmDeviceObject;
    HANDLE fileHandle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    fileHandle = NULL;

    // Get the FDO from our WDFDEVICE.
    //
    wdmDeviceObject = WdfDeviceWdmGetDeviceObject(Device);

    // Given the pointer to the FDO, get a handle.
    //
    ntStatus = ObOpenObjectByPointer(wdmDeviceObject,
                                     OBJ_KERNEL_HANDLE,
                                     NULL,
                                     WRITE_DAC,
                                     0,
                                     KernelMode,
                                     &fileHandle);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ObOpenObjectByPointer() fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Set the security that's already in the FDO onto the file handle thus setting
    // the security Access Control Layer (ACL) up and down the device stack.
    //

    // 'The member of struct should not be accessed by a driver'
    //
    #pragma warning(suppress:28175)
    ntStatus = ZwSetSecurityObject(fileHandle,
                                   DACL_SECURITY_INFORMATION,
                                   wdmDeviceObject->SecurityDescriptor);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ZwSetSecurityObject() fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    // Cleanup...
    //
    if (fileHandle != NULL)
    {
        ZwClose(fileHandle);
        fileHandle = NULL;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#endif // defined(DMF_KERNEL_MODE)

#pragma code_seg("PAGE")
VOID
DMF_Utility_EventLoggingNamesGet(
    _In_ WDFDEVICE Device,
    _Out_ PCWSTR* DeviceName,
    _Out_ PCWSTR* Location
    )
/*++

Routine Description:

    Get the device name and location for a given WDFDEVICE.

Arguments:

    Device -- The given WDFDEVICE.
    DeviceName - The returned device name string.
    Location - The returned location string.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    WDFMEMORY deviceNameMemory = NULL;
    WDFMEMORY locationMemory = NULL;

    // We want both memory objects to be children of the device so they will
    // be deleted automatically when the device is removed.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = Device;

    // First get the length of the string. If the FriendlyName is not there then get the 
    // length of device description.
    //
    ntStatus = WdfDeviceAllocAndQueryProperty(Device,
                                              DevicePropertyFriendlyName,
                                              NonPagedPoolNx,
                                              &objectAttributes,
                                              &deviceNameMemory);
    if (!NT_SUCCESS(ntStatus))
    {
        ntStatus = WdfDeviceAllocAndQueryProperty(Device,
                                                  DevicePropertyDeviceDescription,
                                                  NonPagedPoolNx,
                                                  &objectAttributes,
                                                  &deviceNameMemory);
    }

    if (NT_SUCCESS(ntStatus))
    {
        *DeviceName = (PCWSTR)WdfMemoryGetBuffer(deviceNameMemory, 
                                                 NULL);
    }
    else
    {
        *DeviceName = L"(error retrieving name)";
    }

    // Retrieve the device location string.
    //
    ntStatus = WdfDeviceAllocAndQueryProperty(Device,
                                              DevicePropertyLocationInformation,
                                              NonPagedPoolNx,
                                              WDF_NO_OBJECT_ATTRIBUTES,
                                              &locationMemory);
    if (NT_SUCCESS(ntStatus))
    {
        *Location = (PCWSTR)WdfMemoryGetBuffer(locationMemory, 
                                               NULL);
    }
    else
    {
        *Location = L"(error retrieving location)";
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#if !defined(DMF_USER_MODE)

typedef
NTSTATUS
(*PFN_IO_GET_ACTIVITY_ID_IRP) (
    _In_ IRP* Irp,
    _Out_ LPGUID Guid
    );

// Global function pointer set in DriverEntry.
//
PFN_IO_GET_ACTIVITY_ID_IRP g_DMF_IoGetActivityIdIrp;

GUID
DMF_Utility_ActivityIdFromRequest(
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    Given a WDFREQUEST, get its corresponding Activity Id. If it cannot be retrieved use the
    handle of the given WDFREQUEST.

Arguments:

    Request - The given WDFREQUEST.

Return Value:

    GUID that represents the returned Activity Id.

--*/
{
    GUID activityId = {0};
    NTSTATUS ntstatus;

    // Only try to get the function pointer if it has not been retrieved yet.
    //
    if (NULL == g_DMF_IoGetActivityIdIrp)
    {
        UNICODE_STRING functionName;

        // IRP activity ID functions are available on some versions, save them into
        // globals (or NULL if not available)
        //
        RtlInitUnicodeString(&functionName, 
                             L"IoGetActivityIdIrp");
        g_DMF_IoGetActivityIdIrp = (PFN_IO_GET_ACTIVITY_ID_IRP) (ULONG_PTR)MmGetSystemRoutineAddress(&functionName);
    }

    if (g_DMF_IoGetActivityIdIrp != NULL)
    {
        IRP* irp;

        // Use activity ID generated by application (or IO manager)
        //
        irp = WdfRequestWdmGetIrp(Request);
        ntstatus = g_DMF_IoGetActivityIdIrp(irp, 
                                            &activityId);
    }
    else
    {
        ntstatus = STATUS_UNSUCCESSFUL;
    }

    if (! NT_SUCCESS(ntstatus))
    {
        // Fall back to using the WDFREQUEST handle as the Activity Id.
        //
        RtlCopyMemory(&activityId, 
                      &Request, 
                      sizeof(WDFREQUEST));
    }

    return activityId;
}

#endif // !defined(DMF_USER_MODE)

GUID
DMF_Utility_ActivityIdFromDevice(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Given a WDFDEVICE, get its corresponding Activity Id.

Arguments:

    Request - The given WDFREQUEST.

Return Value:

    GUID that represents the returned Activity Id.

--*/
{
    GUID activity;

    RtlCopyMemory(&activity, 
                  &Device, 
                  sizeof(WDFDEVICE));

    return activity;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Utility_LogEmitString(
    _In_ DMFMODULE DmfModule,
    _In_ DmfLogDataSeverity DmfLogDataSeverity,
    _In_ WCHAR* FormatString,
    ...
    )
/*++

Routine Description:

    This routine raises events by calling the ETW callback function registered by
    the client. It creates a DMF_EVENT structure for the Module with a String type
    EventData argument.
    NOTE: Do not let user-mode driver pass user requests here.
    NOTE: Passing a single-byte string with a wide %s can cause stack overrun. 

Arguments:

    DmfModule - DMF Module raising the event.
    DmfLogDataSeverity - Used by Client to identify the severity of the event.
    FormatString - The format specifier for each argument passed below.
    ... - Variable list of insertion strings.

Return Value:

    None. If there is an error, no error is logged.

--*/
{
    DMF_OBJECT* dmfObject;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    va_list argumentList;
    NTSTATUS ntStatus;
    WDFMEMORY  writeBufferMemoryHandle;
    WDF_OBJECT_ATTRIBUTES attributes;
    DMF_LOG_DATA dmfLogData;
    UNICODE_STRING outputString;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    DmfAssert((DmfLogDataSeverity >= DmfLogDataSeverity_Critical) &&
              (DmfLogDataSeverity < DmfLogDataSeverity_Maximum));

    writeBufferMemoryHandle = NULL;

    // Extract object to get to event callback.
    //
    dmfObject = DMF_ModuleToObject(DmfModule);

    DmfAssert(dmfObject->ParentDevice != NULL);
    dmfDeviceContext = DmfDeviceContextGet(dmfObject->ParentDevice);

    if (NULL == dmfDeviceContext->EvtDmfDeviceLog)
    {
        // Client driver did not register the callback so there is
        // nothing to do.
        //
        goto Exit;
    }

    // Initialize the Unicode string. Allow for final zero termination.
    //
    outputString.Length = 0;
    outputString.MaximumLength = (DMF_EVENTLOG_MAXIMUM_LENGTH_OF_STRING + 1) * sizeof(WCHAR);

    // Allocate buffer for the output Unicode string.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    ntStatus = WdfMemoryCreate(&attributes,
                                NonPagedPoolNx,
                                DMF_TAG7,
                                outputString.MaximumLength,
                                &writeBufferMemoryHandle,
                                (PVOID*)&outputString.Buffer);

    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

#if defined(DMF_USER_MODE)

    // Zero out the string buffer.
    //
    ZeroMemory(outputString.Buffer,
               outputString.MaximumLength);

    va_start(argumentList,
             FormatString);

    HRESULT hResult;

    hResult = StringCchVPrintfW(outputString.Buffer,
                                DMF_EVENTLOG_MAXIMUM_LENGTH_OF_STRING,
                                FormatString,
                                argumentList);

    va_end(argumentList);

    // Allow truncated results to pass to Client.
    //
    if (hResult != S_OK &&
        hResult != STRSAFE_E_INSUFFICIENT_BUFFER)
    {
        DmfAssert(FALSE);
        goto Exit;
    }

#else

    // Zero out the string buffer.
    //
    RtlZeroMemory(outputString.Buffer,
                  outputString.MaximumLength);

    va_start(argumentList,
             FormatString);

    // Put arguments inside format strings.
    //
    ntStatus = RtlUnicodeStringVPrintf(&outputString,
                                       FormatString,
                                       argumentList);

    va_end(argumentList);

    if (!NT_SUCCESS(ntStatus))
    {
        DmfAssert(FALSE);
        goto Exit;
    }
#endif

    device = DMF_ParentDeviceGet(DmfModule);

    // Send the string to the callback.
    //
    dmfLogData.DmfLogDataType = DmfLogDataType_String;
    dmfLogData.DmfLogDataSeverity = DmfLogDataSeverity;
    dmfLogData.LogData.StringArgument.Message = outputString.Buffer;
    dmfDeviceContext->EvtDmfDeviceLog(device,
                                      dmfLogData);


Exit:

    // Clear all allocated memory.
    //
    if (writeBufferMemoryHandle != NULL)
    {
        WdfObjectDelete(writeBufferMemoryHandle);
    }

    FuncExitNoReturn(DMF_TRACE);
}

VOID
DMF_Utility_TransferList(
    _Out_ LIST_ENTRY* DestinationList, 
    _In_ LIST_ENTRY* SourceList
    )
/*++

Routine Description:

    Transfers the head in SourceList to DestinationList LIST_ENTRY structure.

Arguments:

    DestinationList - Pointer to LIST_ENTRY destination.
    SourceList - Pointer to LIST_ENTRY source.

Return Value:

    None

--*/
{
    if (IsListEmpty(SourceList))
    {
        InitializeListHead(DestinationList);
    }
    else
    {
        DestinationList->Flink = SourceList->Flink;
        DestinationList->Blink = SourceList->Blink;
        DestinationList->Flink->Blink = DestinationList;
        DestinationList->Blink->Flink = DestinationList;
        InitializeListHead(SourceList);
    }
}  

_Must_inspect_result_
NTSTATUS
DMF_Utility_TemperatureInDeciKelvin(
    _In_ INT64 Celcius,
    _Out_ UINT64* DeciKelvin
    )
/*++

Routine Description:

    Converts Celsius temperature into deci-Kelvin.

Arguments:

    Celcius - Temperature in Celcius
    DeciKelvin - Temperature in DeciKelvin

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    LONGLONG productResult;
    LONGLONG sumResult;

    // This converts Celcius to DeciCelcius.
    //
    const LONGLONG multiplier = 10;
    // This converts DeciCelcius to DeciKelvin
    //
    const LONGLONG addend = 2731;

    // deciKelvin = (Celcius * 10) + 2731

#if defined(DMF_USER_MODE)

    HRESULT hResult;

    hResult = Long64Mult(Celcius,
                         multiplier,
                         &productResult);
    if (hResult != S_OK)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    hResult = Long64Add(productResult,
                        addend,
                        &sumResult);
    if (hResult != S_OK)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    *DeciKelvin = (UINT64)sumResult;
    ntStatus = STATUS_SUCCESS;

#elif defined(DMF_KERNEL_MODE)

    ntStatus = RtlLong64Mult(Celcius,
                             multiplier,
                             &productResult);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ntStatus = RtlLong64Add(productResult,
                            addend,
                            &sumResult);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    *DeciKelvin = (UINT64)sumResult;

#endif

Exit:

    return ntStatus;
}

_Must_inspect_result_
NTSTATUS
DMF_Utility_TemperatureInDeciKelvin32(
    _In_ INT32 Celcius,
    _Out_ UINT32* DeciKelvin
    )
/*++

Routine Description:

    Converts Celsius temperature into deci-Kelvin.

Arguments:

    Celcius - Temperature in Celcius
    DeciKelvin - Temperature in DeciKelvin

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    INT productResult;
    INT sumResult;

    // This converts Celcius to DeciCelcius.
    //
    const INT multiplier = 10;
    // This converts DeciCelcius to DeciKelvin
    //
    const INT addend = 2731;

    // deciKelvin = (Celcius * 10) + 2731

#if defined(DMF_USER_MODE)

    HRESULT hResult;

    hResult = Int32Mult(Celcius,
                        multiplier,
                        &productResult);
    if (hResult != S_OK)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    hResult = Int32Add(productResult,
                       addend,
                       &sumResult);
    if (hResult != S_OK)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    *DeciKelvin = (UINT64)sumResult;
    ntStatus = STATUS_SUCCESS;

#elif defined(DMF_KERNEL_MODE)

    ntStatus = RtlInt32Mult(Celcius,
                            multiplier,
                            &productResult);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ntStatus = RtlInt32Add(productResult,
                           addend,
                           &sumResult);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    *DeciKelvin = (UINT32)sumResult;

#endif

Exit:

    return ntStatus;
}

#define BYTES_PER_ROW 16
// Size of memory used to store the string representation of a single row
// 3 bytes required for each byte in the buffer for hex representation and space i.e. 2 -> "02 "
// 1 byte required for end of string character
//
#define BUFFER_ROW_STRING_SIZE_IN_BYTES BYTES_PER_ROW * 3 + 1

VOID
DMF_Utility_LogBuffer(
    _In_reads_(BufferSize) VOID* Buffer,
    _In_ size_t BufferSize
    )
/*++

Routine Description:

    Logs buffer in hex format with 16 bytes per row.

Arguments:

    Buffer - The buffer to be logged.
    BufferSize - The number of bytes in Buffer.

Return Value:

    VOID

--*/
{
    size_t bufferRemaining;
    UINT8* data;
    char bufferRowString[BUFFER_ROW_STRING_SIZE_IN_BYTES];

    FuncEntry(DMF_TRACE);

    bufferRemaining = BufferSize;
    data = (UINT8*)Buffer;

    while (bufferRemaining >= BYTES_PER_ROW)
    {
        // Build string for a single row of bytes.
        //
        sprintf_s(bufferRowString,
                  BUFFER_ROW_STRING_SIZE_IN_BYTES,
                  "");
        for (size_t bufferRowIndex = 0; bufferRowIndex < BYTES_PER_ROW; bufferRowIndex++)
        {
            sprintf_s(bufferRowString,
                      BUFFER_ROW_STRING_SIZE_IN_BYTES,
                      "%s %02x",
                      bufferRowString,
                      data[bufferRowIndex]);
        }

        TraceInformation(DMF_TRACE, "%s", bufferRowString);

        bufferRemaining -= BYTES_PER_ROW;
        data += BYTES_PER_ROW;
    }
    
    if (bufferRemaining > 0)
    {
        // Build string for remaining bytes in buffer.
        //
        sprintf_s(bufferRowString,
                  BUFFER_ROW_STRING_SIZE_IN_BYTES,
                  "");
        for (size_t bufferRowIndex = 0; bufferRowIndex < bufferRemaining; bufferRowIndex++)
        {
            sprintf_s(bufferRowString,
                      BUFFER_ROW_STRING_SIZE_IN_BYTES,
                      "%s %02x",
                      bufferRowString,
                      data[bufferRowIndex]);
        }

        TraceInformation(DMF_TRACE, "%s", bufferRowString);
    }

    FuncExitVoid(DMF_TRACE);
}

#define CRC_INITIAL_VALUE 0xFFFFU
#define CRC(CrcValue, Data) ((UINT16)((CrcValue) << 8) ^ CrcTable[(((CrcValue) >> 8) ^ ((UINT16)(Data) & 0xFFU))])

static
const
UINT16
CrcTable[256] =
{
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7, 0x8108,
    0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef, 0x1231, 0x0210,
    0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6, 0x9339, 0x8318, 0xb37b,
    0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de, 0x2462, 0x3443, 0x0420, 0x1401,
    0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee,
    0xf5cf, 0xc5ac, 0xd58d, 0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6,
    0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d,
    0xc7bc, 0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b, 0x5af5,
    0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12, 0xdbfd, 0xcbdc,
    0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a, 0x6ca6, 0x7c87, 0x4ce4,
    0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41, 0xedae, 0xfd8f, 0xcdec, 0xddcd,
    0xad2a, 0xbd0b, 0x8d68, 0x9d49, 0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13,
    0x2e32, 0x1e51, 0x0e70, 0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a,
    0x9f59, 0x8f78, 0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e,
    0xe16f, 0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e, 0x02b1,
    0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256, 0xb5ea, 0xa5cb,
    0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d, 0x34e2, 0x24c3, 0x14a0,
    0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xa7db, 0xb7fa, 0x8799, 0x97b8,
    0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657,
    0x7676, 0x4615, 0x5634, 0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9,
    0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882,
    0x28a3, 0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92, 0xfd2e,
    0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9, 0x7c26, 0x6c07,
    0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1, 0xef1f, 0xff3e, 0xcf5d,
    0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8, 0x6e17, 0x7e36, 0x4e55, 0x5e74,
    0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

_Must_inspect_result_
_IRQL_requires_same_
UINT16
DMF_Utility_CrcCompute(
    _In_reads_bytes_(NumberOfBytes) UINT8* Message,
    _In_ UINT32 NumberOfBytes
    )
/*++

Routine Description:

    This function computes the 16-bit CRC of the given message and
    returns it.

Arguments:

    Message - Pointer to the given message.
    NumberOfBytes - Number of bytes to compute the CRC over.

Return Value:

    16-bit CRC corresponding to the given message.

--*/
{
    UINT32 messageIndex;
    UINT16 accumulatedValue;

    accumulatedValue = CRC_INITIAL_VALUE;
    for (messageIndex = 0U; messageIndex < NumberOfBytes; messageIndex++)
    {
        accumulatedValue = CRC(accumulatedValue,
                               Message[messageIndex]);
    }

    return accumulatedValue;
}

_IRQL_requires_same_
VOID
DMF_Utility_SystemTimeCurrentGet(
    _Out_ PLARGE_INTEGER CurrentSystemTime
    )
/*++

Routine Description:

    This function fetches the current system time in UTC.

Arguments:

    CurrentSystemTime - Pointer to store current system time.

Return Value:

    Current system time in UTC as LARGE INTEGER.

--*/
{
#if defined(DMF_KERNEL_MODE)
    KeQuerySystemTime(CurrentSystemTime);
#elif defined(DMF_USER_MODE)
    FILETIME SystemTimeAsFileTime;
    GetSystemTimeAsFileTime(&SystemTimeAsFileTime);
    CurrentSystemTime->LowPart = SystemTimeAsFileTime.dwLowDateTime;
    CurrentSystemTime->HighPart = SystemTimeAsFileTime.dwHighDateTime;
#endif
}

_Must_inspect_result_
_IRQL_requires_same_
BOOLEAN
DMF_Utility_LocalTimeToUniversalTimeConvert(
    _In_ DMF_TIME_FIELDS* LocalTimeFields,
    _Out_ DMF_TIME_FIELDS* UtcTimeFields
    )
/*++

Routine Description:

    Converts the given local time to universal time.

Arguments:

    LocalTimeFields - The given local time.
    UtcTimeFields - The returned universal time on success.

Return Value:

    TRUE if the call succeeds; FALSE, otherwise.

--*/
{
    BOOLEAN returnValue;

    RtlZeroMemory(UtcTimeFields,
                  sizeof(DMF_TIME_FIELDS));

#if defined(DMF_KERNEL_MODE)
    LARGE_INTEGER convertedTime;
    TIME_FIELDS _localTimeFields;
    TIME_FIELDS _utcTimeFields;

    _localTimeFields.Day = LocalTimeFields->Day;
    _localTimeFields.Month = LocalTimeFields->Month;
    _localTimeFields.Year = LocalTimeFields->Year;
    _localTimeFields.Hour = LocalTimeFields->Hour;
    _localTimeFields.Minute = LocalTimeFields->Minute;
    _localTimeFields.Second = LocalTimeFields->Second;
    _localTimeFields.Milliseconds = LocalTimeFields->Milliseconds;
    _localTimeFields.Weekday = LocalTimeFields->Weekday;

    if (!RtlTimeFieldsToTime(&_localTimeFields,
                             &convertedTime))
    {
        returnValue = FALSE;
        goto Exit;
    }
    else
    {
        // Convert Local time to UTC time
        //
        ExLocalTimeToSystemTime(&convertedTime,
                                &convertedTime);
    }

    RtlTimeToTimeFields(&convertedTime,
                        &_utcTimeFields);

    UtcTimeFields->Day = _utcTimeFields.Day;
    UtcTimeFields->Month = _utcTimeFields.Month;
    UtcTimeFields->Year = _utcTimeFields.Year;
    UtcTimeFields->Hour = _utcTimeFields.Hour;
    UtcTimeFields->Minute = _utcTimeFields.Minute;
    UtcTimeFields->Second = _utcTimeFields.Second;
    UtcTimeFields->Milliseconds = _utcTimeFields.Milliseconds;
    UtcTimeFields->Weekday = _utcTimeFields.Weekday;

    returnValue = TRUE;
#elif defined(DMF_USER_MODE)
    SYSTEMTIME _localTimeFields;
    SYSTEMTIME _utcTimeFields;

    _localTimeFields.wDay = LocalTimeFields->Day;
    _localTimeFields.wMonth = LocalTimeFields->Month;
    _localTimeFields.wYear = LocalTimeFields->Year;
    _localTimeFields.wHour = LocalTimeFields->Hour;
    _localTimeFields.wMinute = LocalTimeFields->Minute;
    _localTimeFields.wSecond = LocalTimeFields->Second;
    _localTimeFields.wMilliseconds = LocalTimeFields->Milliseconds;
    _localTimeFields.wDayOfWeek = LocalTimeFields->Weekday;

    // Convert Local Time to Coordinated Universal Time (UTC).
    //
    if (!TzSpecificLocalTimeToSystemTime(NULL,
                                         &_localTimeFields,
                                         &_utcTimeFields))
    {
        returnValue = FALSE;
        goto Exit;
    }
    
    UtcTimeFields->Day = _utcTimeFields.wDay;
    UtcTimeFields->Month = _utcTimeFields.wMonth;
    UtcTimeFields->Year = _utcTimeFields.wYear;
    UtcTimeFields->Hour = _utcTimeFields.wHour;
    UtcTimeFields->Minute = _utcTimeFields.wMinute;
    UtcTimeFields->Second = _utcTimeFields.wSecond;
    UtcTimeFields->Milliseconds = _utcTimeFields.wMilliseconds;
    UtcTimeFields->Weekday = _utcTimeFields.wDayOfWeek;

    returnValue = TRUE;
#endif

Exit:

    return returnValue;
}

// eof: DmfUtility.c
//
