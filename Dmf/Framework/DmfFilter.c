/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfFilter.c

Abstract:

    DMF Implementation:

    Support for DMF Filter Drivers.

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#include "DmfIncludeInternal.h"

#if defined(DMF_INCLUDE_TMH)
#include "DmfFilter.tmh"
#endif

#if defined(DMF_KERNEL_MODE)

typedef struct
{
    // The only Filter Control Device for each driver.
    //
    WDFDEVICE FilterControlDevice;
    // List of all the filtered WDFDEVICE objects.
    //
    WDFCOLLECTION FilterDeviceCollection;
    // A lock for the above list.
    //
    WDFWAITLOCK FilterDeviceCollectionLock;
} DMF_FILTER_CONTROL_GLOBALS, *PDMF_FILTER_CONTROL_GLOBALS;

// The single instance of all Filter Control Object variables.
//
DMF_FILTER_CONTROL_GLOBALS g_DMF_FilterControlGlobals;

_Must_inspect_result_
NTSTATUS
DMF_FilterControl_GlobalCreate(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Create Filter Control Object's global variables.

Arguments:

    Device - The given WDFDEVICE.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDRIVER driver;
    WDF_OBJECT_ATTRIBUTES attributes;

    driver = WdfDeviceGetDriver(Device);

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    // When we create FilterDeviceCollection and FilterDeviceCollectionLock, we need to set ParentObject as the driver.
    // Then FilterDeviceCollection and FilterDeviceCollectionLock will be automatically deleted.
    //
    attributes.ParentObject = driver;

    // These need to be created only once for each Driver.
    //
    if (g_DMF_FilterControlGlobals.FilterDeviceCollection == NULL)
    {
        ntStatus = WdfCollectionCreate(&attributes,
                                       &g_DMF_FilterControlGlobals.FilterDeviceCollection);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfCollectionCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    if (g_DMF_FilterControlGlobals.FilterDeviceCollectionLock == NULL)
    {
        ntStatus = WdfWaitLockCreate(&attributes,
                                     &g_DMF_FilterControlGlobals.FilterDeviceCollectionLock);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfWaitLockCreate fails: ntStatus=%!STATUS!", ntStatus);
            WdfObjectDelete(&g_DMF_FilterControlGlobals.FilterDeviceCollection);
            g_DMF_FilterControlGlobals.FilterDeviceCollection = NULL;
            goto Exit;
        }
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

VOID
DMF_FilterControl_Lock()
/*++

Routine Description:

    Lock the Filter Control Object global variables using global lock.

Arguments:

    None

Return Value:

    None

--*/
{
    DmfAssert(g_DMF_FilterControlGlobals.FilterDeviceCollectionLock != NULL);
    WdfWaitLockAcquire(g_DMF_FilterControlGlobals.FilterDeviceCollectionLock,
                       NULL);
}

VOID
DMF_FilterControl_Unlock()
/*++

Routine Description:

    Unlock the Filter Control Object global variables using global lock.

Arguments:

    None

Return Value:

    None

--*/
{
    DmfAssert(g_DMF_FilterControlGlobals.FilterDeviceCollectionLock != NULL);
    WdfWaitLockRelease(g_DMF_FilterControlGlobals.FilterDeviceCollectionLock);
}

_IRQL_always_function_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_FilterControl_DeviceCreate(
    _In_ WDFDEVICE Device,
    _In_opt_ DMF_CONFIG_BranchTrack* FilterBranchTrackConfig,
    _In_opt_ PWDF_IO_QUEUE_CONFIG QueueConfig,
    _In_ WCHAR* ControlDeviceName
    )
/*++

Routine Description:

    Create a Control Device for the given WdfDevice instance.
    Store the Control Device handle in DMF device context.
    Enable BranchTrack for Control Device.

    NOTE: Client Driver must delete the control device object after the framework has deleted the given WdfDevice object.
          To determine when the framework has deleted the given Device Object, Client Driver should provide
          EvtCleanupCallback functions for the object and invoke DMF_FilterControl_DeviceDelete in that callback.

Arguments:

    Device - The given WDFDEVICE.
    FilterBranchTrackConfig - BranchTrack Module Config.
    QueueConfig - The Client Driver passes an initialized structure or NULL if not used.
    ControlDeviceName - The name of the Filter Control Device to create.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDRIVER driver;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    PWDFDEVICE_INIT deviceInit;
    WDFDEVICE controlDevice;
    UNICODE_STRING controlDeviceName;
    WDFQUEUE queue;
    PDMFDEVICE_INIT dmfDeviceInit;
    ULONG numberOfDevicesInCollection;

    controlDevice = NULL;
    dmfDeviceInit = NULL;
    queue = NULL;

    ntStatus = DMF_FilterControl_GlobalCreate(Device);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Filter Control Global Create fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    driver = WdfDeviceGetDriver(Device);
    dmfDeviceContext = DmfDeviceContextGet(Device);

    // DMF_FilterControl_DeviceCreate should be called only once per WdfDevice instance.
    //
    DmfAssert(dmfDeviceContext->WdfControlDevice == NULL);

    DMF_FilterControl_Lock();

    // Add the device into the list of currently running filter devices
    // which is necessary for BranchTrack.
    //
    ntStatus = WdfCollectionAdd(g_DMF_FilterControlGlobals.FilterDeviceCollection,
                                Device);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfCollectionAdd fails: ntStatus=%!STATUS!", ntStatus);

        DMF_FilterControl_Unlock();
        goto Exit;
    }

    numberOfDevicesInCollection = WdfCollectionGetCount(g_DMF_FilterControlGlobals.FilterDeviceCollection);

    // We can unlock here because another WdfCollectionAdd cannot occur till the end of this function
    // due to the sequential nature of its caller DeviceAdd.
    //
    DMF_FilterControl_Unlock();

    if (1 == numberOfDevicesInCollection)
    {
        // In order to create a control device, we first need to allocate a
        // WDFDEVICE_INIT structure and set all properties.
        //
        deviceInit = WdfControlDeviceInitAllocate(driver,
                                                  &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX);
        if (NULL == deviceInit)
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfControlDeviceInitAllocate fails: ntStatus=STATUS_INSUFFICIENT_RESOURCES");
            goto Error;
        }

        dmfDeviceInit = DMF_DmfControlDeviceInitAllocate(deviceInit);
        if (dmfDeviceInit == NULL)
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_DmfControlDeviceInitAllocate fails: ntStatus=STATUS_INSUFFICIENT_RESOURCES");
            goto Error;
        }

        DMF_DmfControlDeviceInitSetClientDriverDevice(dmfDeviceInit,
                                                      Device);

        // Ensure only a single application can talk to the Control Device at a time.
        //
        WdfDeviceInitSetExclusive(deviceInit,
                                  TRUE);

        // It is mandatory that Filter Control Devices have this name assigned, otherwise the
        // symbolic link cannot be created.
        //
        DmfAssert(ControlDeviceName != NULL);
        RtlUnicodeStringInit(&controlDeviceName,
                             ControlDeviceName);
        ntStatus = WdfDeviceInitAssignName(deviceInit,
                                           &controlDeviceName);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceInitAssignName fails: ntStatus=%!STATUS!", ntStatus);
            goto Error;
        }

        ntStatus = WdfDeviceCreate(&deviceInit,
                                   WDF_NO_OBJECT_ATTRIBUTES,
                                   &controlDevice);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Error;
        }

        if (QueueConfig != NULL)
        {
            // Client Driver provides QueueConfig if it wants to process IOCTLs from User-mode
            // In that case Create the default queue for control device here, to enable Client IOCTL callback to be dispatched.
            // If not, DMF will create a default queue for control device.
            //
            DMF_DmfDeviceInitHookQueueConfig(dmfDeviceInit,
                                             QueueConfig);
            ntStatus = WdfIoQueueCreate(controlDevice,
                                        QueueConfig,
                                        WDF_NO_OBJECT_ATTRIBUTES,
                                        &queue);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoQueueCreate fails: ntStatus=%!STATUS!", ntStatus);
                goto Error;
            }
        }

        DMF_DmfDeviceInitSetBranchTrackConfig(dmfDeviceInit,
                                              FilterBranchTrackConfig);

        ntStatus = DMF_ModulesCreate(controlDevice,
                                     &dmfDeviceInit);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModulesCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Error;
        }

        // Control devices must notify WDF when they are done initializing. I/O is
        // rejected until this call is made.
        //
        WdfControlFinishInitializing(controlDevice);

        // When control device is created for the first time, store it in the filter control global variable.
        // This will be used by all subsequent devices.
        //
        g_DMF_FilterControlGlobals.FilterControlDevice = controlDevice;
    }

    ntStatus = STATUS_SUCCESS;
    // Assign the global filter control device to the control device in device context.
    // This allows Modules to have easy access to this device.
    //
    dmfDeviceContext->WdfControlDevice = g_DMF_FilterControlGlobals.FilterControlDevice;
    goto Exit;

Error:

    DMF_FilterControl_Lock();
    // Remove item added above since it is no longer valid because this function has failed.
    //
    WdfCollectionRemoveItem(g_DMF_FilterControlGlobals.FilterDeviceCollection,
                            0);
    DMF_FilterControl_Unlock();

    if (controlDevice != NULL)
    {
        // Release the reference on the newly created object, since
        // we couldn't initialize it.
        //
        WdfObjectDelete(controlDevice);
        controlDevice = NULL;
        g_DMF_FilterControlGlobals.FilterControlDevice = NULL;
    }

    if (deviceInit != NULL)
    {
        WdfDeviceInitFree(deviceInit);
        deviceInit = NULL;
    }

Exit:

    return ntStatus;
}

_IRQL_always_function_max_(PASSIVE_LEVEL)
VOID
DMF_FilterControl_DeviceDelete(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    This routine deletes the control device which was created.

Arguments:

    Device - The given WDFDEVICE.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    ULONG numberOfDevicesInCollection;
    ULONG collectionIndex;
    WDFDEVICE deviceToDelete;
    WDFDEVICE deviceInCollection;
    BOOLEAN foundDeviceInCollection;

    FuncEntry(DMF_TRACE);

    deviceToDelete = NULL;
    foundDeviceInCollection = FALSE;

    DMF_FilterControl_Lock();

    numberOfDevicesInCollection = WdfCollectionGetCount(g_DMF_FilterControlGlobals.FilterDeviceCollection);
    for (collectionIndex = 0; collectionIndex < numberOfDevicesInCollection; collectionIndex++)
    {
        deviceInCollection = (WDFDEVICE)WdfCollectionGetItem(g_DMF_FilterControlGlobals.FilterDeviceCollection,
                                                             collectionIndex);
        if (Device == deviceInCollection)
        {
            foundDeviceInCollection = TRUE;
            WdfCollectionRemoveItem(g_DMF_FilterControlGlobals.FilterDeviceCollection,
                                    collectionIndex);
            // Reduce the count of devices remaining in the collection. 
            // The Control Device is deleted when this count goes to 0.
            //
            numberOfDevicesInCollection--;
            break;
        }
    }
    // Device was not found in collection.
    // This can happen when DMF_FilterControl_DeviceCreate fails but client driver ignores the failure.
    //
    if (FALSE == foundDeviceInCollection)
    {
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Device 0x%p not found in Filter Device Collection", Device);
    }   

    dmfDeviceContext = DmfDeviceContextGet(Device);

    if (0 == numberOfDevicesInCollection)
    {
        // We should avoid holding locks when calling into WDF to avoid deadlocks.
        // So store the device to delete in context in a local variable,
        // clear the device in context while lock is held and then delete the local variable later.
        // This device to delete and the global filter control device can be NULL in the case when 
        // DMF_FilterControl_DeviceCreate was attempted but failed.
        //
        deviceToDelete = dmfDeviceContext->WdfControlDevice;
        DmfAssert(deviceToDelete == g_DMF_FilterControlGlobals.FilterControlDevice);
        dmfDeviceContext->WdfControlDevice = NULL;
        g_DMF_FilterControlGlobals.FilterControlDevice = NULL;
    }

    DMF_FilterControl_Unlock();

    // The last Filter Object is deleted so delete the Filter Control Object.
    //
    if (deviceToDelete != NULL)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Delete WdfControlDevice=0x%p", deviceToDelete);
        WdfObjectDelete(deviceToDelete);
    }

    FuncExitVoid(DMF_TRACE);
}

#endif // defined(DMF_KERNEL_MODE)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus Filter
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#if defined(DMF_KERNEL_MODE)

// WDM child device context
// 
typedef struct _WDM_CHILD_DEVICE_EXTENSION
{
    // GUID to identify WDM child device.
    // 
    GUID Signature;

    // Target Device Object
    //
    PDEVICE_OBJECT TargetDeviceObject;

    // Physical Device Object
    //
    PDEVICE_OBJECT PhysicalDeviceObject;

    // Parent ChildList entry
    // 
    LIST_ENTRY ListEntry;

    // Parent WDF device object
    // 
    WDFDEVICE Parent;

    // Child WDF wrapper object
    // 
    DMFBUSCHILDDEVICE Child;

    // TRUE if PDO is attached, FALSE otherwise
    // 
    BOOLEAN IsExisting;
} WDM_CHILD_DEVICE_EXTENSION;

// Parent bus device context
// 
typedef struct _PARENT_BUS_DEVICE_CONTEXT
{
    //
    // List of child device (relations)
    // 
    LIST_ENTRY ChildList;

    //
    // Spin lock protecting child list access
    // 
    KSPIN_LOCK ChildListLock;
} PARENT_BUS_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PARENT_BUS_DEVICE_CONTEXT, DMF_BusFilter_GetParentContext)

// Bus child device context
// 
typedef struct _BUS_CHILD_DEVICE_CONTEXT
{
    // WDM device object
    // 
    PDEVICE_OBJECT DeviceObject;
} BUS_CHILD_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BUS_CHILD_DEVICE_CONTEXT, DMF_BusFilter_GetChildContext)

// Context for work item to drop to PASSIVE_LEVEL on Device Add
// 
typedef struct _DEVICE_ADD_WORK_ITEM_CONTEXT
{
    WDFDEVICE Device;

    PDEVICE_OBJECT PhysicalDeviceObject;
} DEVICE_ADD_WORK_ITEM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_ADD_WORK_ITEM_CONTEXT, DMF_BusFilter_DeviceAddWorkItemContextGet)

// Context for work item to drop to PASSIVE_LEVEL on Device Remove
// 
typedef struct _DEVICE_REMOVE_WORK_ITEM_CONTEXT
{
    PDEVICE_OBJECT DeviceObject;

    DMF_BusFilter_CONFIG* Configuration;
} DEVICE_REMOVE_WORK_ITEM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_REMOVE_WORK_ITEM_CONTEXT, DMF_BusFilter_DeviceRemoveWorkItemContextGet)

typedef
_Function_class_(EVT_DMF_BusFilter_DispatchPnp)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
EVT_DMF_BusFilter_DispatchPnp(
    _In_ DMFBUSCHILDDEVICE ChildDevice,
    _In_ PIRP Irp
    );

// Module-internal context data
// 
typedef struct _DMF_CONTEXT_BusFilter
{
    // Copy of the module configuration
    // 
    DMF_BusFilter_CONFIG Configuration;

    // Hooked dispatch table
    // 
    PDRIVER_DISPATCH MajorDispatchFunctions[IRP_MJ_MAXIMUM_FUNCTION + 1];

    // PNP minor functions dispatch routines
    // 
    EVT_DMF_BusFilter_DispatchPnp* PnPMinorDispatchFunctions[IRP_MN_DEVICE_ENUMERATED + 1];
} BusFilter_Context;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BusFilter_Context,
                                   BusFilterContextGet);

// {678CBB8D-019F-4D07-912A-73E2E568B148}
DEFINE_GUID(GUID_DMF_BUSFILTER_SIGNATURE,
    0x678cbb8d, 0x19f, 0x4d07, 0x91, 0x2a, 0x73, 0xe2, 0xe5, 0x68, 0xb1, 0x48);

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
void
DMF_BusFilter_Relations_RemoveDevicePassive(
    _In_ WDFWORKITEM WorkItem
    )
/*++

Routine Description:

    Processes child device removal at PASSIVE_LEVEL

Arguments:

    Workitem - WDF workitem handle.

Return Value:

    None

--*/
{
    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    const DEVICE_REMOVE_WORK_ITEM_CONTEXT* workItemContext = DMF_BusFilter_DeviceRemoveWorkItemContextGet(WorkItem);
    const WDM_CHILD_DEVICE_EXTENSION* extension = (WDM_CHILD_DEVICE_EXTENSION*)workItemContext->DeviceObject->DeviceExtension;
    const DMF_BusFilter_CONFIG* config = workItemContext->Configuration;

    if (config->EvtDeviceRemove)
    {
        config->EvtDeviceRemove(extension->Parent,
                                extension->Child);
    }

    WdfObjectDelete(extension->Child);
    IoDetachDevice(extension->TargetDeviceObject);
    IoDeleteDevice(workItemContext->DeviceObject);

    WdfObjectDelete(WorkItem);

    FuncExitNoReturn(DMF_TRACE);
}
#pragma code_seg()

_IRQL_requires_max_(DISPATCH_LEVEL)
static
void
DMF_BusFilter_Relations_RemoveDevice(
    _In_ PDEVICE_OBJECT DeviceObject
    )
/*++

Routine Description:

    Processes child device removal.

Arguments:

    DeviceObject - Parent device object.

Return Value:

    None

--*/
{
    KLOCK_QUEUE_HANDLE handle;
    WDM_CHILD_DEVICE_EXTENSION* extension = (WDM_CHILD_DEVICE_EXTENSION*)DeviceObject->DeviceExtension;
    PARENT_BUS_DEVICE_CONTEXT* parentContext = DMF_BusFilter_GetParentContext(extension->Parent);
    const BusFilter_Context* context = BusFilterContextGet(WdfGetDriver());
    const DMF_BusFilter_CONFIG* config = &context->Configuration;

    FuncEntry(DMF_TRACE);

    if (extension->IsExisting)
    {
        goto Exit;
    }

    KeAcquireInStackQueuedSpinLock(&parentContext->ChildListLock,
                                   &handle);
    RemoveEntryList(&extension->ListEntry);
    KeReleaseInStackQueuedSpinLock(&handle);

    // This can be invoked at DISPATCH_LEVEL, so call
    // DMF_BusFilter_Relations_RemoveDevicePassive via work item to
    // drop down to PASSIVE_LEVEL for IoDetachDevice and IoDeleteDevice.
    //

    if (KeGetCurrentIrql() != PASSIVE_LEVEL)
    {
        TraceVerbose(DMF_TRACE, "%!FUNC! called at %!irql!", KeGetCurrentIrql());

        WDF_OBJECT_ATTRIBUTES attributes;
        WDF_WORKITEM_CONFIG workItemConfig;
        WDFWORKITEM workItem;

        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                                DEVICE_REMOVE_WORK_ITEM_CONTEXT);
        attributes.ParentObject = extension->Parent;

        WDF_WORKITEM_CONFIG_INIT(&workItemConfig,
                                 DMF_BusFilter_Relations_RemoveDevicePassive);

        NTSTATUS ntStatus = WdfWorkItemCreate(&workItemConfig,
                                              &attributes,
                                              &workItem);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceError(DMF_TRACE, "WdfWorkItemCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        DEVICE_REMOVE_WORK_ITEM_CONTEXT* workItemContext = DMF_BusFilter_DeviceRemoveWorkItemContextGet(workItem);

        workItemContext->DeviceObject = DeviceObject;
        workItemContext->Configuration = (DMF_BusFilter_CONFIG*)&context->Configuration;

        WdfWorkItemEnqueue(workItem);
    }
    else
    {
        TraceVerbose(DMF_TRACE, "%!FUNC! called at %!irql!", KeGetCurrentIrql());

        if (config->EvtDeviceRemove)
        {
            config->EvtDeviceRemove(extension->Parent, extension->Child);
        }

        WdfObjectDelete(extension->Child);
        IoDetachDevice(extension->TargetDeviceObject);
        IoDeleteDevice(DeviceObject);
    }

Exit:

    FuncExitNoReturn(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
NTSTATUS
DMF_BusFilter_DispatchPnp(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp,
    _In_ UCHAR MinorCode
    )
/*++

Routine Description:

    Handles PnP requests.

Arguments:

    DeviceObject - Parent device object.
    Irp - Irp with PnP request.
    MinorCode - Request minor code.

Return Value:

    NTSTATUS

--*/
{
    const WDM_CHILD_DEVICE_EXTENSION* extension = (WDM_CHILD_DEVICE_EXTENSION*)DeviceObject->DeviceExtension;
    const BusFilter_Context* context = BusFilterContextGet(WdfGetDriver());

    if (MinorCode == IRP_MN_REMOVE_DEVICE)
    {
        // Handle child device removal
        // 
        DMF_BusFilter_Relations_RemoveDevice(DeviceObject);
    }
    else if (MinorCode <= IRP_MN_DEVICE_ENUMERATED)
    {
        if (context->PnPMinorDispatchFunctions[MinorCode] != NULL)
        {
            //
            // Forward to PnP minor code dispatch routines
            // 
            return context->PnPMinorDispatchFunctions[MinorCode](extension->Child, Irp);
        }
    }

    // Forward to lower driver
    // 
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(extension->TargetDeviceObject,
                        Irp);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
NTSTATUS
DMF_BusFilter_DispatchHandler(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
    )
/*++

Routine Description:

    Dispatch routine handler for all IRPs

Arguments:

    DeviceObject - Parent device object.
    Irp - Irp with request.

Return Value:

    NTSTATUS

--*/
{
    const WDM_CHILD_DEVICE_EXTENSION* extension = (WDM_CHILD_DEVICE_EXTENSION*)DeviceObject->DeviceExtension;
    const PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    const BusFilter_Context* context = BusFilterContextGet(WdfGetDriver());

    if (!IsEqualGUID(extension->Signature,
                     GUID_DMF_BUSFILTER_SIGNATURE))
    {
        return context->MajorDispatchFunctions[stack->MajorFunction](DeviceObject, Irp);
    }

    // Handle PNP requests
    // 
    if (stack->MajorFunction == IRP_MJ_PNP)
    {
        return DMF_BusFilter_DispatchPnp(DeviceObject, Irp, stack->MinorFunction);
    }

    // Forward to lower driver
    // 
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(extension->TargetDeviceObject, Irp);
}

_IRQL_requires_max_(APC_LEVEL)
static
NTSTATUS
DMF_BusFilter_Relations_AddDevice(
    _In_ WDFDEVICE Device,
    _In_ PDEVICE_OBJECT PhysicalDeviceObject
    )
/*++

Routine Description:

    Creates proxy child device for bus PDO.

Arguments:

    Device - Child device to add.
    PhysicalDeviceObject - Parent device object.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus = STATUS_NOT_IMPLEMENTED;
    WDF_OBJECT_ATTRIBUTES attributes;
    PLIST_ENTRY entry = NULL;
    KLOCK_QUEUE_HANDLE handle;
    WDM_CHILD_DEVICE_EXTENSION* childExtension = NULL;
    PDEVICE_OBJECT filterDeviceObject = NULL;
    BOOLEAN preexisting = FALSE;
    DMFBUSCHILDDEVICE child = NULL;
    BUS_CHILD_DEVICE_CONTEXT* childContext = NULL;
    PARENT_BUS_DEVICE_CONTEXT* parentContext = DMF_BusFilter_GetParentContext(Device);
    const PDEVICE_OBJECT deviceObject = WdfDeviceWdmGetDeviceObject(Device);
    const BusFilter_Context* context = BusFilterContextGet(WdfGetDriver());
    const DMF_BusFilter_CONFIG* config = &context->Configuration;

    FuncEntry(DMF_TRACE);

    if (parentContext == NULL)
    {
        ntStatus = STATUS_INVALID_DEVICE_STATE;
        goto Exit;
    }

    KeAcquireInStackQueuedSpinLock(&parentContext->ChildListLock,
                                   &handle);

    // Find and update PDO status
    // 
    for (
        entry = parentContext->ChildList.Flink;
        entry != &parentContext->ChildList;
        entry = entry->Flink
        )
    {
        childExtension = CONTAINING_RECORD(entry,
                                           WDM_CHILD_DEVICE_EXTENSION,
                                           ListEntry);

        if (childExtension->PhysicalDeviceObject == PhysicalDeviceObject)
        {
            preexisting = TRUE;
            childExtension->IsExisting = TRUE;
            break;
        }
    }

    KeReleaseInStackQueuedSpinLock(&handle);

    if (preexisting)
    {
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                            BUS_CHILD_DEVICE_CONTEXT);
    attributes.ParentObject = Device;

    // Create piggyback framework object for WDM child device object
    // 
    ntStatus = WdfObjectCreate(&attributes, (WDFOBJECT*)&child);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceError(DMF_TRACE, "WdfObjectCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Create WDM device
    // 
    ntStatus = IoCreateDevice(deviceObject->DriverObject,
                              sizeof(WDM_CHILD_DEVICE_EXTENSION),
                              NULL,
                              config->DeviceType,
                              FILE_DEVICE_SECURE_OPEN | config->DeviceCharacteristics,
                              FALSE,
                              &filterDeviceObject);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceError(DMF_TRACE, "IoCreateDevice fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Link WDM and WDF device together.
    // 
    childContext = DMF_BusFilter_GetChildContext(child);
    childContext->DeviceObject = filterDeviceObject;

    childExtension = (WDM_CHILD_DEVICE_EXTENSION*)filterDeviceObject->DeviceExtension;
    RtlZeroMemory(childExtension,
                  sizeof(WDM_CHILD_DEVICE_EXTENSION));
    RtlCopyMemory(&childExtension->Signature,
                  &GUID_DMF_BUSFILTER_SIGNATURE,
                  sizeof(GUID));
    childExtension->Parent = Device;
    childExtension->Child = child;

    childExtension->PhysicalDeviceObject = PhysicalDeviceObject;

    childExtension->TargetDeviceObject = IoAttachDeviceToDeviceStack(filterDeviceObject,
                                                                     PhysicalDeviceObject);
    if (childExtension->TargetDeviceObject == NULL)
    {
        IoDeleteDevice(filterDeviceObject);
        ntStatus = STATUS_NO_SUCH_DEVICE;
        goto Exit;
    }

    filterDeviceObject->Flags |= childExtension->TargetDeviceObject->Flags &
                                 (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_INRUSH | DO_POWER_PAGABLE);

    if (config->EvtDeviceAdd)
    {
        ntStatus = config->EvtDeviceAdd(Device, child);

        if (!NT_SUCCESS(ntStatus))
        {
            TraceError(DMF_TRACE, "EvtDeviceAdd fails: ntStatus=%!STATUS!", ntStatus);

            IoDetachDevice(childExtension->TargetDeviceObject);
            IoDeleteDevice(filterDeviceObject);
            goto Exit;
        }
    }

    KeAcquireInStackQueuedSpinLock(&parentContext->ChildListLock,
                                   &handle);
    childExtension->IsExisting = TRUE;
    InsertTailList(&parentContext->ChildList,
                   &childExtension->ListEntry);
    KeReleaseInStackQueuedSpinLock(&handle);

    filterDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    ntStatus = STATUS_SUCCESS;

Exit:

    if (!NT_SUCCESS(ntStatus) && child != NULL)
    {
        WdfObjectDelete(child);
    }

    FuncExit(DMF_TRACE, "status=%!STATUS!", ntStatus);

    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
void 
DMF_BusFilter_Relations_AddDevicePassive(
    WDFWORKITEM WorkItem
    )
/*++

Routine Description:

    Calls DMF_BusFilter_Relations_AddDevice at PASSIVE_LEVEL

Arguments:

    WorkItem - Associated WDFWORKITEM.

Return Value:

    None

--*/
{
    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    const DEVICE_ADD_WORK_ITEM_CONTEXT* workItemContext = DMF_BusFilter_DeviceAddWorkItemContextGet(WorkItem);

    NTSTATUS ntStatus = DMF_BusFilter_Relations_AddDevice(workItemContext->Device,
                                                          workItemContext->PhysicalDeviceObject);

    if (!NT_SUCCESS(ntStatus))
    {
        TraceError(DMF_TRACE, "DMF_BusFilter_Relations_AddDevice fails: ntStatus=%!STATUS!", ntStatus);
    }

    WdfObjectDelete(WorkItem);

    FuncExitNoReturn(DMF_TRACE);
}
#pragma code_seg()

_IRQL_requires_max_(DISPATCH_LEVEL)
static
NTSTATUS
DMF_BusFilter_QueryBusRelationsCompleted(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp,
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Bus relations query completed routine.

Arguments:

    DeviceObject - Parent device object.
    Irp - Query Bus Relations IRP.
    Device - Target WDFDEVICE.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus = STATUS_NOT_IMPLEMENTED;
    PARENT_BUS_DEVICE_CONTEXT* parentContext = DMF_BusFilter_GetParentContext(Device);
    PDEVICE_RELATIONS deviceRelations = NULL;
    KLOCK_QUEUE_HANDLE handle;
    WDM_CHILD_DEVICE_EXTENSION* childExtension = NULL;

    UNREFERENCED_PARAMETER(DeviceObject);

    FuncEntry(DMF_TRACE);

    if (Irp->PendingReturned)
    {
        IoMarkIrpPending(Irp);
    }

    if (!NT_SUCCESS(Irp->IoStatus.Status))
    {
        goto Exit;
    }

    if (parentContext == NULL)
    {
        goto Exit;
    }

    KeAcquireInStackQueuedSpinLock(&parentContext->ChildListLock,
                                   &handle);

    // Reset child states
    // 
    for (
        LIST_ENTRY* entry = parentContext->ChildList.Flink;
        entry != &parentContext->ChildList;
        entry = entry->Flink
        )
    {
        childExtension = CONTAINING_RECORD(entry,
                                           WDM_CHILD_DEVICE_EXTENSION,
                                           ListEntry);
        childExtension->IsExisting = FALSE;
    }

    KeReleaseInStackQueuedSpinLock(&handle);

    deviceRelations = (PDEVICE_RELATIONS)Irp->IoStatus.Information;

    if (deviceRelations == NULL)
    {
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_WORKITEM_CONFIG workItemConfig;
    WDFWORKITEM workItem;

    // Walk through device relations.
    // 
    for (ULONG index = 0; index < deviceRelations->Count; index++)
    {
        // This can get invoked at DISPATCH_LEVEL, so we need to call
        // DMF_BusFilter_Relations_AddDevice via work item to drop down
        // to PASSIVE_LEVEL since IoCreateDevice requires APC_LEVEL max!
        //

        if (KeGetCurrentIrql() != PASSIVE_LEVEL)
        {
            TraceVerbose(DMF_TRACE, "%!FUNC! called at %!irql!",  KeGetCurrentIrql());

            WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                                    DEVICE_ADD_WORK_ITEM_CONTEXT);
            attributes.ParentObject = Device;

            WDF_WORKITEM_CONFIG_INIT(&workItemConfig,
                                     DMF_BusFilter_Relations_AddDevicePassive);
            ntStatus = WdfWorkItemCreate(&workItemConfig,
                                         &attributes,
                                         &workItem);
            if (!NT_SUCCESS(ntStatus))
            {
                TraceError(DMF_TRACE, "WdfWorkItemCreate fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }

            DEVICE_ADD_WORK_ITEM_CONTEXT* workItemContext = DMF_BusFilter_DeviceAddWorkItemContextGet(workItem);

            workItemContext->Device = Device;
            workItemContext->PhysicalDeviceObject = deviceRelations->Objects[index];

            WdfWorkItemEnqueue(workItem);
        }
        else
        {
            TraceVerbose(DMF_TRACE, "%!FUNC! called at %!irql!",  KeGetCurrentIrql());

            ntStatus = DMF_BusFilter_Relations_AddDevice(Device,
                                                         deviceRelations->Objects[index]);
            if (!NT_SUCCESS(ntStatus))
            {
                TraceError(DMF_TRACE, "DMF_BusFilter_Relations_AddDevice fails: ntStatus=%!STATUS!", ntStatus);
            }
        }
    }

Exit:

    FuncExitNoReturn(DMF_TRACE);

    return STATUS_CONTINUE_COMPLETION;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
NTSTATUS
DMF_BusFilter_PreprocessQueryBusRelations(
    _In_ WDFDEVICE Device,
    _In_ PIRP Irp
    )
/*++

Routine Description:

    Pre-processes IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_RELATIONS.

Arguments:

    DeviceObject - Parent device object.
    Irp - IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_RELATIONS request.

Return Value:

    NTSTATUS

--*/
{
    const PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

    if (
        stack->MajorFunction != IRP_MJ_PNP ||
        stack->MinorFunction != IRP_MN_QUERY_DEVICE_RELATIONS ||
        stack->Parameters.QueryDeviceRelations.Type != BusRelations
        )
    {
        IoSkipCurrentIrpStackLocation(Irp);
    }
    else
    {
        IoCopyCurrentIrpStackLocationToNext(Irp);
        IoSetCompletionRoutine(Irp,
                               (PIO_COMPLETION_ROUTINE)DMF_BusFilter_QueryBusRelationsCompleted,
                               Device,
                               TRUE,
                               TRUE,
                               TRUE);
    }

    return WdfDeviceWdmDispatchPreprocessedIrp(Device,
                                               Irp);
}

#pragma code_seg("PAGE")
static
NTSTATUS
DMF_BusFilter_PnP_StartDevice(
    _In_ DMFBUSCHILDDEVICE ChildDevice,
    _In_ PIRP Irp
    )
/*++

Routine Description:

    Handles IRP_MN_START_DEVICE

Arguments:

    ChildDevice - Associated child device.
    Irp - IRP_MJ_PNP / IRP_MN_START_DEVICE request.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    const BusFilter_Context* context = BusFilterContextGet(WdfGetDriver());
    const DMF_BusFilter_CONFIG* config = &context->Configuration;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    if (!IoForwardIrpSynchronously(DMF_BusFilter_WdmAttachedDeviceGet(ChildDevice),
                                   Irp))
    {
        TraceError(DMF_TRACE, "IoForwardIrpSynchronously fails: Irp=0x%p", Irp);

        Irp->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
    }
    else if (NT_SUCCESS(Irp->IoStatus.Status) && config->EvtDeviceStarted)
    {
        config->EvtDeviceStarted(ChildDevice,
                                 Irp);
    }

    ntStatus = Irp->IoStatus.Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

static
NTSTATUS
DMF_BusFilter_PnP_DeviceEnumerated(
    _In_ DMFBUSCHILDDEVICE ChildDevice,
    _In_ PIRP Irp
    )
/*++

Routine Description:

    Handles IRP_MN_DEVICE_ENUMERATED.

Arguments:

    ChildDevice - Associated child device.
    Irp - IRP_MJ_PNP / IRP_MN_DEVICE_ENUMERATED request.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    const BusFilter_Context* context = BusFilterContextGet(WdfGetDriver());
    const DMF_BusFilter_CONFIG* config = &context->Configuration;

    FuncEntry(DMF_TRACE);

    if (config->EvtDeviceEnumerated)
    {
        config->EvtDeviceEnumerated(ChildDevice,
                                    Irp);
    }

    // Forward to the parent bus driver
    //
    IoSkipCurrentIrpStackLocation(Irp);
    ntStatus = IoCallDriver(DMF_BusFilter_WdmAttachedDeviceGet(ChildDevice),
                            Irp);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

static
NTSTATUS
DMF_BusFilter_PnP_QueryId(
    _In_ DMFBUSCHILDDEVICE ChildDevice,
    _In_ PIRP Irp
    )
/*++

Routine Description:

    Handles IRP_MN_QUERY_ID.

Arguments:

    ChildDevice - Associated child device.
    Irp - IRP_MJ_PNP / IRP_MN_QUERY_ID request.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    const BusFilter_Context* context = BusFilterContextGet(WdfGetDriver());
    const DMF_BusFilter_CONFIG* config = &context->Configuration;

    FuncEntry(DMF_TRACE);

    // Forward immediately if client driver has no handler
    // 
    if (config->EvtDeviceQueryId == NULL)
    {
        IoSkipCurrentIrpStackLocation(Irp);
        ntStatus = IoCallDriver(DMF_BusFilter_WdmAttachedDeviceGet(ChildDevice),
                                Irp);

        FuncExit(DMF_TRACE, "status=%!STATUS!", ntStatus);
        return ntStatus;
    }

    // If client driver didn't do anything with the IRP...
    // 
    if (!config->EvtDeviceQueryId(ChildDevice, Irp))
    {
        // ...forward it prior to completion
        // 
        if (!IoForwardIrpSynchronously(DMF_BusFilter_WdmAttachedDeviceGet(ChildDevice),
                                       Irp))
        {
            TraceError(DMF_TRACE, "IoForwardIrpSynchronously fails: Irp=0x%p", Irp);

            Irp->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
        }
    }

    // Complete the Irp.
    //
    ntStatus = Irp->IoStatus.Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    FuncExit(DMF_TRACE, "status=%!STATUS!", ntStatus);

    return ntStatus;
}

static
NTSTATUS
DMF_BusFilter_PnP_QueryInterface(
    _In_ DMFBUSCHILDDEVICE ChildDevice,
    _In_ PIRP Irp
    )
/*++

Routine Description:

    Handles IRP_MN_QUERY_INTERFACE.

Arguments:

    ChildDevice - Associated child device.
    Irp - IRP_MJ_PNP / IRP_MN_QUERY_INTERFACE request.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    const BusFilter_Context* context = BusFilterContextGet(WdfGetDriver());
    const DMF_BusFilter_CONFIG* config = &context->Configuration;

    FuncEntry(DMF_TRACE);

    // Forward immediately if client driver has no handler
    // 
    if (config->EvtDeviceQueryInterface == NULL)
    {
        IoSkipCurrentIrpStackLocation(Irp);
        ntStatus = IoCallDriver(DMF_BusFilter_WdmAttachedDeviceGet(ChildDevice),
                                Irp);

        FuncExit(DMF_TRACE, "status=%!STATUS!", ntStatus);
        return ntStatus;
    }

    // If client driver didn't do anything with the IRP...
    // 
    if (!config->EvtDeviceQueryInterface(ChildDevice, Irp))
    {
        // ...forward it prior to completion
        // 
        if (!IoForwardIrpSynchronously(DMF_BusFilter_WdmAttachedDeviceGet(ChildDevice),
                                       Irp))
        {
            TraceError(DMF_TRACE, "IoForwardIrpSynchronously fails: Irp=0x%p", Irp);

            Irp->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
        }
    }

    // Complete the Irp
    //
    ntStatus = Irp->IoStatus.Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    FuncExit(DMF_TRACE, "status=%!STATUS!", ntStatus);

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// BusFilter Public Calls by Client
//

#pragma warning(disable:4995)
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BusFilter_Initialize(
    _In_ DMF_BusFilter_CONFIG* BusFilterConfig
    )
/*++

Routine Description:

    Called by Client Driver to initialize DMF BusFilter operations from DriverEntry().

Arguments:

    BusFilterConfig - Client Driver configuration parameters.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES attributes;
    BusFilter_Context* contextBusFilter;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    contextBusFilter = NULL;

    // Config is required
    // 
    if (BusFilterConfig == NULL)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // Driver object must be already created
    // 
    if (!WdfGetDriver())
    {
        ntStatus = STATUS_NOT_SUPPORTED;
        goto Exit;
    }

    if (!BusFilterConfig->DriverObject)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                            BusFilter_Context);

    // Attach context to driver object.
    // 
    ntStatus = WdfObjectAllocateContext(WdfGetDriver(),
                                        &attributes,
                                        (void**)&contextBusFilter);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceError(DMF_TRACE, "WdfObjectAllocateContext fails: ntStatus=%!STATUS!",ntStatus);
        goto Exit;
    }

    // Save copy of config in context to invoke callback routines later.
    // 
    RtlCopyMemory(&contextBusFilter->Configuration,
                  BusFilterConfig,
                  sizeof(DMF_BusFilter_CONFIG));

    ULONG index;
    PDRIVER_DISPATCH* pDispatch;

    // Store original dispatch routine pointers and overwrite with our own
    // 
    #pragma warning(disable:28175)
    for (
        index = 0,
        pDispatch = BusFilterConfig->DriverObject->MajorFunction;
        index <= IRP_MJ_MAXIMUM_FUNCTION;
        index++, pDispatch++
        )
    {
        contextBusFilter->MajorDispatchFunctions[index] = *pDispatch;
        *pDispatch = DMF_BusFilter_DispatchHandler;
    }
    #pragma warning(default:28175)

    // PnP minor code dispatch routines
    // 
    contextBusFilter->PnPMinorDispatchFunctions[IRP_MN_START_DEVICE] = DMF_BusFilter_PnP_StartDevice;
    contextBusFilter->PnPMinorDispatchFunctions[IRP_MN_DEVICE_ENUMERATED] = DMF_BusFilter_PnP_DeviceEnumerated;
    contextBusFilter->PnPMinorDispatchFunctions[IRP_MN_QUERY_ID] = DMF_BusFilter_PnP_QueryId;
    contextBusFilter->PnPMinorDispatchFunctions[IRP_MN_QUERY_INTERFACE] = DMF_BusFilter_PnP_QueryInterface;

    // Clear invalid characteristics (see MS docs)
    // 
    BusFilterConfig->DeviceCharacteristics &= ~(FILE_AUTOGENERATED_DEVICE_NAME |
                                                FILE_CHARACTERISTIC_TS_DEVICE |
                                                FILE_CHARACTERISTIC_WEBDAV_DEVICE |
                                                FILE_DEVICE_IS_MOUNTED |
                                                FILE_VIRTUAL_VOLUME);

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "status=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()
#pragma warning(default:4995)

#pragma code_seg("PAGE")
NTSTATUS
DMF_BusFilter_DeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
/*++

Routine Description:

    Creates bus WDF device. 

Arguments:

    Driver - Associated WDFDRIVER.
    DeviceInit - WDF PWDFDEVICE_INIT.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus = STATUS_NOT_IMPLEMENTED;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFDEVICE device = NULL;
    UCHAR minorPnP = IRP_MN_QUERY_DEVICE_RELATIONS;
    PDMFDEVICE_INIT dmfDeviceInit = NULL;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    const BusFilter_Context* context = BusFilterContextGet(Driver);
    const DMF_BusFilter_CONFIG* config = &context->Configuration;

    WdfFdoInitSetFilter(DeviceInit);

    // Attach IRP preprocessor.
    // 
    ntStatus = WdfDeviceInitAssignWdmIrpPreprocessCallback(DeviceInit,
                                                           DMF_BusFilter_PreprocessQueryBusRelations,
                                                           IRP_MJ_PNP,
                                                           &minorPnP,
                                                           1);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceError(DMF_TRACE, "WdfDeviceInitAssignWdmIrpPreprocessCallback fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Don't initialize with context here as client driver might decide
    // to set their own context memory in EvtPreBusDeviceAdd
    // 
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    // Call pre-device-creation callback, if set.
    // 
    if (config->EvtPreBusDeviceAdd)
    {
        ntStatus = config->EvtPreBusDeviceAdd(Driver,
                                              DeviceInit,
                                              &attributes,
                                              &dmfDeviceInit);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceError(DMF_TRACE, "EvtPreBusDeviceAdd fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    // Client driver is using DMF modules.
    // 
    if (dmfDeviceInit != NULL)
    {
        DMF_DmfFdoSetFilter(dmfDeviceInit);
    }

    // Create device object
    // 
    ntStatus = WdfDeviceCreate(&DeviceInit,
                               &attributes,
                               &device);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceError(DMF_TRACE, "WdfDeviceCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    PARENT_BUS_DEVICE_CONTEXT* parentContext = NULL;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                            PARENT_BUS_DEVICE_CONTEXT);

    // Add bus device context.
    // 
    ntStatus = WdfObjectAllocateContext(device,
                                        &attributes,
                                        (void**)&parentContext);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceError(DMF_TRACE, "WdfObjectAllocateContext fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    InitializeListHead(&parentContext->ChildList);
    KeInitializeSpinLock(&parentContext->ChildListLock);

    // Call post-device-creation callback, if set.
    // 
    if (config->EvtPostBusDeviceAdd)
    {
        ntStatus = config->EvtPostBusDeviceAdd(device,
                                               dmfDeviceInit);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceError(DMF_TRACE, "EvtPostBusDeviceAdd fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

Exit:

    if (!NT_SUCCESS(ntStatus) && dmfDeviceInit != NULL)
    {
        DMF_DmfDeviceInitFree(&dmfDeviceInit);
    }

    if (!NT_SUCCESS(ntStatus) && device != NULL)
    {
        WdfObjectDelete(device);
    }

    FuncExit(DMF_TRACE, "status=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

PDEVICE_OBJECT
DMF_BusFilter_WdmDeviceObjectGet(
    _In_ DMFBUSCHILDDEVICE ChildDevice
    )
/*++

Routine Description:

    Returns DEVICE_OBJECT associated with a given DMFBUSCHILDDEVICE.

Arguments:

    ChildDevice - The given DMFBUSCHILDDEVICE.

Return Value:

    The associated DEVICE_OBJECT.

--*/
{
    const BUS_CHILD_DEVICE_CONTEXT* childContext = DMF_BusFilter_GetChildContext(ChildDevice);

    if (childContext)
    {
        return childContext->DeviceObject;
    }

    return NULL;
}

PDEVICE_OBJECT
DMF_BusFilter_WdmAttachedDeviceGet(
    _In_ DMFBUSCHILDDEVICE ChildDevice
    )
/*++

Routine Description:

    Returns the attached DEVICE_OBJECT associated with a given DMFBUSCHILDDEVICE.

Arguments:

    ChildDevice - The given DMFBUSCHILDDEVICE.

Return Value:

    The attached DEVICE_OBJECT.

--*/
{
    const BUS_CHILD_DEVICE_CONTEXT* childContext = DMF_BusFilter_GetChildContext(ChildDevice);

    if (childContext)
    {
        const WDM_CHILD_DEVICE_EXTENSION* childExtension = (WDM_CHILD_DEVICE_EXTENSION*)childContext->DeviceObject->DeviceExtension;

        if (IsEqualGUID(childExtension->Signature,
                        GUID_DMF_BUSFILTER_SIGNATURE))
        {
            return childExtension->TargetDeviceObject;
        }
    }

    return NULL;
}

PDEVICE_OBJECT
DMF_BusFilter_WdmPhysicalDeviceGet(
    _In_ DMFBUSCHILDDEVICE ChildDevice
    )
/*++

Routine Description:

    Returns the associated physical DEVICE_OBJECT associated with a given DMFBUSCHILDDEVICE.

Arguments:

    ChildDevice - The given DMFBUSCHILDDEVICE.

Return Value:

    The associated physical (parent) DEVICE_OBJECT.

--*/
{
    const BUS_CHILD_DEVICE_CONTEXT* childContext = DMF_BusFilter_GetChildContext(ChildDevice);

    if (childContext)
    {
        const WDM_CHILD_DEVICE_EXTENSION* childExtension = (WDM_CHILD_DEVICE_EXTENSION*)childContext->DeviceObject->DeviceExtension;

        if (IsEqualGUID(childExtension->Signature,
                        GUID_DMF_BUSFILTER_SIGNATURE))
        {
            return childExtension->PhysicalDeviceObject;
        }
    }

    return NULL;
}

#endif // defined(DMF_KERNEL_MODE)

// eof: DmfFilter.c
//
