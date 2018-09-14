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
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_HidTarget.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#include <initguid.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
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

// {55F3D844-8F9E-4EBD-AE33-EB778524CEEF}
DEFINE_GUID(GUID_CUSTOM_DEVINTERFACE, 0x55f3d844, 0x8f9e, 0x4ebd, 0xae, 0x33, 0xeb, 0x77, 0x85, 0x24, 0xce, 0xef);

EVT_WDF_REQUEST_COMPLETION_ROUTINE HidTarget_ReadCompletionRoutine;

_Use_decl_annotations_
VOID
HidTarget_ReadCompletionRoutine(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context
    )
/*++

Routine Description:

    Called when the read request completes.

Arguments:

    Request - The completed read request
    Target - IO target
    Params - Request completion parameters
    Context - Request context

Return Value:

    VOID

--*/
{
    UCHAR* buffer;
    size_t length;
    DMF_CONTEXT_HidTarget* moduleContext;
    DMFMODULE dmfModule;

    UNREFERENCED_PARAMETER(Target);

    buffer = NULL;
    length = 0;
    dmfModule = DMFMODULEVOID_TO_MODULE(Context);
    ASSERT(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    if (! NT_SUCCESS(Params->IoStatus.Status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "ReadCompletionRoutine fails: ntStatus=%!STATUS!",
                    Params->IoStatus.Status);
    }
    else if (Params->Type == WdfRequestTypeRead)
    {
        buffer = (UCHAR*)WdfMemoryGetBuffer(Params->Parameters.Read.Buffer,
                                            NULL);

        length = Params->Parameters.Read.Length;

        moduleContext->EvtHidInputReport(dmfModule,
                                         buffer,
                                         (ULONG)length);
    }

    if (Request != NULL)
    {
        WdfObjectDelete(Request);
    }
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

    FuncEntry(DMF_TRACE_HidTarget);

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
                    DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
                    "WdfIoTargetOpen fails: ntStatus=%!STATUS!",
                    ntStatus);
        WdfObjectDelete(resultIoTarget);
        goto Exit;
    }

    *IoTarget = resultIoTarget;

Exit:

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
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

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE_HidTarget);

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
                    DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
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

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE_HidTarget);

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

    FuncExitVoid(DMF_TRACE_HidTarget);
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

    FuncEntry(DMF_TRACE_HidTarget);

    isFound = FALSE;

    for (pidIndex = 0; pidIndex < PidListArraySize; pidIndex++)
    {
        if (LookForPid == PidList[pidIndex])
        {
            isFound = TRUE;
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        DMF_TRACE_HidTarget,
                        "found supported PID: 0x%x",
                        LookForPid);
            break;
        }
    }

    FuncExit(DMF_TRACE_HidTarget, "isFound=%d", isFound);

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
    _Out_ BOOLEAN* IsDeviceMatched)
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


    FuncEntry(DMF_TRACE_HidTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_AttachedDeviceGet(DmfModule);

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
                    DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
                    "IOCTL_Hid_GET_COLLECTION_INFORMATION fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    if (0 == hidCollectionInformation.DescriptorSize)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "hidCollectionInformation.DescriptorSize==0, ntStatus=%!STATUS!",
                    ntStatus);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE_HidTarget,
                "IOCTL_Hid_GET_COLLECTION_INFORMATION returned VID = 0x%x",
                hidCollectionInformation.VendorID);

    // Check VID/PID 
    //
    if (hidCollectionInformation.VendorID != moduleConfig->VendorId)
    {
        TraceEvents(TRACE_LEVEL_WARNING,
                    DMF_TRACE_HidTarget,
                    "IOCTL_Hid_GET_COLLECTION_INFORMATION unsupported VID");
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
                    "IOCTL_Hid_GET_COLLECTION_DESCRIPTOR fails: %!STATUS!",
                    ntStatus);
        goto Exit;
    }

    ntStatus = HidP_GetCaps(preparsedHidData,
                            &hidCaps);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "HidP_GetCaps() fails: %!STATUS!",
                    ntStatus);
        goto Exit;
    }

    // Check the usage and usage page.
    //
    if ((hidCaps.Usage != moduleConfig->VendorUsage) ||
        (hidCaps.UsagePage != moduleConfig->VendorUsagePage))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
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

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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
    _Out_ BOOLEAN* IsDeviceMatched)
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

    FuncEntry(DMF_TRACE_HidTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Look for the custom symbolic link that was created for the device specified by client.
    // Match up reported symbolic link against the saved 'SymbolicLinkToSearch'.
    //
    SIZE_T matchLength;

    ntStatus = STATUS_SUCCESS;
    *IsDeviceMatched = FALSE;

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE_HidTarget,
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

    ASSERT(symbolicLinkNameToSearchSavedBuffer != NULL);
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
                DMF_TRACE_HidTarget,
                "Found a matching local device");

    *IsDeviceMatched = TRUE;

Exit:

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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

    ASSERT(DevicePath != NULL);
    ASSERT(IsTopLevelCollection != NULL);

    FuncEntry(DMF_TRACE_HidTarget);

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

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "HidTarget_MatchCheck fails: %!STATUS!",
                    ntStatus);
        goto Exit;
    }

    *IsTopLevelCollection = matchedDeviceFound;

Exit:

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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

    device = DMF_AttachedDeviceGet(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    isTopLevelCollection = FALSE;
    ntStatus = HidTarget_IsAccessoryTopLevelCollection(DmfModule,
                                                       SymbolicLinkName,
                                                       &isTopLevelCollection);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "HidTarget_IsAccessoryTopLevelCollection fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    if (! isTopLevelCollection)
    {
        // It is not the device the Client Driver is looking for.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE_HidTarget,
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
            ASSERT(FALSE);
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE_HidTarget,
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
                        DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
                    "Symbolic link was already initialized");
        ASSERT(FALSE);
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
                        DMF_TRACE_HidTarget,
                        "WdfIoTargetCreate fails: ntStatus=%!STATUS!",
                        ntStatus);
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE_HidTarget,
                    "Created IOTarget for target HID device");

        // Cache the Hid Properties for this target.
        //
        ntStatus = HidTarget_DeviceProperyGet(DmfModule);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE_HidTarget,
                        "HidTarget_DeviceProperyGet fails: ntStatus=%!STATUS!",
                        ntStatus);

            TraceEvents(TRACE_LEVEL_INFORMATION,
                        DMF_TRACE_HidTarget,
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
                        DMF_TRACE_HidTarget,
                        "Module Open Fails; Destroying IOTarget for target HID device,ntStatus=%!STATUS!",
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

    device = DMF_AttachedDeviceGet(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ASSERT(SymbolicLinkName);
    targetMatched = FALSE;

    DMF_ModuleLock(DmfModule);

    if (WDF_NO_HANDLE == moduleContext->SymbolicLinkNameMemory)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
                    "Length test fails");
        goto Exit;
    }

    ASSERT(symbolicLinkNameSaved != NULL);

    matchLength = RtlCompareMemory((VOID*)symbolicLinkNameSaved,
                                   SymbolicLinkName->Buffer,
                                   SymbolicLinkName->Length);
    if (SymbolicLinkName->Length != matchLength)
    {
        // This code path is valid on unplug as several devices not associated with this instance
        // may disappear.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
                    "Removing HID device from notification function");

        // Call the DMF Module Client specific code.
        //
        if (moduleContext->IoTarget != NULL)
        {
            DMF_ModuleClose(DmfModule);
        }
    }

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", STATUS_SUCCESS);

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

    FuncEntry(DMF_TRACE_HidTarget);

    // warning C6387: 'Context' could be '0'.
    //
    #pragma warning(suppress:6387)
    dmfModule = DMFMODULEVOID_TO_MODULE(Context);
    ASSERT(dmfModule != NULL);

    info = (PDEVICE_INTERFACE_CHANGE_NOTIFICATION)NotificationStructure;
    ntStatus = STATUS_SUCCESS;

    if (DMF_Utility_IsEqualGUID((LPGUID)&(info->Event),
                                (LPGUID)&GUID_DEVICE_INTERFACE_ARRIVAL))
    {
        ASSERT(info->SymbolicLinkName);

        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE_HidTarget,
                    "GUID_DEVICE_INTERFACE_ARRIVAL Found HID Device...Query state collection");

        ntStatus = HidTarget_MatchedTargetGet(dmfModule,
                                              info->SymbolicLinkName);
    }
    else if (DMF_Utility_IsEqualGUID((LPGUID)&(info->Event),
                                     (LPGUID)&GUID_DEVICE_INTERFACE_REMOVAL))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE_HidTarget,
                    "GUID_DEVICE_INTERFACE_REMOVAL");

        ntStatus = HidTarget_MatchedTargetDestroy(dmfModule,
                                                  info->SymbolicLinkName);
    }

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE_HidTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    parentDevice = DMF_AttachedDeviceGet(DmfModule);
    ASSERT(parentDevice != NULL);

    deviceObject = WdfDeviceWdmGetDeviceObject(parentDevice);
    ASSERT(deviceObject != NULL);
    driverObject = deviceObject->DriverObject;

    ASSERT(NULL == moduleContext->HidInterfaceNotification);
    ntStatus = IoRegisterPlugPlayNotification(EventCategoryDeviceInterfaceChange,
                                              PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
                                              (void*)InterfaceGuid,
                                              driverObject,
                                              (PDRIVER_NOTIFICATION_CALLBACK_ROUTINE)HidTarget_InterfaceArrivalCallbackForLocalOrRemoteKernel,
                                              (VOID*)DmfModule,
                                              &(moduleContext->HidInterfaceNotification));

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE_HidTarget,
                "IoRegisterPlugPlayNotification: Notification Entry 0x%p ntStatus = %!STATUS!",
                moduleContext->HidInterfaceNotification, ntStatus);

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE_HidTarget);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Create a custom interface and symbolic link for the device specified by client.
    // Newly created symbolic link is saved for lookup at arrival callback.
    //
    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE_HidTarget,
                "Creating Custom Interface for target HID device 0x%p",
                moduleConfig->HidTargetToConnect);

    interfaceGuid = &GUID_CUSTOM_DEVINTERFACE;
    ntStatus = HidTarget_InterfaceCreateForLocal(DmfModule,
                                                 interfaceGuid,
                                                 moduleConfig->HidTargetToConnect);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "HidTarget_CreateInterfaceForDevice fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    ntStatus = HidTarget_NotificationRegisterForLocalOrRemoteKernel(DmfModule,
                                                                    interfaceGuid);

Exit:

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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
                    DMF_TRACE_HidTarget,
                    "Destroy Notification Entry 0x%p",
                    moduleContext->HidInterfaceNotification);

        ntStatus = IoUnregisterPlugPlayNotificationEx(moduleContext->HidInterfaceNotification);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        DMF_TRACE_HidTarget,
                        "IoUnregisterPlugPlayNotificationEx() fails: ntStatus=%!STATUS!",
                        ntStatus);
            ASSERT(FALSE);
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
                    DMF_TRACE_HidTarget,
                    "IoUnregisterPlugPlayNotificationEx() skipped.");
    }

Exit:

    FuncExitVoid(DMF_TRACE_HidTarget);
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
        ASSERT(EventData->u.DeviceInterface.SymbolicLink);
        UNICODE_STRING symbolicLinkName;

        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE_HidTarget,
                    "Processing interface arrival %ws",
                    EventData->u.DeviceInterface.SymbolicLink);
        RtlInitUnicodeString(&symbolicLinkName,
                             EventData->u.DeviceInterface.SymbolicLink);

        ntStatus = HidTarget_MatchedTargetGet(dmfModule,
                                              &symbolicLinkName);
    }
    else if (Action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL)
    {
        ASSERT(EventData->u.DeviceInterface.SymbolicLink);
        UNICODE_STRING symbolicLinkName;

        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE_HidTarget,
                    "Processing interface removal %ws",
                    EventData->u.DeviceInterface.SymbolicLink);
        RtlInitUnicodeString(&symbolicLinkName,
                             EventData->u.DeviceInterface.SymbolicLink);

        ntStatus = HidTarget_MatchedTargetDestroy(dmfModule,
                                                  &symbolicLinkName);
    }

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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
                        DMF_TRACE_HidTarget,
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
                            DMF_TRACE_HidTarget,
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
                        DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
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

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE_HidTarget);

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
                    DMF_TRACE_HidTarget,
                    "Processing existing interfaces- START");

        ntStatus = HidTarget_MatchedTargetForExistingInterfacesGet(DmfModule,
                                                             interfaceGuid);

        TraceEvents(TRACE_LEVEL_VERBOSE,
                    DMF_TRACE_HidTarget,
                    "Processing existing interfaces- END");

        // Should always return success here, since notification might be called back later for the desired device.
        //
        ntStatus = STATUS_SUCCESS;
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "CM_Register_Notification fails: configRet=0x%x",
                    configRet);

        ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE_HidTarget,
                "Created Notification Entry 0x%p",
                moduleContext->HidInterfaceNotification);

Exit:

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE_HidTarget);

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
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "WdfIoTargetCreate fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    WDF_IO_TARGET_OPEN_PARAMS openParams;
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_FILE(&openParams,
                                                NULL);
    ntStatus = WdfIoTargetOpen(target,
                               &openParams);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "WdfIoTargetOpen fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    DMF_ModuleLock(DmfModule);
    lockHeld = TRUE;

    ASSERT(moduleContext->IoTarget == NULL);

    moduleContext->IoTarget = target;
    moduleContext->EvtHidInputReport = moduleConfig->EvtHidInputReport;

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE_HidTarget,
                "Created IOTarget for downlevel stack");

    // Cache the Hid Properties for this target.
    //
    ntStatus = HidTarget_DeviceProperyGet(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "HidTarget_DeviceProperyGet fails: ntStatus=%!STATUS!",
                    ntStatus);

        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE_HidTarget,
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
                    DMF_TRACE_HidTarget,
                    "Module Open Fails; Destroying IOTarget for target HID device,ntStatus=%!STATUS!",
                    ntStatus);

        HidTarget_IoTargetDestroy(moduleContext);
    }

Exit:

    if (lockHeld)
    {
        DMF_ModuleUnlock(DmfModule);
    }

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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
                        DMF_TRACE_HidTarget,
                        "Destroy Notification Entry 0x%p",
                        moduleContext->HidInterfaceNotification);

            CONFIGRET cr;
            cr = CM_Unregister_Notification(moduleContext->HidInterfaceNotification);
            if (cr != CR_SUCCESS)
            {
                NTSTATUS ntStatus;
                ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE_HidTarget,
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
                        DMF_TRACE_HidTarget,
                        "CM_Unregister_Notification skipped.");
        }
    }

Exit:

    FuncExitVoid(DMF_TRACE_HidTarget);
}
#pragma code_seg()

#endif // !defined(DMF_USER_MODE)

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

    FuncEntry(DMF_TRACE_HidTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // The Notification should not be enabled at this time. It should have been unregistered.
    //
    ASSERT(NULL == moduleContext->HidInterfaceNotification);
    DMF_ModuleDestroy(DmfModule);
    DmfModule = NULL;

    FuncExitVoid(DMF_TRACE_HidTarget);
}
#pragma code_seg()

#pragma code_seg("PAGE")
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

    FuncEntry(DMF_TRACE_HidTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // This function should not be not called twice.
    //
    ASSERT(NULL == moduleContext->HidInterfaceNotification);

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

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
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
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_HidTarget_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Hid.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_HidTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_ClientCallbackOpen(DmfModule);

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_HidTarget_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Hid.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_HidTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_HidTarget);

    DMF_ClientCallbackClose(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Close the associated target.
    //
    HidTarget_IoTargetDestroy(moduleContext);

    FuncExitVoid(DMF_TRACE_HidTarget);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_Hid;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_HidTarget;

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

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_HidTarget);
    DmfCallbacksDmf_HidTarget.ModuleInstanceDestroy = DMF_HidTarget_Destroy;
    DmfCallbacksDmf_HidTarget.DeviceOpen = DMF_HidTarget_Open;
    DmfCallbacksDmf_HidTarget.DeviceClose = DMF_HidTarget_Close;
    DmfCallbacksDmf_HidTarget.DeviceNotificationRegister = DMF_HidTarget_NotificationRegister;
    DmfCallbacksDmf_HidTarget.DeviceNotificationUnregister = DMF_HidTarget_NotificationUnregister;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_Hid,
                                            HidTarget,
                                            DMF_CONTEXT_HidTarget,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware);

    DmfModuleDescriptor_Hid.CallbacksDmf = &DmfCallbacksDmf_HidTarget;
    DmfModuleDescriptor_Hid.ModuleConfigSize = sizeof(DMF_CONFIG_HidTarget);

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_Hid,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "DMF_ModuleCreate fails: ntStatus=%!STATUS!",
                    ntStatus);
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
    WDF_MEMORY_DESCRIPTOR memoryDescriptor;
    WDF_REQUEST_SEND_OPTIONS options;
    DMF_CONTEXT_HidTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_HidTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_Hid);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "DMF_ModuleReference");
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memoryDescriptor,
                                      Buffer,
                                      BufferLength);

    WDF_REQUEST_SEND_OPTIONS_INIT(&options,
                                  WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
    if (TimeoutMs > 0)
    {
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,
                                             WDF_REL_TIMEOUT_IN_MS(TimeoutMs));
    }

    ntStatus = WdfIoTargetSendReadSynchronously(moduleContext->IoTarget,
                                                NULL,
                                                &memoryDescriptor,
                                                NULL,
                                                &options,
                                                NULL);

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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
    WDF_MEMORY_DESCRIPTOR memoryDescriptor;
    WDF_REQUEST_SEND_OPTIONS options;
    DMF_CONTEXT_HidTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_HidTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_Hid);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "DMF_ModuleReference");
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memoryDescriptor,
                                      Buffer,
                                      BufferLength);
    WDF_REQUEST_SEND_OPTIONS_INIT(&options,
                                  WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
    if (TimeoutMs > 0)
    {
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,
                                             WDF_REL_TIMEOUT_IN_MS(TimeoutMs));
    }

    ntStatus = WdfIoTargetSendWriteSynchronously(moduleContext->IoTarget,
                                                 NULL,
                                                 &memoryDescriptor,
                                                 NULL,
                                                 &options,
                                                 NULL);

    DMF_ModuleDereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_FeatureGet(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR FeatureId,
    _In_ UCHAR* Buffer,
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
    WDF_MEMORY_DESCRIPTOR outputDescriptor;
    PHIDP_PREPARSED_DATA preparsedData;
    CHAR* report;
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;
    WDFDEVICE device;
    WDFMEMORY reportMemory;
    WDF_OBJECT_ATTRIBUTES attributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_HidTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_Hid);

    preparsedData = NULL;
    report = NULL;
    reportMemory = WDF_NO_HANDLE;

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "DMF_ModuleReference");
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_AttachedDeviceGet(DmfModule);

    if (NumberOfBytesToCopy > BufferSize)
    {
        ASSERT(FALSE);
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "WdfMemoryCreate for report fails: ntStatus=%!STATUS!", ntStatus);
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
                    DMF_TRACE_HidTarget,
                    "HidP_InitializeReportForID fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor,
                                      report,
                                      moduleContext->HidCaps.FeatureReportByteLength);
    ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                 NULL,
                                                 IOCTL_HID_GET_FEATURE,
                                                 NULL,
                                                 &outputDescriptor,
                                                 NULL,
                                                 NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "WdfIoTargetSendIoctlSynchronously fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    if (OffsetOfDataToCopy + NumberOfBytesToCopy > moduleContext->HidCaps.FeatureReportByteLength)
    {
        ASSERT(FALSE);
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

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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
    WDF_MEMORY_DESCRIPTOR outputDescriptor;
    PHIDP_PREPARSED_DATA preparsedData;
    CHAR* report;
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;
    WDFDEVICE device;
    WDFMEMORY reportMemory;
    WDF_OBJECT_ATTRIBUTES attributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_HidTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_Hid);

    preparsedData = NULL;
    report = NULL;
    reportMemory = WDF_NO_HANDLE;

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "DMF_ModuleReference");
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_AttachedDeviceGet(DmfModule);

    if (NumberOfBytesToCopy > BufferSize)
    {
        ASSERT(FALSE);
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "Insufficient Buffer Length ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    preparsedData = (PHIDP_PREPARSED_DATA)WdfMemoryGetBuffer(moduleContext->PreparsedDataMemory,
                                                             NULL);

    if (OffsetOfDataToCopy + NumberOfBytesToCopy > moduleContext->HidCaps.FeatureReportByteLength)
    {
        ASSERT(FALSE);
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "WdfMemoryCreate for report fails: ntStatus=%!STATUS!", ntStatus);
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "HidP_InitializeReportForID ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor,
                                      report,
                                      moduleContext->HidCaps.FeatureReportByteLength);

    // When the data to copy is partial, get the full feature report
    // so that the partial contents can be copied into it.
    //
    if (OffsetOfDataToCopy + NumberOfBytesToCopy < moduleContext->HidCaps.FeatureReportByteLength)
    {
        // Get the Feature report Buffer
        //
        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_HID_GET_FEATURE,
                                                     NULL,
                                                     &outputDescriptor,
                                                     NULL,
                                                     NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "WdfIoTargetSendIoctlSynchronously ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    // Copy the data from caller's buffer to the feature report.
    //
    RtlCopyMemory(&report[OffsetOfDataToCopy],
                  Buffer,
                  NumberOfBytesToCopy);

    ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                 NULL,
                                                 IOCTL_HID_SET_FEATURE,
                                                 &outputDescriptor,
                                                 NULL,
                                                 NULL,
                                                 NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "WdfIoTargetSendIoctlSynchronously ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    if (reportMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(reportMemory);
        reportMemory = WDF_NO_HANDLE;
    }

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidTarget_InputRead(
    _In_ DMFMODULE DmfModule,
    _In_ USHORT ReportLength
    )
/*++

Routine Description:

    Submits an input report read request

Arguments:

    DmfModule - This Module's handle.
    ReportLength - Expected length of input report

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidTarget* moduleContext;
    WDFREQUEST  request;
    WDFMEMORY  memory;
    WDF_OBJECT_ATTRIBUTES attributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_HidTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_Hid);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "DMF_ModuleReference");
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    ntStatus = WdfRequestCreate(&attributes,
                                moduleContext->IoTarget,
                                &request);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "WdfRequestCreate ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = request;
    ntStatus = WdfMemoryCreate(&attributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               ReportLength,
                               &memory,
                               NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "WdfMemoryCreate ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Format and send the request.
    //
    ntStatus = WdfIoTargetFormatRequestForRead(moduleContext->IoTarget,
                                               request,
                                               memory,
                                               NULL,
                                               NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "WdfIoTargetFormatRequestForRead ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WdfRequestSetCompletionRoutine(request,
                                   HidTarget_ReadCompletionRoutine,
                                   DmfModule);

    if (! WdfRequestSend(request,
                         moduleContext->IoTarget,
                         NULL))
    {
        ntStatus = WdfRequestGetStatus(request);
        if (NT_SUCCESS(ntStatus))
        {
            ntStatus = STATUS_INVALID_DEVICE_STATE;
        }

        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "WdfRequestSend fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

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
    WDF_MEMORY_DESCRIPTOR outputDescriptor;
    WDF_REQUEST_SEND_OPTIONS options;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_HidTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_Hid);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "DMF_ModuleReference");
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor,
                                      Buffer,
                                      BufferSize);

    WDF_REQUEST_SEND_OPTIONS_INIT(&options,
                                  WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
    if (TimeoutMs > 0)
    {
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,
                                             WDF_REL_TIMEOUT_IN_MS(TimeoutMs));
    }

    ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                 NULL,
                                                 IOCTL_HID_SET_OUTPUT_REPORT,
                                                 &outputDescriptor,
                                                 NULL,
                                                 &options,
                                                 NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "WdfIoTargetSendIoctlSynchronously fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE_HidTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_Hid);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE_HidTarget,
                    "DMF_ModuleReference");
        goto ExitNoRelease;
    }

    ASSERT(PreparsedData != NULL);
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

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

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

    FuncEntry(DMF_TRACE_HidTarget);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_Hid);

    preparsedData = NULL;
    reportLength = 0;
    preparsedDataMemory = WDF_NO_HANDLE;
    reportMemoryLocal = WDF_NO_HANDLE;

    ntStatus = DMF_ModuleReference(DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "DMF_ModuleReference");
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_AttachedDeviceGet(DmfModule);

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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "Invalid report type: %d", ReportType);
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "WdfMemoryCreate for report fails: ntStatus=%!STATUS!", ntStatus);
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_HidTarget, "HidP_InitializeReportForID ntStatus=%!STATUS!", ntStatus);
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

    FuncExit(DMF_TRACE_HidTarget, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// eof: Dmf_HidTarget.c
//
