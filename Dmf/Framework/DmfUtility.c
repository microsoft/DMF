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

#include "DmfUtility.tmh"

#pragma warning (disable : 4100 4131)
int __cdecl
_purecall (
    VOID
    )
/*++

Routine Description:

    In case there is a problem with undefined virtual functions.

Arguments:

    None

Return Value:

    Zero.

--*/
{
    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid Code Path");
    ASSERT(FALSE);

    FuncExitVoid(DMF_TRACE);

    return 0;
}

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

    ASSERT(Device != NULL);
    ASSERT((DeviceInterfaceGuid != NULL) || (SymbolicLinkName != NULL));

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

#if !defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_Must_inspect_result_
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
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

#endif // !defined(DMF_USER_MODE)

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

////////////////////////////////////////////////////////////////////////////////////////////////
//
// Definitions used by the Event Log functions.
//
////////////////////////////////////////////////////////////////////////////////////////////////
//

// Identifiers for format specifiers supported in DMF event logs.
//
typedef enum
{
    FormatSize_INVALID,
    FormatSize_CHAR,
    FormatSize_SHORT,
    FormatSize_INT,
    FormatSize_POINTER
} DMF_EventLogFormatSizeType;

// The table of insertion strings.
//
typedef struct
{
    // The insertion string data to write to event log = null terminated 16 bit wide character strings.
    //
    WCHAR ArrayOfInsertionStrings[DMF_EVENTLOG_MAXIMUM_NUMBER_OF_INSERTION_STRINGS][DMF_EVENTLOG_MAXIMUM_INSERTION_STRING_LENGTH];
    // The number of insertion strings.
    //
    ULONG NumberOfInsertionStrings;
} EVENTLOG_INSERTION_STRING_TABLE;

static
_IRQL_requires_max_(PASSIVE_LEVEL)
DMF_EventLogFormatSizeType
DMF_DMF_EventLogFormatSizeTypeGet(
    _Inout_ PWCHAR FormatString
    )
/*++

Routine Description:

    This routine takes extracts the format specifier from a format string and returns a unique identifier based on it.

Arguments:

    FormatString - The format string

Return Value:

    DMF_EventLogFormatSizeType - A unique identifier based on the format specifier.

--*/
{
    DMF_EventLogFormatSizeType returnValue;

    ASSERT(FormatString != NULL);
    returnValue = FormatSize_INVALID;
    while ((*FormatString != L'\0') &&
        (FormatSize_INVALID == returnValue))
    {
        if ((L'%' == *FormatString) &&
            (*(FormatString + 1) != L'\0'))
        {
            WCHAR nextCharacter;

            nextCharacter = *(FormatString + 1);
            switch (nextCharacter)
            {
                case L'%':
                {
                    break;
                }
                case L'c':
                {
                    returnValue = FormatSize_CHAR;
                    break;
                }
                case L'd':
                case L'u':
                case L'x':
                case L'X':
                {
                    returnValue = FormatSize_INT;
                    break;
                }
                case L'p':
                // Support for Wide strings.
                //
                case L's':
                // Support for ANSI strings.
                //
                case L'S':
                {
                    returnValue = FormatSize_POINTER;
                    break;
                }
                default:
                {
                    ASSERT(FALSE);
                    goto Exit;
                }
            }
        }
        FormatString++;
    }

Exit:

    return returnValue;
}

static
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_EventLogInsertionStringTableAllocate(
    _Out_ EVENTLOG_INSERTION_STRING_TABLE** EventLogInsertionStringTable,
    _Out_ WDFMEMORY* EventLogInsertionStringTableMemory,
    _In_ INT NumberOfInsertionStrings,
    _In_ va_list ArgumentList,
    _In_ INT NumberOfFormatStrings,
    _In_ PWCHAR* FormatStrings
    )
/*++

Routine Description:

    This routine allocates an event log insertion string table and assigns it with entries formed from
    the Format strings and Argument list passed to it.

Arguments:

    EventLogInsertionStringTable - Pointer to a location that receives a handle to EventLogInsertionStringTable.
    EventLogInsertionStringTableMemory - Pointer to a location that receives a handle to memory associated with EventLogInsertionStringTable.
    NumberOfInsertionStrings - Number of insertion strings.
    ArgumentList - List of arguments (integers / characters / strings / pointers).
    NumberOfFormatStrings - Number of format strings.
    FormatStrings - Format strings with format specifiers for the arguments passed in ArgumentList.

Return Value:

    NTSTATUS

--*/
{
    INT stringIndex;
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES attributes;
    EVENTLOG_INSERTION_STRING_TABLE* eventLogInsertionStringTable;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    ASSERT(NumberOfFormatStrings == NumberOfInsertionStrings);

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    ntStatus = WdfMemoryCreate(&attributes,
                               NonPagedPoolNx,
                               DMF_TAG,
                               sizeof(EVENTLOG_INSERTION_STRING_TABLE),
                               EventLogInsertionStringTableMemory,
                               (VOID**)EventLogInsertionStringTable);

    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Unable to allocate eventLogStringTable");
        *EventLogInsertionStringTable = NULL;
        *EventLogInsertionStringTableMemory = NULL;
        goto Exit;
    }

    ASSERT(NULL != *EventLogInsertionStringTable);
    ASSERT(NULL != *EventLogInsertionStringTableMemory);

    eventLogInsertionStringTable = *EventLogInsertionStringTable;

    RtlZeroMemory(eventLogInsertionStringTable,
                  sizeof(EVENTLOG_INSERTION_STRING_TABLE));

    ASSERT(NumberOfInsertionStrings <= DMF_EVENTLOG_MAXIMUM_NUMBER_OF_INSERTION_STRINGS);
    for (stringIndex = 0; stringIndex < NumberOfFormatStrings; stringIndex++)
    {
        WCHAR* formatString;
        DMF_EventLogFormatSizeType formatSize;

        formatString = FormatStrings[stringIndex];
        ASSERT(formatString != NULL);
        ASSERT(wcslen(FormatStrings[stringIndex]) < DMF_EVENTLOG_MAXIMUM_INSERTION_STRING_LENGTH);

        formatSize = DMF_DMF_EventLogFormatSizeTypeGet(formatString);
        switch (formatSize)
        {
            case FormatSize_CHAR:
            {
                CHAR argument;

                argument = va_arg(ArgumentList,
                                  CHAR);

                swprintf_s(eventLogInsertionStringTable->ArrayOfInsertionStrings[stringIndex],
                           DMF_EVENTLOG_MAXIMUM_INSERTION_STRING_LENGTH,
                           formatString,
                           argument);
                break;
            }
            case FormatSize_INT:
            {
                INT argument;

                argument = va_arg(ArgumentList,
                                  INT);

                swprintf_s(eventLogInsertionStringTable->ArrayOfInsertionStrings[stringIndex],
                           DMF_EVENTLOG_MAXIMUM_INSERTION_STRING_LENGTH,
                           formatString,
                           argument);
                break;
            }
            case FormatSize_POINTER:
            {
                VOID* argument;

                argument = va_arg(ArgumentList,
                                  VOID*);

                swprintf_s(eventLogInsertionStringTable->ArrayOfInsertionStrings[stringIndex],
                           DMF_EVENTLOG_MAXIMUM_INSERTION_STRING_LENGTH,
                           formatString,
                           argument);
                break;
            }
            default:
            {
                ASSERT(FALSE);
                ASSERT(0 == eventLogInsertionStringTable->NumberOfInsertionStrings);
                goto Exit;
            }
        }
    }

    eventLogInsertionStringTable->NumberOfInsertionStrings = NumberOfInsertionStrings;
    ASSERT(eventLogInsertionStringTable->NumberOfInsertionStrings <= DMF_EVENTLOG_MAXIMUM_NUMBER_OF_INSERTION_STRINGS);

Exit:

    FuncExit(DMF_TRACE, "eventLogStringTable=0x%p", *EventLogInsertionStringTable);

    return ntStatus;
}

static
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_EventLogInsertionStringTableDeallocate(
    _In_ WDFMEMORY EventLogInsertionStringTableMemory
    )
/*++

Routine Description:

    This routine deallocates memory allocated to the insertion string table.

Arguments:

    EventLogInsertionStringTableMemory - Memory handle associated with the insertion string table.

Return Value:

    None. If there is an error, no error is logged.

--*/
{
    FuncEntry(DMF_TRACE);

    WdfObjectDelete(EventLogInsertionStringTableMemory);

    FuncExitNoReturn(DMF_TRACE);
}

static
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Utility_InsertionStringTableCreate(
    _Out_ EVENTLOG_INSERTION_STRING_TABLE** EventLogInsertionStringTable,
    _Out_ WDFMEMORY* EventLogInsertionStringTableMemory,
    _In_ INT NumberOfInsertionStrings,
    _In_ va_list ArgumentList,
    _In_ INT NumberOfFormatStrings,
    _In_opt_ PWCHAR* FormatStrings
    )
/*++

Routine Description:

    This routine creates the insertion string table.

Arguments:

    EventLogInsertionStringTable - Pointer to a location that receives a handle to EventLogInsertionStringTable.
    EventLogInsertionStringTableMemory - Pointer to a location that receives a handle to memory associated with EventLogInsertionStringTable.
    NumberOfInsertionStrings - Number of insertion strings.
    ArgumentList - List of arguments (integers / characters / strings / pointers).
    NumberOfFormatStrings - Number of format strings.
    FormatStrings - Format strings with format specifiers for the arguments passed in ArgumentList.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    ntStatus = STATUS_SUCCESS;

    ASSERT(NumberOfInsertionStrings == NumberOfFormatStrings);
    ASSERT(NumberOfInsertionStrings <= DMF_EVENTLOG_MAXIMUM_NUMBER_OF_INSERTION_STRINGS);
    if (0 == NumberOfInsertionStrings)
    {
        ASSERT(NULL == FormatStrings);
        *EventLogInsertionStringTable = NULL;
        *EventLogInsertionStringTableMemory = NULL;
        goto Exit;
    }

    ASSERT(FormatStrings != NULL);
    // FormatStrings must not be NULL. But it is not because we have asserted above.
    //
    #pragma warning(suppress:6387)
    ntStatus = DMF_EventLogInsertionStringTableAllocate(EventLogInsertionStringTable,
                                                        EventLogInsertionStringTableMemory,
                                                        NumberOfInsertionStrings,
                                                        ArgumentList,
                                                        NumberOfFormatStrings,
                                                        FormatStrings);
    if (NT_SUCCESS(ntStatus))
    {
        ASSERT(NULL != *EventLogInsertionStringTable);
        ASSERT(NULL != *EventLogInsertionStringTableMemory);
    }
    else
    {
        ASSERT(NULL == *EventLogInsertionStringTable);
        ASSERT(NULL == *EventLogInsertionStringTableMemory);
    }

Exit:

    return ntStatus;
}

#if !defined(DMF_USER_MODE)

// Event log support for Kernel-mode Drivers.
//
static
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Utility_EventLogEntryWriteToEventLogFile(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ NTSTATUS ErrorCode,
    _In_ NTSTATUS FinalNtStatus,
    _In_ ULONG UniqueId,
    _In_ ULONG TextLength,
    _In_opt_ PCWSTR Text,
    _In_opt_ EVENTLOG_INSERTION_STRING_TABLE* EventLogInsertionStringTable
    )
/*++

Routine Description:

    This routine allocates an error log entry, copies the supplied data
    to it, and requests that it be written to the event log file. This is the root
    function that writes all logging entries.

Arguments:

    DriverObject - WDM Driver object.

    ErrorCode - ErrorCode from header generated by mc compiler.
    FinalNtStatus - The final NTSTATUS for the error being logged.
    UniqueId - A unique long word that identifies the particular
               call to this function.
    NumberOfInsertionStrings - Number of insertion strings.
    TextLength - The length in bytes (including the terminating NULL)
                 of the Text string.
    Text - Additional data to add to be included in the error log.
    EventLogInsertionStringTable - A structure containing a table of insertion strings
                                   and the number of insertion strings.

Return Value:

    None. If there is an error, no error is logged.

--*/
{
    PIO_ERROR_LOG_PACKET errorLogEntry;
    SIZE_T totalLength;
    SIZE_T textOffset;
    SIZE_T insertionStringOffset;
    ULONG stringIndex;
    ULONG numberOfInsertionStrings;

    FuncEntry(DMF_TRACE);

    ASSERT(DriverObject != NULL);

    textOffset = 0;
    insertionStringOffset = 0;
    numberOfInsertionStrings = 0;

    totalLength = sizeof(IO_ERROR_LOG_PACKET);

    // Determine if we need space for the text.
    //
    if (NULL != Text)
    {
        ASSERT(TextLength > 0);
        // Calculate how big the error log packet needs to be to hold everything.
        // Get offset for text data.
        //
        textOffset = totalLength;

        // Add one for NULL terminating character.
        //
        totalLength += ((TextLength + 1) * sizeof(WCHAR));
    }
    else
    {
        ASSERT(TextLength == 0);
    }

    if (EventLogInsertionStringTable != NULL)
    {
        insertionStringOffset = totalLength;
        numberOfInsertionStrings = EventLogInsertionStringTable->NumberOfInsertionStrings;

        ASSERT(numberOfInsertionStrings > 0);
        ASSERT(numberOfInsertionStrings <= DMF_EVENTLOG_MAXIMUM_NUMBER_OF_INSERTION_STRINGS);
        for (stringIndex = 0; stringIndex < numberOfInsertionStrings; stringIndex++)
        {
            // Add one for NULL terminating character.
            //
            totalLength += (wcslen(EventLogInsertionStringTable->ArrayOfInsertionStrings[stringIndex]) + 1) * (sizeof(WCHAR));
        }
    }

    // Determine if the text will fit into the error log packet.
    //
    if (totalLength > ERROR_LOG_MAXIMUM_SIZE)
    {
        ASSERT(FALSE);
        totalLength = sizeof(IO_ERROR_LOG_PACKET);
    }

    errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(DriverObject,
                                                                  (UCHAR)totalLength);

    // If errorLogEntry in NULL, this is not considered an error situation.  Just return.
    //
    if (errorLogEntry != NULL)
    {
        // Initialize the error log packet.
        //
        errorLogEntry->MajorFunctionCode = 0;
        errorLogEntry->RetryCount = 0;
        errorLogEntry->EventCategory = 0;
        errorLogEntry->ErrorCode = ErrorCode;
        errorLogEntry->UniqueErrorValue = UniqueId;
        errorLogEntry->FinalStatus = FinalNtStatus;
        errorLogEntry->SequenceNumber = 0;
        errorLogEntry->IoControlCode = 0;

        if ((totalLength > textOffset) && (NULL != Text))
        {
            // Able to include text
            //
            errorLogEntry->DumpDataSize = (USHORT)TextLength;

            RtlCopyMemory(errorLogEntry->DumpData,
                          Text,
                          (totalLength - textOffset));
        }

        // Write the insertion strings.
        //
        if ((totalLength > insertionStringOffset) && (numberOfInsertionStrings > 0))
        {
            ASSERT(numberOfInsertionStrings <= DMF_EVENTLOG_MAXIMUM_NUMBER_OF_INSERTION_STRINGS);
            // Able to include insertion strings.
            //
            errorLogEntry->NumberOfStrings = (USHORT)numberOfInsertionStrings;
            errorLogEntry->StringOffset = (USHORT)insertionStringOffset;

            for (stringIndex = 0; stringIndex < numberOfInsertionStrings; stringIndex++)
            {
                size_t stringLength;
                UCHAR* targetAddress;
                WCHAR* sourceString;

                // Add one for NULL terminator.
                //
                sourceString = EventLogInsertionStringTable->ArrayOfInsertionStrings[stringIndex];
                stringLength = (wcslen(sourceString) + 1) * (sizeof(WCHAR));
                targetAddress = (UCHAR*)errorLogEntry + insertionStringOffset;
                RtlCopyMemory(targetAddress,
                              sourceString,
                              stringLength);
                insertionStringOffset += stringLength;
            }
        }
        else
        {
            // Couldn't include or caller didn't specify a text string.
            //
            errorLogEntry->NumberOfStrings = 0;
            errorLogEntry->StringOffset = 0;
        }

        // Request that the error log packet be written to the error log file.
        //
        IoWriteErrorLogEntry(errorLogEntry);
    }

    FuncExitNoReturn(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Utility_EventLogEntryWriteDriverObject(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ NTSTATUS ErrorCode,
    _In_ NTSTATUS FinalNtStatus,
    _In_ ULONG UniqueId,
    _In_ ULONG TextLength,
    _In_opt_ PCWSTR Text,
    _In_ INT NumberOfFormatStrings,
    _In_opt_ PWCHAR* FormatStrings,
    _In_ INT NumberOfInsertionStrings,
    ...
    )
/*++

Routine Description:

    This routine creates an insertion string table and requests the arguments and the table be written to event log file.

Arguments:

    DriverObject - WDM Driver object.
    ErrorCode - ErrorCode from header generated by mc compiler.
    FinalNtStatus - The final NTSTATUS for the error being logged.
    UniqueId - A unique long word that identifies the particular
               call to this function.
    TextLength - The length in bytes (including the terminating NULL)
                 of the Text string.
    Text - Additional data to add to be included in the error log.
    NumberOfFormatStrings - Number of format strings.
    FormatStrings - An array of format specifiers for each argument passed below.
    NumberOfInsertionStrings - Number of insertion strings.
    ... - Variable list of insertion strings.

Return Value:

    None. If there is an error, no error is logged.

--*/
{
    EVENTLOG_INSERTION_STRING_TABLE* eventLogStringTable;
    va_list argumentList;
    WDFMEMORY eventLogStringTableMemory;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    eventLogStringTable = NULL;
    eventLogStringTableMemory = NULL;

    ASSERT(NumberOfInsertionStrings <= DMF_EVENTLOG_MAXIMUM_NUMBER_OF_INSERTION_STRINGS);

    va_start(argumentList,
             NumberOfInsertionStrings);

    ntStatus = DMF_Utility_InsertionStringTableCreate(&eventLogStringTable,
                                                      &eventLogStringTableMemory,
                                                      NumberOfInsertionStrings,
                                                      argumentList,
                                                      NumberOfFormatStrings,
                                                      FormatStrings);

    va_end(argumentList);

    DMF_Utility_EventLogEntryWriteToEventLogFile(DriverObject,
                                                 ErrorCode,
                                                 FinalNtStatus,
                                                 UniqueId,
                                                 TextLength,
                                                 Text,
                                                 eventLogStringTable);

    if (eventLogStringTableMemory)
    {
        DMF_EventLogInsertionStringTableDeallocate(eventLogStringTableMemory);
        eventLogStringTableMemory = NULL;
        eventLogStringTable = NULL;
    }

    FuncExitNoReturn(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Utility_EventLogEntryWriteDriver(
    _In_ WDFDRIVER Driver,
    _In_ NTSTATUS ErrorCode,
    _In_ NTSTATUS FinalNtStatus,
    _In_ ULONG UniqueId,
    _In_ ULONG TextLength,
    _In_opt_ PCWSTR Text,
    _In_ INT NumberOfFormatStrings,
    _In_opt_ PWCHAR* FormatStrings,
    _In_ INT NumberOfInsertionStrings,
    ...
    )
/*++

Routine Description:

    This routine creates an insertion string table and requests the arguments and the table be
    written to event log file.
    This function requires a WDFDRIVER object. It is designed to be called from Client Driver
    when a WDFDEVICE object is not available.

Arguments:

    Driver - WDF Driver.
    ErrorCode - ErrorCode from header generated by mc compiler.
    FinalNtStatus - The final NTSTATUS for the error being logged.
    UniqueId - A unique long word that identifies the particular
               call to this function.
    TextLength - The length in bytes (including the terminating NULL)
                 of the Text string.
    Text - Additional data to add to be included in the error log.
    NumberOfFormatStrings - Number of format strings.
    FormatStrings - An array of format specifiers for each argument passed below.
    NumberOfInsertionStrings - Number of insertion strings.
    ... - Variable list of insertion strings.

Return Value:

    None. If there is an error, no error is logged.

--*/
{
    PDRIVER_OBJECT driverObject;
    EVENTLOG_INSERTION_STRING_TABLE* eventLogStringTable;
    va_list argumentList;
    WDFMEMORY eventLogStringTableMemory;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    eventLogStringTable = NULL;
    eventLogStringTableMemory = NULL;

    ASSERT(NumberOfInsertionStrings <= DMF_EVENTLOG_MAXIMUM_NUMBER_OF_INSERTION_STRINGS);

    va_start(argumentList,
             NumberOfInsertionStrings);

    ntStatus = DMF_Utility_InsertionStringTableCreate(&eventLogStringTable,
                                                      &eventLogStringTableMemory,
                                                      NumberOfInsertionStrings,
                                                      argumentList,
                                                      NumberOfFormatStrings,
                                                      FormatStrings);

    va_end(argumentList);

    // Get the associated WDM Driver Object.
    //
    driverObject = WdfDriverWdmGetDriverObject(Driver);

    DMF_Utility_EventLogEntryWriteToEventLogFile(driverObject,
                                                 ErrorCode,
                                                 FinalNtStatus,
                                                 UniqueId,
                                                 TextLength,
                                                 Text,
                                                 eventLogStringTable);

    if (eventLogStringTableMemory)
    {
        DMF_EventLogInsertionStringTableDeallocate(eventLogStringTableMemory);
        eventLogStringTableMemory = NULL;
        eventLogStringTable = NULL;
    }

    FuncExitNoReturn(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Utility_EventLogEntryWriteDevice(
    _In_ WDFDEVICE Device,
    _In_ NTSTATUS ErrorCode,
    _In_ NTSTATUS Status,
    _In_ ULONG UniqueId,
    _In_ ULONG TextLength,
    _In_opt_ PCWSTR Text,
    _In_ INT NumberOfFormatStrings,
    _In_opt_ PWCHAR* FormatStrings,
    _In_ INT NumberOfInsertionStrings,
    ...
    )
/*++

Routine Description:

    This routine creates an insertion string table and requests
    the arguments and the table be written to event log file.
    This function requires a WDFDEVICE object.

Arguments:

    Device - WDF Device.
    ErrorCode - ErrorCode from header generated by mc compiler.
    FinalNtStatus - The final NTSTATUS for the error being logged.
    UniqueId - A unique long word that identifies the particular
               call to this function.
    TextLength - The length in bytes (including the terminating NULL)
                 of the Text string.
    Text - Additional data to add to be included in the error log.
    NumberOfFormatStrings - Number of format strings.
    FormatStrings - An array of format specifiers for each argument passed below.
    NumberOfInsertionStrings - The number of insertion string arguments that follow.
    ... - Variable list of insertion strings.

Return Value:

    None. If there is an error, no error is logged.

--*/
{
    EVENTLOG_INSERTION_STRING_TABLE* eventLogStringTable;
    va_list argumentList;
    WDFDRIVER driver;
    PDRIVER_OBJECT driverObject;
    WDFMEMORY eventLogStringTableMemory;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    eventLogStringTable = NULL;
    eventLogStringTableMemory = NULL;

    ASSERT(NumberOfInsertionStrings <= DMF_EVENTLOG_MAXIMUM_NUMBER_OF_INSERTION_STRINGS);
    ASSERT(Device != NULL);

    va_start(argumentList,
             NumberOfInsertionStrings);

    ntStatus = DMF_Utility_InsertionStringTableCreate(&eventLogStringTable,
                                                      &eventLogStringTableMemory,
                                                      NumberOfInsertionStrings,
                                                      argumentList,
                                                      NumberOfFormatStrings,
                                                      FormatStrings);

    va_end(argumentList);

    // Get the associated WDF Driver.
    //
    driver = WdfDeviceGetDriver(Device);
    ASSERT(driver != NULL);
    // Get the associated WDM Driver Object.
    //
    driverObject = WdfDriverWdmGetDriverObject(driver);

    DMF_Utility_EventLogEntryWriteToEventLogFile(driverObject,
                                                 ErrorCode,
                                                 Status,
                                                 UniqueId,
                                                 TextLength,
                                                 Text,
                                                 eventLogStringTable);

    if (eventLogStringTableMemory)
    {
        DMF_EventLogInsertionStringTableDeallocate(eventLogStringTableMemory);
        eventLogStringTableMemory = NULL;
        eventLogStringTable = NULL;
    }

    FuncExitNoReturn(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Utility_EventLogEntryWriteDmfModule(
    _In_ DMFMODULE DmfModule,
    _In_ NTSTATUS ErrorCode,
    _In_ NTSTATUS FinalNtStatus,
    _In_ ULONG UniqueId,
    _In_ ULONG TextLength,
    _In_opt_ PCWSTR Text,
    _In_ INT NumberOfFormatStrings,
    _In_opt_ PWCHAR* FormatStrings,
    _In_ INT NumberOfInsertionStrings,
    ...
    )
/*++

Routine Description:

    This routine creates an insertion string table and requests
    the arguments and the table be written to event log file.
    This function requires a DMF_OBJECT* object.

Arguments:

    DmfModule - This Module's handle.
    ErrorCode - ErrorCode from header generated by Message Compiler (MC.exe).
    FinalNtStatus - The final NTSTATUS for the error being logged.
    UniqueId - A unique long word that identifies the particular
               call to this function.
    TextLength - The length in bytes (including the terminating NULL)
                 of the Text string.
    Text - Additional data to add to be included in the error log.
    NumberOfFormatStrings - Number of format strings.
    FormatStrings - An array of format specifiers for each argument passed below.
    NumberOfInsertionStrings - Number of insertion strings.
    ... - Variable list of insertion strings.

Return Value:

    None. If there is an error, no error is logged.

--*/
{
    WDFDEVICE device;
    WDFDRIVER driver;
    PDRIVER_OBJECT driverObject;
    EVENTLOG_INSERTION_STRING_TABLE* eventLogStringTable;
    va_list argumentList;
    WDFMEMORY eventLogStringTableMemory;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    eventLogStringTable = NULL;
    eventLogStringTableMemory = NULL;

    ASSERT(NumberOfInsertionStrings <= DMF_EVENTLOG_MAXIMUM_NUMBER_OF_INSERTION_STRINGS);
    ASSERT(NumberOfInsertionStrings == NumberOfFormatStrings);

    va_start(argumentList,
             NumberOfInsertionStrings);

    ntStatus = DMF_Utility_InsertionStringTableCreate(&eventLogStringTable,
                                                      &eventLogStringTableMemory,
                                                      NumberOfInsertionStrings,
                                                      argumentList,
                                                      NumberOfFormatStrings,
                                                      FormatStrings);

    va_end(argumentList);

    // Get the associated WDM Device.
    //
    device = DMF_ParentDeviceGet(DmfModule);
    // Get the associated WDM Driver.
    //
    driver = WdfDeviceGetDriver(device);
    ASSERT(driver != NULL);
    // Get the associated WDM Driver Object.
    //
    driverObject = WdfDriverWdmGetDriverObject(driver);

    DMF_Utility_EventLogEntryWriteToEventLogFile(driverObject,
                                                 ErrorCode,
                                                 FinalNtStatus,
                                                 UniqueId,
                                                 TextLength,
                                                 Text,
                                                 eventLogStringTable);

    if (eventLogStringTableMemory)
    {
        DMF_EventLogInsertionStringTableDeallocate(eventLogStringTableMemory);
        eventLogStringTableMemory = NULL;
        eventLogStringTable = NULL;
    }

    FuncExitNoReturn(DMF_TRACE);
}

#else

// Event log support for User-mode Drivers.
//

// The table of insertion strings.
//
typedef struct
{
    // The insertion string data to write to event log = null terminated 16 bit wide character strings.
    //
    LPWSTR* InsertionStrings;
    // The number of insertion strings.
    //
    ULONG NumberOfInsertionStrings;
} DMF_INSERTION_STRING_LIST;

NTSTATUS
DMF_Utility_InsertionStringListCreate(
    _In_ EVENTLOG_INSERTION_STRING_TABLE* EventLogInsertionStringTable,
    _Out_ DMF_INSERTION_STRING_LIST** InsertionStringList
    )
/*++

Routine Description:

    This routine creates a closely packed insertion string list.

Arguments:

    EventLogInsertionStringTable - the insertion string table containing the insertion strings that will be part of the list.
    InsertionStringList - Pointer to a location that receives a handle to DMF_INSERTION_STRING_LIST.

Return Value:

    NTSTATUS - status of the Create operation.

--*/
{
    ULONG stringIndex;
    ULONG numberOfInsertionStrings;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    *InsertionStringList = NULL;

    if (NULL == EventLogInsertionStringTable)
    {
        goto Exit;
    }

    numberOfInsertionStrings = EventLogInsertionStringTable->NumberOfInsertionStrings;

    ASSERT(numberOfInsertionStrings > 0);
    ASSERT(numberOfInsertionStrings <= DMF_EVENTLOG_MAXIMUM_NUMBER_OF_INSERTION_STRINGS);

    // Allocate memory for the DMF_INSERTION_STRING_LIST structure.
    //
    *InsertionStringList = (DMF_INSERTION_STRING_LIST*)malloc(sizeof(DMF_INSERTION_STRING_LIST));
    if (NULL == *InsertionStringList)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    (*InsertionStringList)->InsertionStrings = NULL;
    (*InsertionStringList)->NumberOfInsertionStrings = 0;

    // Allocate memory to store a pointer to each Insertion String.
    //
    (*InsertionStringList)->InsertionStrings = (LPWSTR*)malloc(sizeof(LPWSTR) * numberOfInsertionStrings);
    if (NULL == (*InsertionStringList)->InsertionStrings)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        free(*InsertionStringList);
        *InsertionStringList = NULL;
        goto Exit;
    }

    for (stringIndex = 0; stringIndex < numberOfInsertionStrings; stringIndex++)
    {
        // Allocate memory for each insertion string to NULL.
        // 
        // 'warning C6386: Buffer overrun while writing to '((*InsertionStringList))->InsertionStrings':  the writable size is 'sizeof(WCHAR *)*numberOfInsertionStrings' bytes, but '16' bytes might be written.'
        //
        #pragma warning(suppress:6386)
        (*InsertionStringList)->InsertionStrings[stringIndex] = (LPWSTR)malloc(sizeof(WCHAR) * (wcslen(EventLogInsertionStringTable->ArrayOfInsertionStrings[stringIndex]) + 1));
    }

    // Copy the insertion strings from the table to the memory allocated.
    //
    for (stringIndex = 0; stringIndex < numberOfInsertionStrings; stringIndex++)
    {
        size_t stringLength;
        WCHAR* sourceString;

        // Add one for NULL terminator.
        //
        sourceString = EventLogInsertionStringTable->ArrayOfInsertionStrings[stringIndex];
        stringLength = (wcslen(sourceString) + 1) * (sizeof(WCHAR));
        // 'warning C6386: Buffer overrun while writing to '((*InsertionStringList))->InsertionStrings':  the writable size is 'sizeof(WCHAR *)*numberOfInsertionStrings' bytes, but '16' bytes might be written.'
        //
        #pragma warning(suppress:6385)
        RtlCopyMemory((*InsertionStringList)->InsertionStrings[stringIndex],
                      sourceString,
                      stringLength);
    }

    (*InsertionStringList)->NumberOfInsertionStrings = numberOfInsertionStrings;

Exit:

    FuncExitNoReturn(DMF_TRACE);
    return ntStatus;
}

void
DMF_Utility_InsertionStringListDestroy(
    _In_ DMF_INSERTION_STRING_LIST* InsertionStringList
    )
/*++

Routine Description:

    This routine destroys the insertion string list.

Arguments:

    InsertionStringList - Pointer to a handle to DMF_INSERTION_STRING_LIST.

Return Value:

    NTSTATUS - status of the Destroy operation.

--*/
{
    ULONG stringIndex;
    ULONG numberOfInsertionStrings;

    ASSERT(NULL != InsertionStringList);
    ASSERT(NULL != (InsertionStringList)->InsertionStrings);

    numberOfInsertionStrings = (InsertionStringList)->NumberOfInsertionStrings;
    ASSERT(numberOfInsertionStrings > 0);

    // Free memory associated with each string.
    //
    for (stringIndex = 0; stringIndex < numberOfInsertionStrings; stringIndex++)
    {
        free((InsertionStringList)->InsertionStrings[stringIndex]);
        (InsertionStringList)->InsertionStrings[stringIndex] = NULL;
    }

    // Free memory associated with the pointer to strings.
    //
    free((InsertionStringList)->InsertionStrings);
    (InsertionStringList)->InsertionStrings = NULL;
    (InsertionStringList)->NumberOfInsertionStrings = 0;

    // Free memory associated with the DMF_INSERTION_STRING_LIST structure.
    //
    free(InsertionStringList);

}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Utility_EventLogEntryWriteUserMode(
    _In_ PWSTR Provider,
    _In_ WORD EventType,
    _In_ DWORD EventId,
    _In_ INT NumberOfFormatStrings,
    _In_opt_ PWCHAR* FormatStrings,
    _In_ INT NumberOfInsertionStrings,
    ...
    )
/*++

Routine Description:

    This routine writes an event from a User-mode driver to the system event log file.

Arguments:

    Provider - Provider of the event.
    EventType - Type of the event: EVENTLOG_SUCCESS/EVENTLOG_ERROR_TYPE/EVENTLOG_INFORMATION_TYPE/EVENTLOG_WARNING_TYPE
    EventId - EventId from header generated by Message Compiler (MC.exe).
    NumberOfFormatStrings - Number of format strings.
    FormatStrings - An array of format specifiers for each argument passed below.
    NumberOfInsertionStrings - Number of insertion strings.
    ... - Variable list of insertion strings.

Return Value:

    None. If there is an error, no error is logged.

--*/
{
    EVENTLOG_INSERTION_STRING_TABLE* eventLogStringTable;
    DMF_INSERTION_STRING_LIST* insertionStringList;
    va_list argumentList;
    HANDLE eventSource;
    WDFMEMORY eventLogStringTableMemory;
    NTSTATUS ntStatus;
    LPWSTR* insertionStrings;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    eventSource = NULL;
    eventLogStringTable = NULL;
    eventLogStringTableMemory = NULL;
    insertionStrings = NULL;

    ASSERT(Provider != NULL);
    ASSERT(NumberOfInsertionStrings <= DMF_EVENTLOG_MAXIMUM_NUMBER_OF_INSERTION_STRINGS);
    ASSERT(NumberOfInsertionStrings == NumberOfFormatStrings);

    // Create the insertion string table.
    //
    va_start(argumentList,
             NumberOfInsertionStrings);

    ntStatus = DMF_Utility_InsertionStringTableCreate(&eventLogStringTable,
                                                      &eventLogStringTableMemory,
                                                      NumberOfInsertionStrings,
                                                      argumentList,
                                                      NumberOfFormatStrings,
                                                      FormatStrings);

    va_end(argumentList);

    // Create the closely packed insertion string list.
    //
    ntStatus = DMF_Utility_InsertionStringListCreate(eventLogStringTable,
                                                     &insertionStringList);
    if (! NT_SUCCESS(ntStatus))
    {
        ASSERT(NULL == insertionStringList);
        NumberOfInsertionStrings = 0;
    }
    else if (NumberOfInsertionStrings > 0)
    {
        insertionStrings = insertionStringList->InsertionStrings;
    }

    // 'Banned Crimson API Usage:  RegisterEventSourceW is a Banned Crimson API'.
    //
    #pragma warning(suppress: 28735)
    eventSource = RegisterEventSource(NULL, 
                                      Provider);
    if (NULL == eventSource)
    {
        goto Exit;
    }

    ReportEvent(eventSource,
                EventType,
                0,
                EventId,
                NULL,
                (WORD)NumberOfInsertionStrings,
                0,
                (LPCWSTR*)insertionStrings,
                NULL);

Exit:

    if (eventSource)
    {
        DeregisterEventSource(eventSource);
    }

    // Destroy the closely packed insertion string list.
    //
    if (insertionStringList)
    {
        DMF_Utility_InsertionStringListDestroy(insertionStringList);
        insertionStringList = NULL;
    }

    // Destroy the insertion string table.
    //
    if (eventLogStringTableMemory)
    {
        DMF_EventLogInsertionStringTableDeallocate(eventLogStringTableMemory);
        eventLogStringTableMemory = NULL;
        eventLogStringTable = NULL;
    }

    FuncExitNoReturn(DMF_TRACE);
}
#endif // !defined(DMF_USER_MODE)

// eof: DmfUtility.c
//
