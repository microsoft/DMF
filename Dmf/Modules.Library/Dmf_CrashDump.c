/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_CrashDump.c

Abstract:

    Provides support for Crash Dump Management.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if !defined(DMF_USER_MODE)

#include "Dmf_CrashDump_Public.h"

#else

#include "CSystemTelemetryDevice.h"

#include <powrprof.h>

#endif // !defined(DMF_USER_MODE)

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_CrashDump.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// It is the Bug Check code this driver issues when the DEBUG build receives an 
// IOCTL to crash the machine.
//
#define BUG_CHECK_PRIVATE   0xDEADDEAD

// It means that an application created a Data Source, possibly wrote to it, and then ended...purposefully
// leaving the Data Source allocated in case of a system crash. This is an expected situation. This define
// allows the driver to make sure such Data Sources remain allocated during the duration of the life of
// the driver.
//
#define FILE_OBJECT_ORPHAN      (WDFFILEOBJECT)(-1)

// Length of the Encryption key (which is the GUID reformatted). Length does not include the terminating NULL.
//
#define ENCRYPTION_KEY_STRING_SIZE (sizeof("1111111122223333D1D2D3D4D5D6D7D8") - sizeof(CHAR)) 

// Information for each Crash Dump Data Source.
// A Crash Dump Data Source produces data that must be written to the crash dump
// data file if a crash should happen.
//
typedef struct
{
    // Ring Buffers for each Data Source.
    //
    DMFMODULE DmfModuleDataSourceRingBuffer;

    // Array of file pointers that corresponds to DmfModuleDataSourceRingBuffer entries.
    //
    WDFFILEOBJECT FileObject[DataSourceModeMaximum];

    // GUID for each Ring Buffer.
    //
    GUID RingBufferGuid;

    // Encryption key for each Ring Buffer. The key is the GUID reformatted and 
    // is NULL terminated.
    //
    CHAR RingBufferEncryptionKey[ENCRYPTION_KEY_STRING_SIZE + sizeof(CHAR)];

    // Size of Encryption key not including the NULL.
    //
    ULONG RingBufferEncryptionKeySize;

    // Ring Buffer Data Location.
    //
    VOID* RingBufferData;

    // Ring Buffer Data Length.
    //
    ULONG RingBufferSize;

    // Ring Buffer record size (size of each entry).
    // (Used for validation purposes from User-mode.)
    //
    ULONG RingBufferSizeOfEachEntry;

    // This index is used when obfuscating the data in the Ring Buffer.
    //
    ULONG CurrentRingBufferIndex;
} DATA_SOURCE;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_CrashDump
{
#if !defined(DMF_USER_MODE)
    // Number of data sources. It includes the Client Driver's Ring Buffer as well as
    // any User-mode Crash Dump Data Sources.
    //
    ULONG DataSourceCount;

    // Crash Dump Context for Additional data from this driver.
    //
    KBUGCHECK_REASON_CALLBACK_RECORD BugCheckCallbackRecordAdditional;

    // Management information for all the Crash Dump Data Sources.
    //
    DATA_SOURCE* DataSource;

    // Crash Dump Context for Ring Buffers.
    // NOTE: Keep this here so the math with this element is easier. It should be in DATA_SOURCE
    //       but element is used in the callback.
    //
    KBUGCHECK_REASON_CALLBACK_RECORD* BugCheckCallbackRecordRingBuffer;

    // The Triage Dump Data Array.
    //
    WDFMEMORY TriageDumpDataArrayMemory;
    KTRIAGE_DUMP_DATA_ARRAY* TriageDumpDataArray;

    // Crash Dump Context for Triage Dump Data Array callback.
    //
    KBUGCHECK_REASON_CALLBACK_RECORD BugCheckCallbackRecordTriageDumpData;

    // Keep track of whether or not the Client has been surprise removed for clean up of
    // data transferred via files.
    //
    BOOLEAN SurpriseRemoved;
#else
    // User-mode access to System Telemetry Driver.
    // NOTE: This object must be dynamically allocated to ensure that constructors/destructors are called.
    //
    CSystemTelemetryDevice* m_SystemTelemetryDevice;
#endif  // !defined(DMF_USER_MODE)
} DMF_CONTEXT_CrashDump;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(CrashDump)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(CrashDump)

// Memory pool tag.
//
#define MemoryTag 'oMDC'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#if !defined(DMF_USER_MODE)

// This global variable is necessary because there is no way to get a context passed into the callbacks.
//
DMFMODULE g_DmfModuleCrashDump;

KBUGCHECK_REASON_CALLBACK_ROUTINE CrashDump_BugCheckSecondaryDumpDataCallbackRingBuffer;

_Function_class_(EVT_DMF_RingBuffer_Enumeration)
BOOLEAN
CrashDump_RingBufferElementsFirstBufferGet(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _In_opt_ VOID* CallbackContext
    )
/*++

Routine Description:

    Obfuscate/Unobfuscate Ring Buffer data one element at a time.

Arguments:

    DmfModule - Child Module.
    Buffer - A single entry in the Ring Buffer.
    BufferSize - The size of a single entry in the Ring Buffer.
    CallbackContext - The DATA_SOURCE* which contains the key and current index into the total
                      Ring Buffer data.

Return Value:

    FALSE because only the first element should be enumerated.

--*/
{
    DATA_SOURCE* dataSource;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(BufferSize);

    dataSource = (DATA_SOURCE*)CallbackContext;

    // The first time this callback is called, it is for the first block of data in the
    // Ring Buffer. Store that address so that it can be written to the crash dump file.
    // This is a bit unclean, but the system will crash immediately afterward.
    //
    
    DmfAssert(NULL == dataSource->RingBufferData);
    // 'Dereferencing NULL pointer. 'dataSource' contains the same NULL value as 'CallbackContext' did.'
    //
    #pragma warning(suppress:28182)
    dataSource->RingBufferData = Buffer;

    // Stop enumerating.
    //
    return FALSE;
}

_Function_class_(EVT_DMF_RingBuffer_Enumeration)
BOOLEAN
CrashDump_RingBufferElementsXor(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _In_opt_ VOID* CallbackContext
    )
/*++

Routine Description:

    Obfuscate/Unobfuscate Ring Buffer data one element at a time.

Arguments:

    DmfModule - Child Module.
    Buffer - A single entry in the Ring Buffer.
    BufferSize - The size of a single entry in the Ring Buffer.
    CallbackContext - The DATA_SOURCE* which contains the key and current index into the total
                      Ring Buffer data.

Return Value:

    None

--*/
{
    DATA_SOURCE* dataSource;

    UNREFERENCED_PARAMETER(DmfModule);

    dataSource = (DATA_SOURCE*)CallbackContext;

    // Element is index in EncryptionKey which wraps around when it reaches the length of EncryptionKey.
    // 'Dereferencing NULL pointer. 'dataSource' contains the same NULL value as 'CallbackContext' did.'
    //
    #pragma warning(suppress:28182)
    while (dataSource->CurrentRingBufferIndex < BufferSize)
    {
        DWORD encryptionKeyIndex;

        encryptionKeyIndex = dataSource->CurrentRingBufferIndex % dataSource->RingBufferEncryptionKeySize;
        Buffer[dataSource->CurrentRingBufferIndex]  = dataSource->RingBufferEncryptionKey[encryptionKeyIndex] ^ Buffer[dataSource->CurrentRingBufferIndex];
        dataSource->CurrentRingBufferIndex++;
    }

    // Continue enumeration.
    //
    return TRUE;
}

_Use_decl_annotations_
VOID
CrashDump_BugCheckSecondaryDumpDataCallbackRingBuffer(
    _In_ KBUGCHECK_CALLBACK_REASON Reason,
    _In_ struct _KBUGCHECK_REASON_CALLBACK_RECORD *Record,
    _Inout_ VOID* ReasonSpecificData,
    _In_ ULONG ReasonSpecificDataLength
    )
/*++

Routine Description:

    The Bug Check Callback for Ring Buffer data that the Client Driver needs to write. This
    callback is called twice for each registered callback. Once for a size query and a second time for the
    data query. To debug this code, cause a BSOD using the debugger then "go" when the first break happens.
    Then, after the crash dump data is almost finished writing, this callback will execute twice.
    NOTE: No context is passed to this function, therefore a global variable is necessary.

Arguments:

    See MSDN: KbCallbackSecondaryDumpData Callback Routine

Return Value:

    None

--*/
{
    KBUGCHECK_SECONDARY_DUMP_DATA* secondaryDumpData;
    DMF_CONTEXT_CrashDump* moduleContext;
    DMF_CONFIG_CrashDump* moduleConfig;
    ULONG dataSourceIndex;
    DMFMODULE dmfModuleRingBuffer;
    GUID ringBufferGuid;
    ULONG totalLength;
    DATA_SOURCE* dataSource;

    UNREFERENCED_PARAMETER(Reason);
    UNREFERENCED_PARAMETER(Record);
    UNREFERENCED_PARAMETER(ReasonSpecificDataLength);

    secondaryDumpData = (KBUGCHECK_SECONDARY_DUMP_DATA*)ReasonSpecificData;
    moduleContext = DMF_CONTEXT_GET(g_DmfModuleCrashDump);
    DmfAssert(moduleContext != NULL);

    moduleConfig = DMF_CONFIG_GET(g_DmfModuleCrashDump);
    DmfAssert(moduleConfig != NULL);

    // Get the Ring Buffer information to be output.
    //
    dataSourceIndex = (ULONG)(Record - moduleContext->BugCheckCallbackRecordRingBuffer);
    DmfAssert(dataSourceIndex < moduleContext->DataSourceCount);
    dataSource = &moduleContext->DataSource[dataSourceIndex];
    dmfModuleRingBuffer = dataSource->DmfModuleDataSourceRingBuffer;
    ringBufferGuid = dataSource->RingBufferGuid;

    if (NULL == secondaryDumpData->OutBuffer)
    {
        // Reorder the Ring Buffer in order so that the first enumerated buffer is the
        // at the beginning of the Ring Buffer.
        //
        DMF_RingBuffer_Reorder(dmfModuleRingBuffer,
                               FALSE);
        dataSource->RingBufferData = NULL;
        DMF_RingBuffer_Enumerate(dataSource->DmfModuleDataSourceRingBuffer,
                                 FALSE,
                                 CrashDump_RingBufferElementsFirstBufferGet,
                                 dataSource);
    }
    else if (secondaryDumpData->OutBuffer == secondaryDumpData->InBuffer)
    {
        // Now, prepare the data that will be written to the crash dump file.
        //     

        // First the GUID.
        //
        memcpy(&secondaryDumpData->Guid,
               &ringBufferGuid,
               sizeof(GUID));

        // Obfuscate the data in the Ring Buffer and get the starting address of the Ring Buffer.
        //
        dataSource->CurrentRingBufferIndex = 0;
        DMF_RingBuffer_Enumerate(dataSource->DmfModuleDataSourceRingBuffer,
                                 FALSE,
                                 CrashDump_RingBufferElementsXor,
                                 dataSource);
        dataSource->CurrentRingBufferIndex = 0;
    }

    if (dataSource->RingBufferData != NULL)
    {
        // Copy over the Ring Buffer data.
        //
        DmfAssert(dataSource->RingBufferData != NULL);
        secondaryDumpData->OutBuffer = dataSource->RingBufferData;
        DmfAssert(dataSource->RingBufferSize != 0);
        totalLength = dataSource->RingBufferSize;

        if (totalLength > secondaryDumpData->MaximumAllowed)
        {
            totalLength = secondaryDumpData->MaximumAllowed;
        }
        secondaryDumpData->OutBufferLength = totalLength;
    }
    else
    {
        // There is no data in the Ring Buffer.
        //
        secondaryDumpData->OutBuffer = NULL;
        secondaryDumpData->OutBufferLength = 0;
    }
    // Now the crash dump knows where the Ring Buffer data is and how big it is.
    //
}

KBUGCHECK_REASON_CALLBACK_ROUTINE CrashDump_BugCheckSecondaryDumpDataCallbackAdditional;

VOID
CrashDump_BugCheckSecondaryDumpDataCallbackAdditional(
    _In_ KBUGCHECK_CALLBACK_REASON Reason,
    _In_ struct _KBUGCHECK_REASON_CALLBACK_RECORD *Record,
    _Inout_ VOID* ReasonSpecificData,
    _In_ ULONG ReasonSpecificDataLength
    )
/*++

Routine Description:

    The Bug Check Callback for Additional Data (data not in Ring Buffer) that the Client Driver needs to write. This
    callback is called twice for each registered callback. Once for a size query and a second time for the
    data query. To debug this code, cause a BSOD using the debugger then "go" when the first break happens.
    Then, after the crash dump data is almost finished writing, this callback will execute twice.
    NOTE: No context is passed to this function, therefore a global variable is necessary.

Arguments:

    See MSDN: KbCallbackSecondaryDumpData Callback Routine

Return Value:

    None

--*/
{
    KBUGCHECK_SECONDARY_DUMP_DATA* secondaryDumpData;
    DMF_CONTEXT_CrashDump* moduleContext;
    DMF_CONFIG_CrashDump* moduleConfig;

    UNREFERENCED_PARAMETER(Reason);
    UNREFERENCED_PARAMETER(Record);
    UNREFERENCED_PARAMETER(ReasonSpecificDataLength);

    secondaryDumpData = (KBUGCHECK_SECONDARY_DUMP_DATA*)ReasonSpecificData;
    moduleContext = DMF_CONTEXT_GET(g_DmfModuleCrashDump);
    DmfAssert(moduleContext != NULL);

    moduleConfig = DMF_CONFIG_GET(g_DmfModuleCrashDump);
    DmfAssert(moduleConfig != NULL);

    if (NULL == secondaryDumpData->OutBuffer)
    {
        // Tell caller how much additional data will be copied.
        //
        DmfAssert(moduleConfig->SecondaryData.EvtCrashDumpQuery != NULL);
        moduleConfig->SecondaryData.EvtCrashDumpQuery(g_DmfModuleCrashDump,
                                                      &secondaryDumpData->OutBuffer,
                                                      &secondaryDumpData->OutBufferLength);
    }
    else if (secondaryDumpData->OutBuffer == secondaryDumpData->InBuffer)
    {
        ULONG totalLength;

        // Now, output the data by using our own buffer.
        //     

        // First the GUID.
        //
        memcpy(&secondaryDumpData->Guid,
               &moduleConfig->SecondaryData.AdditionalDataGuid,
               sizeof(GUID));

        // Tell the client driver to copy over its data.
        //
        totalLength = secondaryDumpData->MaximumAllowed;
        DmfAssert(moduleConfig->SecondaryData.EvtCrashDumpWrite != NULL);
        moduleConfig->SecondaryData.EvtCrashDumpWrite(g_DmfModuleCrashDump,
                                                      &secondaryDumpData->OutBuffer,
                                                      &totalLength);
        if (totalLength > secondaryDumpData->MaximumAllowed)
        {
            totalLength = secondaryDumpData->MaximumAllowed;
        }
        secondaryDumpData->OutBufferLength = totalLength;
    }
}

KBUGCHECK_REASON_CALLBACK_ROUTINE CrashDump_BugCheckTriageDumpDataCallback;

VOID
CrashDump_BugCheckTriageDumpDataCallback(
    _In_ KBUGCHECK_CALLBACK_REASON Reason,
    _In_ struct _KBUGCHECK_REASON_CALLBACK_RECORD* Record,
    _Inout_ VOID* ReasonSpecificData,
    _In_ ULONG ReasonSpecificDataLength
    )
/*++

Routine Description:

    The Bug Check Callback for Triage Dump Data.  This will call the registered
    Client Driver callback, if present, for an additional opportunity to
    store blocks in the array.

Arguments:

    See MSDN: KbCallbackAddTriageDumpData callback routine

Return Value:

    None

--*/
{
    KBUGCHECK_TRIAGE_DUMP_DATA* triageDumpData;
    DMF_CONTEXT_CrashDump* moduleContext;
    DMF_CONFIG_CrashDump* moduleConfig;

    UNREFERENCED_PARAMETER(Reason);
    UNREFERENCED_PARAMETER(Record);
    UNREFERENCED_PARAMETER(ReasonSpecificDataLength);

    triageDumpData = (KBUGCHECK_TRIAGE_DUMP_DATA*)ReasonSpecificData;

    moduleContext = DMF_CONTEXT_GET(g_DmfModuleCrashDump);
    moduleConfig = DMF_CONFIG_GET(g_DmfModuleCrashDump);

    // This callback is supported only for crash dump.
    //
    if ((triageDumpData->Flags & KB_TRIAGE_DUMP_DATA_FLAG_BUGCHECK_ACTIVE) == 0)
    {
        goto Exit;
    }

    DmfAssert(moduleContext->TriageDumpDataArray != NULL);

    if (moduleConfig->TriageDumpData.EvtCrashDumpStoreTriageDumpData != NULL)
    {
        moduleConfig->TriageDumpData.EvtCrashDumpStoreTriageDumpData(g_DmfModuleCrashDump,
                                                                     triageDumpData->BugCheckCode,
                                                                     triageDumpData->BugCheckParameter1,
                                                                     triageDumpData->BugCheckParameter2,
                                                                     triageDumpData->BugCheckParameter3,
                                                                     triageDumpData->BugCheckParameter4);
    }

    // Pass the final array for processing by BugCheck.
    //
    triageDumpData->DataArray = moduleContext->TriageDumpDataArray;

Exit:
    ;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
CrashDump_DataSourceWriteInternal(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG DataSourceIndex,
    _In_ UCHAR* Buffer,
    _In_ ULONG BufferLength
    )
/*++

Routine Description:

    Write data to an Auxiliary Data Source. This function is called two ways:
    1) Trusted using RINGBUFFER_INDEX_SELF at PASSIVE_LEVEL or DISPATCH_LEVEL. In this case, it is not
        necessary to lock because the Client Driver is expected to call this function in a proper manner.
    2) Untrusted not using RINGBUFFER_INDEX_SELF at PASSIVE_LEVEL *only*. In this case, since the call from
        User-mode is untrusted, locking must be used around this call.

Arguments:

    DmfModule - This Module's handle.
    DataSourceIndex -  The Data Source to write to.
    Buffer - The data to write.
    BufferLength - The number of bytes to write.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;
    DATA_SOURCE* dataSource;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(DataSourceIndex < moduleContext->DataSourceCount);
    dataSource = &moduleContext->DataSource[DataSourceIndex];

    // Only a trusted caller makes this call.
    //
    DmfAssert(BufferLength <= dataSource->RingBufferSizeOfEachEntry);

    // Write the data to the data source.
    //
    // NOTE: This function assume a trusted caller. BufferLength must be less than or equal to
    // the size of each entry in the Ring Buffer.
    //
    ntStatus = DMF_RingBuffer_Write(dataSource->DmfModuleDataSourceRingBuffer,
                                    Buffer,
                                    BufferLength);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
CrashDump_DataSourceReadInternal(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG DataSourceIndex,
    _Out_ UCHAR* Buffer,
    _In_ ULONG BufferLength
    )
/*++

Routine Description:

    Read data from an Auxiliary Data Source. This function is called two ways:
    1) Trusted using RINGBUFFER_INDEX_SELF at PASSIVE_LEVEL or DISPATCH_LEVEL. In this case, it is not
        necessary to lock because the Client Driver is expected to call this function in a proper manner.
    2) Untrusted not using RINGBUFFER_INDEX_SELF at PASSIVE_LEVEL *only*. In this case, since the call from
        User-mode is untrusted, locking must be used around this call.

Arguments:

    DmfModule - This Module's handle.
    DataSourceIndex -  The Data Source to read from.
    Buffer - The data to write.
    BufferLength - The number of bytes to write.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;
    DATA_SOURCE* dataSource;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(DataSourceIndex < moduleContext->DataSourceCount);
    dataSource = &moduleContext->DataSource[DataSourceIndex];

    // Only a trusted caller makes this call.
    //
    DmfAssert(BufferLength <= dataSource->RingBufferSizeOfEachEntry);

    // Read the Ring Buffer data entry to the Buffer.
    //
    // NOTE: This function assume a trusted caller. BufferLength must be greater
    // the size of each entry in the Ring Buffer.
    //
    ntStatus = DMF_RingBuffer_Read(dataSource->DmfModuleDataSourceRingBuffer,
                                   Buffer,
                                   BufferLength);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
CrashDump_DataSourceCaptureInternal(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG DataSourceIndex,
    _Inout_ UCHAR* Buffer,
    _In_ ULONG BufferLength,
    _Out_ ULONG* BytesWritten
    )
/*++

Routine Description:

    Capture the Ring Buffer data from an Auxiliary Data Source. This function is called two ways:
    1) Trusted using RINGBUFFER_INDEX_SELF at PASSIVE_LEVEL or DISPATCH_LEVEL. In this case, it is not
        necessary to lock because the Client Driver is expected to call this function in a proper manner.
    2) Untrusted not using RINGBUFFER_INDEX_SELF at PASSIVE_LEVEL *only*. In this case, since the call from
        User-mode is untrusted, locking must be used around this call.

Arguments:

    DmfModule - This Module's handle.
    DataSourceIndex -  The Data Source to read from.
    Buffer - The data to write.
    BufferLength - The number of bytes to write.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;
    DATA_SOURCE* dataSource;

    DmfAssert(NULL != Buffer);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(DataSourceIndex < moduleContext->DataSourceCount);
    dataSource = &moduleContext->DataSource[DataSourceIndex];

    // Only a trusted caller makes this call.
    //
    DmfAssert(BufferLength <= dataSource->RingBufferSize);

    // Write the Ring Buffer data array to the Buffer.
    //
    // NOTE: This function assume a trusted caller. BufferLength must be greater
    // the size of each entry in the Ring Buffer.
    //
    ntStatus = DMF_RingBuffer_ReadAll(dataSource->DmfModuleDataSourceRingBuffer,
                                      Buffer,
                                      BufferLength,
                                      BytesWritten);

    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
LONG
CrashDump_FileHandleSlotFind(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject,
    _In_ DataSourceModeType DataSourceMode
    )
/*++

Routine Description:

    Find a corresponding Data Source Index given a File Object. The File Object corresponds to the file handle
    used by the file system, so it allows for a unique mapping.

Arguments:

    DmfModule - This Module's handle.
    FileObject - The unique file system "handle" to find.
    ReadOrWrite - File opened for read, write or either.

Return Value:

    Data Source Index corresponding to the FileObject or RINGBUFFER_INDEX_INVALID if the FileObject is not found.

--*/
{
    LONG returnValue;
    ULONG fileHandleIndex;
    DMF_CONTEXT_CrashDump* moduleContext;

    PAGED_CODE();

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    returnValue = RINGBUFFER_INDEX_INVALID;

    for (fileHandleIndex = RINGBUFFER_INDEX_CLIENT_FIRST; fileHandleIndex < moduleContext->DataSourceCount; fileHandleIndex++)
    {
        DATA_SOURCE* dataSource;

        dataSource = &moduleContext->DataSource[fileHandleIndex];
        if (dataSource->FileObject[DataSourceMode] == FileObject)
        {
            // The file object has been found.
            //
            returnValue = (LONG)fileHandleIndex;
            break;
        }
    }

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
LONG
CrashDump_FileHandleSlotFindForDestroy(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Find a corresponding Data Source Index given a File Object. The File Object corresponds to the file handle
    used by the file system, so it allows for a unique mapping.
    In the Destroy mode, both read and write handles are returned.

Arguments:

    DmfModule - This Module's handle.
    FileObject - The unique file system "handle" to find.
    ReadOrWrite - File opened for read, write or either.

Return Value:

    Data Source Index corresponding to the FileObject or RINGBUFFER_INDEX_INVALID if the FileObject is not found.

--*/
{
    LONG returnValue;
    ULONG fileHandleIndex;
    DMF_CONTEXT_CrashDump* moduleContext;

    PAGED_CODE();

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    returnValue = RINGBUFFER_INDEX_INVALID;

    for (fileHandleIndex = RINGBUFFER_INDEX_CLIENT_FIRST; fileHandleIndex < moduleContext->DataSourceCount; fileHandleIndex++)
    {
        DATA_SOURCE* dataSource;

        dataSource = &moduleContext->DataSource[fileHandleIndex];
        // NOTE: Update if more modes are added.
        //
        if ((dataSource->FileObject[DataSourceModeRead] == FileObject) ||
            (dataSource->FileObject[DataSourceModeWrite] == FileObject))
        {
            // The file object has been found.
            //
            returnValue = (LONG)fileHandleIndex;
            break;
        }
    }

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
BOOLEAN
CrashDump_GuidCompare(
    _In_ GUID* Left,
    _In_ GUID* Right
    )
/*++

Routine Description:

    Compare Guids.

Arguments:

    Left - First GUID to compare.
    Right - The second GUID to compare (with First).

Return Value:

    TRUE if the GUIDs match.
    FALSE if the GUIDs do not match.

--*/
{
    BOOLEAN match;

    PAGED_CODE();

    if (RtlCompareMemory(Left,
                         Right,
                         sizeof(GUID))
        == sizeof(GUID))
    {
        match = TRUE;
    }
    else
    {
        match = FALSE;
    }

    return match;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
LONG
CrashDump_FileHandlerSlotFindByGuid(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject,
    _In_ DataSourceModeType ReadOrWrite,
    _In_ const GUID* Guid
    )
/*++

Routine Description:

    Find a corresponding Data Source Index given a GUID Object. The GUID Object corresponds
    to the file handle used by the file system, so it allows for a unique mapping.

Arguments:

    DmfModule - This Module's handle.
    FileObject - The unique file system "handle" to add to an existing data source
    ReadOrWrite - File opened for read, write or either.
    Guid - Guid used to find data source entry

Return Value:

    Data Source Index corresponding to the Guid or RINGBUFFER_INDEX_INVALID if the Guid is not found.

--*/
{
    LONG returnValue;
    ULONG fileHandleIndex;
    DMF_CONTEXT_CrashDump* moduleContext;
    GUID zeroGuid;

    PAGED_CODE();

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    returnValue = RINGBUFFER_INDEX_INVALID;

    // Prevent applications from sending guids that match the default initialized GUID.
    //
    memset(&zeroGuid,
           0,
           sizeof(zeroGuid));
    if (CrashDump_GuidCompare((GUID*)Guid,
                              &zeroGuid))
    {
        goto Exit;
    }

    for (fileHandleIndex = RINGBUFFER_INDEX_CLIENT_FIRST; fileHandleIndex < moduleContext->DataSourceCount; fileHandleIndex++)
    {
        DATA_SOURCE* dataSource;

        dataSource = &moduleContext->DataSource[fileHandleIndex];

        if (CrashDump_GuidCompare((GUID*)Guid,
                                  &dataSource->RingBufferGuid))
        {
            dataSource->FileObject[ReadOrWrite] = FileObject;

            returnValue = (LONG)fileHandleIndex;
            goto Exit;
        }
    }

Exit:

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
LONG
CrashDump_FileHandlerSlotFindByGuidForReuse(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject,
    _In_ DataSourceModeType ReadOrWrite,
    _In_ const GUID* Guid
    )
/*++

Routine Description:

    Find a corresponding Data Source Index given a GUID Object. The GUID Object corresponds
    to the file handle used by the file system, so it allows for a unique mapping. If the GUID
    is found and it is not currently in use, that record is returned. In this way, Data Sources
    can be appended to even after Clients have closed handles.

Arguments:

    DmfModule - This Module's handle.
    FileObject - The unique file system "handle" to add to an existing data source
    ReadOrWrite - File opened for read, write or either.
    Guid - Guid used to find data source entry

Return Value:

    Data Source Index corresponding to the Guid or RINGBUFFER_INDEX_INVALID if the Guid is not found.

--*/
{
    LONG returnValue;
    ULONG fileHandleIndex;
    DMF_CONTEXT_CrashDump* moduleContext;
    GUID zeroGuid;

    PAGED_CODE();

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    returnValue = RINGBUFFER_INDEX_INVALID;

    // Prevent applications from sending guids that match the default initialized GUID.
    //
    memset(&zeroGuid,
           0,
           sizeof(zeroGuid));
    if (CrashDump_GuidCompare((GUID*)Guid,
                              &zeroGuid))
    {
        goto Exit;
    }

    for (fileHandleIndex = RINGBUFFER_INDEX_CLIENT_FIRST; fileHandleIndex < moduleContext->DataSourceCount; fileHandleIndex++)
    {
        DATA_SOURCE* dataSource;

        dataSource = &moduleContext->DataSource[fileHandleIndex];

        if (CrashDump_GuidCompare((GUID*)Guid,
                                  &dataSource->RingBufferGuid) &&
            ((NULL == dataSource->FileObject[ReadOrWrite]) ||
             (FILE_OBJECT_ORPHAN == dataSource->FileObject[ReadOrWrite])))
        {
            dataSource->FileObject[ReadOrWrite] = FileObject;

            returnValue = (LONG)fileHandleIndex;
            goto Exit;
        }
    }

Exit:

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
LONG
CrashDump_FileHandleSlotAllocate(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Allocate a corresponding Data Source Index given a File Object. The File Object corresponds to the file handle
    used by the file system, so it allows for a unique mapping.

Arguments:

    DmfModule - This Module's handle.
    FileObject -  The unique file system "handle" to allocate a Data Source index for.
    ReadOrWrite - File opened for read, write or either.

Return Value:

    Data Source Index corresponding to the FileObject or RINGBUFFER_INDEX_INVALID if the FileObject already
    corresponds to a Data Source Index.

--*/
{
    LONG returnValue;
    ULONG fileHandleIndex;
    DMF_CONTEXT_CrashDump* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    // Make sure this handle is not already in use.
    //
    returnValue = CrashDump_FileHandleSlotFind(DmfModule,
                                               FileObject,
                                               DataSourceModeWrite);
    if (returnValue != RINGBUFFER_INDEX_INVALID)
    {
        // It already exists...
        //
        returnValue = RINGBUFFER_INDEX_INVALID;
        goto Exit;
    }

    // Find a new location for the handle.
    //
    for (fileHandleIndex = RINGBUFFER_INDEX_CLIENT_FIRST; fileHandleIndex < moduleContext->DataSourceCount; fileHandleIndex++)
    {
        DATA_SOURCE* dataSource;

        dataSource = &moduleContext->DataSource[fileHandleIndex];
        if (NULL == dataSource->FileObject[DataSourceModeWrite])
        {
            // Empty slot found.
            //
            dataSource->FileObject[DataSourceModeWrite] = FileObject;
            returnValue = (LONG)fileHandleIndex;
            goto Exit;
        }
    }

Exit:

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
LONG
CrashDump_FileHandleSlotFree(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Free a corresponding Data Source Index given a File Object. The File Object corresponds to the file handle
    used by the file system, so it allows for a unique mapping.

Arguments:

    DmfModule - This Module's handle.
    FileObject -  The unique file system "handle" to free.

Return Value:

    Data Source Index corresponding to the FileObject or RINGBUFFER_INDEX_INVALID if the FileObject is not found.

--*/
{
    LONG returnValue;
    ULONG fileHandleIndex;
    DMF_CONTEXT_CrashDump* moduleContext;
    DATA_SOURCE* dataSource;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    // Find a slot that has the FileObject open for either read or write.
    //
    fileHandleIndex = CrashDump_FileHandleSlotFindForDestroy(DmfModule,
                                                             FileObject);
    returnValue = fileHandleIndex;
    if (RINGBUFFER_INDEX_INVALID == fileHandleIndex)
    {
        goto Exit;
    }

    DmfAssert(fileHandleIndex < moduleContext->DataSourceCount);
    dataSource = &moduleContext->DataSource[fileHandleIndex];
    // NOTE: Update for additional modes.
    //
    if (FileObject == dataSource->FileObject[DataSourceModeRead])
    {
        dataSource->FileObject[DataSourceModeRead] = NULL;
    }
    else
    {
        dataSource->FileObject[DataSourceModeWrite] = NULL;
    }

Exit:

    return returnValue;
}
#pragma code_seg()

enum
{
    CrashDump_ModuleIndex_RingBuffer,
    CrashDump_NumberOfModules
};

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
CrashDump_DmfCreate_RingBuffer(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG DataSourceIndex,
    _In_ ULONG ItemCount,
    _In_ ULONG ItemSize
    )
/*++

Routine Description:

    Create and Open Ring Buffer of the corresponding Data Source.
    NOTE: In this Module the Dependent Module is dynamically created. Generally, Dependent Modules are
          created when the Parent Module is created. But, in this case that information is not known.

Arguments:

    DmfModule - This Module's handle.
    DataSourceIndex - The index of the corresponding Data Source.
    ItemCount - The number of entries to create in the Ring Buffer.
    ItemSize - The size of each entry in the Ring Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_RingBuffer moduleConfigRingBuffer;
    DMF_CONTEXT_CrashDump* moduleContext;
    DMF_CONFIG_CrashDump* moduleConfig;
    WDFDEVICE device;
    DATA_SOURCE* dataSource;
    WDF_OBJECT_ATTRIBUTES attributes;
    DMF_MODULE_ATTRIBUTES moduleAttributes;

    PAGED_CODE();

    ntStatus = STATUS_INVALID_PARAMETER;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Do some sanity checking of values.
    //
    if (0 == ItemCount)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid ItemCount=0");
        goto Exit;
    }

    if (0 == ItemSize)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid ItemSize=0");
        goto Exit;
    }

    if (DataSourceIndex != RINGBUFFER_INDEX_SELF)
    {
        // Validate that the size of the User-mode Ring Buffer is not larger than the maximum specified by
        // Client Driver. This is especially important to ensure User-mode component does not use
        // too much memory when creating a Data Source. Realistically, 8-K bytes should be the 
        // maximum. The allocated NonPagedPool is very precious.
        //
        ULONG ringBufferSize;

        ringBufferSize = ItemCount * ItemSize;
        DmfAssert(moduleConfig->SecondaryData.RingBufferMaximumSize > 0);
        if (ringBufferSize > moduleConfig->SecondaryData.RingBufferMaximumSize)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ringBufferSize=%d RingBufferMaximumSize=%d", ringBufferSize, moduleConfig->SecondaryData.RingBufferMaximumSize);
            goto Exit;
        }
    }
    else
    {
        // The Client Driver is trusted to choose an appropriate buffer size for its own Ring Buffer.
        //
    }

    device = DMF_ParentDeviceGet(DmfModule);

    DmfAssert(DataSourceIndex < moduleContext->DataSourceCount);
    dataSource = &moduleContext->DataSource[DataSourceIndex];
    DmfAssert(NULL == dataSource->DmfModuleDataSourceRingBuffer);

    // RingBuffer
    // ----------
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = device;

    DMF_CONFIG_RingBuffer_AND_ATTRIBUTES_INIT(&moduleConfigRingBuffer,
                                              &moduleAttributes);
    moduleConfigRingBuffer.ItemCount = ItemCount;
    moduleConfigRingBuffer.ItemSize = ItemSize;
    moduleConfigRingBuffer.Mode = RingBuffer_Mode_DeleteOldestIfFullOnWrite;
    moduleAttributes.ClientModuleInstanceName = "DataSourceRingBuffer";
    ntStatus = DMF_RingBuffer_Create(device,
                                     &moduleAttributes,
                                     &attributes,
                                     &dataSource->DmfModuleDataSourceRingBuffer);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_RingBuffer_Create DataSourceIndex=%d fails: ntStatus=%!STATUS!", DataSourceIndex, ntStatus);
        goto Exit;
    }

    DmfAssert(NULL != dataSource->DmfModuleDataSourceRingBuffer);

    // Get the size of the Ring Buffer to be written.
    //
    DMF_RingBuffer_TotalSizeGet(dataSource->DmfModuleDataSourceRingBuffer,
                                &dataSource->RingBufferSize);
    dataSource->RingBufferSizeOfEachEntry = ItemSize;

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
CrashDump_DmfDestroy_RingBuffer(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG DataSourceIndex
    )
/*++

Routine Description:

    Close and Destroy the Ring Buffer of the corresponding Data Source.

Arguments:

    DmfModule - This Module's handle.
    DataSourceIndex - The index of the corresponding Data Source.

Return Value:

    None

--*/
{
    DMF_CONTEXT_CrashDump* moduleContext;
    DATA_SOURCE* dataSource;

    PAGED_CODE();

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(DataSourceIndex < moduleContext->DataSourceCount);
    dataSource = &moduleContext->DataSource[DataSourceIndex];

    // Close and Destroy the DMF Module for the associated Ring Buffer.
    //
    if (dataSource->DmfModuleDataSourceRingBuffer != NULL)
    {
        DmfAssert(dataSource->DmfModuleDataSourceRingBuffer != NULL);
        WdfObjectDelete(dataSource->DmfModuleDataSourceRingBuffer);
        dataSource->DmfModuleDataSourceRingBuffer = NULL;

        DmfAssert((NULL == dataSource->FileObject[DataSourceModeRead]) && 
                  (NULL == dataSource->FileObject[DataSourceModeWrite]));
    }

    RtlZeroMemory(&dataSource->RingBufferGuid,
                  sizeof(GUID));
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
CrashDump_DataSourceCreateInternal(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG DataSourceIndex,
    _In_ ULONG ItemCount,
    _In_ ULONG SizeOfEachEntry,
    _In_ const GUID* Guid
    )
/*++

Routine Description:

    Create a Data Source. It registers a corresponding Kernel Bug Check callback and allocates all resources
    used by the Data Source. The corresponding Data Source Index will be unavailable for writing, but not
    available to be created.

Arguments:

    DmfModule - This Module's handle.
    DataSourceIndex - The Data Source to create.
    ItemCount - The number of entries allocated in the corresponding Ring Buffer.
    SizeOfEachEntry - The size of each entry in the Ring Buffer.
    Guid - GUID that identifies the Data Source.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;
    DMF_CONFIG_CrashDump* moduleConfig;
    DATA_SOURCE* dataSource;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    DmfAssert(DataSourceIndex < moduleContext->DataSourceCount);
    dataSource = &moduleContext->DataSource[DataSourceIndex];
    if (dataSource->DmfModuleDataSourceRingBuffer != NULL)
    {
        // Do not assert here because untrusted components can call. Only assert for
        // trusted index.
        //
        DmfAssert(DataSourceIndex != RINGBUFFER_INDEX_SELF);
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DataSourceIndex=%d Handle already open", DataSourceIndex);
        goto Exit;
    }

    // Create the ring buffer for the data source.
    //
    ntStatus = CrashDump_DmfCreate_RingBuffer(DmfModule,
                                              DataSourceIndex,
                                              ItemCount,
                                              SizeOfEachEntry);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CrashDump_DmfCreate_RingBuffer DataSourceIndex=%d", DataSourceIndex);
        goto Exit;
    }

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Save the Crash Dump Data Source GUID that is written to the file.
    //
    dataSource->RingBufferGuid = *Guid;

    //  Create the encryption key used by DMF_RingBuffer_Xor.
    //
    dataSource->RingBufferEncryptionKeySize = ENCRYPTION_KEY_STRING_SIZE;

    sprintf_s((CHAR*)dataSource->RingBufferEncryptionKey,
              dataSource->RingBufferEncryptionKeySize + sizeof(CHAR),
              "%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X",
              dataSource->RingBufferGuid.Data1,
              dataSource->RingBufferGuid.Data2,
              dataSource->RingBufferGuid.Data3,
              dataSource->RingBufferGuid.Data4[0],
              dataSource->RingBufferGuid.Data4[1],
              dataSource->RingBufferGuid.Data4[2],
              dataSource->RingBufferGuid.Data4[3],
              dataSource->RingBufferGuid.Data4[4],
              dataSource->RingBufferGuid.Data4[5],
              dataSource->RingBufferGuid.Data4[6],
              dataSource->RingBufferGuid.Data4[7]);

    DmfAssert(strlen(dataSource->RingBufferEncryptionKey) <= ENCRYPTION_KEY_STRING_SIZE);

    // Register the callback function that is called for all the Ring Buffers.
    //
    KeInitializeCallbackRecord(&moduleContext->BugCheckCallbackRecordRingBuffer[DataSourceIndex]);
    if (! KeRegisterBugCheckReasonCallback(&moduleContext->BugCheckCallbackRecordRingBuffer[DataSourceIndex],
                                           CrashDump_BugCheckSecondaryDumpDataCallbackRingBuffer,
                                           KbCallbackSecondaryDumpData,
                                           moduleConfig->ComponentName))
    {
        // The Crash Dump Callback cannot be created. In this case, destroy the Ring Buffer created above because both the Ring Buffer and
        // the callback must be instantiated or neither must be instantiated. The reason for that is that there is no way
        // to check if the callback registration was successful later on. Furthermore, it is important to remember that
        // the callback must be registered after the Ring Buffer has been successfully created.
        //
        CrashDump_DmfDestroy_RingBuffer(DmfModule,
                                        DataSourceIndex);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "KeRegisterBugCheckReasonCallback DataSourceIndex=%d", DataSourceIndex);
        ntStatus = STATUS_INVALID_PARAMETER;
        // It should not happen.
        //
        DmfAssert(FALSE);
        goto Exit;
    }

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
CrashDump_DataSourceDestroyInternal(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG DataSourceIndex
    )
/*++

Routine Description:

    Destroy a Data Source. It unregisters the corresponding Kernel Bug Check callback and frees all resources
    used by the Data Source. The corresponding Data Source Index will be unavailable for writing, but available
    for creation.

Arguments:

    DmfModule - This Module's handle.
    DataSourceIndex - The Data Source to destroy.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(DataSourceIndex < moduleContext->DataSourceCount);

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    // Unregister the callback for the Ring Buffer. This Bug Check Callback is unregistered while the Ring Buffer used by
    // the callback is still allocated.
    //
    if (! KeDeregisterBugCheckReasonCallback(&moduleContext->BugCheckCallbackRecordRingBuffer[DataSourceIndex]))
    {
        // It can fail here if the call back could not be allocated due to resource allocation failure.
        //
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "KeDeregisterBugCheckReasonCallback DataSourceIndex=%d", DataSourceIndex);
    }

    // Close and Destroy the Ring Buffer.
    //
    CrashDump_DmfDestroy_RingBuffer(DmfModule,
                                    DataSourceIndex);

    ntStatus = STATUS_SUCCESS;

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
CrashDump_DataSourceDestroyAuxiliaryInternal(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Destroy a Data Source. It unregisters the corresponding Kernel Bug Check callback and frees all resources
    used by the Data Source. The corresponding Data Source Index will be unavailable for writing, but available
    for creation.

Arguments:

    DmfModule - This Module's handle.
    FileObject - The Data Source Reference to destroy.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;
    ULONG dataSourceIndex;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // It is called by a trusted component.
    //
    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    // Deallocate the slot so it can be used by another client.
    //
    dataSourceIndex = CrashDump_FileHandleSlotFree(DmfModule,
                                                   FileObject);
    if (RINGBUFFER_INDEX_INVALID == dataSourceIndex)
    {
        ntStatus = STATUS_INVALID_HANDLE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CrashDump_FileHandleSlotFree FileObject=%p", FileObject);
        goto Exit;
    }

    // Destroy the Auxiliary Data Source.
    //
    ntStatus = CrashDump_DataSourceDestroyInternal(DmfModule,
                                                   dataSourceIndex);
    DmfAssert(NT_SUCCESS(ntStatus));

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
CrashDump_DataSourceCreateAuxiliary(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject,
    _In_ ULONG ItemCount,
    _In_ ULONG SizeOfEachEntry,
    _In_ const GUID* Guid
    )
/*++

Routine Description:

    Create a Auxiliary Data Source. It registers a corresponding Kernel Bug Check callback and allocates all resources
    used by the Data Source. The corresponding Data Source Index will be unavailable for writing, but not
    available to be created.

Arguments:

    DmfModule - This Module's handle.
    FileObject -  The Data Source Reference to create.
    ItemCount: The number of entries allocated in the corresponding Ring Buffer.
    SizeOfEachEntry: The size of each entry in the Ring Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;
    ULONG dataSourceIndex;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // It is called by untrusted component. It must be locked.
    //
    DMF_ModuleLock(DmfModule);

    // Slot was not found using the FileObject. See if a slot exists associate to the Guid.
    //
    dataSourceIndex = CrashDump_FileHandlerSlotFindByGuidForReuse(DmfModule,
                                                                  FileObject,
                                                                  DataSourceModeWrite,
                                                                  Guid);
    if (RINGBUFFER_INDEX_INVALID != dataSourceIndex)
    {
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    // Find an unused Ring Buffer and allocate it.
    //
    dataSourceIndex = CrashDump_FileHandleSlotAllocate(DmfModule,
                                                       FileObject);
    if (RINGBUFFER_INDEX_INVALID == dataSourceIndex)
    {
        ntStatus = STATUS_NO_MORE_FILES;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No more space for additional Ring Buffers.");
        goto Exit;
    }

    ntStatus = CrashDump_DataSourceCreateInternal(DmfModule,
                                                  dataSourceIndex,
                                                  ItemCount,
                                                  SizeOfEachEntry,
                                                  Guid);
    if (! NT_SUCCESS(ntStatus))
    {
        // The slot was allocated above...free it now since it is not in use.
        //
        dataSourceIndex = CrashDump_FileHandleSlotFree(DmfModule,
                                                       FileObject);
        DmfAssert(dataSourceIndex != RINGBUFFER_INDEX_INVALID);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CrashDump_DataSourceCreateInternal ntStatus=%!STATUS!", ntStatus);
    }

Exit:

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
CrashDump_DataSourceOpenAuxiliary(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject,
    _Out_ ULONG* DataSourceIndex,
    _In_ const GUID* Guid
    )
/*++

Routine Description:

    Create a Auxiliary Data Source. It registers a corresponding Kernel Bug Check callback and allocates all resources
    used by the Data Source. The corresponding Data Source Index will be unavailable for writing, but not
    available to be created.

Arguments:

    DmfModule - This Module's handle.
    FileObject - The Data Source Reference to create.
    ItemCount - The number of entries allocated in the corresponding Ring Buffer.
    SizeOfEachEntry - The size of each entry in the Ring Buffer.
    Guid - The Guid to associated with the new entry.
    ReadOrWrite - File opened for read, write or either.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;
    ULONG dataSourceIndex;

    PAGED_CODE();

    ntStatus = STATUS_UNSUCCESSFUL;
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // It is called by untrusted component. It must be locked.
    //
    DMF_ModuleLock(DmfModule);
    // Slot was not found using the FileObject. See if a slot exists associate to the Guid.
    //
    dataSourceIndex = CrashDump_FileHandlerSlotFindByGuid(DmfModule,
                                                          FileObject,
                                                          DataSourceModeRead,
                                                          Guid);
    if (RINGBUFFER_INDEX_INVALID != dataSourceIndex)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "CrashDump_FileHandlerSlotFindByGuid Existing dataSource =0x%08X", dataSourceIndex);
        *DataSourceIndex = dataSourceIndex;
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

Exit:

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
CrashDump_DataSourceCreateSelf(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG ItemCount,
    _In_ ULONG SizeOfEachEntry,
    _In_ const GUID* Guid
    )
/*++

Routine Description:

    Create the Client Driver's Data Source. It registers a corresponding Kernel Bug Check callback and allocates all resources
    used by the Data Source. The corresponding Data Source Index will be unavailable for writing, but not available to be created.

Arguments:

    DmfModule - This Module's handle.
    ItemCount - The number of entries allocated in the corresponding Ring Buffer.
    SizeOfEachEntry - The size of each entry in the Ring Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Create the Client Driver's Ring Buffer. Since the caller is trusted (because it is called by the Client Driver), it is not necessary
    // to lock this object. However, since the function to destroy the Ring Buffer's assumes the caller has locked the object since it can
    // be used by untrusted components. Therefore, acquire the lock.
    //
    DMF_ModuleLock(DmfModule);

    ntStatus = CrashDump_DataSourceCreateInternal(DmfModule,
                                                  RINGBUFFER_INDEX_SELF,
                                                  ItemCount,
                                                  SizeOfEachEntry,
                                                  Guid);

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
CrashDump_TriageDataCreateInternal(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Allocate a triage dump data array, and register a Triage Dump Data Kernel Bug Check callback.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;
    DMF_CONFIG_CrashDump* moduleConfig;
    ULONG bufferSize;
    ULONG arraySize;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    FuncEntry(DMF_TRACE);

    arraySize = moduleConfig->TriageDumpData.TriageDumpDataArraySize;
    if (arraySize == 0)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        DmfAssert(FALSE);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid Array size");
        goto Exit;
    }

    // Allocate and initialize the triage dump data array
    //
    bufferSize = ((FIELD_OFFSET(KTRIAGE_DUMP_DATA_ARRAY, Blocks)) + (sizeof(KADDRESS_RANGE) * arraySize));
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = g_DmfModuleCrashDump;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               bufferSize,
                               &moduleContext->TriageDumpDataArrayMemory,
                               (VOID**)&moduleContext->TriageDumpDataArray);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = KeInitializeTriageDumpDataArray(moduleContext->TriageDumpDataArray,
                                               bufferSize);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "KeInitializeTriageDumpDataArray fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

     TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Registering Bug Check Triage Dump Data Callback");
    // Set up the callback record. This is set up even if the Client did not provide its own callback
    // since the array could be populated during runtime and must still be added in this callback.
    //
    if (! KeRegisterBugCheckReasonCallback(&moduleContext->BugCheckCallbackRecordTriageDumpData,
                                          CrashDump_BugCheckTriageDumpDataCallback,
                                          KbCallbackTriageDumpData,
                                          moduleConfig->ComponentName))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "KeRegisterBugCheckReasonCallback TriageDumpData");
        ntStatus = STATUS_INVALID_PARAMETER;
        // It should not happen.
        //
        DmfAssert(FALSE);
        goto Exit;
    }

    if (moduleConfig->TriageDumpData.EvtCrashDumpStoreTriageDumpData == NULL)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "No Triage Data Callback provided");
    }

Exit:

    if (! NT_SUCCESS(ntStatus))
    {
        if (moduleContext->TriageDumpDataArrayMemory != NULL)
        {
            WdfObjectDelete(moduleContext->TriageDumpDataArrayMemory);
            moduleContext->TriageDumpDataArrayMemory = NULL;
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
CrashDump_TriageDataDestroyInternal(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

     Unregisters the corresponding Bug Check Triage Dump Data Callback and frees all resources.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None.

--*/
{
    DMF_CONFIG_CrashDump* moduleConfig;
    DMF_CONTEXT_CrashDump* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    // A callback could not be registered without an array, so check for the
    // existence of an array first.
    //
    if (moduleContext->TriageDumpDataArray != NULL)
    {
        // Unregister the callback. This Bug Check Callback is unregistered while the triage data array
        // is still allocated
        //

        if (moduleContext->BugCheckCallbackRecordTriageDumpData.Reason != KbCallbackInvalid)
        {
            if (! KeDeregisterBugCheckReasonCallback(&moduleContext->BugCheckCallbackRecordTriageDumpData))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "KeDeregisterBugCheckReasonCallback TriageData");
            }
        }

        WdfObjectDelete(moduleContext->TriageDumpDataArrayMemory);
        moduleContext->TriageDumpDataArrayMemory = NULL;
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
CrashDump_DataSourceDestroyAuxiliary(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Destroy a Data Source. It unregisters the corresponding Kernel Bug Check callback and frees all resources
    used by the Data Source. The corresponding Data Source Index will be unavailable for writing, but available
    for creation.

Arguments:

    DmfModule - This Module's handle.
    FileObject - The Data Source Reference to destroy.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // It is called by untrusted component. It must be locked.
    //
    DMF_ModuleLock(DmfModule);

    ntStatus = CrashDump_DataSourceDestroyAuxiliaryInternal(DmfModule,
                                                            FileObject);

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
CrashDump_DataSourceWriteAuxiliary(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject,
    _In_ UCHAR* Buffer,
    _In_ ULONG BufferLength
    )
/*++

Routine Description:

    Write data to an Auxiliary Data Source.

Arguments:

    DmfModule - This Module's handle.
    FileObject - The Data Source Reference to write to.
    Buffer - The data to write.
    BufferLength - The number of bytes to write.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;
    ULONG dataSourceIndex;
    DATA_SOURCE* dataSource;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    // This call is from untrusted PASSIVE_LEVEL component. This object's lock must be acquired.
    //
    DMF_ModuleLock(DmfModule);

    dataSourceIndex = CrashDump_FileHandleSlotFind(DmfModule,
                                                   FileObject,
                                                   DataSourceModeWrite);

    if (RINGBUFFER_INDEX_INVALID == dataSourceIndex)
    {
        ntStatus = STATUS_INVALID_HANDLE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CrashDump_FileHandleSlotFind FileObject=%p", FileObject);
        goto Exit;
    }

    // Write the data from the Data Source to its respective Ring Buffer. If it does not fit, an error 
    // is returned from the Ring Buffer.
    //
    DmfAssert(dataSourceIndex < moduleContext->DataSourceCount);
    dataSource = &moduleContext->DataSource[dataSourceIndex];

    // If the file object is valid, then this handle must be open.
    //
    DmfAssert(dataSource->DmfModuleDataSourceRingBuffer != NULL);

    // This call may be made by an untrusted caller. Validate the size of the write.
    //
    if (BufferLength > dataSource->RingBufferSizeOfEachEntry)
    {
        ntStatus = STATUS_BUFFER_OVERFLOW;
        goto Exit;
    }

    ntStatus = CrashDump_DataSourceWriteInternal(DmfModule,
                                                 dataSourceIndex,
                                                 Buffer,
                                                 BufferLength);

Exit:

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
CrashDump_DataSourceReadAuxiliary(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject,
    _Out_ UCHAR* Buffer,
    _In_ ULONG BufferLength
    )
/*++

Routine Description:

    Read data from an Auxiliary Data Source.

Arguments:

    DmfModule - This Module's handle.
    FileObject - The Data Source Reference to read from.
    Buffer - Read data being returned.
    BufferLength - The number of bytes to write.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;
    ULONG dataSourceIndex;
    DATA_SOURCE* dataSource;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    // This call is from untrusted PASSIVE_LEVEL component. This object's lock must be acquired.
    //
    DMF_ModuleLock(DmfModule);

    dataSourceIndex = CrashDump_FileHandleSlotFind(DmfModule,
                                                   FileObject,
                                                   DataSourceModeRead);

    if (RINGBUFFER_INDEX_INVALID == dataSourceIndex)
    {
        ntStatus = STATUS_INVALID_HANDLE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CrashDump_FileHandleSlotFind FileObject=%p", FileObject);
        goto Exit;
    }

    // Read the data from its respective Ring Buffer. If it does not fit, an error 
    // is returned from the Ring Buffer.
    //
    DmfAssert(dataSourceIndex < moduleContext->DataSourceCount);
    dataSource = &moduleContext->DataSource[dataSourceIndex];

    // If the file object is valid, then this handle must be open.
    //
    DmfAssert(dataSource->DmfModuleDataSourceRingBuffer != NULL);

    // This call may be made by an untrusted caller. Validate the size of the buffer for the read data size.
    //
    if (BufferLength < dataSource->RingBufferSizeOfEachEntry)
    {
        ntStatus = STATUS_BUFFER_OVERFLOW;
        goto Exit;
    }

    ntStatus = CrashDump_DataSourceReadInternal(DmfModule,
                                                dataSourceIndex,
                                                Buffer,
                                                BufferLength);

Exit:

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
CrashDump_DataSourceCaptureAuxiliary(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject,
    _Out_writes_(BufferLength) UCHAR* Buffer,
    _In_ ULONG BufferLength,
    _Out_ ULONG* BytesWritten
    )
/*++

Routine Description:

    Capture data from an Auxiliary Data Source Ring Buffer.

Arguments:

    DmfModule - This Module's handle.
    FileObject - The Data Source Reference to capture from.
    Buffer - Captured data being returned.
    BufferLength - The number of bytes to write (must be size of Ring Buffer or greater).

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;
    ULONG dataSourceIndex;
    DATA_SOURCE* dataSource;

    PAGED_CODE();

    *BytesWritten = 0;
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    DmfAssert(Buffer != NULL);

    DmfAssert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    // This call is from untrusted PASSIVE_LEVEL component. This object's lock must be acquired.
    //
    DMF_ModuleLock(DmfModule);

    dataSourceIndex = CrashDump_FileHandleSlotFind(DmfModule,
                                                   FileObject,
                                                   DataSourceModeRead);

    if (RINGBUFFER_INDEX_INVALID == dataSourceIndex)
    {
        ntStatus = STATUS_INVALID_HANDLE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CrashDump_FileHandleSlotFind FileObject=%p", FileObject);
        goto Exit;
    }

    // From the respective Ring Buffer,capture the Ring Buffer. If it does not fit, an error 
    // is returned from the Ring Buffer.
    //
    DmfAssert(dataSourceIndex < moduleContext->DataSourceCount);
    dataSource = &moduleContext->DataSource[dataSourceIndex];

    // If the file object is valid, then this handle must be open.
    //
    DmfAssert(dataSource->DmfModuleDataSourceRingBuffer != NULL);

    // This call may be made by an untrusted caller. Validate the size of the buffer for the read data size.
    //
    if (BufferLength < dataSource->RingBufferSize)
    {
        ntStatus = STATUS_BUFFER_OVERFLOW;
        goto Exit;
    }

    //'Using uninitialized memory '*Buffer'.'
    //
    #pragma warning(suppress: 6001)
    ntStatus = CrashDump_DataSourceCaptureInternal(DmfModule,
                                                   dataSourceIndex,
                                                   Buffer,
                                                   BufferLength,
                                                   BytesWritten);

Exit:

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
CrashDump_DataSourceOrphanCreate(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Orphans a Data Source Index. It means the Data Source has been created and written to, but left purposefully
    created so that when the system crash the Data Source data buffers can be written to the crash dump.

Arguments:

    DmfModule - This Module's handle.
    FileObject - The Data Source Reference to write to.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_CrashDump* moduleContext;
    ULONG dataSourceIndex;
    DATA_SOURCE* dataSource;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    // This call is from untrusted PASSIVE_LEVEL component. This object's lock must be acquired.
    //
    DMF_ModuleLock(DmfModule);

    dataSourceIndex = CrashDump_FileHandleSlotFindForDestroy(DmfModule,
                                                             FileObject);

    if (RINGBUFFER_INDEX_INVALID == dataSourceIndex)
    {
        // It is not an error because the Data Source could have been destroyed by the application.
        //
        goto Exit;
    }

    DmfAssert(dataSourceIndex < moduleContext->DataSourceCount);
    dataSource = &moduleContext->DataSource[dataSourceIndex];

    DmfAssert(dataSource->DmfModuleDataSourceRingBuffer != NULL);

    if (dataSource->FileObject[DataSourceModeRead] == FileObject)
    {
        // Read Mode data sources are never orphaned. Just clear the read fileobject slot.
        //
        dataSource->FileObject[DataSourceModeRead] = NULL;
        goto Exit;
    }

    if (dataSource->FileObject[DataSourceModeWrite] == FileObject)
    {
        // This Data Source Index will never be used again for the life of the driver.
        //
        dataSource->FileObject[DataSourceModeWrite] = FILE_OBJECT_ORPHAN;
    }

Exit:

    DMF_ModuleUnlock(DmfModule);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
NTSTATUS
CrashDump_DataSourceCreateFromRequest(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    Create a data source from a Request.

Arguments:

    DmfModule - This Module's handle.
    Request - The request with the information about how to create the Data Source.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFMEMORY memory;
    size_t bufferSize;
    void* inputBuffer;
    DATA_SOURCE_CREATE* dataSourceCreate;
    WDFFILEOBJECT fileObject;

    PAGED_CODE();

    *BytesReturned = 0;

    // Retrieve the input buffer which contains the information about the Ring Buffer to create.
    //
    ntStatus = WdfRequestRetrieveInputMemory(Request,
                                             &memory);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    bufferSize = 0;
    inputBuffer = WdfMemoryGetBuffer(memory,
                                     &bufferSize);
    if (NULL == inputBuffer)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    if (bufferSize < sizeof(DATA_SOURCE_CREATE))
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    dataSourceCreate = (DATA_SOURCE_CREATE*)inputBuffer;

    // Get the unique identifier for this request.
    //
    fileObject = WdfRequestGetFileObject(Request);
    if (NULL == fileObject)
    {
        ntStatus = STATUS_INVALID_ADDRESS;
        goto Exit;
    }

    // Create the ring buffer for the data source.
    //
    ntStatus = CrashDump_DataSourceCreateAuxiliary(DmfModule,
                                                   fileObject,
                                                   dataSourceCreate->EntriesCount,
                                                   dataSourceCreate->EntrySize,
                                                   &dataSourceCreate->Guid);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    *BytesReturned = bufferSize;

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
NTSTATUS
CrashDump_DataSourceOpenFromRequest(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    Open a Data Source from a Request.

Arguments:

    DmfModule - This Module's handle.
    Request - The request with the information about the Data Source to open.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFMEMORY memory;
    size_t inputBufferSize;
    void* inputBuffer;
    size_t outputBufferSize;
    void* outputBuffer;
    DATA_SOURCE_CREATE* dataSourceCreate;
    WDFFILEOBJECT fileObject;
    ULONG dataSourceIndex;
    DATA_SOURCE_CREATE* dataSourceReturn;
    DATA_SOURCE* dataSource;
    DMF_CONTEXT_CrashDump* moduleContext;

    PAGED_CODE();

    inputBufferSize = 0;
    outputBufferSize = 0;
    *BytesReturned = 0;

    // Retrieve the input buffer which contains the information about the Ring Buffer to create.
    //
    ntStatus = WdfRequestRetrieveInputMemory(Request,
                                             &memory);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    inputBuffer = WdfMemoryGetBuffer(memory,
                                     &inputBufferSize);
    if (NULL == inputBuffer)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    if (inputBufferSize < sizeof(DATA_SOURCE_CREATE))
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    dataSourceCreate = (DATA_SOURCE_CREATE*)inputBuffer;

    // Retrieve the output buffer which contains the information about the Ring Buffer to create.
    //
    ntStatus = WdfRequestRetrieveOutputMemory(Request,
                                              &memory);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    outputBuffer = WdfMemoryGetBuffer(memory,
                                      &outputBufferSize);
    if (NULL == outputBuffer)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    if (outputBufferSize < sizeof(DATA_SOURCE_CREATE))
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    dataSourceReturn = (DATA_SOURCE_CREATE*)outputBuffer;

    // Get the unique identifier for this request.
    //
    fileObject = WdfRequestGetFileObject(Request);
    if (NULL == fileObject)
    {
        ntStatus = STATUS_INVALID_ADDRESS;
        goto Exit;
    }

    // Create the ring buffer for the data source.
    //
    dataSourceIndex = 0;
    ntStatus = CrashDump_DataSourceOpenAuxiliary(DmfModule,
                                                 fileObject,
                                                 &dataSourceIndex,
                                                 &dataSourceCreate->Guid);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CrashDump_DataSourceOpenAuxiliary fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(dataSourceIndex < moduleContext->DataSourceCount);

    dataSource = &moduleContext->DataSource[dataSourceIndex];
    dataSourceReturn->EntrySize = dataSource->RingBufferSizeOfEachEntry;
    DmfAssert(dataSource->RingBufferSizeOfEachEntry != 0);
    dataSourceReturn->EntriesCount = dataSource->RingBufferSize / dataSource->RingBufferSizeOfEachEntry;
    dataSourceReturn->Guid = dataSourceCreate->Guid;

    // Allow caller to validate the bytes written.
    //
    *BytesReturned = sizeof(DATA_SOURCE_CREATE);

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
NTSTATUS
CrashDump_DataSourceDestroyFromRequest(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    Destroy a Data Source from a Request.

Arguments:

    DmfModule - This Module's handle.
    Request - The request with the information about the Data Source to destroy.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    WDFFILEOBJECT fileObject;

    PAGED_CODE();

    *BytesReturned = 0;

    // Get the unique identifier for this request.
    //
    fileObject = WdfRequestGetFileObject(Request);
    if (NULL == fileObject)
    {
        ntStatus = STATUS_INVALID_ADDRESS;
        goto Exit;
    }

    // Destroy the Ring Buffer.
    //
    CrashDump_DataSourceDestroyAuxiliary(DmfModule,
                                         fileObject);

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
NTSTATUS
CrashDump_DataSourceWriteFromRequest(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    Write data from a Data Source Request to the corresponding Ring Buffer.

Arguments:

    DmfModule - This Module's handle.
    Request - The request with the data to write to the Data Source.

Return Value:

    NTSTATUS

--*/
{
    // Write data an entry to the Data Source's Ring Buffer.
    //
    NTSTATUS ntStatus;
    WDFFILEOBJECT fileObject;
    WDFMEMORY memory;
    size_t bufferSize;
    void* inputBuffer;

    PAGED_CODE();

    *BytesReturned = 0;

    // Retrieve the data that is to be written to the Data Source.
    //
    ntStatus = WdfRequestRetrieveInputMemory(Request,
                                             &memory);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    bufferSize = 0;
    inputBuffer = WdfMemoryGetBuffer(memory,
                                     &bufferSize);
    if (NULL == inputBuffer)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // Get the unique identifier for this request.
    //
    fileObject = WdfRequestGetFileObject(Request);
    if (NULL == fileObject)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // Allow caller to validate the bytes written.
    //
    *BytesReturned = bufferSize;

    // Write the data from the Data Source to its respective Ring Buffer. If it does not fit, an error 
    // is returned from the Ring Buffer.
    //
    ntStatus = CrashDump_DataSourceWriteAuxiliary(DmfModule,
                                                  fileObject,
                                                  (UCHAR*)inputBuffer,
                                                  (ULONG)bufferSize);

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
NTSTATUS
CrashDump_DataSourceReadFromRequest(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    Read data from a Data Source and return it in a request.

Arguments:

    DmfModule - This Module's handle.
    Request - The request where the read data is returned.

Return Value:

    NTSTATUS

--*/
{
    // Write Data Source's Ring Buffer to buffer.
    //
    NTSTATUS ntStatus;
    WDFFILEOBJECT fileObject;
    WDFMEMORY memory;
    size_t bufferSize;
    void* outputBuffer;

    PAGED_CODE();

    *BytesReturned = 0;

    // Retrieve the output buffer that is to be written to.
    //
    ntStatus = WdfRequestRetrieveOutputMemory(Request,
                                              &memory);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    bufferSize = 0;
    outputBuffer = WdfMemoryGetBuffer(memory,
                                      &bufferSize);
    if (NULL == outputBuffer)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // Get the unique identifier for this request.
    //
    fileObject = WdfRequestGetFileObject(Request);
    if (NULL == fileObject)
    {
        ntStatus = STATUS_INVALID_ADDRESS;
        goto Exit;
    }

    // Read the data from the Ring Buffer to the outputBuffer. If it does not fit, an error  
    // is returned from the Ring Buffer.
    //
    ntStatus = CrashDump_DataSourceReadAuxiliary(DmfModule,
                                                 fileObject,
                                                 (UCHAR*)outputBuffer,
                                                 (ULONG)bufferSize);

// Allow caller to validate the bytes read.
//
    *BytesReturned = bufferSize;

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
NTSTATUS
CrashDump_DataSourceCaptureFromRequest(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    Read all the data from a Data Source and return it in a request.

Arguments:

    DmfModule - This Module's handle.
    Request - The request where the read data is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFFILEOBJECT fileObject;
    WDFMEMORY memory;
    size_t bufferSize;
    ULONG bytesWritten;
    void* outputBuffer;

    PAGED_CODE();

    bytesWritten = 0;
    bufferSize = 0;
    *BytesReturned = 0;

    // Retrieve the output buffer that is to be written to.
    //
    ntStatus = WdfRequestRetrieveOutputMemory(Request,
                                              &memory);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    outputBuffer = WdfMemoryGetBuffer(memory,
                                      &bufferSize);
    if (NULL == outputBuffer)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // Get the unique identifier for this request.
    //
    fileObject = WdfRequestGetFileObject(Request);
    if (NULL == fileObject)
    {
        ntStatus = STATUS_INVALID_ADDRESS;
        goto Exit;
    }

    // Capture the  Ring Buffer data to the outputBuffer. If it does not fit, an error  
    // is returned from the Ring Buffer.
    //
    ntStatus = CrashDump_DataSourceCaptureAuxiliary(DmfModule,
                                                    fileObject,
                                                    (UCHAR*)outputBuffer,
                                                    (ULONG)bufferSize,
                                                    &bytesWritten);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Tell caller total bytes written to buffer. Caller will know number of records because 
    // caller knows size of each entry.
    //
    *BytesReturned = bytesWritten;

Exit:

    return ntStatus;
}
#pragma code_seg()

#endif // !defined(DMF_USER_MODE)

#if !defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
CrashDump_IoctlHandler(
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

   This routine reads static data of the GPU.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    Queue - The WDFQUEUE associated with Request.
    Request - Request data, not used.
    IoctlCode - IOCTL to be used in the command.
    InputBuffer - Input data buffer.
    InputBufferSize - Input data buffer size, not used.
    OutputBuffer - Output data buffer.
    OutputBufferSize - Output data buffer size, not used.
    BytesReturned - Amount of data to be sent back.

Return Value:

    STATUS_SUCCESS always

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;
    DMFMODULE dmfModule;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(BytesReturned);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // This Module is the parent of the Child Module that is passed in.
    // (Module callbacks always receive the Child Module's handle.)
    //
    dmfModule = DMF_ParentModuleGet(DmfModule);
    DmfAssert(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "Request=0x%p OutputBufferLength=%d InputBufferLength=%d IoControlCode=%d",
                Request, (int)OutputBufferSize, (int)InputBufferSize, IoctlCode);

    switch (IoctlCode)
    {
        case IOCTL_DATA_SOURCE_CREATE:
        {
            // It is a request to create a new Ring Buffer for a Data Source.
            //
            ntStatus = CrashDump_DataSourceCreateFromRequest(dmfModule,
                                                             Request,
                                                             BytesReturned);
            break;
        }
        case IOCTL_DATA_SOURCE_DESTROY:
        {
            // It is a request to destroy a Ring Buffer for a Data Source.
            // Generally, speaking, an application will *not* call this IOCTL because the purpose
            // of creating a Data Source is to keep it resident in case of a crash. This IOCTL is 
            // only called in cases when the application determines it is no longer necessary to 
            // maintain that data.
            //

            // TODO: Make sure this is not called for READ MODE.
            //

            ntStatus = CrashDump_DataSourceDestroyFromRequest(dmfModule,
                                                              Request,
                                                              BytesReturned);
            break;
        }
        case IOCTL_DATA_SOURCE_WRITE:
        {
            // It is a request to write data to a Ring Buffer for a Data Source. This IOCTL allows the
            // application to write data that is written to the crash dump in case of BSOD.
            //
            ntStatus = CrashDump_DataSourceWriteFromRequest(dmfModule,
                                                            Request,
                                                            BytesReturned);
            break;
        }
        case IOCTL_DATA_SOURCE_READ:
        {
            // It is a request to read the data from a Ring Buffer for a Data Source. This IOCTL allows the
            // application to read data that is written to the crash dump in case of BSOD.
            //
            ntStatus = CrashDump_DataSourceReadFromRequest(dmfModule,
                                                           Request,
                                                           BytesReturned);
            break;
        }
        case IOCTL_DATA_SOURCE_OPEN:
        {
            // It is a request to open an existing Data Source.
            //
            ntStatus = CrashDump_DataSourceOpenFromRequest(dmfModule,
                                                           Request,
                                                           BytesReturned);
            break;
        }
        case IOCTL_DATA_SOURCE_CAPTURE:
        {
            // It is a request to capture all the data from a Ring Buffer for a Data Source. This IOCTL allows the
            // application to capture the data that is written to the crash dump in case of BSOD.
            //
            ntStatus = CrashDump_DataSourceCaptureFromRequest(dmfModule,
                                                              Request,
                                                              BytesReturned);
            break;
        }
#if defined(DEBUG)
        case IOCTL_CRASH_DRIVER:
        {
            ntStatus = STATUS_SUCCESS;

            // Intentionally crash the driver.
            //

            // 'Consider using another function instead.'
            //
            #pragma warning(suppress: 28159)
            KeBugCheckEx(BUG_CHECK_PRIVATE,
                         (ULONG_PTR)dmfModule,
                         (ULONG_PTR)NULL,
                         (ULONG_PTR)NULL,
                         (ULONG_PTR)NULL);

            // It will never get here!
            //
            break;
        }
#endif // defined(DEBUG)
        default:
        {
            DmfAssert(FALSE);
            ntStatus = STATUS_NOT_SUPPORTED;
            break;
        }
    }

    FuncExit(DMF_TRACE, "%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#endif // !defined(DMF_USER_MODE)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#if !defined(DMF_USER_MODE)

/*++

Routine Description:

    Called upon closing of the User-mode File Handle.

Arguments:

    DmfModule - This Module's handle.
    FileObject - The file object previously submitted to this driver.

Return Value:

    None

++*/
#pragma code_seg("PAGE")
_Function_class_(DMF_ModuleFileClose)
BOOLEAN
DMF_CrashDump_FileClose(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
{
    DMF_CONTEXT_CrashDump* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // If the Data Source has been destroyed by the application, this function does nothing.
    // If the Data Source has not been destroyed by the application (the most likely case),
    // this function causes that Data Source "slot" to remain resident for the duration
    // of the life of the driver where the memory has been allocated. This is by design so
    // that when a crash dump happens that data will be written to the crash dump file.
    //
    if (! moduleContext->SurpriseRemoved)
    {
        CrashDump_DataSourceOrphanCreate(DmfModule,
                                         FileObject);
    }

    return TRUE;
}
#pragma code_seg()

_Function_class_(DMF_ModuleSurpriseRemoval)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_CrashDump_SurpriseRemoval(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Template callback for ModuleInSurpriseRemoval for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_CrashDump* moduleContext;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleContext->SurpriseRemoved = TRUE;

    FuncExitVoid(DMF_TRACE);
}

#endif // !defined(DMF_USER_MODE)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_ModuleInstanceDestroy)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_CrashDump_Destroy(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Destroy an instance of this Module.
    This callback is used to clear the global pointer.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

#if !defined(DMF_USER_MODE)
    // Clear global instance handle.
    //
    g_DmfModuleCrashDump = NULL;
#endif // !defined(DMF_USER_MODE)
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_CrashDump_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type CrashDump.

    NOTE: This function is called during initialization. It initializes the Client Driver's Ring Buffer.
          Since that Ring Buffer will only be used after this function executes, it is not necessary
          to acquire the lock. The lock is only used for accessing the User-mode Crash Dump Data
          Source data structures since they are created dynamically.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;
    DMF_CONFIG_CrashDump* moduleConfig;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

#if !defined(DMF_USER_MODE)
    // Runtime check in case the value ever comes from the registry.
    //
    if (moduleConfig->SecondaryData.DataSourceCount > CrashDump_MAXIMUM_NUMBER_OF_DATA_SOURCES)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "moduleConfig->SecondaryData.DataSourceCount=%d > %d", moduleConfig->SecondaryData.DataSourceCount, CrashDump_MAXIMUM_NUMBER_OF_DATA_SOURCES);
        DmfAssert(FALSE);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // The Client Driver must request both crash dump callbacks, or no crash dump callback.
    //
    if ((moduleConfig->SecondaryData.EvtCrashDumpWrite != NULL) &&
        (NULL == moduleConfig->SecondaryData.EvtCrashDumpQuery))
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid Callback ntStatus=%!STATUS!", ntStatus);
        DmfAssert(FALSE);
        goto Exit;
    }

    // If the Client Driver does not request a Ring Buffer, nor an Additional Bug Check Callback,
    // nor Triage Dump Data,  and not User-mode Ring Buffers, there is no reason to load this Module.
    //
    if ((NULL == moduleConfig->SecondaryData.EvtCrashDumpWrite) &&
        (0 == moduleConfig->SecondaryData.DataSourceCount) &&
        ((0 == moduleConfig->SecondaryData.BufferCount) || (0 == moduleConfig->SecondaryData.BufferSize)) &&
        (0 == moduleConfig->TriageDumpData.TriageDumpDataArraySize))
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid Callback ntStatus=%!STATUS!", ntStatus);
        DmfAssert(FALSE);
        goto Exit;
    }

    // The Client Driver must request both crash dump callbacks, or no crash dump callback.
    // In most cases, one Data Source is specified.
    //
    if (moduleConfig->SecondaryData.EvtCrashDumpQuery != moduleConfig->SecondaryData.EvtCrashDumpWrite)
    {
        if ((NULL == moduleConfig->SecondaryData.EvtCrashDumpQuery) ||
            (NULL == moduleConfig->SecondaryData.EvtCrashDumpWrite))
        {
            ntStatus = STATUS_INVALID_PARAMETER;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Either both or no Callbacks must be specified ntStatus=%!STATUS!", ntStatus);
            DmfAssert(FALSE);
            goto Exit;
        }
    }
    else
    {
        if (NULL != moduleConfig->SecondaryData.EvtCrashDumpQuery)
        {
            // NOTE: Be careful. If there are two separate functions, but both functions happen to
            //       execute the same code, the optimizer will combine them into the same function
            //       in Release Build, but not Debug Build.
            //
            TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Both Callbacks point to same function.");
        }
    }

    // Store the number of allocated Data Sources. Add one to make space for the Client Driver's Ring Buffer. This
    // is the most commonly used Ring Buffer.
    //
    moduleContext->DataSourceCount = moduleConfig->SecondaryData.DataSourceCount + 1;

    // Allocate space for the Data Sources.
    //
    #pragma warning( suppress : 4996 )
    moduleContext->DataSource = (DATA_SOURCE *)ExAllocatePoolWithTag(NonPagedPoolNx,
                                                                     sizeof(DATA_SOURCE) * moduleContext->DataSourceCount,
                                                                     MemoryTag);
    if (NULL == moduleContext->DataSource)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DataSource ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    RtlZeroMemory(moduleContext->DataSource,
                  sizeof(DATA_SOURCE) * moduleContext->DataSourceCount);

    // Allocate space for the Bug Check Callback Records.
    //
    #pragma warning( suppress : 4996 )
    moduleContext->BugCheckCallbackRecordRingBuffer = (KBUGCHECK_REASON_CALLBACK_RECORD *)ExAllocatePoolWithTag(NonPagedPoolNx,
                                                                                                          sizeof(KBUGCHECK_REASON_CALLBACK_RECORD) * moduleContext->DataSourceCount,
                                                                                                          MemoryTag);
    if (NULL == moduleContext->BugCheckCallbackRecordRingBuffer)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "BugCheckCallbackRecordRingBuffer ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    RtlZeroMemory(moduleContext->BugCheckCallbackRecordRingBuffer,
                  sizeof(KBUGCHECK_REASON_CALLBACK_RECORD) * moduleContext->DataSourceCount);

    if (moduleConfig->SecondaryData.BufferCount > 0)
    {
        DmfAssert(moduleConfig->SecondaryData.BufferSize > 0);

        // Ring Buffer Index 0 is reserved for this driver. Allocate it now.
        //
        ntStatus = CrashDump_DataSourceCreateSelf(DmfModule,
                                                  moduleConfig->SecondaryData.BufferCount,
                                                  moduleConfig->SecondaryData.BufferSize,
                                                  &moduleConfig->SecondaryData.RingBufferDataGuid);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CrashDump_DataSourceCreateSelf ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }
    else
    {
        // The client has specified that a Ring Buffer callback is not needed.
        //
        DmfAssert(0 == moduleConfig->SecondaryData.BufferSize);
    }

    // If the Client Driver has specified an Additional Bug Check Callback in addition to or instead of the Ring Buffer
    // create that callback.
    //
    if (moduleConfig->SecondaryData.EvtCrashDumpWrite != NULL)
    {
        DmfAssert(moduleConfig->SecondaryData.EvtCrashDumpQuery != NULL);

        // Register the callback function that is called for the driver that instantiates this object.
        //
        KeInitializeCallbackRecord(&moduleContext->BugCheckCallbackRecordAdditional);
        if (! KeRegisterBugCheckReasonCallback(&moduleContext->BugCheckCallbackRecordAdditional,
                                               CrashDump_BugCheckSecondaryDumpDataCallbackAdditional,
                                               KbCallbackSecondaryDumpData,
                                               moduleConfig->ComponentName))
        {
            // It can fail due to resource allocation failure.
            //
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "KeRegisterBugCheckReasonCallback");
            ntStatus = STATUS_INVALID_PARAMETER;
            goto Exit;
        }
    }
    else
    {
        // Client Driver has specified it does not need a an Additional Bug Check Callback.
        //
    }

    // If the Client Driver has specified a triage dump data array and optional
    // Bug Check callback, allocate the array and register the callback.
    //
    if (moduleConfig->TriageDumpData.TriageDumpDataArraySize > 0)
    {
        // The OS will not add the triage dump data callback data to the dump 
        // without a valid component name, so check it right here.
        if (moduleConfig->ComponentName == NULL)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CrashDump Config Missing Component Name");
            DmfAssert(moduleConfig->ComponentName != NULL);
            ntStatus = STATUS_INVALID_PARAMETER;
            goto Exit;
        }

        // NOTE: No lock is needed as Open is called by DMF synchronously.
        //
        ntStatus = CrashDump_TriageDataCreateInternal(DmfModule);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CrashDump_TriageDataCreateInternal ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }
    else
    {
        // Client Driver has specified it does not need a triage dump data array. There should not
        // be a callback registered in this case.
        //
        DmfAssert(moduleConfig->TriageDumpData.EvtCrashDumpStoreTriageDumpData == NULL);
    }

    ntStatus = STATUS_SUCCESS;
#else
    // In User-mode this Module just uses the User-mode class which talks to SystemTelemetry.sys.
    // In order to enforce that the constructors and destructors are called, it is mandatory to allocate
    // this object dynamically.
    // TODO: Verify that exception is not thrown if fails.
    //
    moduleContext->m_SystemTelemetryDevice = new CSystemTelemetryDevice;
    if (NULL == moduleContext->m_SystemTelemetryDevice)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    // Now, just initialize the data source for write using the parameters from Module Config.
    //
    bool returnValue;
    returnValue = moduleContext->m_SystemTelemetryDevice->InitializeForWrite(moduleConfig->SecondaryData.BufferCount,
                                                                             moduleConfig->SecondaryData.BufferSize,
                                                                             moduleConfig->SecondaryData.RingBufferDataGuid,
                                                                             RetentionPolicyDoNotRetain);
    if (! returnValue)
    {
        // Ignore error.
        //
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;
#endif // !defined(DMF_USER_MODE)

Exit:

    // BUGBUG - Allocations must be cleaned up if Open fails, as Close will not be called.
    // TODO free all the buffers
    //

    return ntStatus;
}
#pragma code_seg()

#if !defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
CrashDump_DataSourceDestroySelf(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Destroy the Client Driver's Ring Buffer. Since the caller is trusted (because it is called by the Client Driver), it is not necessary
    to lock this object. However, since the function to destroy the Ring Buffer's assumes the caller has locked the object since it can
    be used by untrusted components. Therefore, acquire the lock.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    DMF_ModuleLock(DmfModule);
    ntStatus = CrashDump_DataSourceDestroyInternal(DmfModule,
                                                   RINGBUFFER_INDEX_SELF);
    DmfAssert(NT_SUCCESS(ntStatus));
    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}
#pragma code_seg()

#endif // !defined(DMF_USER_MODE)

#if !defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_CrashDump_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type CrashDump.
    Kernel-mode Version

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_CrashDump* moduleContext;
    DMF_CONFIG_CrashDump* moduleConfig;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Unregister the Additional Bug Check Callback, if the Client Driver registered one.
    //
    if (moduleConfig->SecondaryData.EvtCrashDumpWrite != NULL)
    {
        // The Client Driver registered an Additional Bug Check Callback.
        //
        DmfAssert(moduleConfig->SecondaryData.EvtCrashDumpQuery != NULL);
        if (! KeDeregisterBugCheckReasonCallback(&moduleContext->BugCheckCallbackRecordAdditional))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "KeDeregisterBugCheckReasonCallback");
            // This can happen with resource failure injection, so do not assert.
            //
        }
    }
    else
    {
        // The Client Driver did not register an Additional Bug Check Callback.
        //
    }

    // Shutdown the Client Driver Bug Check Ring Buffer, if the Client Driver registered it.
    //
    if (moduleContext->DataSource != NULL)
    {
        DmfAssert(RINGBUFFER_INDEX_SELF < moduleContext->DataSourceCount);
        if (moduleContext->DataSource[RINGBUFFER_INDEX_SELF].DmfModuleDataSourceRingBuffer != NULL)
        {
            DmfAssert(moduleConfig->SecondaryData.BufferCount > 0);
            DmfAssert(moduleConfig->SecondaryData.BufferSize > 0);

            // Close the SELF Ring Buffer.
            //
            CrashDump_DataSourceDestroySelf(DmfModule);
        }
        else
        {
            // The client has specified that a Ring Buffer callback is not needed, or an error occurred during
            // initialization and the Ring Buffer callback was not created.
            //
        }
    }
    else
    {
        // It is possible that memory allocation of the array of pointers failed due to low memory.
        //
    }

    // User-mode access should be shut down by now, but in the interest of being super safe, acquire the lock in case
    // the Client Driver has incorrectly left User-mode access open.
    //
    // NOTE: It is not necessary to lock above because the above code only uses the RINGBUFFER_INDEX_SELF structures
    //       which are no longer in use when this function is called.
    //
    DMF_ModuleLock(DmfModule);

    // If the Crash Dump Module is destroyed, it is necessary to close any dependent modules that remain open.
    // This can happen because an application can create a Data Source, and leave it created even after it terminates.
    // For this reason, it is possible and legitimate, for Data Sources to be open during shutdown of this object.
    // Thus, before shutting down, close any open Data Sources.
    //
    // This is RARE broken symmetry, but is due to the asymmetric nature of the function of the Crash Dump Module.
    // 
    if (moduleContext->DataSource != NULL)
    {
        for (ULONG dataSourceIndex = RINGBUFFER_INDEX_CLIENT_FIRST; dataSourceIndex < moduleContext->DataSourceCount; dataSourceIndex++)
        {
            DATA_SOURCE* dataSource;
            NTSTATUS ntStatus;

            dataSource = &moduleContext->DataSource[dataSourceIndex];

            // Just need to call CrashDump_DataSourceDestroyAuxiliaryInternal with one fileObject. 
            // If second fileObject is present, it will be taken care of as part of destroy.
            //
            // NOTE: Update if more modes are added.
            //
            if (dataSource->FileObject[DataSourceModeWrite] != NULL)
            {
                ntStatus = CrashDump_DataSourceDestroyAuxiliaryInternal(DmfModule,
                                                                        dataSource->FileObject[DataSourceModeWrite]);
                DmfAssert(NT_SUCCESS(ntStatus));
            }

            if (dataSource->FileObject[DataSourceModeRead] != NULL)
            {
                ntStatus = CrashDump_DataSourceDestroyAuxiliaryInternal(DmfModule,
                                                                        dataSource->FileObject[DataSourceModeRead]);
                DmfAssert(NT_SUCCESS(ntStatus));
            }
        }
    }
    else
    {
        // It is possible that memory allocation of the array of pointers failed due to low memory.
        //
    }

    // Deregister the Triage Dump Data Callback and free its resources.
    //
    CrashDump_TriageDataDestroyInternal(DmfModule);

    DMF_ModuleUnlock(DmfModule);

    // All Data Sources are shut down. Free associated data now.
    //
    if (moduleContext->BugCheckCallbackRecordRingBuffer != NULL)
    {
        DmfAssert(moduleContext->DataSourceCount > 0);
        ExFreePoolWithTag(moduleContext->BugCheckCallbackRecordRingBuffer,
                          MemoryTag);
        moduleContext->BugCheckCallbackRecordRingBuffer = NULL;
    }
    else
    {
        // Allocation could have failed due to low memory.
        //
    }

    if (moduleContext->DataSource != NULL)
    {
        DmfAssert(moduleContext->DataSourceCount > 0);
        ExFreePoolWithTag(moduleContext->DataSource,
                          MemoryTag);
        moduleContext->DataSource = NULL;
    }
    else
    {
        // Allocation could have failed due to low memory.
        //
    }

    moduleContext->DataSourceCount = 0;
}
#pragma code_seg()

#else

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_CrashDump_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type CrashDump.
    User-mode Version

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_CrashDump* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Enforce that the destructors get called by deallocating.
    //
    if (moduleContext->m_SystemTelemetryDevice != NULL)
    {
        delete moduleContext->m_SystemTelemetryDevice;
        moduleContext->m_SystemTelemetryDevice = NULL;
    }
}
#pragma code_seg()

#endif // !defined(DMF_USER_MODE)

#if !defined(DMF_USER_MODE)

// NOTE: To reuse previous legacy code, let that code validate input/output buffer sizes.
//
IoctlHandler_IoctlRecord CrashDump_IoctlSpecification[] =
{
    { IOCTL_DATA_SOURCE_CREATE, 0, 0, CrashDump_IoctlHandler, FALSE },
    { IOCTL_DATA_SOURCE_DESTROY, 0, 0, CrashDump_IoctlHandler, FALSE },
    { IOCTL_DATA_SOURCE_WRITE, 0, 0, CrashDump_IoctlHandler, FALSE },
    { IOCTL_DATA_SOURCE_READ, 0, 0, CrashDump_IoctlHandler, FALSE },
    { IOCTL_DATA_SOURCE_OPEN, 0, 0, CrashDump_IoctlHandler, FALSE },
    { IOCTL_DATA_SOURCE_CAPTURE, 0, 0, CrashDump_IoctlHandler, FALSE },
#if defined(DEBUG)
    { IOCTL_CRASH_DRIVER, 0, 0, CrashDump_IoctlHandler, FALSE },
#endif // defined(DEBUG)
};

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_CrashDump_ChildModulesAdd(
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
    DMF_CONFIG_CrashDump* moduleConfig;
    DMF_CONFIG_IoctlHandler ioctlHandlerModuleConfig;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (moduleConfig->SecondaryData.DataSourceCount > 0)
    {
        // IoctlHandler
        // ------------
        //
        DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(&ioctlHandlerModuleConfig,
                                                    &moduleAttributes);
        ioctlHandlerModuleConfig.DeviceInterfaceGuid = GUID_DEVINTERFACE_CrashDump;
        ioctlHandlerModuleConfig.AccessModeFilter = IoctlHandler_AccessModeFilterAdministratorOnly;
        ioctlHandlerModuleConfig.EvtIoctlHandlerAccessModeFilter = NULL;
        ioctlHandlerModuleConfig.IoctlRecordCount = ARRAYSIZE(CrashDump_IoctlSpecification);
        ioctlHandlerModuleConfig.IoctlRecords = CrashDump_IoctlSpecification;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         NULL);
    }
    else
    {
        // There should only be a single driver that hosts Auxiliary Data Sources.
        // Thus, only a single driver should set DataSourceCount to non-zero.
        //
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_CrashDump_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type CrashDump.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_CrashDump;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_CrashDump;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_CrashDump;
    DMFMODULE dmfModule;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    dmfModule = NULL;

#if !defined(DMF_USER_MODE)
    if (g_DmfModuleCrashDump != NULL)
    {
        // Only one instance of this Module can exist at a time. This handle is used to pass context into the crash
        // dump callbacks called by the OS.
        //
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Only one instance of this Module can exist at time");
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }
#endif // !defined(DMF_USER_MODE)

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_CrashDump);
    dmfCallbacksDmf_CrashDump.ModuleInstanceDestroy = DMF_CrashDump_Destroy;
    dmfCallbacksDmf_CrashDump.DeviceOpen = DMF_CrashDump_Open;
    dmfCallbacksDmf_CrashDump.DeviceClose = DMF_CrashDump_Close;
#if !defined(DMF_USER_MODE)
    dmfCallbacksDmf_CrashDump.ChildModulesAdd = DMF_CrashDump_ChildModulesAdd;
#endif // !defined(DMF_USER_MODE)

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_CrashDump);
#if !defined(DMF_USER_MODE)
    dmfCallbacksWdf_CrashDump.ModuleSurpriseRemoval = DMF_CrashDump_SurpriseRemoval;
    dmfCallbacksWdf_CrashDump.ModuleFileClose = DMF_CrashDump_FileClose;
#endif // !defined(DMF_USER_MODE)

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_CrashDump,
                                            CrashDump,
                                            DMF_CONTEXT_CrashDump,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_CrashDump.CallbacksDmf = &dmfCallbacksDmf_CrashDump;
    dmfModuleDescriptor_CrashDump.CallbacksWdf = &dmfCallbacksWdf_CrashDump;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_CrashDump,
                                &dmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

#if !defined(DMF_USER_MODE)

    // Save global context. The Crash Dump callbacks do not have a context passed into them.
    //
    g_DmfModuleCrashDump = dmfModule;

#endif // !defined(DMF_USER_MODE)

Exit:

    *DmfModule = dmfModule;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_CrashDump_DataSourceWriteSelf(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* Buffer,
    _In_ ULONG BufferLength
    )
/*++

Routine Description:

    Write data to the Client Driver's Data Source.

Arguments:

    DmfModule - This Module's handle.
    Buffer - The data to write.
    BufferLength - The number of bytes to write.

Return Value:

    NTSTATUS

--*/
{
    // Write data an entry to the Data Source's Ring Buffer.
    //
    NTSTATUS ntStatus;

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 CrashDump);

#if !defined(DMF_USER_MODE)
    // Write the data from the Data Source to its respective Ring Buffer. If it does not fit, an error 
    // is returned from the Ring Buffer.
    //
    // NOTE: This call is trusted because it is only made from Client Driver (not User-mode). It is not necessary, nor is it possible,
    //       to acquire the lock the PASSIVE_LEVEL lock for this object here.
    //
    ntStatus = CrashDump_DataSourceWriteInternal(DmfModule,
                                                 RINGBUFFER_INDEX_SELF,
                                                 Buffer,
                                                 BufferLength);
#else
    // In User-mode just use the helper class.
    // This will route the data to SystemTelemetry.sys.
    //
    DMF_CONTEXT_CrashDump* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    DmfAssert(moduleContext->m_SystemTelemetryDevice != NULL);
    moduleContext->m_SystemTelemetryDevice->DataSourceWrite(Buffer,
                                                            BufferLength);
    ntStatus = STATUS_SUCCESS;
#endif // !defined(DMF_USER_MODE)

    return ntStatus;
}

#if !defined(DMF_USER_MODE)

#if IS_WIN10_19H1_OR_LATER

_IRQL_requires_same_
NTSTATUS
DMF_CrashDump_TriageDumpDataAdd(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* Data,
    _In_ ULONG DataLength
    )
/*++

Routine Description:

    Add a Client Driver buffer to the Triage Dump Buffer list. This does not copy
    the memory but adds the address of the buffer and length to the triage data array
    so it will be marked for inclusion in a kernel minidump.  This could be called at
    any IRQL, depending on if it was called during the BugCheck callback or earlier.

Arguments:

    DmfModule - This Module's handle.
    Data - The data to mark for inclusion in triage dump.
    DataLength - The size of the data.

Return Value:

    NTSTATUS

--*/
{

    NTSTATUS ntStatus;
    DMF_CONTEXT_CrashDump* moduleContext;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    DmfAssert(moduleContext != NULL);

    if (moduleContext->TriageDumpDataArray == NULL)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
    }
    else
    {
        // Add the block to the list. The validity of the buffer does not need to be
        // checked at this time, it will not cause a fault later if it is invalid.
        //
        ntStatus = KeAddTriageDumpDataBlock(moduleContext->TriageDumpDataArray,
                                            Data,
                                            DataLength);
    }

    FuncExit(DMF_TRACE, "Buffer = 0x%p, Length = %d, ntStatus=%!STATUS!", Data, DataLength, ntStatus);

    return ntStatus;
}

#endif // IS_WIN10_19H1_OR_LATER

#endif // !defined(DMF_USER_MODE)

// eof: Dmf_CrashDump.c
//
