/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_HidDeviceListener.c

Abstract:

    HidDeviceListener notifies client of arrival and removal of HID devices specified in the Module's configuration.

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
#include "Dmf_HidDeviceListener.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_HidDeviceListener
{
    // HID Interface arrival/removal notification handle.
    //
#if defined(DMF_USER_MODE)
    HCMNOTIFICATION HidInterfaceNotification;
#else
    VOID* HidInterfaceNotification;
#endif // defined(DMF_USER_MODE)

    // Collection of symbolic link names of matched devices.
    // 
    // This collection is used when a HID device is removed. It removed device's name is checked against the collection
    // to identify if it was one of the devices that matches the configurations.This is used for remote targets since there
    // can be multiple devices matching the specified configuration.
    //
    WDFCOLLECTION MatchedDevicesSymbolicLinkNames;
} DMF_CONTEXT_HidDeviceListener;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(HidDeviceListener)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(HidDeviceListener)

// Memory Pool Tag.
//
#define MemoryTag 'LedH'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
HidDeviceListener_MatchedDevicesAdd(
    _In_ DMFMODULE DmfModule,
    _In_ PUNICODE_STRING SymbolicLinkName,
    _Out_ BOOLEAN* Added
    )
/*++

Routine Description:

    Add new symbolic link name to the collection of matched symbolic link names.

Arguments:

    DmfModule - This Module's handle.
    SymbolicLinkName - The symbolic link name that of a matched hid device.
    Added - Boolean indicating if the symbolic link name was added to the collection.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidDeviceListener* moduleContext;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFSTRING symbolicLinkNameString;
    WDFSTRING existingSymbolicLinkNameString;
    UNICODE_STRING existingSymbolicLinkName;
    LONG unicodeCompareResult;
    ULONG collectionCount;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ntStatus = STATUS_SUCCESS;
    *Added = FALSE;

    DMF_ModuleLock(DmfModule);

    // Check if the symbolic link name is already exists in collection.
    // 
    // This can happen in the usermode case where one needs to both regiser for device arrival as well as scan
    // for existing device. If a new device is added after the registration but before the scan existing device completes,
    // both paths will detect the new device and attempt to add it to the collection.
    //
    collectionCount = WdfCollectionGetCount(moduleContext->MatchedDevicesSymbolicLinkNames);
    for (ULONG collectionIndex = 0; collectionIndex < collectionCount; collectionIndex++)
    {
        existingSymbolicLinkNameString = (WDFSTRING)WdfCollectionGetItem(moduleContext->MatchedDevicesSymbolicLinkNames,
                                                                         collectionIndex);

        WdfStringGetUnicodeString(existingSymbolicLinkNameString,
                                  &existingSymbolicLinkName);

        unicodeCompareResult = RtlCompareUnicodeString(SymbolicLinkName,
                                                       &existingSymbolicLinkName,
                                                       TRUE);
        if (unicodeCompareResult == 0)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "%S already added", SymbolicLinkName->Buffer);
            goto Exit;
        }
    }

    // Add SymbolicLinkName to list of matching devices.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = moduleContext->MatchedDevicesSymbolicLinkNames;
    ntStatus = WdfStringCreate(SymbolicLinkName,
                               &attributes,
                               &symbolicLinkNameString);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfStringCreate fails: %!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = WdfCollectionAdd(moduleContext->MatchedDevicesSymbolicLinkNames,
                                symbolicLinkNameString);
    if (!NT_SUCCESS(ntStatus))
    {
        WdfObjectDelete(symbolicLinkNameString);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfCollectionAdd fails: %!STATUS!", ntStatus);
        goto Exit;
    }

    *Added = TRUE;

Exit:

    DMF_ModuleUnlock(DmfModule);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
HidDeviceListener_HandleHidDeviceArrival(
    _In_ DMFMODULE DmfModule,
    _In_ PUNICODE_STRING SymbolicLinkName
    )
/*++

Routine Description:

    Handles arrival of new hid device. Check to see if the device matches the specification in the Module's config.

Arguments:

    DmfModule - This Module's handle.
    SymbolicLinkName - The symbolic link name that of the hid device that arrived.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFIOTARGET ioTarget;
    WDFDEVICE device;
    DMF_CONTEXT_HidDeviceListener* moduleContext;
    DMF_CONFIG_HidDeviceListener* moduleConfig;
    HID_COLLECTION_INFORMATION hidCollectionInformation;
    PHIDP_PREPARSED_DATA preparsedHidData;
    WDFMEMORY preparsedHidDataMemory;
    HIDP_CAPS hidCaps;
    WDF_IO_TARGET_OPEN_PARAMS openParams;
    WDF_MEMORY_DESCRIPTOR outputDescriptor;
    WDF_OBJECT_ATTRIBUTES attributes;
    BOOLEAN pidFound;
    BOOLEAN symbolicLinkNameAdded;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    device = DMF_ParentDeviceGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    symbolicLinkNameAdded = FALSE;
    ioTarget = NULL;
    preparsedHidData = NULL;
    preparsedHidDataMemory = NULL;

    RtlZeroMemory(&hidCollectionInformation,
                  sizeof(hidCollectionInformation));

    // Open the device to be queried.
    // NOTE: When opening HID device for enumeration purposes (to see if
    // it is the required device, the Open Mode should be zero and share should be Read/Write.
    //
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams,
                                                SymbolicLinkName,
                                                0);

    openParams.ShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE;

    // Create an I/O target object.
    //
    ntStatus = WdfIoTargetCreate(device,
                                 WDF_NO_OBJECT_ATTRIBUTES,
                                 &ioTarget);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Try to open the target.
    //
    ntStatus = WdfIoTargetOpen(ioTarget,
                               &openParams);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetOpen fails: ntStatus=%!STATUS!", ntStatus);
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
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "IOCTL_HID_GET_COLLECTION_INFORMATION fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (0 == hidCollectionInformation.DescriptorSize)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "hidCollectionInformation.DescriptorSize==0, ntStatus=%!STATUS!", ntStatus);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "VID = 0x%x", hidCollectionInformation.VendorID);

    // Check VID/PID
    //
    if (hidCollectionInformation.VendorID != moduleConfig->VendorId)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Unsupported VID");
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "PID = 0x%x",  hidCollectionInformation.ProductID);

    // See if it is one of the PIDs that the Client wants.
    //
    if (moduleConfig->ProductIdsCount > 0)
    {
        pidFound = FALSE;

        for (ULONG pidIndex = 0; pidIndex < moduleConfig->ProductIdsCount; pidIndex++)
        {
            if (hidCollectionInformation.ProductID == moduleConfig->ProductIds[pidIndex])
            {
                pidFound = TRUE;
                TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "found supported PID: 0x%x", moduleConfig->ProductIds[pidIndex]);
                break;
            }
        }

        if (pidFound != TRUE)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Unsupported PID");
            goto Exit;
        }

    }

    // Get Hid Descriptor.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = device;
    ntStatus = WdfMemoryCreate(&attributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               hidCollectionInformation.DescriptorSize,
                               &preparsedHidDataMemory,
                               (VOID**)&preparsedHidData);
    if (!NT_SUCCESS(ntStatus))
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        preparsedHidDataMemory = NULL;
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
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "IOCTL_HID_GET_COLLECTION_DESCRIPTOR fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Get Hid Capabilities.
    //
    ntStatus = HidP_GetCaps(preparsedHidData,
                            &hidCaps);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "HidP_GetCaps() fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Check the usage and usage page.
    //
    if ((hidCaps.Usage != moduleConfig->Usage) ||
        (hidCaps.UsagePage != moduleConfig->UsagePage))
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "incorrect usage or usage page failed");
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Found matching device %S", SymbolicLinkName->Buffer);

    // Add symbolic link name to collection of matched devices.
    //
    ntStatus = HidDeviceListener_MatchedDevicesAdd(DmfModule,
                                                   SymbolicLinkName,
                                                   &symbolicLinkNameAdded);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "HidDeviceListener_MatchedDevicesAdd() fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (symbolicLinkNameAdded == TRUE
        && moduleConfig->EvtHidTargetDeviceArrivalCallback != NULL)
    {
        // Notify user of matching device arrival.
        //
        moduleConfig->EvtHidTargetDeviceArrivalCallback(DmfModule,
                                                        SymbolicLinkName,
                                                        ioTarget,
                                                        preparsedHidData,
                                                        &hidCollectionInformation);
    }
  
Exit:

    if (ioTarget != NULL)
    {
        WdfIoTargetClose(ioTarget);
        WdfObjectDelete(ioTarget);
    }

    if (preparsedHidDataMemory != NULL)
    {
        WdfObjectDelete(preparsedHidDataMemory);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
HidDeviceListener_HandleHidDeviceRemoval(
    _In_ DMFMODULE DmfModule,
    _In_ PUNICODE_STRING SymbolicLinkName
    )
/*++

Routine Description:

    Handles removal of new hid device. Check to see if the device matches the specification in the Module's config.

Arguments:

    DmfModule - This Module's handle.
    SymbolicLinkName - The symbolic link name of the hid device that is being removed.

Return Value:

    VOID

--*/
{
    DMF_CONTEXT_HidDeviceListener* moduleContext;
    DMF_CONFIG_HidDeviceListener* moduleConfig;
    ULONG collectionCount;
    WDFSTRING symbolicLinkNameString;
    UNICODE_STRING symbolicLinkName;
    LONG unicodeCompareResult;
    BOOLEAN symbolicNameFound;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    symbolicLinkNameString = NULL;
    symbolicNameFound = FALSE;

    DMF_ModuleLock(DmfModule);

    // Check if device being removed is in the matched device collection; this would mean that it is a matched device.
    //
    collectionCount = WdfCollectionGetCount(moduleContext->MatchedDevicesSymbolicLinkNames);
    for (ULONG collectionIndex = 0; collectionIndex < collectionCount; collectionIndex++)
    {
        symbolicLinkNameString = (WDFSTRING)WdfCollectionGetItem(moduleContext->MatchedDevicesSymbolicLinkNames,
                                                                 collectionIndex);

        WdfStringGetUnicodeString(symbolicLinkNameString,
                                  &symbolicLinkName);

        unicodeCompareResult = RtlCompareUnicodeString(SymbolicLinkName,
                                                       &symbolicLinkName,
                                                       TRUE);
        if (unicodeCompareResult == 0)
        {
            symbolicNameFound = TRUE;
            break;
        }
    }

    if (symbolicNameFound == TRUE)
    {
        WdfCollectionRemove(moduleContext->MatchedDevicesSymbolicLinkNames,
                            symbolicLinkNameString);
        WdfObjectDelete(symbolicLinkNameString);

        // Notify client of matching device removal.
        //
        if (moduleConfig->EvtHidTargetDeviceRemovalCallback != NULL)
        {
            moduleConfig->EvtHidTargetDeviceRemovalCallback(DmfModule,
                                                            SymbolicLinkName);
        }
    }

    DMF_ModuleUnlock(DmfModule);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#if !defined(DMF_USER_MODE)
#pragma code_seg("PAGE")
_Function_class_(DRIVER_NOTIFICATION_CALLBACK_ROUTINE)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
HidDeviceListener_InterfaceArrivalRemovalCallbackKernel(
    _In_ VOID* NotificationStructure,
    _Inout_opt_ VOID* Context
    )
/*++

Routine Description:

    PnP notification function that is called when a HID device arrives or removes.

Arguments:

    NotificationStructure - Gives information about the enumerated device.
    Context - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    PDEVICE_INTERFACE_CHANGE_NOTIFICATION info;
    DMFMODULE dmfModule;
    DMF_CONFIG_HidDeviceListener* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // warning C6387: 'Context' could be '0'.
    //
    #pragma warning(suppress:6387)
    dmfModule = DMFMODULEVOID_TO_MODULE(Context);
    DmfAssert(dmfModule != NULL);
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    info = (PDEVICE_INTERFACE_CHANGE_NOTIFICATION)NotificationStructure;
    ntStatus = STATUS_SUCCESS;

    if (DMF_Utility_IsEqualGUID((LPGUID)&(info->Event),
                                (LPGUID)&GUID_DEVICE_INTERFACE_ARRIVAL))
    {
        DmfAssert(info->SymbolicLinkName);

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "GUID_DEVICE_INTERFACE_ARRIVAL Found HID Device %S", info->SymbolicLinkName->Buffer);

        ntStatus = HidDeviceListener_HandleHidDeviceArrival(dmfModule,
                                                            info->SymbolicLinkName);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "HidDeviceListener_HandleHidDeviceArrival fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }
    else if (DMF_Utility_IsEqualGUID((LPGUID)&(info->Event),
                                     (LPGUID)&GUID_DEVICE_INTERFACE_REMOVAL))
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "GUID_DEVICE_INTERFACE_REMOVAL %S", info->SymbolicLinkName->Buffer);

        HidDeviceListener_HandleHidDeviceRemoval(dmfModule,
                                                 info->SymbolicLinkName);
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
HidDeviceListener_NotificationRegisterKernel(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Register for notification for all hid devices arrival/removal.

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
    DMF_CONTEXT_HidDeviceListener* moduleContext;

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
                                              (void*)&GUID_DEVINTERFACE_HID,
                                              driverObject,
                                              (PDRIVER_NOTIFICATION_CALLBACK_ROUTINE)HidDeviceListener_InterfaceArrivalRemovalCallbackKernel,
                                              (VOID*)DmfModule,
                                              &(moduleContext->HidInterfaceNotification));

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "IoRegisterPlugPlayNotification: Notification Entry 0x%p ntStatus=%!STATUS!", moduleContext->HidInterfaceNotification, ntStatus);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
HidDeviceListener_NotificationUnregisterKernel(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Unregister a notification for hid device arriave/removal.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidDeviceListener* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->HidInterfaceNotification != NULL)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Destroy Notification Entry 0x%p", moduleContext->HidInterfaceNotification);

        ntStatus = IoUnregisterPlugPlayNotificationEx(moduleContext->HidInterfaceNotification);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "IoUnregisterPlugPlayNotificationEx() fails: ntStatus=%!STATUS!", ntStatus);
            DmfAssert(FALSE);
            goto Exit;
        }

        moduleContext->HidInterfaceNotification = NULL;
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "IoUnregisterPlugPlayNotificationEx() skipped.");
        DmfAssert(FALSE);
    }

Exit:

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#else

#pragma code_seg("PAGE")
DWORD
HidDeviceListener_InterfaceArrivalCallbackUser(
    _In_ HCMNOTIFICATION Notify,
    _In_opt_ VOID* Context,
    _In_ CM_NOTIFY_ACTION Action,
    _In_reads_bytes_(EventDataSize) PCM_NOTIFY_EVENT_DATA EventData,
    _In_ DWORD EventDataSize
    )
/*++

Routine Description:

    Callback called when the notification that is registered detects an arrival or
    removal of a device interface of any Hid device.

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

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Processing interface arrival %ws", EventData->u.DeviceInterface.SymbolicLink);
        RtlInitUnicodeString(&symbolicLinkName,
                             EventData->u.DeviceInterface.SymbolicLink);

        ntStatus = HidDeviceListener_HandleHidDeviceArrival(dmfModule,
                                                            &symbolicLinkName);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "HidDeviceListener_HandleHidDeviceArrival fails: ntStatus=%!STATUS!", ntStatus);
        }
    }
    else if (Action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL)
    {
        DmfAssert(EventData->u.DeviceInterface.SymbolicLink);
        UNICODE_STRING symbolicLinkName;

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Processing interface removal %ws", EventData->u.DeviceInterface.SymbolicLink);
        RtlInitUnicodeString(&symbolicLinkName,
                             EventData->u.DeviceInterface.SymbolicLink);

        HidDeviceListener_HandleHidDeviceRemoval(dmfModule,
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
_Must_inspect_result_
NTSTATUS
HidDeviceListener_MatchedTargetForExistingInterfacesGet(
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
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Get_Device_Interface_List_Size failed with Result %d and lastError %!WINERROR!", cr, lastError);
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
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "HeapFree failed with lastError %!WINERROR!", lastError);
                ntStatus = NTSTATUS_FROM_WIN32(lastError);
                deviceInterfaceList = NULL;
                goto Exit;
            }
        }

        deviceInterfaceList = (PWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, deviceInterfaceListLength * sizeof(WCHAR));
        if (deviceInterfaceList == NULL)
        {
            lastError = GetLastError();
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "HeapAlloc failed with lastError %!WINERROR!", lastError);
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
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Get_Device_Interface_List failed with Result %d and lastError %!WINERROR!", cr, lastError);
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
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "[index %d] Processing interface %ws", index, currentInterface);

        RtlInitUnicodeString(&symbolicLinkName,
                             currentInterface);

        ntStatus = HidDeviceListener_HandleHidDeviceArrival(DmfModule,
                                                            &symbolicLinkName);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "HidDeviceListener_HandleHidDeviceArrival fails: ntStatus=%!STATUS! symbolicLinkName=%S", ntStatus, symbolicLinkName.Buffer);
            goto Exit;
        }

        index++;
    }

Exit:

    if (deviceInterfaceList != NULL)
    {
        if (!HeapFree(GetProcessHeap(),
                      0,
                      deviceInterfaceList))
        {
            // Not a critical error.
            //
            lastError = GetLastError();
            TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "HeapFree failed with lastError %!WINERROR!", lastError);
        }

        deviceInterfaceList = NULL;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
HidDeviceListener_NotificationRegisterUser(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Register for a notification for all Hid device interfaces.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidDeviceListener* moduleContext;
    DMF_CONFIG_HidDeviceListener* moduleConfig;
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
                                         (PCM_NOTIFY_CALLBACK)HidDeviceListener_InterfaceArrivalCallbackUser,
                                         &(moduleContext->HidInterfaceNotification));
    // Target device might already be there. So try now.
    //
    if (configRet == CR_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Processing existing interfaces- START");

        ntStatus = HidDeviceListener_MatchedTargetForExistingInterfacesGet(DmfModule,
                                                                           interfaceGuid);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "HidDeviceListener_MatchedTargetForExistingInterfacesGet fails: ntStatus=%!STATUS!", ntStatus);
            // Should always return success here, since notification might be called back later for the desired device.
            //
            ntStatus = STATUS_SUCCESS;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Processing existing interfaces- END");
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Register_Notification fails: configRet=0x%x", configRet);

        ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()


#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
HidDeviceListener_NotificationUnregisterUser(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Unregister for a notification for all Hid device interfaces.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_HidDeviceListener* moduleContext;
    DMF_CONFIG_HidDeviceListener* moduleConfig;

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->HidInterfaceNotification != NULL)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,  "Destroy Notification Entry 0x%p", moduleContext->HidInterfaceNotification);

        CONFIGRET cr;
        cr = CM_Unregister_Notification(moduleContext->HidInterfaceNotification);
        if (cr != CR_SUCCESS)
        {
            NTSTATUS ntStatus;
            ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Unregister_Notification fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        moduleContext->HidInterfaceNotification = NULL;
    }
    else
    {
        // Allow caller to unregister notification even if it has not been registered.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "CM_Unregister_Notification skipped.");
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

_Function_class_(DMF_ModuleSelfManagedIoCleanup)
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_HidDeviceListener_SelfManagedIoCleanup(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

   HidDeviceListener callback for ModuleSelfManagedIoCleanup.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

#if defined DMF_USER_MODE
    HidDeviceListener_NotificationUnregisterUser(DmfModule);
#else
    HidDeviceListener_NotificationUnregisterKernel(DmfModule);
#endif

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_Function_class_(DMF_ModuleSelfManagedIoInit)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_HidDeviceListener_SelfManagedIoInit(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

   HidDeviceListener callback for ModuleSelfManagedIoInit.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidDeviceListener* moduleContext;
    DMF_CONFIG_HidDeviceListener* moduleConfig;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // This function should not be not called twice.
    //
    DmfAssert(NULL == moduleContext->HidInterfaceNotification);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

#if defined DMF_USER_MODE
    ntStatus = HidDeviceListener_NotificationRegisterUser(DmfModule);
#else
    ntStatus = HidDeviceListener_NotificationRegisterKernel(DmfModule);
#endif
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "HidDeviceListener_NotificationRegister fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_HidDeviceListener_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type HidDeviceListener.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidDeviceListener* moduleContext;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleContext->MatchedDevicesSymbolicLinkNames = NULL;
    moduleContext->HidInterfaceNotification = NULL;

    // Create collection used to track the symbolic link names of all the devices that match the HID device described
    // in the Module's config when they arrive. This is used to check if a HID device being removed is one of the
    // matched devices that arrived earlier.
    //
    // Note: Collection does not need to be deleted in ModuleClose. This Module will be closed when it is being destroyed.
    //       Therefore, the Collection will also be cleaned up since it is parented to the Module.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;

    ntStatus = WdfCollectionCreate(&objectAttributes,
                                   &moduleContext->MatchedDevicesSymbolicLinkNames);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfCollectionCreate fails: ntStatus=%!STATUS!", ntStatus);
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
DMF_HidDeviceListener_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type HidDeviceListener.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

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
DMF_HidDeviceListener_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type HidDeviceListener.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_HidDeviceListener;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_HidDeviceListener;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_HidDeviceListener;
    DMF_CONFIG_HidDeviceListener* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_HidDeviceListener);
    dmfCallbacksDmf_HidDeviceListener.DeviceOpen = DMF_HidDeviceListener_Open;
    dmfCallbacksDmf_HidDeviceListener.DeviceClose = DMF_HidDeviceListener_Close;

    
    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_HidDeviceListener);
    dmfCallbacksWdf_HidDeviceListener.ModuleSelfManagedIoInit = DMF_HidDeviceListener_SelfManagedIoInit;
    dmfCallbacksWdf_HidDeviceListener.ModuleSelfManagedIoCleanup = DMF_HidDeviceListener_SelfManagedIoCleanup;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_HidDeviceListener,
                                            HidDeviceListener,
                                            DMF_CONTEXT_HidDeviceListener,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_HidDeviceListener.CallbacksDmf = &dmfCallbacksDmf_HidDeviceListener;
    dmfModuleDescriptor_HidDeviceListener.CallbacksWdf = &dmfCallbacksWdf_HidDeviceListener;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_HidDeviceListener,
                                DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    moduleConfig = DMF_CONFIG_GET(*DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// eof: Dmf_HidDeviceListener.c
//
