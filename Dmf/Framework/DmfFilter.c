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

#include "DmfFilter.tmh"

#if !defined(DMF_USER_MODE)

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

#endif // !defined(DMF_USER_MODE)

// eof: DmfFilter.c
//
