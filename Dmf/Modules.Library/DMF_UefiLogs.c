/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_UefiLogs.c

Abstract:

    This Module provides UEFI log extraction, parsing, and output capabilities for Intel UEFI.

Environment:

    Kernel-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_UefiLogs.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_UefiLogs
{
    // Handle to the Dmf_File child Module.
    //
    DMFMODULE DmfModuleFile;
    // Path to store the log files to.
    //
    WDFSTRING UefiLogPath;
    // UefiOperation.
    //
    DMFMODULE DmfModuleUefiOperation;
    // QueuedWorkItem
    //
    DMFMODULE DmfModuleQueuedWorkItem;
} DMF_CONTEXT_UefiLogs;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(UefiLogs)

// This Module has no CONFIG.
//
DMF_MODULE_DECLARE_NO_CONFIG(UefiLogs)

#define MemoryTag 'LFEU'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// ASCII character for carriage return
//
#define CARRIAGE_RETURN (0xa)

// Signature of a valid ADVANCED_LOGGER_INFO structure.
//
#define LOGGER_INFO_SIGNATURE 0x474F4C41

// Signature of a valid ADVANCED_LOGGER_MESSAGE_ENTRY structure.
//
#define LOGGER_MESSAGE_ENTRY_SIGNATURE 0x534D4C41

// Name of the variable that is queried for UEFI logs.
//
DECLARE_CONST_UNICODE_STRING(UefiVariableName, L"V0");

// GUID for the logs request.
//
DEFINE_GUID(UEFI_LOGS_GUID,
            0xA021BF2B, 0x34ED, 0x4A98, 0x85, 0x9C, 0x42, 0x0E, 0xF9, 0x4F, 0x3E, 0x94);

// Macros for time unit conversion.
//
#define SECONDS_PER_MINUTE 60
#define MINUTES_PER_HOUR 60
#define HOURS_PER_DAY 24

// Logger Information structure
//

// EFI Time Abstraction:
//  Year:       2000 - 20XX
//  Month:      1 - 12
//  Day:        1 - 31
//  Hour:       0 - 23
//  Minute:     0 - 59
//  Second:     0 - 59
//  Nanosecond: 0 - 999,999,999
//  TimeZone:   -1440 to 1440 or 2047
//
typedef struct
{
    UINT16 Year;
    UINT8 Month;
    UINT8 Day;
    UINT8 Hour;
    UINT8 Minute;
    UINT8 Second;
    UINT8 Pad1;
    UINT32 Nanosecond;
    INT16 TimeZone;
    UINT8 Daylight;
    UINT8 Pad2;
} EFI_TIME;

#pragma pack (push, 1)
typedef volatile struct
{
    // Signature 'ALOG'
    //
    UINT32 Signature;
    // Current Version
    //
    UINT16 Version;
    // Reserved for future
    //
    UINT16 Reserved;
    // Fixed pointer to start of log
    //
    UINT64 LogBuffer;
    // Where to store next log entry.
    //
    UINT64 LogCurrent;
    // Number of bytes of messages missed.
    //
    UINT32 DiscardedSize;
    // Size of allocated buffer.
    //
    UINT32 LogBufferSize;
    // Log in permanent RAM
    //
    BOOLEAN InPermanentRAM;
    // After ExitBootServices
    //
    BOOLEAN AtRuntime;
    // After VirtualAddressChange
    //
    BOOLEAN GoneVirtual;
    // HdwPort initialized
    //
    BOOLEAN HdwPortInitialized;
    // HdwPort is Disabled
    //
    BOOLEAN HdwPortDisabled;
    // Reserved field.
    //
    BOOLEAN Reserved2[3];
    // Ticks per second for log timing
    //
    UINT64 TimerFrequency;
    // Ticks when Time Acquired
    //
    UINT64 TicksAtTime;
    // Uefi Time Field
    //
    EFI_TIME Time;
} ADVANCED_LOGGER_INFO;

typedef struct
{
    // Signature
    //
    UINT32 Signature;
    // Debug Level
    //
    UINT32 DebugLevel;
    // Time stamp
    //
    UINT64 TimeStamp;
    // Number of bytes in Message
    //
    UINT16 MessageLengthBytes;
} ADVANCED_LOGGER_MESSAGE_ENTRY;
#pragma pack (pop)

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
UefiLogs_BufferStringAppend(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* DestinationBuffer,
    _In_ size_t MaximumSizeOfBufferBytes,
    _Out_ VOID** StringEndAddress,
    _In_z_ CHAR* FormatString,
    ...
    )
/*++

Routine Description:

    This helper function copies over passed format string and arguments to
    a buffer and returns the buffer address at the end of the copied string.

Arguments:

    DmfModule - This Module's handle.
    DestinationBuffer - Address of the start of the string destination.
    MaximumSizeOfBufferBytes - Destination size for bounds check.
    StringEndAddress - Address of the buffer after copying null-terminated string.
    FormatString - The format specifier for each argument passed below.
    ... - Variable list of insertion strings.

Return Value:

    NTSTATUS.

--*/
{
    va_list argumentList;
    NTSTATUS ntStatus;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    va_start(argumentList,
             FormatString);

    // Put arguments inside format strings.
    //

#if defined(DMF_USER_MODE)
    HRESULT hResult;
    ntStatus = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(StringEndAddress);

    hResult = StringCchVPrintfA((STRSAFE_LPSTR)DestinationBuffer,
                                MaximumSizeOfBufferBytes,
                                FormatString,
                                argumentList);
    // Allow truncated results to pass to Client.
    //
    if (hResult != S_OK &&
        hResult != STRSAFE_E_INSUFFICIENT_BUFFER)
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_UNSUCCESSFUL;
    }
    size_t stringLength = strlen((CHAR*)DestinationBuffer);
    CHAR* endAddress = (CHAR*)DestinationBuffer + stringLength;
    CHAR** endAddressWrite = (CHAR**)StringEndAddress;
    *endAddressWrite = endAddress;
#elif defined(DMF_KERNEL_MODE)
    size_t remainingBytes;
    ntStatus = RtlStringCbVPrintfExA((NTSTRSAFE_PSTR)DestinationBuffer,
                                     MaximumSizeOfBufferBytes,
                                     (NTSTRSAFE_PSTR*)StringEndAddress,
                                     &remainingBytes,
                                     STRSAFE_NULL_ON_FAILURE,
                                     FormatString,
                                     argumentList);
#endif

    va_end(argumentList);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
UefiLogs_BufferTimeAppend(
    _In_ DMFMODULE DmfModule,
    _In_ ADVANCED_LOGGER_INFO* LoggerInfo,
    _In_ ADVANCED_LOGGER_MESSAGE_ENTRY* LoggerMessageEntry,
    _In_ VOID* DestinationBuffer,
    _In_ size_t MaximumSizeOfBufferBytes,
    _Inout_ size_t* LineSize
    )
/*++

Routine Description:

    This helper function calculates the timestamp, converts it to string and
    appends it at the beginning of a line.

Arguments:

    DmfModule - This Module's handle.
    LoggerInfo - UEFI logs header structure.
    LoggerMessageEntry - UEFI logs message header structure.
    DestinationBuffer - Address of the start of the intended string destination.
    MaximumSizeOfBufferBytes - Destination size for bounds check.
    LineSize - Number of bytes of string written.

Return Value:

    NTSTATUS.

--*/
{
    NTSTATUS ntStatus;
    EFI_TIME newTime;
    double timeToAdd;
    VOID* stringEndAddress;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    if (LoggerInfo->TimerFrequency != 0)
    {
        timeToAdd = LoggerMessageEntry->TimeStamp / (double)LoggerInfo->TimerFrequency;
    }
    else
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "LoggerInfo returned incorrect Frequency. Skipping calculation to prevent divide by zero error.");
        goto Exit;
    }

    // Convert timeToAdd in seconds.
    //
    timeToAdd = timeToAdd + LoggerInfo->Time.Second;
    newTime.Second = (UINT8)timeToAdd;

    // Copy time information from LoggerInfo.
    //
    newTime.Minute = LoggerInfo->Time.Minute;
    newTime.Hour = LoggerInfo->Time.Hour;
    newTime.Day = LoggerInfo->Time.Day;
    newTime.Month = LoggerInfo->Time.Month;
    newTime.Year = LoggerInfo->Time.Year;

    // Check for time carryover.
    //
    if (newTime.Second >= SECONDS_PER_MINUTE)
    {
        newTime.Minute += newTime.Second / SECONDS_PER_MINUTE;
        newTime.Second = newTime.Second % SECONDS_PER_MINUTE;
        if (newTime.Minute >= MINUTES_PER_HOUR)
        {
            newTime.Hour += newTime.Minute / MINUTES_PER_HOUR;
            newTime.Minute = newTime.Minute % MINUTES_PER_HOUR;
            if (newTime.Hour >= HOURS_PER_DAY)
            {
                newTime.Day += newTime.Hour / HOURS_PER_DAY;
                newTime.Hour = newTime.Hour % HOURS_PER_DAY;
                // Stopping here because more advanced logic is needed to check
                // number of days based on month etc.
            }
        }
    }

    // Add this time to the log.
    //
    ntStatus = UefiLogs_BufferStringAppend(DmfModule,
                                           DestinationBuffer,
                                           MaximumSizeOfBufferBytes,
                                           &stringEndAddress,
                                           "%u-%u-%u %u:%u:%u : ",
                                           newTime.Year,
                                           newTime.Month,
                                           newTime.Day,
                                           newTime.Hour,
                                           newTime.Minute,
                                           newTime.Second);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Could not add timestamp to UEFI log. ntStatus=%!STATUS!", ntStatus);
        *LineSize = 0;
        goto Exit;
    }

    *LineSize += ((UINT64)stringEndAddress - (UINT64)DestinationBuffer);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_QueuedWorkItem_Callback)
ScheduledTask_Result_Type
UefiLogs_Retrieve_QueuedWorkItemCallback(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext
    )
/*++

Routine Description:

    Retrieve logs from UEFI and store them as file and ETW events.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    DMFMODULE dmfModuleUefiLogs;
    ScheduledTask_Result_Type scheduledTaskResult;
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDFMEMORY uefiLogMemory;
    VOID* uefiLog;
    WDFMEMORY parsedUefiLogMemory;
    VOID* parsedUefiLog;
    WDFMEMORY eventLogMemory;
    VOID* eventLogLine;
    DMF_CONTEXT_UefiLogs* moduleContext;
    ULONG blobSize;
    size_t maximumBytesToCopy;
    size_t eventLogSize;
    ADVANCED_LOGGER_INFO* loggerInfo;
    UCHAR* parsedLogHead;
    UCHAR* uefiLogHead;
    UCHAR* eventLogLineHead;
    size_t lineSize;
    size_t timeStampSize;
    ADVANCED_LOGGER_MESSAGE_ENTRY* loggerMessageEntry;
    UCHAR* messageText;
    UCHAR* uefiLogEndAddress;
    UCHAR* parsedLogHeadEnd;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(ClientBuffer);
    UNREFERENCED_PARAMETER(ClientBufferContext);

    dmfModuleUefiLogs = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleUefiLogs);

    scheduledTaskResult = ScheduledTask_WorkResult_Success;
    uefiLog = NULL;
    uefiLogMemory = NULL;
    parsedUefiLogMemory = NULL;
    eventLogMemory = NULL;
    maximumBytesToCopy = (DMF_EVENTLOG_MAXIMUM_LENGTH_OF_STRING * sizeof(WCHAR)) + sizeof(L"\0");
    eventLogSize = maximumBytesToCopy + sizeof(L"\0");
    blobSize = 0;

    ntStatus = DMF_UefiOperation_FirmwareEnvironmentVariableAllocateGet(moduleContext->DmfModuleUefiOperation,
                                                                        (PUNICODE_STRING)&UefiVariableName,
                                                                        (LPGUID)&UEFI_LOGS_GUID,
                                                                        (VOID**)&uefiLog,
                                                                        &blobSize,
                                                                        &uefiLogMemory,
                                                                        NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_WARNING,
                    DMF_TRACE,
                    "DMF_UefiOperation_FirmwareEnvironmentVariableAllocateGet fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               PagedPool,
                               MemoryTag,
                               eventLogSize,
                               &eventLogMemory,
                               (VOID**)&(eventLogLine));
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Clear so that parsing logic can just copy characters without terminating the string.
    //
    RtlZeroMemory(eventLogLine,
                  eventLogSize);

    // Parse logs.
    //
    loggerInfo = (ADVANCED_LOGGER_INFO*)uefiLog;
    if (loggerInfo->Signature != LOGGER_INFO_SIGNATURE)
    {
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Unknown NVRAM Log signature = %u",loggerInfo->Signature);
        goto Exit;
    }

    ntStatus = WdfMemoryCreate(&objectAttributes,
                               PagedPool,
                               MemoryTag,
                               blobSize,
                               &parsedUefiLogMemory,
                               (VOID**)&(parsedUefiLog));
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    RtlZeroMemory(parsedUefiLog,
                  blobSize);

    parsedLogHead = (UCHAR*) parsedUefiLog;
    parsedLogHeadEnd = (UCHAR*) parsedLogHead + blobSize;

    uefiLogHead = (UCHAR*)uefiLog + sizeof(ADVANCED_LOGGER_INFO);
    eventLogLineHead = (UCHAR*)eventLogLine;
    lineSize = 0;
    timeStampSize = 0;

    // Add timestamp to the beginning of the log.
    //
    ntStatus = UefiLogs_BufferStringAppend(dmfModuleUefiLogs,
                                           parsedUefiLog,
                                           blobSize,
                                           (VOID**)&parsedLogHead,
                                           "NVRAM Log Time: %u-%u-%u %u:%u:%u\n",
                                           loggerInfo->Time.Year,
                                           loggerInfo->Time.Month,
                                           loggerInfo->Time.Day,
                                           loggerInfo->Time.Hour,
                                           loggerInfo->Time.Minute,
                                           loggerInfo->Time.Second);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UefiLogs_BufferStringAppend fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Add first timestamp to ETW.
    //
    DMF_Utility_LogEmitString(dmfModuleUefiLogs,
                              DmfLogDataSeverity_Informational,
                              L"%S",
                              parsedUefiLog);

    uefiLogEndAddress = (UCHAR*)uefiLog + blobSize;
    while (uefiLogHead < uefiLogEndAddress)
    {
        loggerMessageEntry = (ADVANCED_LOGGER_MESSAGE_ENTRY*)uefiLogHead;
        // Message is located at the end of the loggerMessageEntry.
        //
        messageText = uefiLogHead + sizeof(ADVANCED_LOGGER_MESSAGE_ENTRY);
        if (loggerMessageEntry->Signature != LOGGER_MESSAGE_ENTRY_SIGNATURE)
        {
            TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Unknown NVRAM Log signature");
            break;
        }

        // Add timestamp to line if this is the start of the line.
        //
        if (lineSize == 0)
        {
            UefiLogs_BufferTimeAppend(dmfModuleUefiLogs,
                                      loggerInfo,
                                      loggerMessageEntry,
                                      eventLogLineHead,
                                      eventLogSize,
                                      &timeStampSize);
            eventLogLineHead += timeStampSize;
            lineSize += timeStampSize;
        }

        // Add message to line.
        //
        if (eventLogLineHead + loggerMessageEntry->MessageLengthBytes <= ((UCHAR*)eventLogLine) + maximumBytesToCopy)
        {
            RtlCopyMemory(eventLogLineHead,
                          messageText,
                          loggerMessageEntry->MessageLengthBytes);
        }
        else
        {
            // The payload of the message is too long.
            //
            DmfAssert(FALSE);
            // Cannot trust the rest of the data. Exit now.
            //
            break;
        }

        // Move the event log head to last character of the string which was just extracted
        // to check if it is end of line.
        //
        eventLogLineHead += loggerMessageEntry->MessageLengthBytes - 1;
        lineSize += loggerMessageEntry->MessageLengthBytes;

        // Check if last character is ASCII end of line.
        //
        if (*eventLogLineHead == CARRIAGE_RETURN)
        {
            // Copy the line to parsed buffer.
            //
            if (parsedLogHead + lineSize <= parsedLogHeadEnd)
            {
                // 'Possibly incorrect single element annotation on buffer'
                //
                #pragma warning(suppress: 26007)
                RtlCopyMemory(parsedLogHead,
                              eventLogLine,
                              lineSize);
                parsedLogHead += lineSize;
            }
            else
            {
                // Data won't fit into target buffer to write to file.
                //
                DmfAssert(FALSE);
                // Stop processing.
                //
                break;
            }

            // Send line out as ETW event (if it is not an empty line).
            // Empty lines in UEFI logs have 2 characters, return carriage and newline.
            //
            if (lineSize > timeStampSize + sizeof('\n') + sizeof('\r'))
            {
                DMF_Utility_LogEmitString(dmfModuleUefiLogs,
                                          DmfLogDataSeverity_Informational,
                                          L"%S",
                                          eventLogLine);
            }

            // Clear out the line.
            //
            RtlZeroMemory(eventLogLine,
                          lineSize);

            // Set head back to start.
            //
            eventLogLineHead = (UCHAR*)eventLogLine;
            lineSize = 0;
            timeStampSize = 0;
        }
        else
        {
            // Move head forward for next copy.
            //
            eventLogLineHead += 1;
        }

        uefiLogHead += sizeof(ADVANCED_LOGGER_MESSAGE_ENTRY) + loggerMessageEntry->MessageLengthBytes;

        // Ensure 8 byte alignment of the uefiLogHead.
        //
        uefiLogHead = (UCHAR*)((((UINT64)uefiLogHead + 7) / 8) * 8);
    }

    // UEFI logs obtained. Create log file.
    //
    ntStatus = DMF_File_Write(moduleContext->DmfModuleFile,
                              moduleContext->UefiLogPath,
                              parsedUefiLogMemory);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_File_Write fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    if (uefiLogMemory != NULL)
    {
        WdfObjectDelete(uefiLogMemory);
    }
    if (parsedUefiLogMemory != NULL)
    {
        WdfObjectDelete(parsedUefiLogMemory);
    }
    if (eventLogMemory != NULL)
    {
        WdfObjectDelete(eventLogMemory);
    }

    if (!NT_SUCCESS(ntStatus))
    {
        scheduledTaskResult = ScheduledTask_WorkResult_Fail;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return scheduledTaskResult;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_Function_class_(DMF_ModuleD0Entry)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_UefiLogs_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    UefiLogs callback for ModuleD0Entry for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    PreviousState - The WDF Power State that the given DMF Module exits from.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_UefiLogs* moduleContext;

    UNREFERENCED_PARAMETER(PreviousState);

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ntStatus = STATUS_SUCCESS;

    // Save the UEFI logs if there is a D0 transition from a reboot.
    //
    if (PreviousState == WdfPowerDeviceD3Final)
    {
        DMF_QueuedWorkItem_Enqueue(moduleContext->DmfModuleQueuedWorkItem,
                                   NULL,
                                   0);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_UefiLogs_ModuleD0Entry ntStatus=%!STATUS!", ntStatus);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_UefiLogs_ChildModulesAdd(
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
    DMF_CONTEXT_UefiLogs* moduleContext;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_QueuedWorkItem moduleConfigQueuedWorkItem;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // File
    //
    DMF_File_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleFile);

    // UefiOperation
    //
    DMF_UefiOperation_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleUefiOperation);

    // QueuedWorkItem
    //
    DMF_CONFIG_QueuedWorkItem_AND_ATTRIBUTES_INIT(&moduleConfigQueuedWorkItem,
                                                  &moduleAttributes);
    moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.BufferCount = 1;
    moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.BufferSize = sizeof(CHAR);
    moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.PoolType = PagedPool;
    moduleConfigQueuedWorkItem.BufferQueueConfig.SourceSettings.EnableLookAside = FALSE;
    moduleAttributes.PassiveLevel = TRUE;
    moduleConfigQueuedWorkItem.EvtQueuedWorkitemFunction = UefiLogs_Retrieve_QueuedWorkItemCallback;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleQueuedWorkItem);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_UefiLogs_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type UefiLogs.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NT_STATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDRIVER driver;
    WDFDEVICE device;
    DMF_CONTEXT_UefiLogs* moduleContext;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    device = DMF_ParentDeviceGet(DmfModule);
    driver = WdfDeviceGetDriver(device);

#if defined (DMF_USER_MODE)
    // Expand %temp% directory before passing it as path to DMF_File_Create.
    //
    BOOL result;
    DWORD expandResult;
    WCHAR* uefiLogPathExpanded;
    size_t sizeToAllocate = MAX_PATH * sizeof(WCHAR);
    WDFMEMORY expandedPathMemory;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               PagedPool,
                               MemoryTag,
                               sizeToAllocate,
                               &expandedPathMemory,
                               (VOID**)&(uefiLogPathExpanded));
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }
    WCHAR* uefiLogPathWstr = L"\\\\?\\%temp%\\Surface";
    WCHAR* uefiLogFilenameWstr = L"UEFI.log";
    WCHAR uefiLogFullName[MAX_PATH];
    expandResult = ExpandEnvironmentStrings(uefiLogPathWstr,
                                            uefiLogPathExpanded,
                                            MAX_PATH);
    if (expandResult == 0)
    {
        DWORD lastError = GetLastError();
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE,"ExpandEnvironmentStrings fails: Error = %llu\n", lastError);
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }
    swprintf_s(uefiLogFullName,
               L"%s\\%s",
               uefiLogPathExpanded,
               uefiLogFilenameWstr);
    // Create directory for storing the file.
    //
    result = CreateDirectory(uefiLogPathExpanded,
                             NULL);
    if (!result)
    {
        DWORD lastError = GetLastError();
        if (lastError != ERROR_ALREADY_EXISTS)
        {
            TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE,"CreateDirectory() fails: Error = %llu\n", lastError);
            ntStatus = STATUS_UNSUCCESSFUL;
            goto Exit;
        }
    }
    UNICODE_STRING uefiLogPathUnicode;
    RtlInitUnicodeString(&uefiLogPathUnicode,
                         uefiLogFullName);

#else
    DECLARE_CONST_UNICODE_STRING(uefiLogPathUnicode, L"\\DosDevices\\C:\\Users\\Default\\Surface\\UEFI.log");
#endif

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfStringCreate(&uefiLogPathUnicode,
                               &objectAttributes,
                               &moduleContext->UefiLogPath);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfStringCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    return ntStatus;
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
DMF_UefiLogs_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type UefiLogs.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_UefiLogs;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_UefiLogs;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_UefiLogs;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_UefiLogs);
    dmfCallbacksDmf_UefiLogs.ChildModulesAdd = DMF_UefiLogs_ChildModulesAdd;
    dmfCallbacksDmf_UefiLogs.DeviceOpen = DMF_UefiLogs_Open;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_UefiLogs);
    dmfCallbacksWdf_UefiLogs.ModuleD0Entry = DMF_UefiLogs_ModuleD0Entry;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_UefiLogs,
                                            UefiLogs,
                                            DMF_CONTEXT_UefiLogs,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_UefiLogs.CallbacksDmf = &dmfCallbacksDmf_UefiLogs;
    dmfModuleDescriptor_UefiLogs.CallbacksWdf = &dmfCallbacksWdf_UefiLogs;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_UefiLogs,
                                DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_UefiLogs.c
//
