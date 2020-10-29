/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_HidTarget.c

Abstract:

    Supports requests to a device connected via HID.
    NOTE: Must add HidParse.lib to link dependencies when using this Module.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_HidTarget.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_HidTarget
{
    // HID Interface arrival/removal notification handle.
    // 
#if defined(DMF_USER_MODE)
    HCMNOTIFICATION HidInterfaceNotification;
#else
    VOID* HidInterfaceNotification;
#endif // defined(DMF_USER_MODE)
    // Underlying HID Device Target.
    //
    WDFIOTARGET IoTarget;
    // Path Name of HID Device.
    //
    WDFMEMORY SymbolicLinkNameMemory;
    // Input report call back.
    //
    EVT_DMF_HidTarget_InputReport* EvtHidInputReport;
    // Copy of the Symbolic Name of HID Device.
    //
    WDFMEMORY SymbolicLinkToSearchMemory;
    // Cached PreparsedData and Hid Caps.
    // These remains constant for a specific hid device.
    WDFMEMORY PreparsedDataMemory;
    HIDP_CAPS HidCaps;
    // Child ContinuousRequestTarget DMF Module.
    //
    DMFMODULE DmfModuleContinuousRequestTarget;
    // BufferPool for input report read requests sent using
    // Dmf_HidTarget_InputRead().
    //
    DMFMODULE DmfModuleBufferPoolInputReport;
    // ThreadedBufferQueue for processing returned input report read requests
    // sent using Dmf_HidTarget_InputReadEx().
    //
    DMFMODULE DmfModuleThreadedBufferQueueInputReport;
} DMF_CONTEXT_HidTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(HidTarget)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(HidTarget)

// Memory Pool Tag.
//
#define MemoryTag 'MdiH'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// For HidD* API.
//
#include <hidsdi.h>

// Used to determine number of buffers pre-allocated for input report read requests sent using
// Dmf_HidTarget_InputRead(). PendedInputReadRequestCount in the module config is not
// used because the client controls the number of requests to pend when using Dmf_HidTarget_InputRead().
//
#define DEFAULT_NUMBER_OF_PENDING_INPUT_READS 4

// {55F3D844-8F9E-4EBD-AE33-EB778524CEEF}
DEFINE_GUID(GUID_CUSTOM_DEVINTERFACE, 0x55f3d844, 0x8f9e, 0x4ebd, 0xae, 0x33, 0xeb, 0x77, 0x85, 0x24, 0xce, 0xef);

_Function_class_(EVT_DMF_ContinuousRequestTarget_BufferOutput)
ContinuousRequestTarget_BufferDisposition
HidTarget_InputReadExCompletionCallback(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Inout_ VOID* ClientBufferContextOutput,
    _In_ NTSTATUS CompletionStatus
    )
/*++

Routine Description:

    Callback function called when the input report read requests sent using Dmf_HidTarget_InputReadEx() completes.
    It provides the output buffer and the return status of the request. Here we get the InputReport
    from the output buffer.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    OutputBuffer - Pointer to output data.
    OutputBufferSize - Size of output data.
    ClientBufferContextOutput - Context associated with the given output buffer.
    CompletionStatus - Return status of Request.

Return Value:

    ContinuousRequestTarget_BufferDisposition - Indicates who owns the output buffer after return.

--*/
{
    ContinuousRequestTarget_BufferDisposition returnValue;
    DMF_CONTEXT_HidTarget* moduleContext;
    PVOID clientBufferInputReport;
    NTSTATUS ntStatus;
    DMFMODULE dmfModuleHidTarget;

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(ClientBufferContextOutput);

    dmfModuleHidTarget = DMF_ParentModuleGet(DmfModule);

    moduleContext = DMF_CONTEXT_GET(dmfModuleHidTarget);

    ntStatus = DMF_ModuleReference(dmfModuleHidTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        returnValue = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndStopStreaming;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus= %!STATUS!", ntStatus);
        goto ExitNoRelease;
    }

    if (CompletionStatus == STATUS_CANCELLED || CompletionStatus == STATUS_DEVICE_NOT_CONNECTED)
    {
        // Stop streaming if the requests are cancelled or the device is disconnected.
        // This means that the streaming has either been stopped by the client or the iotarget is disconnected
        // respectively.
        //
        returnValue = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndStopStreaming;
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Input report read requests will no longer be pended: CompletionStatus=%!STATUS!", CompletionStatus);
        goto Exit;
    }
    else if (! NT_SUCCESS(CompletionStatus))
    {
        // Other failure conditions do not necessitate that streaming needs to be stopped.
        //
        returnValue = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Input report read request fails: CompletionStatus=%!STATUS!", CompletionStatus);
        goto Exit;
    }

    returnValue = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming;

    ntStatus = DMF_ThreadedBufferQueue_Fetch(moduleContext->DmfModuleThreadedBufferQueueInputReport,
                                             &clientBufferInputReport,
                                             NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ThreadedBufferQueue_Fetch fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Copy input report from callback buffer.
    //
    RtlCopyMemory(clientBufferInputReport,
                  OutputBuffer,
                  OutputBufferSize);

    // Write input report to consumer buffer.
    //
    DMF_ThreadedBufferQueue_Enqueue(moduleContext->DmfModuleThreadedBufferQueueInputReport,
                                    clientBufferInputReport);
Exit:

    DMF_ModuleDereference(dmfModuleHidTarget);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ContinuousRequestTarget_BufferDisposition=%d", returnValue);

    return returnValue;
}

_Function_class_(EVT_DMF_ContinuousRequestTarget_SendCompletion)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
HidTarget_InputReadCompletionCallback(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientRequestContext,
    _In_reads_(InputBufferBytesWritten) VOID* InputBuffer,
    _In_ size_t InputBufferBytesWritten,
    _In_reads_(OutputBufferBytesRead) VOID* OutputBuffer,
    _In_ size_t OutputBufferBytesRead,
    _In_ NTSTATUS CompletionStatus
    )
/*++

Routine Description:

    Callback function called when the input report read requests sent using Dmf_HidTarget_InputRead() completes.
    It provides the output buffer and the return status of the request. Here we get the InputReport
    from the output buffer.

Arguments:

    DmfModule - Module handle.
    ClientRequestContext - This Module's context which was set as request context during send.
    InputBuffer - Input Buffer sent as part of the request.
    InputBufferBytesWritten - Size of InputBuffer
    OutputBuffer - Output Buffer sent as part of the request.
    OutputBufferBytesRead - Size of OutputBuffer
    CompletionStatus - Request completion status.

Return Value:

    VOID

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;
    DMFMODULE dmfModuleHidTarget;

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(ClientRequestContext);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferBytesWritten);

    dmfModuleHidTarget = DMF_ParentModuleGet(DmfModule);

    moduleContext = DMF_CONTEXT_GET(dmfModuleHidTarget);

    ntStatus = DMF_ModuleReference(dmfModuleHidTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus= %!STATUS!", ntStatus);
        goto ExitNoRelease;
    }

    if (! NT_SUCCESS(CompletionStatus))
    {
        TraceEvents(TRACE_LEVEL_WARNING,
                    DMF_TRACE,
                    "ReadCompletionRoutine fails: ntStatus=%!STATUS!",
                    CompletionStatus);
        goto Exit;
    }

    moduleContext->EvtHidInputReport(dmfModuleHidTarget,
                                     (UCHAR*)OutputBuffer,
                                     (ULONG)OutputBufferBytesRead);

Exit:

    DMF_ModuleDereference(dmfModuleHidTarget);

ExitNoRelease:

    DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolInputReport,
                       OutputBuffer);

    FuncExitVoid(DMF_TRACE);
}

_Function_class_(EVT_DMF_ThreadedBufferQueue_Callback)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
ThreadedBufferQueue_BufferDisposition
HidTarget_InputReportConsumeWork(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* ClientWorkBuffer,
    _In_ ULONG ClientWorkBufferSize,
    _In_ VOID* ClientWorkBufferContext,
    _Out_ NTSTATUS* NtStatus
    )
/*++

Routine Description:

    Callback function for threaded buffer queue when there is an input report to process.
    This is triggered in the request completion callback for input report read requests sent using
    Dmf_HidTarget_InputReadEx().

Arguments:

    DmfModule - DmfModuleThreadedBufferQueueInputReport Module's handle.
    ClientWorkBuffer - Work buffer sent from the Client (Input Report).
    ClientWorkBufferSize - Size of ClientWorkBuffer.
    ClientWorkBufferContext - Work buffer context.
    NtStatus - Status returned.

Return Value:

    ThreadedBufferQueue_BufferDisposition

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;
    DMFMODULE dmfModuleHidTarget;

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ClientWorkBufferContext);
    UNREFERENCED_PARAMETER(NtStatus);

    dmfModuleHidTarget = DMF_ParentModuleGet(DmfModule);

    moduleContext = DMF_CONTEXT_GET(dmfModuleHidTarget);

    ntStatus = DMF_ModuleReference(dmfModuleHidTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleRefrence fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    moduleContext->EvtHidInputReport(dmfModuleHidTarget,
                                     ClientWorkBuffer,
                                     ClientWorkBufferSize);

    DMF_ModuleDereference(dmfModuleHidTarget);

Exit:

    FuncExitVoid(DMF_TRACE);

    return ThreadedBufferQueue_BufferDisposition_WorkComplete;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
HidTarget_IoTargetCreateByName(
    _In_ WDFDEVICE Device,
    _In_ PUNICODE_STRING SymbolicLinkName,
    _In_ ULONG OpenMode,
    _In_ ULONG ShareAccess,
    _Out_ WDFIOTARGET* IoTarget
    )
/*++

Routine Description:

    Helper function that creates a WDFIOTARGET.
    TODO: This function should move into Dmf.

Arguments:

    Device - Client WDFDEVICE.
    SymbolicLinkName - Name of the WDFIOTARGET to open.
    OpenMode - Indicates desired read/write mode.
    ShareAccess- Indicates desired shared access.
    IoTarget - Contains the handle of the opened target.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFIOTARGET resultIoTarget;
    WDF_IO_TARGET_OPEN_PARAMS openParams;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // Ensure the target is only set on success.
    //
    *IoTarget = NULL;

    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams,
                                                SymbolicLinkName,
                                                OpenMode);

    openParams.ShareAccess = ShareAccess;

    // Create an I/O target object.
    //
    ntStatus = WdfIoTargetCreate(Device,
                                 WDF_NO_OBJECT_ATTRIBUTES,
                                 &resultIoTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE,
                    "WdfIoTargetCreate fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    // Try to open the target.
    //
    ntStatus = WdfIoTargetOpen(resultIoTarget, 
                               &openParams);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfIoTargetOpen fails: ntStatus=%!STATUS!",
                    ntStatus);
        WdfObjectDelete(resultIoTarget);
        goto Exit;
    }

    *IoTarget = resultIoTarget;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
HidTarget_DeviceProperyGet(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Helper function to retrieve hid properties- capability and preparsed data.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES attributes;
    DMF_CONTEXT_HidTarget* moduleContext;

    WDF_MEMORY_DESCRIPTOR outputDescriptor;
    HID_COLLECTION_INFORMATION hidCollectionInformation = { 0 };
    WDFMEMORY preparsedDataMemory;
    PHIDP_PREPARSED_DATA preparsedData;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    preparsedDataMemory = WDF_NO_HANDLE;

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor,
                                      (VOID*) &hidCollectionInformation,
                                      sizeof(HID_COLLECTION_INFORMATION));

    // Get the collection information for this device.
    //
    ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                 NULL,
                                                 IOCTL_HID_GET_COLLECTION_INFORMATION,
                                                 NULL,
                                                 &outputDescriptor,
                                                 NULL,
                                                 NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "WdfIoTargetSendIoctlSynchronously fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&attributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               hidCollectionInformation.DescriptorSize,
                               &preparsedDataMemory,
                               (VOID**)&preparsedData);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfMemoryCreate for preparsed data fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor,
                                      (VOID*)preparsedData,
                                      hidCollectionInformation.DescriptorSize);

    ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                 NULL,
                                                 IOCTL_HID_GET_COLLECTION_DESCRIPTOR,
                                                 NULL,
                                                 &outputDescriptor,
                                                 NULL,
                                                 NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfIoTargetSendIoctlSynchronously fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    HIDP_CAPS hidCapsLocal;
    RtlZeroMemory(&hidCapsLocal,
                  sizeof(HIDP_CAPS));

    ntStatus = HidP_GetCaps(preparsedData,
                            &hidCapsLocal);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "HidP_GetCaps() fails: %!STATUS!",
                    ntStatus);
        goto Exit;
    }

    // Copy the properties to Module context.
    //
    RtlCopyMemory(&moduleContext->HidCaps,
                  &hidCapsLocal,
                  sizeof(HIDP_CAPS));

    moduleContext->PreparsedDataMemory = preparsedDataMemory;
    preparsedDataMemory = WDF_NO_HANDLE;

Exit:

    if (preparsedDataMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(preparsedDataMemory);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

#if !defined(DMF_USER_MODE)
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
HidTarget_InterfaceCreateForLocal(
    _In_ DMFMODULE DmfModule,
    _In_ const GUID* InterfaceGuid,
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Helper function that creates a device interface and retrieve the symbolic link.

Arguments:

    DmfModule - This Module's handle.
    InterfaceGuid - InterfaceGuid to be used.
    Device - Client WDFDEVICE.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    UNICODE_STRING deviceReferenceName;
    size_t deviceReferenceNameLength;
    WDFMEMORY memoryHandle;
    WDFSTRING stringHandle;
    WDFMEMORY deviceReferenceNameHandle;
    DMF_CONTEXT_HidTarget* moduleContext;
    DMF_CONFIG_HidTarget* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    memoryHandle = WDF_NO_HANDLE;
    stringHandle = WDF_NO_HANDLE;
    deviceReferenceNameHandle = WDF_NO_HANDLE;
    deviceReferenceName.Buffer = NULL;

    // Create a unique reference string from PDO DeviceObjectName.
    //
    ntStatus = WdfDeviceAllocAndQueryProperty(Device,
                                              DevicePropertyPhysicalDeviceObjectName,
                                              NonPagedPoolNx,
                                              WDF_NO_OBJECT_ATTRIBUTES,
                                              &memoryHandle);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfDeviceAllocAndQueryProperty fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    PWSTR nameBuffer = (PWSTR)WdfMemoryGetBuffer(memoryHandle,
                                                 &deviceReferenceNameLength);
    USHORT sizeToAllocate = (USHORT)deviceReferenceNameLength + sizeof(WCHAR);
    PWCH deviceReferenceNameBuffer;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               sizeToAllocate,
                               &deviceReferenceNameHandle,
                               (VOID**)&deviceReferenceNameBuffer);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfMemoryCreate fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }
    deviceReferenceName.Buffer = deviceReferenceNameBuffer;
    RtlZeroMemory(deviceReferenceName.Buffer,
                  sizeToAllocate);

    deviceReferenceName.Length = (USHORT)deviceReferenceNameLength;
    deviceReferenceName.MaximumLength = sizeToAllocate;
    RtlCopyMemory(deviceReferenceName.Buffer,
                  nameBuffer,
                  deviceReferenceNameLength);

    // Remove '\' and '/' from the reference string as required by WdfDeviceCreateDeviceInterface.
    //
    nameBuffer = deviceReferenceName.Buffer;
    USHORT writeIndex = 0;
    USHORT readIndex = 0;
    while (readIndex < ((sizeToAllocate / sizeof(WCHAR)) - 1))
    {
        if ((nameBuffer[readIndex] != L'\\') && 
            (nameBuffer[readIndex] != L'/'))
        {
            nameBuffer[writeIndex] = nameBuffer[readIndex];
            writeIndex++;
        }
        readIndex++;
    }

    // Update the length of the target string after removing the characters.
    //
    USHORT numberOfRemovedWchars = (readIndex - writeIndex);
    deviceReferenceName.Length -= (USHORT)(numberOfRemovedWchars * sizeof(WCHAR));

    // Use the reference string to differentiate device instances.
    //
    ntStatus = WdfDeviceCreateDeviceInterface(Device,
                                              InterfaceGuid,
                                              &deviceReferenceName);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfDeviceCreateDeviceInterface fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfStringCreate(NULL,
                               &objectAttributes,
                               &stringHandle);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfStringCreate fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    ntStatus = WdfDeviceRetrieveDeviceInterfaceString(Device,
                                                      InterfaceGuid,
                                                      &deviceReferenceName,
                                                      stringHandle);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfDeviceCreateDeviceInterface fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    UNICODE_STRING deviceSymbolicName;
    WdfStringGetUnicodeString(stringHandle,
                              &deviceSymbolicName);

    // Symbolic name unique to the device passed in found; Save it for arrival search.
    //
    sizeToAllocate = deviceSymbolicName.Length;
    WDFMEMORY symbolicLinkToSearchHandle;
    BYTE* symbolicLinkNameToSearchBuffer;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               sizeToAllocate,
                               &symbolicLinkToSearchHandle,
                               (VOID**)&symbolicLinkNameToSearchBuffer);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Could not allocate memory for symbolic link to search");
        goto Exit;
    }

    // NOTE: symbolicLinkNameToSearchBuffer do not have null termination.
    //
    RtlCopyMemory(symbolicLinkNameToSearchBuffer,
                  deviceSymbolicName.Buffer,
                  deviceSymbolicName.Length);

    moduleContext->SymbolicLinkToSearchMemory = symbolicLinkToSearchHandle;

Exit:

    if (memoryHandle != WDF_NO_HANDLE)
    {
        WdfObjectDelete(memoryHandle);
    }

    if (stringHandle != WDF_NO_HANDLE)
    {
        WdfObjectDelete(stringHandle);
    }

    if (deviceReferenceNameHandle != WDF_NO_HANDLE)
    {
        WdfObjectDelete(deviceReferenceNameHandle);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()
#endif

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
HidTarget_IoTargetDestroy(
    _Inout_ DMF_CONTEXT_HidTarget* ModuleContext
    )
/*++

Routine Description:

    Helper function that destroy's this Module's target Hid WDFIOTARGET.
    TODO: This function should move to Dmf.

Arguments:

    ModuleContext - This Module's Module Context (which contains the IoTarget).

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    if (ModuleContext->IoTarget != NULL)
    {
        WdfIoTargetClose(ModuleContext->IoTarget);
        WdfObjectDelete(ModuleContext->IoTarget);
        ModuleContext->IoTarget = NULL;
    }

    if (ModuleContext->SymbolicLinkNameMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(ModuleContext->SymbolicLinkNameMemory);
        ModuleContext->SymbolicLinkNameMemory = WDF_NO_HANDLE;
    }

    if (ModuleContext->SymbolicLinkToSearchMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(ModuleContext->SymbolicLinkToSearchMemory);
        ModuleContext->SymbolicLinkToSearchMemory = WDF_NO_HANDLE;
    }

    if (ModuleContext->PreparsedDataMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(ModuleContext->PreparsedDataMemory);
        ModuleContext->PreparsedDataMemory = WDF_NO_HANDLE;
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
BOOLEAN
HidTarget_IsPidInList(
    _In_ UINT16 LookForPid,
    _In_ UINT16* PidList,
    _In_ ULONG PidListArraySize
    )
/*++

Routine Description:

    Helper function to determine if a given Product Id (Pid) is in a list.

Arguments:

    LookForPid - The given Pid.
    PidList - The given list.
    PidLIstArraySize - Number of entries in the list.

Return Value:

    NTSTATUS

--*/
{
    BOOLEAN isFound;
    ULONG pidIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    isFound = FALSE;

    for (pidIndex = 0; pidIndex < PidListArraySize; pidIndex++)
    {
        if (LookForPid == PidList[pidIndex])
        {
            isFound = TRUE;
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        DMF_TRACE,
                        "found supported PID: 0x%x",
                        LookForPid);
            break;
        }
    }

    FuncExit(DMF_TRACE, "isFound=%d", isFound);

    return isFound;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
HidTarget_MatchCheckForRemote(
    _In_ DMFMODULE DmfModule,
    _In_ PUNICODE_STRING DevicePath,
    _Out_ BOOLEAN* IsDeviceMatched
    )
/*++

Routine Description:

    Checks the Hid attributes to determin the match for device.

Arguments:

    DmfModule - This Module's handle.
    DevicePath - The file name that allows this driver to access the device.
    IsDeviceMatched - The result of the query.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    DMF_CONTEXT_HidTarget* moduleContext;
    DMF_CONFIG_HidTarget* moduleConfig;
    WDFIOTARGET ioTarget;
    HID_COLLECTION_INFORMATION hidCollectionInformation;
    PHIDP_PREPARSED_DATA preparsedHidData;
    HIDP_CAPS hidCaps;
    WDF_MEMORY_DESCRIPTOR outputDescriptor;
    WDFMEMORY memoryPreparsedHidData;
    WDF_OBJECT_ATTRIBUTES attributes;

    PAGED_CODE();


    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ioTarget = NULL;
    *IsDeviceMatched = FALSE;
    preparsedHidData = NULL;
    memoryPreparsedHidData = NULL;
    RtlZeroMemory(&hidCollectionInformation,
                  sizeof(hidCollectionInformation));

    // Open the device to be queried.
    // NOTE: Per OSG (Austin Hodges), when opening HID device for enumeration purposes (to see if 
    // it is the required device, the Open Mode should be zero and share should be Read/Write.
    //
    ntStatus = HidTarget_IoTargetCreateByName(device,
                                              DevicePath,
                                              0,
                                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                                              &ioTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "CreateNewIoTargetByName fails: ntStatus=%!STATUS!",
                    ntStatus);
        ioTarget = NULL;
        goto Exit;
    }

    // Get the collection information.
    //
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor,
                                      (VOID*)&hidCollectionInformation,
                                      sizeof(HID_COLLECTION_INFORMATION));
    ntStatus = WdfIoTargetSendIoctlSynchronously(ioTarget,
                                                 NULL,
                                                 IOCTL_HID_GET_COLLECTION_INFORMATION,
                                                 NULL,
                                                 &outputDescriptor,
                                                 NULL,
                                                 NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "IOCTL_Hid_GET_COLLECTION_INFORMATION fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    if (0 == hidCollectionInformation.DescriptorSize)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "hidCollectionInformation.DescriptorSize==0, ntStatus=%!STATUS!",
                    ntStatus);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "IOCTL_Hid_GET_COLLECTION_INFORMATION returned VID = 0x%x",
                hidCollectionInformation.VendorID);

    // Check VID/PID 
    //
    if (hidCollectionInformation.VendorID != moduleConfig->VendorId)
    {
        TraceEvents(TRACE_LEVEL_WARNING,
                    DMF_TRACE,
                    "IOCTL_Hid_GET_COLLECTION_INFORMATION unsupported VID");
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "IOCTL_Hid_GET_COLLECTION_INFORMATION returned PID = 0x%x",
                hidCollectionInformation.ProductID);

    // See if it is one of the PIDs that the Client wants.
    //
    if ((moduleConfig->PidCount > 0) &&
        (! HidTarget_IsPidInList(hidCollectionInformation.ProductID,
                           moduleConfig->PidsOfDevicesToOpen,
                           moduleConfig->PidCount)))
    {
        TraceEvents(TRACE_LEVEL_WARNING,
                    DMF_TRACE,
                    "IOCTL_Hid_GET_COLLECTION_INFORMATION unsupported PID");
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = device;
    ntStatus = WdfMemoryCreate(&attributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               hidCollectionInformation.DescriptorSize,
                               &memoryPreparsedHidData,
                               (VOID* *)&preparsedHidData);
    if (! NT_SUCCESS(ntStatus))
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "ntStatus=%!STATUS!",
                    ntStatus);
        memoryPreparsedHidData = NULL;
        preparsedHidData = NULL;
        goto Exit;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor,
                                      (VOID*)preparsedHidData,
                                      hidCollectionInformation.DescriptorSize);

    ntStatus = WdfIoTargetSendIoctlSynchronously(ioTarget,
                                                 NULL,
                                                 IOCTL_HID_GET_COLLECTION_DESCRIPTOR,
                                                 NULL,
                                                 &outputDescriptor,
                                                 NULL,
                                                 NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "IOCTL_Hid_GET_COLLECTION_DESCRIPTOR fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    ntStatus = HidP_GetCaps(preparsedHidData,
                            &hidCaps);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "HidP_GetCaps() fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    // Check the usage and usage page.
    //
    if ((hidCaps.Usage != moduleConfig->VendorUsage) ||
        (hidCaps.UsagePage != moduleConfig->VendorUsagePage))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "incorrect usage or usage page failed");
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // At this point a matching device is found.
    //
    *IsDeviceMatched = TRUE;

    // Let the client decide whether this is the device it needs or not.
    //
    if (moduleConfig->EvtHidTargetDeviceSelectionCallback)
    {
        *IsDeviceMatched = moduleConfig->EvtHidTargetDeviceSelectionCallback(DmfModule,
                                                                             DevicePath,
                                                                             ioTarget,
                                                                             preparsedHidData,
                                                                             &hidCollectionInformation);
    }

Exit:

    if (ioTarget != NULL)
    {
        WdfIoTargetClose(ioTarget);
        // Need to delete the Target that was created.
        //
        WdfObjectDelete(ioTarget);
        ioTarget = NULL;
    }

    if (memoryPreparsedHidData != NULL)
    {
        WdfObjectDelete(memoryPreparsedHidData);
        memoryPreparsedHidData = NULL;
        preparsedHidData = NULL;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
HidTarget_MatchCheckForLocal(
    _In_ DMFMODULE DmfModule,
    _In_ PUNICODE_STRING DevicePath,
    _Out_ BOOLEAN* IsDeviceMatched
    )
/*++

Routine Description:

    Checks the custom device specific interface to determin the match for device.

Arguments:

    DmfModule - This Module's handle.
    DevicePath - The file name that allows this driver to access the device.
    IsDeviceMatched - The result of the query.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Look for the custom symbolic link that was created for the device specified by client.
    // Match up reported symbolic link against the saved 'SymbolicLinkToSearch'.
    //
    SIZE_T matchLength;

    ntStatus = STATUS_SUCCESS;
    *IsDeviceMatched = FALSE;

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "Interface Arrival %S",
                DevicePath->Buffer);

    size_t symbolicLinkNameToSearchSavedBufferLength = 0;
    PWCHAR symbolicLinkNameToSearchSavedBuffer = (PWCHAR)WdfMemoryGetBuffer(moduleContext->SymbolicLinkToSearchMemory,
                                                                            &symbolicLinkNameToSearchSavedBufferLength);

    // Strings should be same length.
    //
    if (symbolicLinkNameToSearchSavedBufferLength != DevicePath->Length)
    {
        // This code path is valid on unplug as several devices not associated with this instance
        // may disappear.
        //
        goto Exit;
    }

    DmfAssert(symbolicLinkNameToSearchSavedBuffer != NULL);
    matchLength = RtlCompareMemory((VOID*)symbolicLinkNameToSearchSavedBuffer,
                                    DevicePath->Buffer,
                                    DevicePath->Length);

    if (symbolicLinkNameToSearchSavedBufferLength != matchLength)
    {
        // This code path is valid on unplug as several devices not associated with this instance
        // may disappear.
        //
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "Found a matching local device");

    *IsDeviceMatched = TRUE;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
HidTarget_IsAccessoryTopLevelCollection(
    _In_ DMFMODULE DmfModule,
    _In_ PUNICODE_STRING DevicePath,
    _Out_ BOOLEAN* IsTopLevelCollection
    )
/*++

Routine Description:

    Determine if the given device handle is a device type that the Client wants
    to open.

Arguments:

    DmfModule - This Module's handle.
    DevicePath - The file name that allows this driver to access the device.
    IsTopLevelCollection - The result of the query.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_HidTarget* moduleConfig;
    BOOLEAN matchedDeviceFound;

    PAGED_CODE();

    DmfAssert(DevicePath != NULL);
    DmfAssert(IsTopLevelCollection != NULL);

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    *IsTopLevelCollection = FALSE;

    // Check to see if there is a match for the device that is being looked for.
    // Based on the configuration it is either a remote hid target or a local hid target.
    // Here, Remote means a device which may or may not be on the same devstack and 
    // local means a device which is on the same stack (which is the case when user 
    // has configured to skip enumerating all the hid devices).
    //
    if (!moduleConfig->SkipHidDeviceEnumerationSearch)
    {
        ntStatus = HidTarget_MatchCheckForRemote(DmfModule,
                                                 DevicePath,
                                                 &matchedDeviceFound);
    }
    else
    {
        ntStatus = HidTarget_MatchCheckForLocal(DmfModule,
                                                DevicePath,
                                                &matchedDeviceFound);
    }

    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "HidTarget_MatchCheck fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    *IsTopLevelCollection = matchedDeviceFound;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
HidTarget_MatchedTargetGet(
    _In_ DMFMODULE DmfModule,
    _In_ UNICODE_STRING* SymbolicLinkName
    )
/*++

Routine Description:

    This helper function searches for a matching device and creates an IoTarget to it.

Arguments:

    DmfModule - This Module's handle.
    SymbolicLinkName - Symbolic Link Name to the device that is checked for match.

Return Value:

    NTSTATUS - STATUS_SUCCESS if the device is matched, and IoTarget is opened to it, otherwise error status.

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    DMF_CONTEXT_HidTarget* moduleContext;
    DMF_CONFIG_HidTarget* moduleConfig;
    BOOLEAN isTopLevelCollection;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    isTopLevelCollection = FALSE;
    ntStatus = HidTarget_IsAccessoryTopLevelCollection(DmfModule,
                                                       SymbolicLinkName,
                                                       &isTopLevelCollection);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "HidTarget_IsAccessoryTopLevelCollection fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    if (! isTopLevelCollection)
    {
        // It is not the device the Client Driver is looking for.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "isTopLevelCollection=%d",
                    isTopLevelCollection);

        // Return STATUS_SUCCESS only when a matching device is found.
        //
        ntStatus = STATUS_NOT_FOUND;
        goto Exit;
    }

    moduleContext->EvtHidInputReport = moduleConfig->EvtHidInputReport;

    // Store the symbolic link in the device context if it is not already there.
    // Since this is needed to determine the symbolic link for the target this
    // code needs to execute first and clean itself up on failure.
    //
    if (WDF_NO_HANDLE == moduleContext->SymbolicLinkNameMemory)
    {
        if (0 == SymbolicLinkName->Length)
        {
            DmfAssert(FALSE);
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "Symbolic link length is 0");
            ntStatus = STATUS_INVALID_PARAMETER;
            goto Exit;
        }

        WDFMEMORY symbolicLinkNameMemoryLocal;
        BYTE* symbolicLinkNameBuffer;
        WDF_OBJECT_ATTRIBUTES objectAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        objectAttributes.ParentObject = DmfModule;
        ntStatus = WdfMemoryCreate(&objectAttributes,
                                   NonPagedPoolNx,
                                   MemoryTag,
                                   SymbolicLinkName->Length,
                                   &symbolicLinkNameMemoryLocal,
                                   (VOID**)&symbolicLinkNameBuffer);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "Could not allocate memory for symbolic link");
            goto Exit;
        }

        // NOTE: symbolicLinkNameBuffer does not have null termination.
        //
        RtlCopyMemory(symbolicLinkNameBuffer,
                      SymbolicLinkName->Buffer,
                      SymbolicLinkName->Length);

        moduleContext->SymbolicLinkNameMemory = symbolicLinkNameMemoryLocal;
    }
    else
    {
        // Received a duplicate callback.
        //
        TraceEvents(TRACE_LEVEL_WARNING,
                    DMF_TRACE,
                    "Symbolic link was already initialized");
        DmfAssert(FALSE);
    }

    // These items are cleared up on device removal.
    //
    if (NULL == moduleContext->IoTarget)
    {
        ntStatus = HidTarget_IoTargetCreateByName(device,
                                                  SymbolicLinkName,
                                                  moduleConfig->OpenMode,
                                                  moduleConfig->ShareAccess,
                                                  &moduleContext->IoTarget);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "WdfIoTargetCreate fails: ntStatus=%!STATUS!",
                        ntStatus);
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Created IOTarget for target HID device");

        // Cache the Hid Properties for this target.
        //
        ntStatus = HidTarget_DeviceProperyGet(DmfModule);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "HidTarget_DeviceProperyGet fails: ntStatus=%!STATUS!",
                        ntStatus);

            TraceEvents(TRACE_LEVEL_INFORMATION,
                        DMF_TRACE,
                        "Destroying IOTarget for target HID device");

            HidTarget_IoTargetDestroy(moduleContext);
            goto Exit;
        }

        DMF_ModuleUnlock(DmfModule);
        ntStatus = DMF_ModuleOpen(DmfModule);
        DMF_ModuleLock(DmfModule);

        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        DMF_TRACE,
                        "DMF_ModuleOpen fails: ntStatus=%!STATUS!. Destroying IOTarget for target HID device",
                        ntStatus);

            HidTarget_IoTargetDestroy(moduleContext);
        }
    }
    else
    {
        // WARNING: If the caller specifies PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
        // the operating system might call the PnP notification callback routine twice for a single
        // EventCategoryDeviceInterfaceChange event for an existing interface.
        // Can safely ignore the second call to the callback.
        // The operating system will not call the callback more than twice for a single event
        // So, if the IoTarget is already created, do nothing.
        //
    }

Exit:

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
HidTarget_MatchedTargetDestroy(
    _In_ DMFMODULE DmfModule,
    _In_ UNICODE_STRING* SymbolicLinkName
    )
/*++

Routine Description:

    This helper function searches for a matching device and if match found destroys corresponding IoTarget.

Arguments:

    DmfModule - This Module's handle.
    SymbolicLinkName - Symbolic Link Name to the device that is checked for match.

Return Value:

    NTSTATUS - STATUS_SUCCESS always.

--*/
{
    WDFDEVICE device;
    DMF_CONTEXT_HidTarget* moduleContext;
    DMF_CONFIG_HidTarget* moduleConfig;
    SIZE_T matchLength;
    BOOLEAN targetMatched;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    DmfAssert(SymbolicLinkName);
    targetMatched = FALSE;

    DMF_ModuleLock(DmfModule);

    if (WDF_NO_HANDLE == moduleContext->SymbolicLinkNameMemory)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Matching device was not detected");
        goto Exit;
    }

    size_t symbolicLinkNameSavedLength = 0;
    PWCHAR symbolicLinkNameSaved = (PWCHAR)WdfMemoryGetBuffer(moduleContext->SymbolicLinkNameMemory,
                                                              &symbolicLinkNameSavedLength);
    // Strings should be same length.
    //
    if (symbolicLinkNameSavedLength != SymbolicLinkName->Length)
    {
        // This code path is valid on unplug as several devices not associated with this instance
        // may disappear.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Length test fails");
        goto Exit;
    }

    DmfAssert(symbolicLinkNameSaved != NULL);

    matchLength = RtlCompareMemory((VOID*)symbolicLinkNameSaved,
                                   SymbolicLinkName->Buffer,
                                   SymbolicLinkName->Length);
    if (SymbolicLinkName->Length != matchLength)
    {
        // This code path is valid on unplug as several devices not associated with this instance
        // may disappear.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "matchLength test fails");
        goto Exit;
    }

    // DMF_ModuleClose must be called in unlocked state. Set a flag and call it
    // after the lock is released.
    //
    targetMatched = TRUE;

Exit:

    DMF_ModuleUnlock(DmfModule);

    if (targetMatched)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Removing HID device from notification function");

        // Call the DMF Module Client specific code.
        //
        if (moduleContext->IoTarget != NULL)
        {
            DMF_ModuleClose(DmfModule);
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", STATUS_SUCCESS);

    // Return SUCCESS here always.
    //
    return STATUS_SUCCESS;
}
#pragma code_seg()

#if !defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_Function_class_(DRIVER_NOTIFICATION_CALLBACK_ROUTINE)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
HidTarget_InterfaceArrivalCallbackForLocalOrRemoteKernel(
    _In_ VOID* NotificationStructure,
    _Inout_opt_ VOID* Context
    )
/*++

Routine Description:

    PnP notification function that is called when a HID device is available. 

Arguments:

    NotificationStructure - Gives information about the enumerated device.
    Context - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    PDEVICE_INTERFACE_CHANGE_NOTIFICATION info;
    DMFMODULE dmfModule;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // warning C6387: 'Context' could be '0'.
    //
    #pragma warning(suppress:6387)
    dmfModule = DMFMODULEVOID_TO_MODULE(Context);
    DmfAssert(dmfModule != NULL);

    info = (PDEVICE_INTERFACE_CHANGE_NOTIFICATION)NotificationStructure;
    ntStatus = STATUS_SUCCESS;

    if (DMF_Utility_IsEqualGUID((LPGUID)&(info->Event),
                                (LPGUID)&GUID_DEVICE_INTERFACE_ARRIVAL))
    {
        DmfAssert(info->SymbolicLinkName);

        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "GUID_DEVICE_INTERFACE_ARRIVAL Found HID Device %S",
                    info->SymbolicLinkName->Buffer);

        ntStatus = HidTarget_MatchedTargetGet(dmfModule,
                                              info->SymbolicLinkName);
    }
    else if (DMF_Utility_IsEqualGUID((LPGUID)&(info->Event),
                                     (LPGUID)&GUID_DEVICE_INTERFACE_REMOVAL))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "GUID_DEVICE_INTERFACE_REMOVAL %S",
                    info->SymbolicLinkName->Buffer);

        ntStatus = HidTarget_MatchedTargetDestroy(dmfModule,
                                                  info->SymbolicLinkName);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
HidTarget_NotificationRegisterForLocalOrRemoteKernel(
    _In_ DMFMODULE DmfModule,
    _In_ const GUID* InterfaceGuid
    )
/*++

Routine Description:

    Register for a notification. This function does the work of properly registering
    for a notification of the arrival or existence of the another target that this Module needs to open.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    WDFDEVICE parentDevice;
    PDEVICE_OBJECT deviceObject;
    PDRIVER_OBJECT driverObject;
    DMF_CONTEXT_HidTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    parentDevice = DMF_ParentDeviceGet(DmfModule);
    DmfAssert(parentDevice != NULL);

    deviceObject = WdfDeviceWdmGetDeviceObject(parentDevice);
    DmfAssert(deviceObject != NULL);
    driverObject = deviceObject->DriverObject;

    DmfAssert(NULL == moduleContext->HidInterfaceNotification);
    ntStatus = IoRegisterPlugPlayNotification(EventCategoryDeviceInterfaceChange,
                                              PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
                                              (void*)InterfaceGuid,
                                              driverObject,
                                              (PDRIVER_NOTIFICATION_CALLBACK_ROUTINE)HidTarget_InterfaceArrivalCallbackForLocalOrRemoteKernel,
                                              (VOID*)DmfModule,
                                              &(moduleContext->HidInterfaceNotification));

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "IoRegisterPlugPlayNotification: Notification Entry 0x%p ntStatus=%!STATUS!",
                moduleContext->HidInterfaceNotification, ntStatus);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
HidTarget_NotificationRegisterForLocalKernel(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Register for a notification for the specified device. This helper function creates a device
    specific interface and setups listening for it.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_HidTarget* moduleConfig;
    const GUID *interfaceGuid;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Create a custom interface and symbolic link for the device specified by client.
    // Newly created symbolic link is saved for lookup at arrival callback.
    //
    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "Creating Custom Interface for target HID device 0x%p",
                moduleConfig->HidTargetToConnect);

    interfaceGuid = &GUID_CUSTOM_DEVINTERFACE;
    ntStatus = HidTarget_InterfaceCreateForLocal(DmfModule,
                                                 interfaceGuid,
                                                 moduleConfig->HidTargetToConnect);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "HidTarget_CreateInterfaceForDevice fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    ntStatus = HidTarget_NotificationRegisterForLocalOrRemoteKernel(DmfModule,
                                                                    interfaceGuid);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
inline
NTSTATUS
HidTarget_NotificationRegisterForRemoteKernel(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Register for notification for all hid devices. 

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    PAGED_CODE();

    return HidTarget_NotificationRegisterForLocalOrRemoteKernel(DmfModule,
                                                                &GUID_DEVINTERFACE_HID);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
HidTarget_NotificationUnregisterKernel(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Unregister a notification. DMF calls this callback instead of the Close callback
    when the Open Notification option is selected. This function does the work of properly unregistering
    a notification of the arrival or existence of the another target that this Module needs to open.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    // The notification routine could be called after the IoUnregisterPlugPlayNotification method 
    // has returned which was undesirable. UnRegisterNotify prevents the 
    // notification routine from being called after IoUnregisterPlugPlayNotificationEx/CM_Unregister_Notification returns.
    //
    if (moduleContext->HidInterfaceNotification != NULL)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Destroy Notification Entry 0x%p",
                    moduleContext->HidInterfaceNotification);

        ntStatus = IoUnregisterPlugPlayNotificationEx(moduleContext->HidInterfaceNotification);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        DMF_TRACE,
                        "IoUnregisterPlugPlayNotificationEx() fails: ntStatus=%!STATUS!",
                        ntStatus);
            DmfAssert(FALSE);
            goto Exit;
        }

        moduleContext->HidInterfaceNotification = NULL;

        // The device may or may not have been opened. Close it now
        // because the Close handler will not be called.
        //
        if (moduleContext->IoTarget != NULL)
        {
            DMF_ModuleClose(DmfModule);
        }
    }
    else
    {
        // Allow caller to unregister notification even if it has not been registered.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "IoUnregisterPlugPlayNotificationEx() skipped.");
    }

Exit:

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()


#else

#pragma code_seg("PAGE")
DWORD
HidTarget_InterfaceArrivalCallbackForRemoteUser(
    _In_ HCMNOTIFICATION Notify,
    _In_opt_ VOID* Context,
    _In_ CM_NOTIFY_ACTION Action,
    _In_reads_bytes_(EventDataSize) PCM_NOTIFY_EVENT_DATA EventData,
    _In_ DWORD EventDataSize
    )
/*++

Routine Description:

    Callback called when the notification that is registered detects an arrival or
    removal of an device interface of any Hid device. This function determines
    if the instance of the device is the proper device to open, and if so, opens it.

Arguments:

    Notify- Handle to this notification.
    Context - This Module's handle.
    Action - One of the action in CM_NOTIFY_ACTION.
    EventData - Event Data associated with the notification callback.
    EventDataSize - Size of Event Data.

Return Value:

    DWORD returns ERROR_SUCCESS always.

--*/
{
    DMFMODULE dmfModule;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(Notify);
    UNREFERENCED_PARAMETER(EventDataSize);

    dmfModule = DMFMODULEVOID_TO_MODULE(Context);

    ntStatus = STATUS_SUCCESS;

    if (Action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL)
    {
        DmfAssert(EventData->u.DeviceInterface.SymbolicLink);
        UNICODE_STRING symbolicLinkName;

        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Processing interface arrival %ws",
                    EventData->u.DeviceInterface.SymbolicLink);
        RtlInitUnicodeString(&symbolicLinkName,
                             EventData->u.DeviceInterface.SymbolicLink);

        ntStatus = HidTarget_MatchedTargetGet(dmfModule,
                                              &symbolicLinkName);
    }
    else if (Action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL)
    {
        DmfAssert(EventData->u.DeviceInterface.SymbolicLink);
        UNICODE_STRING symbolicLinkName;

        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Processing interface removal %ws",
                    EventData->u.DeviceInterface.SymbolicLink);
        RtlInitUnicodeString(&symbolicLinkName,
                             EventData->u.DeviceInterface.SymbolicLink);

        ntStatus = HidTarget_MatchedTargetDestroy(dmfModule,
                                                  &symbolicLinkName);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    // Return SUCCESS here always.
    //
    return ERROR_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
HidTarget_MatchedTargetForExistingInterfacesGet(
    _In_ DMFMODULE DmfModule,
    _In_ const GUID* InterfaceGuid
    )
/*++

Routine Description:

    This helper function searches all existing interfaces for the given InterfaceGuid,
    for a matching device and creates an IoTarget to it.

Arguments:

    DmfModule - This Module's handle.
    InterfaceGuid - Guid to search for.

Return Value:

    NTSTATUS - STATUS_SUCCESS if the device is matched, and IoTarget is opened to it, otherwise error status.

--*/
{
    NTSTATUS ntStatus;
    CONFIGRET cr;
    DWORD lastError;
    PWSTR deviceInterfaceList;
    ULONG deviceInterfaceListLength;
    PWSTR currentInterface;
    DWORD index;

    PAGED_CODE();

    deviceInterfaceList = NULL;
    deviceInterfaceListLength = 0;

    // Get the existing Device Interfaces for the given Guid.
    // It is recommended to do this in a loop, as the
    // size can change between the call to CM_Get_Device_Interface_List_Size and 
    // CM_Get_Device_Interface_List.
    //
    do
    {
        cr = CM_Get_Device_Interface_List_Size(&deviceInterfaceListLength,
                                               (LPGUID)InterfaceGuid,
                                               NULL,
                                               CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);
        if (cr != CR_SUCCESS)
        {
            lastError = GetLastError();
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "CM_Get_Device_Interface_List_Size failed with Result %d and lastError %!WINERROR!",
                        cr,
                        lastError);
            ntStatus = NTSTATUS_FROM_WIN32(lastError);
            goto Exit;
        }

        if (deviceInterfaceList != NULL)
        {
            if (! HeapFree(GetProcessHeap(),
                           0,
                           deviceInterfaceList))
            {
                lastError = GetLastError();
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "HeapFree failed with lastError %!WINERROR!",
                            lastError);
                ntStatus = NTSTATUS_FROM_WIN32(lastError);
                deviceInterfaceList = NULL;
                goto Exit;
            }
        }

        deviceInterfaceList = (PWSTR)HeapAlloc(GetProcessHeap(),
                                               HEAP_ZERO_MEMORY,
                                               deviceInterfaceListLength * sizeof(WCHAR));
        if (deviceInterfaceList == NULL)
        {
            lastError = GetLastError();
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "HeapAlloc failed with lastError %!WINERROR!",
                        lastError);
            ntStatus = NTSTATUS_FROM_WIN32(lastError);
            goto Exit;
        }

        cr = CM_Get_Device_Interface_List((LPGUID)InterfaceGuid,
                                          NULL,
                                          deviceInterfaceList,
                                          deviceInterfaceListLength,
                                          CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);

    } while (cr == CR_BUFFER_SMALL);

    if (cr != CR_SUCCESS)
    {
        lastError = GetLastError();
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "CM_Get_Device_Interface_List failed with Result %d and lastError %!WINERROR!",
                    cr,
                    lastError);
        ntStatus = NTSTATUS_FROM_WIN32(lastError);
        goto Exit;
    }

    // Loop through the interfaces for a matching target and open it.
    // Ensure to return STATUS_SUCCESS only on a matched target get.
    //
    ntStatus = STATUS_NOT_FOUND;
    index = 0;
    UNICODE_STRING symbolicLinkName;
    for (currentInterface = deviceInterfaceList;
         *currentInterface;
         currentInterface += wcslen(currentInterface) + 1)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE,
                    DMF_TRACE,
                    "[index %d] Processing interface %ws",
                    index,
                    currentInterface);

        RtlInitUnicodeString(&symbolicLinkName,
                             currentInterface);

        ntStatus = HidTarget_MatchedTargetGet(DmfModule,
                                              &symbolicLinkName);

        // Break if a matching target was found.
        //
        if (ntStatus == STATUS_SUCCESS)
        {
            break;
        }

        index++;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
HidTarget_NotificationRegisterForRemoteUser(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Register for a notification for all Hid device interfaces. This function does the work of properly registering
    for a notification of the arrival or existence of the another target that this Module needs to open.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;
    DMF_CONFIG_HidTarget* moduleConfig;
    CM_NOTIFY_FILTER cmNotifyFilter = { 0 };
    CONFIGRET configRet;
    const GUID* interfaceGuid;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    interfaceGuid = &GUID_DEVINTERFACE_HID;
    cmNotifyFilter.cbSize = sizeof(CM_NOTIFY_FILTER);
    cmNotifyFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    cmNotifyFilter.u.DeviceInterface.ClassGuid = *interfaceGuid;

    configRet = CM_Register_Notification(&cmNotifyFilter,
                                         (VOID*)DmfModule,
                                         (PCM_NOTIFY_CALLBACK)HidTarget_InterfaceArrivalCallbackForRemoteUser,
                                         &(moduleContext->HidInterfaceNotification));
    // Target device might already be there. So try now.
    // 
    if (configRet == CR_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE,
                    DMF_TRACE,
                    "Processing existing interfaces- START");

        ntStatus = HidTarget_MatchedTargetForExistingInterfacesGet(DmfModule,
                                                             interfaceGuid);

        TraceEvents(TRACE_LEVEL_VERBOSE,
                    DMF_TRACE,
                    "Processing existing interfaces- END");

        // Should always return success here, since notification might be called back later for the desired device.
        //
        ntStatus = STATUS_SUCCESS;
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "CM_Register_Notification fails: configRet=0x%x",
                    configRet);

        ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "Created Notification Entry 0x%p",
                moduleContext->HidInterfaceNotification);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
HidTarget_NotificationRegisterForLocalUser(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This function opens the lower level stack as target, and then opens the module.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;
    DMF_CONFIG_HidTarget* moduleConfig;
    WDFIOTARGET target;
    WDF_OBJECT_ATTRIBUTES attributes;
    BOOLEAN lockHeld;
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    lockHeld = FALSE;

    // Get the next lower driver in the stack. Use the special local
    // IO target flag since HID requires a file handle for IO requests.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = moduleConfig->HidTargetToConnect;

    ntStatus = WdfIoTargetCreate(moduleConfig->HidTargetToConnect,
                                 &attributes,
                                 &target);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfIoTargetCreate fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    WDF_IO_TARGET_OPEN_PARAMS openParams;
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_FILE(&openParams,
                                                NULL);
    ntStatus = WdfIoTargetOpen(target,
                               &openParams);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfIoTargetOpen fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    DMF_ModuleLock(DmfModule);
    lockHeld = TRUE;

    DmfAssert(moduleContext->IoTarget == NULL);

    moduleContext->IoTarget = target;
    moduleContext->EvtHidInputReport = moduleConfig->EvtHidInputReport;

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "Created IOTarget for downlevel stack");

    // Cache the Hid Properties for this target.
    //
    ntStatus = HidTarget_DeviceProperyGet(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "HidTarget_DeviceProperyGet fails: ntStatus=%!STATUS!",
                    ntStatus);

        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Destroying IOTarget for target HID device");

        HidTarget_IoTargetDestroy(moduleContext);
        goto Exit;
    }

    DMF_ModuleUnlock(DmfModule);
    ntStatus = DMF_ModuleOpen(DmfModule);
    DMF_ModuleLock(DmfModule);

    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "Module Open Fails; Destroying IOTarget for target HID device,ntStatus=%!STATUS!",
                    ntStatus);

        HidTarget_IoTargetDestroy(moduleContext);
    }

Exit:

    if (lockHeld)
    {
        DMF_ModuleUnlock(DmfModule);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
HidTarget_NotificationUnregisterUser(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Unregister a notification. DMF calls this callback instead of the Close callback
    when the Open Notification option is selected. This function does the work of properly unregistering
    a notification of the arrival or existence of the another target that this Module needs to open.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_HidTarget* moduleContext;
    DMF_CONFIG_HidTarget* moduleConfig;

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // For local, close the target.
    //
    if (moduleConfig->SkipHidDeviceEnumerationSearch)
    {
        if (moduleContext->IoTarget != NULL)
        {
            DMF_ModuleClose(DmfModule);
        }
    }
    else
    {
        // The notification routine could be called after the CM_Unregister_Notification method 
        // has returned which was undesirable. CM_Unregister_Notification prevents the 
        // notification routine from being called after CM_Unregister_Notification returns.
        //
        if (moduleContext->HidInterfaceNotification != NULL)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        DMF_TRACE,
                        "Destroy Notification Entry 0x%p",
                        moduleContext->HidInterfaceNotification);

            CONFIGRET cr;
            cr = CM_Unregister_Notification(moduleContext->HidInterfaceNotification);
            if (cr != CR_SUCCESS)
            {
                NTSTATUS ntStatus;
                ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "CM_Unregister_Notification fails: ntStatus=%!STATUS!", 
                            ntStatus);
                goto Exit;
            }

            moduleContext->HidInterfaceNotification = NULL;

            // The device may or may not have been opened. Close it now
            // because the Close handler will not be called.
            //
            if (moduleContext->IoTarget != NULL)
            {
                DMF_ModuleClose(DmfModule);
            }
        }
        else
        {
            // Allow caller to unregister notification even if it has not been registered.
            //
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        DMF_TRACE,
                        "CM_Unregister_Notification skipped.");
        }
    }

Exit:

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#endif // !defined(DMF_USER_MODE)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_ModuleInstanceDestroy)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_HidTarget_Destroy(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Destroy an instance of a Module of type Hid. This code is not
    strictly necessary, but it is included here to assert that the notification handle
    has been closed. This code can be deleted because DMF performs
    this function automatically.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_HidTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // The Notification should not be enabled at this time. It should have been unregistered.
    //
    DmfAssert(NULL == moduleContext->HidInterfaceNotification);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_NotificationRegister)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_HidTarget_NotificationRegister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Register for a notification. DMF calls this callback instead of the Open callback
    when the Open Notification option is selected. This function does the work of properly registering
    for a notification of the arrival or existence of the another target that this Module needs to open.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;
    DMF_CONFIG_HidTarget* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // This function should not be not called twice.
    //
    DmfAssert(NULL == moduleContext->HidInterfaceNotification);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Skip Search for all Hid Devices if the caller configured explicitly.
    //
    if (!moduleConfig->SkipHidDeviceEnumerationSearch)
    {
#if defined DMF_USER_MODE
        ntStatus = HidTarget_NotificationRegisterForRemoteUser(DmfModule);
#else
        ntStatus = HidTarget_NotificationRegisterForRemoteKernel(DmfModule);
#endif
    }
    else
    {
#if defined DMF_USER_MODE
        ntStatus = HidTarget_NotificationRegisterForLocalUser(DmfModule);
#else
        ntStatus = HidTarget_NotificationRegisterForLocalKernel(DmfModule);
#endif
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_NotificationUnregister)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_HidTarget_NotificationUnregister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Unregister a notification. DMF calls this callback instead of the Close callback
    when the Open Notification option is selected. This function does the work of properly unregistering
    a notification of the arrival or existence of the another target that this Module needs to open.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    PAGED_CODE();

#if defined DMF_USER_MODE
    HidTarget_NotificationUnregisterUser(DmfModule);
#else
    HidTarget_NotificationUnregisterKernel(DmfModule);
#endif

}

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_HidTarget_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type HidTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

   NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_HidTarget* moduleConfig;
    DMF_CONTEXT_HidTarget* moduleContext;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_ContinuousRequestTarget moduleConfigContinuousRequestTarget;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    // Set HidTarget Modules as parent object for dynamically created Modules.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;

    if (moduleContext->HidCaps.InputReportByteLength > 0)
    {
        // Modules created in this if block support sending and processing input reports.
        // They are not necessary if the HID descriptor has input report length greater than zero.
        //

        DMF_CONFIG_BufferPool moduleConfigBufferPool;
        DMF_CONFIG_ThreadedBufferQueue moduleConfigThreadedBufferQueue;

        // Create Threaded Buffer Queue to handle processing of Input Report with client callback.
        //
        DMF_CONFIG_ThreadedBufferQueue_AND_ATTRIBUTES_INIT(&moduleConfigThreadedBufferQueue,
                                                           &moduleAttributes);
        moduleConfigThreadedBufferQueue.EvtThreadedBufferQueueWork = HidTarget_InputReportConsumeWork;
        moduleConfigThreadedBufferQueue.BufferQueueConfig.SourceSettings.BufferContextSize = 0;
        moduleConfigThreadedBufferQueue.BufferQueueConfig.SourceSettings.BufferCount = moduleConfig->PendedInputReadRequestCount;
        moduleConfigThreadedBufferQueue.BufferQueueConfig.SourceSettings.BufferSize = moduleContext->HidCaps.InputReportByteLength;
        moduleConfigThreadedBufferQueue.BufferQueueConfig.SourceSettings.EnableLookAside = TRUE;
        moduleConfigThreadedBufferQueue.BufferQueueConfig.SourceSettings.PoolType = NonPagedPool;
        moduleAttributes.ClientModuleInstanceName = "ThreadedBufferQueueInputReport";
        moduleAttributes.PassiveLevel = TRUE;
        ntStatus = DMF_ThreadedBufferQueue_Create(device,
                                                  &moduleAttributes,
                                                  &objectAttributes,
                                                  &moduleContext->DmfModuleThreadedBufferQueueInputReport);

        // Create Buffer Pool for Input Reports of size retrieved from the HID capability.
        // This will be used for buffers of input report read requests sent using Dmf_HidTarget_InputRead().
        //
        DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT(&moduleConfigBufferPool,
                                                  &moduleAttributes);
        moduleConfigBufferPool.BufferPoolMode = BufferPool_Mode_Source;
        moduleConfigBufferPool.Mode.SourceSettings.EnableLookAside = TRUE;
        moduleConfigBufferPool.Mode.SourceSettings.BufferCount = DEFAULT_NUMBER_OF_PENDING_INPUT_READS;
        moduleConfigBufferPool.Mode.SourceSettings.PoolType = NonPagedPoolNx;
        moduleConfigBufferPool.Mode.SourceSettings.BufferSize = moduleContext->HidCaps.InputReportByteLength;
        moduleConfigBufferPool.Mode.SourceSettings.BufferContextSize = 0;
        moduleAttributes.ClientModuleInstanceName = "BufferPoolInputReports";
        moduleAttributes.PassiveLevel = TRUE;
        ntStatus = DMF_BufferPool_Create(device,
                                         &moduleAttributes,
                                         &objectAttributes,
                                         &moduleContext->DmfModuleBufferPoolInputReport);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BufferPool_Create fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Start the thread for the Threaded Buffer Queue module.
        //
        ntStatus = DMF_ThreadedBufferQueue_Start(moduleContext->DmfModuleThreadedBufferQueueInputReport);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ThreadedBufferQueue_Start Start fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    // Create Continuous Request for streaming Input Reports of size retrieved from the HID capability.
    // NOTE: Hid class would not complete the pended input report read if there is mismatch in buffer size with
    // HidCaps.InputReportByteLength.
    //
    DMF_CONFIG_ContinuousRequestTarget_AND_ATTRIBUTES_INIT(&moduleConfigContinuousRequestTarget,
                                                           &moduleAttributes);
    moduleConfigContinuousRequestTarget.BufferContextInputSize = 0;
    moduleConfigContinuousRequestTarget.BufferContextOutputSize = 0;
    moduleConfigContinuousRequestTarget.BufferInputSize = 0;
    moduleConfigContinuousRequestTarget.BufferOutputSize = moduleContext->HidCaps.InputReportByteLength;
    moduleConfigContinuousRequestTarget.BufferCountInput = 0;
    moduleConfigContinuousRequestTarget.BufferCountOutput = moduleConfig->PendedInputReadRequestCount;
    moduleConfigContinuousRequestTarget.ContinuousRequestCount = moduleConfig->PendedInputReadRequestCount;
    moduleConfigContinuousRequestTarget.ContinuousRequestTargetMode = ContinuousRequestTarget_Mode_Manual;
    moduleConfigContinuousRequestTarget.RequestType = ContinuousRequestTarget_RequestType_Read;
    moduleConfigContinuousRequestTarget.EnableLookAsideOutput = TRUE;
    moduleConfigContinuousRequestTarget.EvtContinuousRequestTargetBufferInput = NULL;
    moduleConfigContinuousRequestTarget.EvtContinuousRequestTargetBufferOutput = HidTarget_InputReadExCompletionCallback;
    moduleConfigContinuousRequestTarget.PoolTypeOutput = NonPagedPoolNx;
    moduleConfigContinuousRequestTarget.CancelAndResendRequestInD0Callbacks = FALSE;
    moduleConfigContinuousRequestTarget.PurgeAndStartTargetInD0Callbacks = FALSE;
    moduleAttributes.ClientModuleInstanceName = "ContinuousRequestTargetInputReport";
    moduleAttributes.PassiveLevel = TRUE;
    ntStatus = DMF_ContinuousRequestTarget_Create(device,
                                                  &moduleAttributes,
                                                  &objectAttributes,
                                                  &moduleContext->DmfModuleContinuousRequestTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_Create fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // IoTarget needs to be set for the reset target module after it has been assigned.
    // It is cleared in the close function of the module.
    //
     DMF_ContinuousRequestTarget_IoTargetSet(moduleContext->DmfModuleContinuousRequestTarget,
                                             moduleContext->IoTarget);

Exit:

    // Perform clean-up if error occurs i.e. module cannot be opened.
    //
    if (! NT_SUCCESS(ntStatus))
    {
        // Stop the thread for the Threaded Buffer Queue module.
        //
        if (moduleContext->DmfModuleThreadedBufferQueueInputReport != NULL)
        {
            DMF_ThreadedBufferQueue_Stop(moduleContext->DmfModuleThreadedBufferQueueInputReport);
        }

        // Clear the IoTarget in the DMF_RequestTarget module.
        //
        DMF_ContinuousRequestTarget_IoTargetClear(moduleContext->DmfModuleContinuousRequestTarget);

        // Delete dynamically created Modules.
        //
        if (moduleContext->DmfModuleThreadedBufferQueueInputReport != NULL)
        {
            WdfObjectDelete(moduleContext->DmfModuleThreadedBufferQueueInputReport);
            moduleContext->DmfModuleThreadedBufferQueueInputReport = NULL;
        }

        if (moduleContext->DmfModuleContinuousRequestTarget != NULL)
        {
            WdfObjectDelete(moduleContext->DmfModuleContinuousRequestTarget);
            moduleContext->DmfModuleContinuousRequestTarget = NULL;
        }

        if (moduleContext->DmfModuleBufferPoolInputReport != NULL)
        {
            WdfObjectDelete(moduleContext->DmfModuleBufferPoolInputReport);
            moduleContext->DmfModuleBufferPoolInputReport = NULL;
        }
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
DMF_HidTarget_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type HidTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_HidTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Close the associated target.
    //
    HidTarget_IoTargetDestroy(moduleContext);

    // Stop the thread for the Threaded Buffer Queue module.
    //
    if (moduleContext->DmfModuleThreadedBufferQueueInputReport != NULL)
    {
        DMF_ThreadedBufferQueue_Stop(moduleContext->DmfModuleThreadedBufferQueueInputReport);
    }

    // Clear the IoTarget in the DMF_RequestTarget module.
    //
    DMF_ContinuousRequestTarget_IoTargetClear(moduleContext->DmfModuleContinuousRequestTarget);

    // Delete dynamically created Modules.
    //
    if (moduleContext->DmfModuleThreadedBufferQueueInputReport != NULL)
    {
        WdfObjectDelete(moduleContext->DmfModuleThreadedBufferQueueInputReport);
        moduleContext->DmfModuleThreadedBufferQueueInputReport = NULL;
    }

    if (moduleContext->DmfModuleContinuousRequestTarget != NULL)
    {
        WdfObjectDelete(moduleContext->DmfModuleContinuousRequestTarget);
        moduleContext->DmfModuleContinuousRequestTarget = NULL;
    }

    if (moduleContext->DmfModuleBufferPoolInputReport != NULL)
    {
        WdfObjectDelete(moduleContext->DmfModuleBufferPoolInputReport);
        moduleContext->DmfModuleBufferPoolInputReport = NULL;
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_HidTarget_TransportAddressWrite(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload
    )
/*++

Routine Description:

    Does nothing

Arguments:

    DmfInterface - Interface module handle.
    Payload - Data to write.

Return Value:

    STATUS_NOT_IMPLEMENTED for now.

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);
    UNREFERENCED_PARAMETER(Payload);

    return STATUS_NOT_IMPLEMENTED;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_HidTarget_TransportAddressRead(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload
    )
/*++

Routine Description:

    Does nothing

Arguments:

    DmfInterface - Interface module handle.
    Payload - Data to write.

Return Value:

    STATUS_NOT_IMPLEMENTED for now.

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);
    UNREFERENCED_PARAMETER(Payload);

    return STATUS_NOT_IMPLEMENTED;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_HidTarget_TransportBufferWrite(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload
    )
/*++

Routine Description:

    Does nothing

Arguments:

    DmfInterface - Interface module handle.
    Payload - Data to write.

Return Value:

    STATUS_NOT_IMPLEMENTED for now.

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);
    UNREFERENCED_PARAMETER(Payload);

    return STATUS_NOT_IMPLEMENTED;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_HidTarget_TransportBufferRead(
    _In_ DMFINTERFACE DmfInterface,
    _In_ BusTransport_TransportPayload* Payload
    )
/*++

Routine Description:

    Does nothing

Arguments:

    DmfInterface - Interface module handle.
    Payload - Data to write.

Return Value:

    STATUS_NOT_IMPLEMENTED for now.

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);
    UNREFERENCED_PARAMETER(Payload);

    return STATUS_NOT_IMPLEMENTED;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_HidTarget_TransportBind(
    _In_ DMFINTERFACE DmfInterface,
    _In_ DMF_INTERFACE_PROTOCOL_BusTarget_BIND_DATA* ProtocolBindData,
    _Out_ DMF_INTERFACE_TRANSPORT_BusTarget_BIND_DATA* TransportBindData
    )
/*++

Routine Description:

    Does nothing

Arguments:

    DmfInterface - Interface module handle.
    ProtocolBindData
    TransportBindData

Return Value:

    STATUS_SUCCESS

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);
    UNREFERENCED_PARAMETER(ProtocolBindData);
    UNREFERENCED_PARAMETER(TransportBindData);

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_HidTarget_TransportUnbind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Does nothing

Arguments:

    DmfInterface - Interface module handle.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_HidTarget_TransportPostBind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Does nothing

Arguments:

    DmfInterface - Interface module handle.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_HidTarget_TransportPreUnbind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Does nothing

Arguments:

    DmfInterface - Interface module handle.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);
}

_Function_class_(DMF_ModuleQueryRemove)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_HidTarget_ModuleQueryRemove(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Callback for Module's Query Remove.
    If the Module is configured to be working in-stack, returns failure, else returns success.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_HidTarget* moduleConfig;

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // If the driver is loaded on the Hidstack, returning failure here prevents HidClass from handling the QueryRemove. 
    // During QueryRemove Hidclass cancels all input report reads from Client drivers prematurely, thus breaking the input report handling path.
    // Return failure for in-stack use. (SkipHidDeviceEnumerationSearch means in-stack).
    //
    if (moduleConfig->SkipHidDeviceEnumerationSearch)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
    }
    else
    {
        ntStatus = STATUS_SUCCESS;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_HidTarget_ModuleQueryRemove returns ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Hid.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Hid;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_HidTarget;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_HidTarget;
    DMF_INTERFACE_TRANSPORT_BusTarget_DECLARATION_DATA BusTargetDeclarationData;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_HidTarget);
    dmfCallbacksDmf_HidTarget.ModuleInstanceDestroy = DMF_HidTarget_Destroy;
    dmfCallbacksDmf_HidTarget.DeviceOpen = DMF_HidTarget_Open;
    dmfCallbacksDmf_HidTarget.DeviceClose = DMF_HidTarget_Close;
    dmfCallbacksDmf_HidTarget.DeviceNotificationRegister = DMF_HidTarget_NotificationRegister;
    dmfCallbacksDmf_HidTarget.DeviceNotificationUnregister = DMF_HidTarget_NotificationUnregister;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_HidTarget);
    dmfCallbacksWdf_HidTarget.ModuleQueryRemove = DMF_HidTarget_ModuleQueryRemove;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Hid,
                                            HidTarget,
                                            DMF_CONTEXT_HidTarget,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware);

    dmfModuleDescriptor_Hid.CallbacksDmf = &dmfCallbacksDmf_HidTarget;
    dmfModuleDescriptor_Hid.CallbacksWdf = &dmfCallbacksWdf_HidTarget;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Hid,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    DMF_INTERFACE_TRANSPORT_BusTarget_DESCRIPTOR_INIT(&BusTargetDeclarationData,
                                                      DMF_HidTarget_TransportPostBind,
                                                      DMF_HidTarget_TransportPreUnbind,
                                                      DMF_HidTarget_TransportBind,
                                                      DMF_HidTarget_TransportUnbind,
                                                      DMF_HidTarget_TransportAddressWrite,
                                                      DMF_HidTarget_TransportAddressRead,
                                                      DMF_HidTarget_TransportBufferWrite,
                                                      DMF_HidTarget_TransportBufferRead);

    // Add the interface to the Transport Module.
    //
    ntStatus = DMF_ModuleInterfaceDescriptorAdd(*DmfModule,
                                                (DMF_INTERFACE_DESCRIPTOR*)&BusTargetDeclarationData);

    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleInterfaceDescriptorAdd fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_BufferRead(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_(BufferLength) VOID* Buffer,
    _In_ ULONG BufferLength,
    _In_ ULONG TimeoutMs
    )
/*++

Routine Description:

    Invoke the BufferRead Callback for this Module.

Arguments:

    DmfModule - This Module's handle.
    Buffer - The address where the read bytes should be written.
    BufferLength - The number of bytes to read.
    TimeoutMs - Timeout value in milliseconds.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 HidTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_ContinuousRequestTarget_SendSynchronously(moduleContext->DmfModuleContinuousRequestTarget,
                                                             NULL,
                                                             0,
                                                             Buffer,
                                                             BufferLength,
                                                             ContinuousRequestTarget_RequestType_Read,
                                                             0,
                                                             TimeoutMs,
                                                             NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_SendSynchronously fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_BufferWrite(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Buffer,
    _In_ ULONG BufferLength,
    _In_ ULONG TimeoutMs
    )
/*++

Routine Description:

    Invoke the BufferWrite Callback for this Module.

Arguments:

    DmfModule - This Module's handle.
    Buffer - The address of the bytes that should be written.
    BufferLength - The number of bytes to write.
    TimeoutMs - Timeout value in milliseconds.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 HidTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_ContinuousRequestTarget_SendSynchronously(moduleContext->DmfModuleContinuousRequestTarget,
                                                             Buffer,
                                                             BufferLength,
                                                             NULL,
                                                             0,
                                                             ContinuousRequestTarget_RequestType_Write,
                                                             0,
                                                             TimeoutMs,
                                                             NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_SendSynchronously fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_FeatureGet(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR FeatureId,
    _Out_ UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _In_ ULONG OffsetOfDataToCopy,
    _In_ ULONG NumberOfBytesToCopy
    )
/*++

Routine Description:

    Sends a Get Feature request to underlying HID device.

Arguments:

    DmfModule - This Module's handle.
    FeatureId - Feature Id to call Get Feature on
    Buffer - Target buffer where read data will be written to
    BufferSize - Size of Buffer in bytes
    OffsetOfDataToCopy - Offset of data from Feature Report buffer to copy from
    NumberOfBytesToCopy - Number of bytes to copy from offset in Feature Report Buffer

Return Value:

    NTSTATUS

--*/
{
    PHIDP_PREPARSED_DATA preparsedData;
    CHAR* report;
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;
    WDFDEVICE device;
    WDFMEMORY reportMemory;
    WDF_OBJECT_ATTRIBUTES attributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 HidTarget);

    preparsedData = NULL;
    report = NULL;
    reportMemory = WDF_NO_HANDLE;

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    if (NumberOfBytesToCopy > BufferSize)
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Insufficient buffer length: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&attributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               moduleContext->HidCaps.FeatureReportByteLength,
                               &reportMemory,
                               (VOID**)&report);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate for report fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    preparsedData = (PHIDP_PREPARSED_DATA)WdfMemoryGetBuffer(moduleContext->PreparsedDataMemory,
                                                             NULL);

    // Start with a zeroed report. If the feature needs to be disabled, this might
    // be all that is required.
    //
    RtlZeroMemory(report,
                  moduleContext->HidCaps.FeatureReportByteLength);

    ntStatus = HidP_InitializeReportForID(HidP_Feature,
                                          (UCHAR)FeatureId,
                                          preparsedData,
                                          report,
                                          moduleContext->HidCaps.FeatureReportByteLength);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "HidP_InitializeReportForID fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    ntStatus = DMF_ContinuousRequestTarget_SendSynchronously(moduleContext->DmfModuleContinuousRequestTarget,
                                                             NULL,
                                                             0,
                                                             report,
                                                             moduleContext->HidCaps.FeatureReportByteLength,
                                                             ContinuousRequestTarget_RequestType_Ioctl,
                                                             IOCTL_HID_GET_FEATURE,
                                                             0,
                                                             NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_SendSynchronously fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (OffsetOfDataToCopy + NumberOfBytesToCopy > moduleContext->HidCaps.FeatureReportByteLength)
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_BUFFER_OVERFLOW;
        goto Exit;
    }

    // Copy the data from the retrieved feature report to the caller's buffer.
    //
    RtlCopyMemory(Buffer,
                  &report[OffsetOfDataToCopy],
                  NumberOfBytesToCopy);

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    if (reportMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(reportMemory);
        reportMemory = WDF_NO_HANDLE;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_FeatureSet(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR FeatureId,
    _In_ UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _In_ ULONG OffsetOfDataToCopy,
    _In_ ULONG NumberOfBytesToCopy
    )
/*++

Routine Description:

    Sends a Set Feature request to underlying HID device.

Arguments:

    DmfModule - This Module's handle.
    FeatureId - Feature Id to call Set Feature on
    Buffer - Source buffer for the data write
    BufferSize - Size of Buffer in bytes
    OffsetOfDataToCopy - Offset of data from Feature Report buffer to write to
    NumberOfBytesToCopy - Number of bytes to copy in Feature Report Buffer

Return Value:

    NTSTATUS

--*/
{
    PHIDP_PREPARSED_DATA preparsedData;
    CHAR* report;
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;
    WDFMEMORY reportMemory;
    WDF_OBJECT_ATTRIBUTES attributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 HidTarget);

    preparsedData = NULL;
    report = NULL;
    reportMemory = WDF_NO_HANDLE;

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (NumberOfBytesToCopy > BufferSize)
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Insufficient Buffer Length ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    preparsedData = (PHIDP_PREPARSED_DATA)WdfMemoryGetBuffer(moduleContext->PreparsedDataMemory,
                                                             NULL);

    if (OffsetOfDataToCopy + NumberOfBytesToCopy > moduleContext->HidCaps.FeatureReportByteLength)
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_BUFFER_OVERFLOW;
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&attributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               moduleContext->HidCaps.FeatureReportByteLength,
                               &reportMemory,
                               (VOID**)&report);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate for report fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Start with a zeroed report.
    //
    RtlZeroMemory(report,
                  moduleContext->HidCaps.FeatureReportByteLength);

    ntStatus = HidP_InitializeReportForID(HidP_Feature,
                                          (UCHAR)FeatureId,
                                          preparsedData,
                                          report,
                                          moduleContext->HidCaps.FeatureReportByteLength);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "HidP_InitializeReportForID fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // When the data to copy is partial, get the full feature report
    // so that the partial contents can be copied into it.
    //
    if (OffsetOfDataToCopy + NumberOfBytesToCopy < moduleContext->HidCaps.FeatureReportByteLength)
    {
        // Get the Feature report Buffer
        //
        ntStatus = DMF_ContinuousRequestTarget_SendSynchronously(moduleContext->DmfModuleContinuousRequestTarget,
                                                                 NULL,
                                                                 0,
                                                                 report,
                                                                 moduleContext->HidCaps.FeatureReportByteLength,
                                                                 ContinuousRequestTarget_RequestType_Ioctl,
                                                                 IOCTL_HID_GET_FEATURE,
                                                                 0,
                                                                 NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_SendSynchronously fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    // Copy the data from caller's buffer to the feature report.
    //
    RtlCopyMemory(&report[OffsetOfDataToCopy],
                  Buffer,
                  NumberOfBytesToCopy);

    ntStatus = DMF_ContinuousRequestTarget_SendSynchronously(moduleContext->DmfModuleContinuousRequestTarget,
                                                             report,
                                                             moduleContext->HidCaps.FeatureReportByteLength,
                                                             NULL,
                                                             0,
                                                             ContinuousRequestTarget_RequestType_Ioctl,
                                                             IOCTL_HID_SET_FEATURE,
                                                             0,
                                                             NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_SendSynchronously fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    if (reportMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(reportMemory);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_FeatureSetEx(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR FeatureId,
    _In_ UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _In_ ULONG OffsetOfDataToCopy,
    _In_ ULONG NumberOfBytesToCopy
    )
/*++

Routine Description:

    Sends a Set Feature request to underlying HID device.
    TODO: There are 3 categories of feature (button, value, and data) and each
          needs to be processed to find the correct report id.

Arguments:

    DmfModule - This Module's handle.
    FeatureId - Feature Id to call Set Feature on
    Buffer - Source buffer for the data write
    BufferSize - Size of Buffer in bytes
    OffsetOfDataToCopy - Offset of data from Feature Report buffer to write to
    NumberOfBytesToCopy - Number of bytes to copy in Feature Report Buffer

Return Value:

    NTSTATUS

--*/
{
    PHIDP_PREPARSED_DATA preparsedData;
    CHAR* report;
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;
    WDFMEMORY reportMemory;
    WDF_OBJECT_ATTRIBUTES attributes;
    PHIDP_VALUE_CAPS valueCaps;
    WDFMEMORY memoryValueCaps;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 HidTarget);

    preparsedData = NULL;
    report = NULL;
    reportMemory = WDF_NO_HANDLE;
    memoryValueCaps = WDF_NO_HANDLE;

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (NumberOfBytesToCopy > BufferSize)
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Insufficient Buffer Length ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    preparsedData = (PHIDP_PREPARSED_DATA)WdfMemoryGetBuffer(moduleContext->PreparsedDataMemory,
                                                             NULL);

    if (moduleContext->HidCaps.NumberFeatureValueCaps == 0)
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid parameter! NumberFeatureValueCaps = 0 : ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Find the size of the hid report based on the Feature Id (aka report id).
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               PagedPool,
                               MemoryTag,
                               sizeof(HIDP_VALUE_CAPS) * moduleContext->HidCaps.NumberFeatureValueCaps,
                               &memoryValueCaps,
                               (PVOID*)&valueCaps);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceError(DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }
        
    USHORT capsCountFound = moduleContext->HidCaps.NumberFeatureValueCaps;
    ntStatus = HidP_GetValueCaps(HidP_Feature, 
                                 valueCaps, 
                                 &capsCountFound, 
                                 preparsedData);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceError(DMF_TRACE, "HidP_GetValueCaps fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }
    DmfAssert(capsCountFound <= moduleContext->HidCaps.NumberFeatureValueCaps);

    USHORT capIndex;
    UINT featureReportByteLength = 0;
    BOOLEAN topLevel = FALSE;

    for (capIndex = 0; capIndex < capsCountFound; capIndex++)
    {
        if (valueCaps[capIndex].ReportID == FeatureId)
        {
            if (capIndex == 0)
            {
                // TODO: Confirm we need to do this.
                //
                topLevel = TRUE;
            }
            // Add space for the Report Id.
            //
            featureReportByteLength = valueCaps[capIndex].ReportCount + 1;
            break;
        }
    }

    if (featureReportByteLength == 0)
    {
        TraceError(DMF_TRACE, "Unable to find FeatureId %d", FeatureId);
        goto Exit;
    }

    if (OffsetOfDataToCopy + NumberOfBytesToCopy > featureReportByteLength)
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_BUFFER_OVERFLOW;
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&attributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               featureReportByteLength,
                               &reportMemory,
                               (VOID**)&report);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate for report fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Start with a zeroed report.
    //
    RtlZeroMemory(report,
                  featureReportByteLength);

    if (topLevel)
    {
        ntStatus = HidP_InitializeReportForID(HidP_Feature,
                                              (UCHAR)FeatureId,
                                              preparsedData,
                                              report,
                                              featureReportByteLength);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "HidP_InitializeReportForID fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    // When the data to copy is partial, get the full feature report
    // so that the partial contents can be copied into it.
    //
    if (OffsetOfDataToCopy + NumberOfBytesToCopy < featureReportByteLength)
    {
        // Get the Feature report Buffer
        //
        ntStatus = DMF_ContinuousRequestTarget_SendSynchronously(moduleContext->DmfModuleContinuousRequestTarget,
                                                                 NULL,
                                                                 0,
                                                                 report,
                                                                 featureReportByteLength,
                                                                 ContinuousRequestTarget_RequestType_Ioctl,
                                                                 IOCTL_HID_GET_FEATURE,
                                                                 0,
                                                                 NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_SendSynchronously fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    // Copy the data from caller's buffer to the feature report.
    //
    RtlCopyMemory(&report[OffsetOfDataToCopy],
                  Buffer,
                  NumberOfBytesToCopy);

    ntStatus = DMF_ContinuousRequestTarget_SendSynchronously(moduleContext->DmfModuleContinuousRequestTarget,
                                                             report,
                                                             featureReportByteLength,
                                                             NULL,
                                                             0,
                                                             ContinuousRequestTarget_RequestType_Ioctl,
                                                             IOCTL_HID_SET_FEATURE,
                                                             0,
                                                             NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_SendSynchronously fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    if (memoryValueCaps != WDF_NO_HANDLE)
    {
        WdfObjectDelete(memoryValueCaps);
    }

    if (reportMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(reportMemory);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_InputRead(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Submits a single input report read request. This function retrieves a buffer and sends it to the target.
    The size of this buffer matches with the input report size the hid expects.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    PUCHAR buffer;
    DMF_CONTEXT_HidTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 HidTarget);

    buffer = NULL;

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Input read should not be pended if the input report size is zero.
    //
    if (moduleContext->HidCaps.InputReportByteLength == 0)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Cannot pend input report with InputReportByteLength of size %d", moduleContext->HidCaps.InputReportByteLength);
        DmfAssert(FALSE);
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

    ntStatus = DMF_BufferPool_Get(moduleContext->DmfModuleBufferPoolInputReport,
                                  (PVOID*)&buffer,
                                  NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_BufferPool_Get fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = DMF_ContinuousRequestTarget_Send(moduleContext->DmfModuleContinuousRequestTarget,
                                                NULL,
                                                0,
                                                buffer,
                                                moduleContext->HidCaps.InputReportByteLength,
                                                ContinuousRequestTarget_RequestType_Read,
                                                0,
                                                0,
                                                HidTarget_InputReadCompletionCallback,
                                                NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        DMF_BufferPool_Put(moduleContext->DmfModuleBufferPoolInputReport,
                           buffer);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_Send fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_HidTarget_InputReadCancel(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

   Cancels all pending input report read requests for Input Reports and waits for all requests to return.
   This function should only be used if DMF_HidTarget_InputReadEx() was used to pend the requests.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 HidTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus= %!STATUS!", ntStatus);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Stop streaming asynchronous requests and wait for request cancellation to complete.
    //
    DMF_ContinuousRequestTarget_StopAndWait(moduleContext->DmfModuleContinuousRequestTarget);

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_InputReadEx(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Submits a number of input report read requests. The number is determined by PendedInputReadRequestCount
    in the module config. The size of the request buffer matches with the input report size the hid expects.
    If this function is used, the read requests can be cancelled using Dmf_InputReadCancel.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 HidTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus= %!STATUS!", ntStatus);
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Input read should not be pended if the input report size is zero.
    //
    if (moduleContext->HidCaps.InputReportByteLength == 0)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Cannot pend input report with InputReportByteLength of size %d", moduleContext->HidCaps.InputReportByteLength);
        DmfAssert(FALSE);
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

    // Start streaming asynchronous requests.
    //
    ntStatus = DMF_ContinuousRequestTarget_Start(moduleContext->DmfModuleContinuousRequestTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_Start fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#if defined(DMF_USER_MODE)
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_InputReportGet(
    _In_ DMFMODULE DmfModule,
    _In_ WDFMEMORY InputReportMemory,
    _Out_ ULONG* InputReportLength
    )
/*++

Routine Description:

    Synchronously reads an Input Report.
    NOTE: This function is not normally used to read Input Reports. Use it only if 
          the underlying device is known to not asynchronously respond reliably. If 
          there is no data available within 5 seconds, this call will complete
          regardless whereas the normally used Methods (DMF_HidTarget_InputRead) and
          (DMF_HidTarget_InputReadEx) will continue to wait.

Arguments:

    DmfModule - This Module's handle.
    InputReportMemory - Retrieved data is written to this buffer.
    InputReportLength - Amount of data read from device.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;
    HANDLE ioTargetFileHandle;
    size_t bufferSize;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 HidTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    PVOID reportBuffer = WdfMemoryGetBuffer(InputReportMemory,
                                            &bufferSize);

    if (NULL == reportBuffer)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryGetBuffer fails");
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    // Let Client know buffer size.
    //
    *InputReportLength = moduleContext->HidCaps.InputReportByteLength;

    if (bufferSize < *InputReportLength)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "BufferSize too small bufferSize=%d expected=%d", 
                    (int)bufferSize, 
                    (int)moduleContext->HidCaps.InputReportByteLength);
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    // HidD APIs require the actual file handle, not the WDFIOTARGET.
    //
    ioTargetFileHandle = WdfIoTargetWdmGetTargetFileHandle(moduleContext->IoTarget);

    // Read input report from the device.
    //
    if (!HidD_GetInputReport(ioTargetFileHandle,
                             reportBuffer,
                             *InputReportLength))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "HidD_GetInputReport fails");
        ntStatus = STATUS_INTERNAL_ERROR;
        goto Exit;
    }

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()
#endif

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_OutputReportSet(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _In_ ULONG TimeoutMs
    )
/*++

Routine Description:

    Sends a Set Output Report request to underlying HID device.

Arguments:

    DmfModule - This Module's handle.
    Buffer - Source buffer for the data write.
    BufferSize - Size of Buffer in bytes.
    TimeoutMs - Timeout value in milliseconds.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 HidTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_ContinuousRequestTarget_SendSynchronously(moduleContext->DmfModuleContinuousRequestTarget,
                                                             Buffer,
                                                             BufferSize,
                                                             NULL,
                                                             0,
                                                             ContinuousRequestTarget_RequestType_Ioctl,
                                                             IOCTL_HID_SET_OUTPUT_REPORT,
                                                             TimeoutMs,
                                                             NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_SendSynchronously fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_PreparsedDataGet(
    _In_ DMFMODULE DmfModule,
    _Out_ PHIDP_PREPARSED_DATA* PreparsedData
    )
/*++

Routine Description:

    Returns the preparsed data associated with the top level collection for the underlying HID device.

Arguments:

    DmfModule - This Module's handle.
    PreparsedData - Pointer to return the Preparsed data

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;
    PHIDP_PREPARSED_DATA preparsedDataLocal;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 HidTarget);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto ExitNoRelease;
    }

    DmfAssert(PreparsedData != NULL);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    if (moduleContext->PreparsedDataMemory == WDF_NO_HANDLE)
    {
        ntStatus = STATUS_INVALID_DEVICE_STATE;
        DMF_ModuleUnlock(DmfModule);
        goto Exit;
    }

    // Preparsed data is an opaque structure for the client.
    // HidP_* method takes these are argument.
    //
    // NOTE:
    // When the hid device is departed, this PreparsedDataMemory in the context gets freed. 
    // Returning a pointer here means client may still have a pointer even when the device had departed.
    // Hid class HidP_* methods would return HIDP_STATUS_INVALID_PREPARSED_DATA if the client used them 
    // after the hid is departed.
    //
    preparsedDataLocal = (PHIDP_PREPARSED_DATA)WdfMemoryGetBuffer(moduleContext->PreparsedDataMemory,
                                                                  NULL);
    *PreparsedData = preparsedDataLocal;

    DMF_ModuleUnlock(DmfModule);

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_ReportCreate(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG ReportType,
    _In_ UCHAR ReportId,
    _Out_ WDFMEMORY* ReportMemory
    )
/*++

Routine Description:

    Creates a memory for the given report type.

Arguments:

    DmfModule - This Module's handle.
    ReportType - Type of report.
    ReportId - Report Id.
    ReportMemory - ReportMemory created by this function.

Return Value:

    NTSTATUS

--*/
{
    HID_COLLECTION_INFORMATION hidCollectionInformation = { 0 };
    PHIDP_PREPARSED_DATA preparsedData;
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;
    WDFDEVICE device;
    WDFMEMORY preparsedDataMemory;
    WDFMEMORY reportMemoryLocal;
    WDF_OBJECT_ATTRIBUTES attributes;
    CHAR* report;
    USHORT reportLength;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 HidTarget);

    preparsedData = NULL;
    reportLength = 0;
    preparsedDataMemory = WDF_NO_HANDLE;
    reportMemoryLocal = WDF_NO_HANDLE;

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleReference fails: ntStatus=%!STATUS!", ntStatus);
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    preparsedData = (PHIDP_PREPARSED_DATA)WdfMemoryGetBuffer(moduleContext->PreparsedDataMemory,
                                                             NULL);

    switch (ReportType)
    {
    case HidP_Feature:
        reportLength = moduleContext->HidCaps.FeatureReportByteLength;
        break;
    case HidP_Input:
        reportLength = moduleContext->HidCaps.InputReportByteLength;
        break;
    case HidP_Output:
        reportLength = moduleContext->HidCaps.OutputReportByteLength;
        break;
    default:
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid report type: %d", ReportType);
        goto Exit;
    }

    // Create a report to send to the device.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&attributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               reportLength,
                               &reportMemoryLocal,
                               (VOID**)&report);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate for report fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Start with a zeroed report. If the feature needs to be disabled, this might
    // be all that is required.
    //
    RtlZeroMemory(report,
                  reportLength);

    ntStatus = HidP_InitializeReportForID((HIDP_REPORT_TYPE)ReportType,
                                          ReportId,
                                          preparsedData,
                                          report,
                                          reportLength);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "HidP_InitializeReportForID fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    *ReportMemory = reportMemoryLocal;

    // Client owns the memory.
    //
    reportMemoryLocal = WDF_NO_HANDLE;

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    // Clean up the memory if Module owns it.
    //
    if (reportMemoryLocal != WDF_NO_HANDLE)
    {
        WdfObjectDelete(reportMemoryLocal);
        reportMemoryLocal = WDF_NO_HANDLE;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()


// eof: Dmf_HidTarget.c
//
