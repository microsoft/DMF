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
                                DMF_TAG,
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

#if defined(DMF_USE_DBGPRINT)

ULONG g_DMF_DebugLevel = TRACE_LEVEL_INFORMATION;
ULONG g_DMF_DebugFlag = 0xff;

#if defined(DBG)

BOOLEAN
DmfPlatform_FormatStringTranslate(
    _In_ CHAR* DebugMessage,
    _Out_writes_(OutStringSize) CHAR* OutString,
    _In_ size_t OutStringSize
    )
/*++

Routine Description:

    Replace special WPP tracing format specifiers with 0x%X so that printf
    can output the values.

Arguments:

    DebugMessage - Original format message to filter.
    OutString - Translated format message.
    OutStringSize - Size in bytes of OutString buffer.

Return Value:

    TRUE if the OutString buffer is large enough.

--*/
{
    BOOLEAN returnValue;
    CHAR* currentCharacterIn;
    CHAR* currentCharacterOut;
    size_t remainingBytes;
    CHAR replacementFormat[] = "0x%X";
    CHAR replacementFormatLlx[] = "%llx";
    size_t replacementFormatSize;
    size_t replacementFormatSizeLlx;

    if (strlen(DebugMessage) + sizeof(CHAR) > OutStringSize)
    {
        strcpy_s(OutString,
                 OutStringSize,
                 "Format string is too long.");
        returnValue = FALSE;
        goto Exit;
    }

    replacementFormatSize = strlen(replacementFormat);
    replacementFormatSizeLlx = strlen(replacementFormatLlx);
    currentCharacterIn = (CHAR*)DebugMessage;
    currentCharacterOut = OutString;
    remainingBytes = OutStringSize;
    while (*currentCharacterIn)
    {
        if (*currentCharacterIn == '%')
        {
            if (*(currentCharacterIn + 1) == '!')
            {
                // Special string.
                //

                // Skip '%'
                //
                currentCharacterIn++;
                // Skip '!'
                //
                currentCharacterIn++;
                // Skip characters inside '!' and '!'.
                //
                while ((*currentCharacterIn) &&
                        (*currentCharacterIn != '!'))
                {
                    currentCharacterIn++;
                }
                if (*currentCharacterIn == '!')
                {
                    // Skip trailing !''.
                    //
                    currentCharacterIn++;
                }
                else
                {
                    // It is a malformed string that starts with %! but
                    // has no trailing !. Just overwrite target buffer 
                    // with error message and get out.
                    //
                    strcpy_s(OutString,
                             OutStringSize,
                             "Error in format string: expected trailing '!'");
                    returnValue = FALSE;
                    goto Exit;
                }
                // Replace with default format string.
                //
                *currentCharacterOut = '\0';
                strcat_s(currentCharacterOut,
                         remainingBytes,
                         replacementFormat);
                if (remainingBytes >= replacementFormatSize)
                {
                    remainingBytes -= replacementFormatSize;
                }
                else
                {
                    // It is a malformed string where there is not enough
                    // space in the target buffer for the rest of the
                    // translated string.
                    //
                    strcpy_s(OutString,
                             OutStringSize,
                             "Error in format string: not enough space");
                    returnValue = FALSE;
                    goto Exit;
                }
                currentCharacterOut += replacementFormatSize;
                continue;
            }
            else if (*(currentCharacterIn + 1) == 's')
            {
                // Due to how _vsnprintf_s works, the CHAR* arguments don't work
                // with 's'. They need 'S' due to default mode where 's' expects
                // WCHAR*
                //
                *currentCharacterOut = 'S';
                currentCharacterOut++;
                remainingBytes--;
                currentCharacterIn++;
                continue;
            }
            else if (*(currentCharacterIn + 1) == 'p')
            {
                // Special string.
                //

                // Skip '%'
                //
                currentCharacterIn++;
                // Skip 'p'
                //
                currentCharacterIn++;
                // Replace with default format string.
                //
                *currentCharacterOut = '\0';
                strcat_s(currentCharacterOut,
                         remainingBytes,
                         replacementFormatLlx);
                if (remainingBytes >= replacementFormatSizeLlx)
                {
                    remainingBytes -= replacementFormatSizeLlx;
                }
                else
                {
                    // It is a malformed string where there is not enough
                    // space in the target buffer for the rest of the
                    // translated string.
                    //
                    strcpy_s(OutString,
                             OutStringSize,
                             "Error in format string: not enough space");
                    returnValue = FALSE;
                    goto Exit;
                }
                currentCharacterOut += replacementFormatSizeLlx;
                continue;
            }
            else
            {
                // Not a special string...fall through and keep copying.
                //
            }
        }
        // Copy the current compatible character.
        //
        *currentCharacterOut = *currentCharacterIn;
        currentCharacterOut++;
        remainingBytes--;
        currentCharacterIn++;
    }
    // Zero terminate the string.
    //
    *currentCharacterOut = '\0';
    // Indicate the original string was translated.
    //
    returnValue = TRUE;

Exit:

    return returnValue;
}

#endif // defined(DBG)

VOID
TraceEvents(
    _In_ ULONG DebugPrintLevel,
    _In_ ULONG DebugPrintFlag,
    _Printf_format_string_ _In_ CHAR* DebugMessage,
    ...
    )
{
#if defined(DBG)
    #define MESSAGE_BUFFER_SIZE 1024
    va_list list;
    CHAR debugMessageBuffer[MESSAGE_BUFFER_SIZE];
    NTSTATUS ntStatus;

    va_start(list,
             DebugMessage);

    if (DebugMessage)
    {
        CHAR translatedDebugMessage[MESSAGE_BUFFER_SIZE];

        DmfPlatform_FormatStringTranslate(DebugMessage,
                                          translatedDebugMessage,
                                          ARRAYSIZE(translatedDebugMessage));

        // Use new safe string functions instead of _vsnprintf.
        // This function takes care of NULL terminating if the message
        // is longer than the buffer.
        //
#if !defined(DMF_USER_MODE)
        ntStatus = RtlStringCbVPrintfA(debugMessageBuffer,
                                       sizeof(debugMessageBuffer),
                                       translatedDebugMessage,
                                       list);
#else
        _vsnprintf_s(debugMessageBuffer,
                     sizeof(debugMessageBuffer),
                     translatedDebugMessage,
                     list);
        ntStatus = STATUS_SUCCESS;
#endif
        if(!NT_SUCCESS(ntStatus))
        {
            DbgPrint("DMF: RtlStringCbVPrintfA fails: ntStatus=0x%x\n", ntStatus);
            goto Exit;
        }
        if (DebugPrintLevel <= TRACE_LEVEL_ERROR ||
            (DebugPrintLevel <= g_DMF_DebugLevel &&
             ((DebugPrintFlag & g_DMF_DebugFlag) == DebugPrintFlag)))
        {
            DbgPrint("DMF:%s\n",
                     debugMessageBuffer);
        }
    }

    va_end(list);

Exit:

    return;
#else
    UNREFERENCED_PARAMETER(DebugPrintLevel);
    UNREFERENCED_PARAMETER(DebugPrintFlag);
    UNREFERENCED_PARAMETER(DebugMessage);
#endif
}

VOID
TraceInformation(
    _In_ ULONG DebugPrintFlag,
    _Printf_format_string_ _In_ CHAR* DebugMessage,
    ...
    )
{
    va_list argumentList;

    va_start(argumentList,
             DebugMessage);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DebugPrintFlag,
                DebugMessage,
                argumentList);

    va_end(argumentList);
}

VOID
TraceVerbose(
    _In_ ULONG DebugPrintFlag,
    _Printf_format_string_ _In_ CHAR* DebugMessage,
    ...
    )
{
    va_list argumentList;

    va_start(argumentList,
             DebugMessage);

    TraceEvents(TRACE_LEVEL_VERBOSE,
                DebugPrintFlag,
                DebugMessage,
                argumentList);

    va_end(argumentList);
}

VOID
TraceError(
    _In_ ULONG DebugPrintFlag,
    _Printf_format_string_ _In_ CHAR* DebugMessage,
    ...
    )
{
    va_list argumentList;

    va_start(argumentList,
             DebugMessage);

    TraceEvents(TRACE_LEVEL_ERROR,
                DebugPrintFlag,
                DebugMessage,
                argumentList);

    va_end(argumentList);
}

VOID
FuncEntryArguments(
    _In_ ULONG DebugPrintFlag,
    _Printf_format_string_ _In_ CHAR* DebugMessage,
    ...
    )
{
    va_list argumentList;

    va_start(argumentList,
             DebugMessage);

    TraceEvents(TRACE_LEVEL_VERBOSE,
                DebugPrintFlag,
                DebugMessage,
                argumentList);

    va_end(argumentList);
}

#endif

// eof: DmfUtility.c
//
