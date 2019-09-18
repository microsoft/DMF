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

_IRQL_always_function_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_FilterControl_DeviceCreate(
    _In_ WDFDEVICE Device,
    _In_opt_ DMF_CONFIG_BranchTrack* FilterBranchTrackConfig,
    _In_opt_ PWDF_IO_QUEUE_CONFIG QueueConfig,
    _In_ PWCHAR ControlDeviceName
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

    controlDevice = NULL;
    dmfDeviceInit = NULL;
    queue = NULL;

    driver = WdfDeviceGetDriver(Device);
    dmfDeviceContext = DmfDeviceContextGet(Device);

    // DMF_FilterControl_DeviceCreate should be called only once per WdfDevice instance.
    //
    DmfAssert(dmfDeviceContext->WdfControlDevice == NULL);

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

    ntStatus = STATUS_SUCCESS;
    goto Exit;

Error:

    if (deviceInit != NULL)
    {
        WdfDeviceInitFree(deviceInit);
        deviceInit = NULL;
    }

    if (controlDevice != NULL)
    {
        // Release the reference on the newly created object, since
        // we couldn't initialize it.
        //
        WdfObjectDelete(controlDevice);
        controlDevice = NULL;
    }

Exit:

    dmfDeviceContext->WdfControlDevice = controlDevice;

    return ntStatus;
}

_IRQL_always_function_max_(PASSIVE_LEVEL)
VOID
DMF_FilterControl_DeviceDelete(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    This routine deletes the control device which was create for the given WdfDevice instance.

Arguments:

    Device - The given WDFDEVICE.

Return Value:

    None

--*/
{
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    FuncEntry(DMF_TRACE);

    dmfDeviceContext = DmfDeviceContextGet(Device);

    if (dmfDeviceContext->WdfControlDevice != NULL)
    {
        WdfObjectDelete(dmfDeviceContext->WdfControlDevice);
        dmfDeviceContext->WdfControlDevice = NULL;
    }

    FuncExitVoid(DMF_TRACE);
}

#endif // !defined(DMF_USER_MODE)

// eof: DmfFilter.c
//
