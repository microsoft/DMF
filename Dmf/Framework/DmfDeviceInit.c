/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfDeviceInit.c

Abstract:

    DMF Implementation:

    This Module has the support for initializing DMF Device Init.

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#include "DmfIncludeInternal.h"

#include "DmfDeviceInit.tmh"

// Memory Pool Tag.
//
#define MemoryTag 'dirB'

typedef struct DMFDEVICE_INIT
{
    // DMF Device Init Memory.
    //
    WDFMEMORY DmfDeviceInitMemory;

    // Is bridge enabled.
    //
    BOOLEAN BridgeEnabled;

    // Bridge Module Config.
    //
    VOID* DmfBridgeConfig;
    WDFMEMORY DmfBridgeConfigMemory;

    // Flag to indicate if Client Driver implements
    // an EVT_WDF_DRIVER_DEVICE_ADD callback.
    //
    BOOLEAN ClientImplementsDeviceAdd;

    // If TRUE, DMF_DmfDeviceInitHookPnpPowerCallbacks was called.
    //
    BOOLEAN PnpPowerCallbacksHooked;

    // If TRUE, DMF_DmfDeviceInitHookPowerPolicycallbacks was called.
    //
    BOOLEAN PowerPolicyCallbacksHooked;

    // If TRUE, DMF_DmfDeviceInitHookFileObjectConfig was called.
    //
    BOOLEAN FileObjectConfigHooked;

    // If TRUE, DMF_DmfDeviceInitHookQueueConfig was called.
    //
    BOOLEAN QueueConfigHooked;

    // DMF Event Callbacks.
    //
    DMF_EVENT_CALLBACKS* DmfEventCallbacks;

    // DMF BranchTrack Module Config.
    //
    DMF_CONFIG_BranchTrack* DmfBranchTrackModuleConfig;

    // DMF LiveKernelDump Module Config.
    //
    DMF_CONFIG_LiveKernelDump* DmfLiveKernelDumpModuleConfig;

    // Is DmfDeviceInit initialized for Control device.
    //
    BOOLEAN IsControlDevice;

    // Client Driver device associated with Control device
    // NULL if IsControlDevice is FALSE.
    //
    WDFDEVICE ClientDriverDevice;

    // Indicates that the Client Driver is a Filter driver.
    //
    BOOLEAN IsFilterDevice;
} *PDMFDEVICE_INIT;

// This is a sentinel for failed allocations. In this way, callers call to allocate always succeeds. It
// eliminates an if() in all Client drivers.
//
struct DMFDEVICE_INIT g_DmfDefaultDeviceInit = { 0 };

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Helper Functions
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_DmfDeviceInitIsBridgeEnabled(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    )
/*++

Routine Description:

    Let the caller know if DMF_Bridge is enabled or not.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.

Return Value:

    TRUE if Bridge is Enabled.
    FALSE otherwise.

--*/
{
    PAGED_CODE();

    ASSERT(DmfDeviceInit != NULL);
    return DmfDeviceInit->BridgeEnabled;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
DMF_CONFIG_Bridge*
DMF_DmfDeviceInitBridgeModuleConfigGet(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    )
/*++

Routine Description:

    Return the pointer to Bridge Module Config store in DMFDEVICE_INIT.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.

Return Value:

    Pointer to bridge Module Config.
    NULL if bridge is not enabled.

--*/
{
    PAGED_CODE();

    ASSERT(DmfDeviceInit != NULL);
    return (DMF_CONFIG_Bridge*)DmfDeviceInit->DmfBridgeConfig;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_DmfDeviceInitClientImplementsDeviceAdd(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    )
/*++

Routine Description:

    Let the caller know if Client Driver implements EVT_WDF_DRIVER_DEVICE_ADD.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.

Return Value:

    TRUE if Client Driver implements EVT_WDF_DRIVER_DEVICE_ADD.
    FALSE otherwise.

--*/
{
    PAGED_CODE();

    ASSERT(DmfDeviceInit != NULL);
    return DmfDeviceInit->ClientImplementsDeviceAdd;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_DmfDeviceInitIsControlDevice(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    )
/*++

Routine Description:

    Let the caller know if DmfDeviceInit is allocated for a Control device.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.

Return Value:

    TRUE if DmfDeviceInit is allocated for a Control device.
    FALSE otherwise.

--*/
{
    PAGED_CODE();

    ASSERT(DmfDeviceInit != NULL);
    return DmfDeviceInit->IsControlDevice;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_DmfDeviceInitIsFilterDriver(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    )
/*++

Routine Description:

    Let the caller know if DmfDeviceInit is allocated for a Filter driver.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.

Return Value:

    TRUE if DmfDeviceInit is allocated for a Control device.
    FALSE otherwise.

--*/
{
    PAGED_CODE();

    ASSERT(DmfDeviceInit != NULL);
    return DmfDeviceInit->IsFilterDevice;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFDEVICE
DMF_DmfControlDeviceInitClientDriverDeviceGet(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    )
/*++

Routine Description:

    Return the Client Driver device associated with DmfDeviceInit.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.

Return Value:

    Client Driver device if DmfDeviceInit is allocated for Control device.
    NULL otherwise.

--*/
{
    PAGED_CODE();

    ASSERT(DmfDeviceInit != NULL);
    return DmfDeviceInit->ClientDriverDevice;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_DmfDeviceInitIsDefaultQueueCreated(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    )
/*++

Routine Description:

    Let the caller know if Default queue is created for the device.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.

Return Value:

    TRUE if Default queue was created.
    FALSE otherwise.

--*/
{
    PAGED_CODE();

    ASSERT(DmfDeviceInit != NULL);
    return DmfDeviceInit->QueueConfigHooked;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
DMF_CONFIG_BranchTrack*
DMF_DmfDeviceInitBranchTrackModuleConfigGet(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    )
/*++

Routine Description:

    Return the pointer to BranchTrack Module Config stored in DMFDEVICE_INIT.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.

Return Value:

    Pointer to BranchTrack Module Config.
    NULL if BranchTrack is not enabled.

--*/
{
    DMF_CONFIG_BranchTrack* branchTrackConfig;

    PAGED_CODE();

    ASSERT(DmfDeviceInit != NULL);

    branchTrackConfig = (DMF_CONFIG_BranchTrack*)DmfDeviceInit->DmfBranchTrackModuleConfig;

    return branchTrackConfig;
}
#pragma code_seg()

#if !defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
DMF_CONFIG_LiveKernelDump*
DMF_DmfDeviceInitLiveKernelDumpModuleConfigGet(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    )
/*++

Routine Description:

    Return the pointer to LiveKernelDump Module Config stored in DMFDEVICE_INIT.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.

Return Value:

    Pointer to LiveKernelDump Module Config.

--*/
{
    DMF_CONFIG_LiveKernelDump* liveKernelDumpConfig;

    PAGED_CODE();

    ASSERT(DmfDeviceInit != NULL);

    liveKernelDumpConfig = (DMF_CONFIG_LiveKernelDump*)DmfDeviceInit->DmfLiveKernelDumpModuleConfig;

    return liveKernelDumpConfig;
}
#pragma code_seg()

#endif // !defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
DMF_EVENT_CALLBACKS*
DMF_DmfDeviceInitDmfEventCallbacksGet(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    )
/*++

Routine Description:

    Return the pointer to DMF Event Callbacks stored in DMFDEVICE_INIT.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.

Return Value:

    Pointer to DMF Event Callbacks.
    NULL if not set.

--*/
{
    PAGED_CODE();

    ASSERT(DmfDeviceInit != NULL);
    return (DMF_EVENT_CALLBACKS*)DmfDeviceInit->DmfEventCallbacks;
}
#pragma code_seg()

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Client Driver APIs related to PDMFDEVICE_INIT
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
PDMFDEVICE_INIT
DMF_DmfDeviceInitAllocate(
    _In_opt_ PWDFDEVICE_INIT DeviceInit
    )
/*++

Routine Description:

    Allocate DmfDeviceInitGlobal variable of type DMFDEVICE_INIT, and return the address of it.

Parameters Description:

    DeviceInit - A pointer to a WDF allocated WDFDEVICE_INIT structure.

Return Value:

    Address of DmfDeviceInitGlobal.

--*/
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
    WDF_FILEOBJECT_CONFIG fileObjectConfig;
    WDF_OBJECT_ATTRIBUTES fileObjectAttributes;
    WDF_POWER_POLICY_EVENT_CALLBACKS powerPolicyCallbacks;
    PDMFDEVICE_INIT dmfDeviceInit;
    WDFMEMORY dmfDeviceInitMemory;
    NTSTATUS ntStatus;

    PAGED_CODE();

    ntStatus = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES,
                               NonPagedPoolNx,
                               MemoryTag,
                               sizeof(struct DMFDEVICE_INIT),
                               &dmfDeviceInitMemory,
                               (VOID**)&dmfDeviceInit);
    if (! NT_SUCCESS(ntStatus))
    {
        // Set the sentinel for failed allocation. Client Driver does not need to check.
        // Failure is dealt with later. (Note: It eliminates an if() for all Client drivers for a condition that will
        // probably never occur.
        //
        dmfDeviceInit = &g_DmfDefaultDeviceInit;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate failed! %!STATUS!", ntStatus);
        goto Exit;
    }

    RtlZeroMemory(dmfDeviceInit,
                  sizeof(struct DMFDEVICE_INIT));

    // Allocate memory to store Bridge Module Config.
    //
    ntStatus = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES,
                               NonPagedPoolNx,
                               MemoryTag,
                               sizeof(DMF_CONFIG_Bridge),
                               &dmfDeviceInit->DmfBridgeConfigMemory,
                               &dmfDeviceInit->DmfBridgeConfig);
    if (! NT_SUCCESS(ntStatus))
    {
        WdfObjectDelete(dmfDeviceInitMemory);
        // Set the sentinel for failed allocation. Client Driver does not need to check.
        // Failure is dealt with later. (Note: It eliminates an if() for all Client drivers for a condition that will
        // probably never occur.
        //
        dmfDeviceInit = &g_DmfDefaultDeviceInit;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate failed! %!STATUS!", ntStatus);
        goto Exit;
    }

    RtlZeroMemory(dmfDeviceInit->DmfBridgeConfig,
                  sizeof(DMF_CONFIG_Bridge));

    dmfDeviceInit->DmfDeviceInitMemory = dmfDeviceInitMemory;
    dmfDeviceInit->BridgeEnabled = TRUE;
    dmfDeviceInit->IsControlDevice = FALSE;
    dmfDeviceInit->ClientDriverDevice = NULL;

    if (DeviceInit != NULL)
    {
        dmfDeviceInit->ClientImplementsDeviceAdd = TRUE;

        WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
        DMF_ContainerPnpPowerCallbacksInit(&pnpPowerCallbacks);
        WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit,
                                               &pnpPowerCallbacks);

        DMF_ContainerFileObjectConfigInit(&fileObjectConfig);

        WDF_OBJECT_ATTRIBUTES_INIT(&fileObjectAttributes);

        WdfDeviceInitSetFileObjectConfig(DeviceInit,
                                         &fileObjectConfig,
                                         &fileObjectAttributes);

        WDF_POWER_POLICY_EVENT_CALLBACKS_INIT(&powerPolicyCallbacks);
        DMF_ContainerPowerPolicyCallbacksInit(&powerPolicyCallbacks);
        WdfDeviceInitSetPowerPolicyEventCallbacks(DeviceInit,
                                                  &powerPolicyCallbacks);
    }
    else
    {
        // If DeviceInit is NULL, do not set any WDF callbacks and
        // do not create a default queue.
        // Drivers which pass NULL for DeviceInit, will Invoke callbacks manually when needed.
        //
        dmfDeviceInit->PnpPowerCallbacksHooked = TRUE;
        dmfDeviceInit->PowerPolicyCallbacksHooked = TRUE;
        dmfDeviceInit->FileObjectConfigHooked = TRUE;
        dmfDeviceInit->QueueConfigHooked = TRUE;
        dmfDeviceInit->ClientImplementsDeviceAdd = FALSE;
    }

Exit:

    return dmfDeviceInit;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
PDMFDEVICE_INIT
DMF_DmfControlDeviceInitAllocate(
    _In_opt_ PWDFDEVICE_INIT DeviceInit
    )
/*++

Routine Description:

    Allocate DmfDeviceInitGlobal variable of type DMFDEVICE_INIT for a control device,
    and return the address of it.

Parameters Description:

    DeviceInit - A pointer to a WDF allocated WDFDEVICE_INIT structure.

Return Value:

    Address of DmfDeviceInitGlobal.

--*/
{
    WDF_FILEOBJECT_CONFIG fileObjectConfig;
    WDF_OBJECT_ATTRIBUTES fileObjectAttributes;
    PDMFDEVICE_INIT dmfDeviceInit;
    WDFMEMORY dmfDeviceInitMemory;
    NTSTATUS ntStatus;

    PAGED_CODE();

    ntStatus = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES,
                               NonPagedPoolNx,
                               MemoryTag,
                               sizeof(struct DMFDEVICE_INIT),
                               &dmfDeviceInitMemory,
                               (VOID**)&dmfDeviceInit);
    if (! NT_SUCCESS(ntStatus))
    {
        dmfDeviceInit = NULL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate failed! %!STATUS!", ntStatus);
        goto Exit;
    }

    RtlZeroMemory(dmfDeviceInit,
                  sizeof(struct DMFDEVICE_INIT));

    // Allocate memory to store Bridge Module Config.
    //
    ntStatus = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES,
                               NonPagedPoolNx,
                               MemoryTag,
                               sizeof(DMF_CONFIG_Bridge),
                               &dmfDeviceInit->DmfBridgeConfigMemory,
                               &dmfDeviceInit->DmfBridgeConfig);
    if (! NT_SUCCESS(ntStatus))
    {
        WdfObjectDelete(dmfDeviceInitMemory);
        dmfDeviceInit = NULL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate failed! %!STATUS!", ntStatus);
        goto Exit;
    }

    RtlZeroMemory(dmfDeviceInit->DmfBridgeConfig,
                  sizeof(DMF_CONFIG_Bridge));

    dmfDeviceInit->DmfDeviceInitMemory = dmfDeviceInitMemory;
    dmfDeviceInit->BridgeEnabled = TRUE;
    dmfDeviceInit->IsControlDevice = TRUE;
    dmfDeviceInit->IsFilterDevice = FALSE;
    dmfDeviceInit->ClientDriverDevice = NULL;

    if (DeviceInit != NULL)
    {
        dmfDeviceInit->ClientImplementsDeviceAdd = TRUE;

        // For Control Device, do not set PnpPower and PowerPolicy callbacks.
        //
        dmfDeviceInit->PnpPowerCallbacksHooked = TRUE;
        dmfDeviceInit->PowerPolicyCallbacksHooked = TRUE;

        DMF_ContainerFileObjectConfigInit(&fileObjectConfig);

        WDF_OBJECT_ATTRIBUTES_INIT(&fileObjectAttributes);

        WdfDeviceInitSetFileObjectConfig(DeviceInit,
                                         &fileObjectConfig,
                                         &fileObjectAttributes);
    }
    else
    {
        // If DeviceInit is NULL, do not set any WDF callbacks and
        // do not create a default queue.
        // Drivers which pass NULL for DeviceInit, will Invoke callbacks manually when needed.
        //
        dmfDeviceInit->PnpPowerCallbacksHooked = TRUE;
        dmfDeviceInit->PowerPolicyCallbacksHooked = TRUE;
        dmfDeviceInit->FileObjectConfigHooked = TRUE;
        dmfDeviceInit->QueueConfigHooked = TRUE;
        dmfDeviceInit->ClientImplementsDeviceAdd = FALSE;
    }

Exit:
    return dmfDeviceInit;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfControlDeviceInitSetClientDriverDevice(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Store the given Client Driver device in DmfDeviceInit structure for Control device.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.
    Device - The given Client Driver device.

Return Value:

    None

--*/
{
    PAGED_CODE();

    // DmfDeviceInit will be set to &g_DmfDefaultDeviceInit if Allocate failed.
    // Since error checking happens only on DMF_ModulesCreate,
    // if DmfDeviceInit is set to &g_DmfDefaultDeviceInit, this function is a NOP.
    //
    if (DmfDeviceInit != &g_DmfDefaultDeviceInit)
    {
        // This API should only be called for Control device.
        //
        ASSERT(DmfDeviceInit->IsControlDevice);

        DmfDeviceInit->ClientDriverDevice = Device;
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_DmfDeviceInitValidate(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    )
/*++

Routine Description:

    Validate DmfDeviceInit.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.

Return Value:

    TRUE if DmfDeviceInit is initialized correctly.

--*/
{
    BOOLEAN dmfDeviceInitValid;

    PAGED_CODE();

    dmfDeviceInitValid = FALSE;

    // Check if the pointer passed in is valid.
    //
    if (DmfDeviceInit == &g_DmfDefaultDeviceInit)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfDeviceInit invalid");
        goto Exit;
    }

    // Check for WDFMEMORY handles.
    //
    if (DmfDeviceInit->DmfDeviceInitMemory == NULL ||
        DmfDeviceInit->DmfBridgeConfigMemory == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfDeviceInit Memory invalid");
        goto Exit;
    }

    // Check if bridge is enabled.
    //
    if (! DmfDeviceInit->BridgeEnabled)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfDeviceInit Bridge not enabled");
        goto Exit;
    }

    // Mandate all hook APIs except QueueConfig.
    // For QueueConfig, if not hooked, DMF will create a default queue.
    //
    if (! DmfDeviceInit->PnpPowerCallbacksHooked)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_DmfDeviceInitHookPnpPowerEventCallbacks not called!");
        goto Exit;
    }
    if (! DmfDeviceInit->PowerPolicyCallbacksHooked)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_DmfDeviceInitHookPowerPolicyEventCallbacks not called!");
        goto Exit;
    }
    if (! DmfDeviceInit->FileObjectConfigHooked)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_DmfDeviceInitHookFileObjectConfig not called!");
        goto Exit;
    }

    // For Control device, Client Driver device has to be set.
    //
    if (DmfDeviceInit->IsControlDevice)
    {
        if (DmfDeviceInit->ClientDriverDevice == NULL)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_DmfControlDeviceInitSetClientDriverDevice not called!");
            goto Exit;
        }
    }
    else
    {
        if (DmfDeviceInit->ClientDriverDevice != NULL)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_DmfControlDeviceInitSetClientDriverDevice should not be called!");
            goto Exit;
        }
    }

    dmfDeviceInitValid = TRUE;

Exit:
    return dmfDeviceInitValid;
}
#pragma code_seg()

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_DmfDeviceInitFree(
    _In_ PDMFDEVICE_INIT* DmfDeviceInitPointer
    )
/*++

Routine Description:

    Free memory allocated for DmfDeviceInit variable.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.

Return Value:

    Address of DmfDeviceInitGlobal.

--*/
{
    PDMFDEVICE_INIT dmfDeviceInit;
    WDFMEMORY dmfDeviceInitMemory;

    ASSERT(DmfDeviceInitPointer != NULL);

    dmfDeviceInit = *DmfDeviceInitPointer;
    if (NULL == dmfDeviceInit)
    {
        // It is possible and legitimate it can be NULL if allocation failed.
        //
        goto Exit;
    }

    ASSERT(dmfDeviceInit != NULL);

    dmfDeviceInitMemory = dmfDeviceInit->DmfDeviceInitMemory;
    ASSERT(dmfDeviceInitMemory != NULL);
    ASSERT(dmfDeviceInit->BridgeEnabled == TRUE);
    ASSERT(dmfDeviceInit->DmfBridgeConfigMemory != NULL);

    WdfObjectDelete(dmfDeviceInit->DmfBridgeConfigMemory);
    dmfDeviceInit->DmfBridgeConfigMemory = NULL;
    WdfObjectDelete(dmfDeviceInitMemory);
    dmfDeviceInitMemory = NULL;
    // Clear this so the caller can free this memory without checking it.
    //
    *DmfDeviceInitPointer = NULL;

Exit:
    ;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfDeviceInitHookPnpPowerEventCallbacks(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _Inout_opt_ PWDF_PNPPOWER_EVENT_CALLBACKS PnpPowerEventCallbacks
    )
/*++

Routine Description:

    Registers driver's Plug and Play and power management event callback functions with DMF and
    replace the driver's Plug and Play and power management event callback functions with DMF's callbacks.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.
    PnpPowerEventCallbacks - A pointer to a caller-initialized WDF_PNPPOWER_EVENT_CALLBACKS structure.

Return Value:

    None

--*/
{
    DMF_CONFIG_Bridge* bridgeModuleConfig;

    PAGED_CODE();

    // DmfDeviceInit will be set to &g_DmfDefaultDeviceInit if Allocate failed.
    // Since error checking happens only on DMF_ModulesCreate,
    // if DmfDeviceInit is set to &g_DmfDefaultDeviceInit, this function is a NOP.
    //
    if (DmfDeviceInit != &g_DmfDefaultDeviceInit)
    {
        // Hook APIs should be called only once per device instance.
        //
        ASSERT(DmfDeviceInit->PnpPowerCallbacksHooked != TRUE);
        // DMF_DmfDeviceInitHookPnpPowerEventCallbacks should not be called for Control device.
        //
        ASSERT(DmfDeviceInit->IsControlDevice != TRUE);

        if (PnpPowerEventCallbacks != NULL)
        {
            bridgeModuleConfig = (DMF_CONFIG_Bridge*)DmfDeviceInit->DmfBridgeConfig;
            ASSERT(bridgeModuleConfig != NULL);

            bridgeModuleConfig->EvtDevicePrepareHardware = PnpPowerEventCallbacks->EvtDevicePrepareHardware;
            bridgeModuleConfig->EvtDeviceReleaseHardware = PnpPowerEventCallbacks->EvtDeviceReleaseHardware;
            bridgeModuleConfig->EvtDeviceD0Entry = PnpPowerEventCallbacks->EvtDeviceD0Entry;
            bridgeModuleConfig->EvtDeviceD0Exit = PnpPowerEventCallbacks->EvtDeviceD0Exit;
            bridgeModuleConfig->EvtDeviceSelfManagedIoCleanup = PnpPowerEventCallbacks->EvtDeviceSelfManagedIoCleanup;
            bridgeModuleConfig->EvtDeviceSelfManagedIoFlush = PnpPowerEventCallbacks->EvtDeviceSelfManagedIoFlush;
            bridgeModuleConfig->EvtDeviceSelfManagedIoInit = PnpPowerEventCallbacks->EvtDeviceSelfManagedIoInit;
            bridgeModuleConfig->EvtDeviceSelfManagedIoSuspend = PnpPowerEventCallbacks->EvtDeviceSelfManagedIoSuspend;
            bridgeModuleConfig->EvtDeviceSelfManagedIoRestart = PnpPowerEventCallbacks->EvtDeviceSelfManagedIoRestart;
            bridgeModuleConfig->EvtDeviceSurpriseRemoval = PnpPowerEventCallbacks->EvtDeviceSurpriseRemoval;
            bridgeModuleConfig->EvtDeviceQueryRemove = PnpPowerEventCallbacks->EvtDeviceQueryRemove;
            bridgeModuleConfig->EvtDeviceQueryStop = PnpPowerEventCallbacks->EvtDeviceQueryStop;
            bridgeModuleConfig->EvtDeviceRelationsQuery = PnpPowerEventCallbacks->EvtDeviceRelationsQuery;
            bridgeModuleConfig->EvtDeviceUsageNotificationEx = PnpPowerEventCallbacks->EvtDeviceUsageNotificationEx;

            DMF_ContainerPnpPowerCallbacksInit(PnpPowerEventCallbacks);
        }

        DmfDeviceInit->PnpPowerCallbacksHooked = TRUE;
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfDeviceInitHookFileObjectConfig(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _Inout_opt_ PWDF_FILEOBJECT_CONFIG FileObjectConfig
    )
/*++

Routine Description:

    Registers event callback functions for the driver's framework file objects with DMF and
    replace the driver's framework file objects event callback functions with DMF's callbacks.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.
    FileObjectConfig - A pointer to a caller-allocated WDF_FILEOBJECT_CONFIG structure.

Return Value:

    None

--*/
{
    DMF_CONFIG_Bridge* bridgeModuleConfig;
    WDF_FILEOBJECT_CONFIG fileObjectConfig;

    PAGED_CODE();

    // DmfDeviceInit will be set to &g_DmfDefaultDeviceInit if Allocate failed.
    // Since error checking happens only on DMF_ModulesCreate,
    // if DmfDeviceInit is set to &g_DmfDefaultDeviceInit, this function is a NOP.
    //
    if (DmfDeviceInit != &g_DmfDefaultDeviceInit)
    {
        // Hook APIs should be called only once per device instance.
        //
        ASSERT(DmfDeviceInit->FileObjectConfigHooked != TRUE);

        if (FileObjectConfig != NULL)
        {
            bridgeModuleConfig = (DMF_CONFIG_Bridge*)DmfDeviceInit->DmfBridgeConfig;
            ASSERT(bridgeModuleConfig != NULL);

            bridgeModuleConfig->EvtFileCreate = FileObjectConfig->EvtDeviceFileCreate;
            bridgeModuleConfig->EvtFileCleanup = FileObjectConfig->EvtFileCleanup;
            bridgeModuleConfig->EvtFileClose = FileObjectConfig->EvtFileClose;

            DMF_ContainerFileObjectConfigInit(&fileObjectConfig);

            FileObjectConfig->EvtDeviceFileCreate = fileObjectConfig.EvtDeviceFileCreate;
            FileObjectConfig->EvtFileCleanup = fileObjectConfig.EvtFileCleanup;
            FileObjectConfig->EvtFileClose = fileObjectConfig.EvtFileClose;
        }

        DmfDeviceInit->FileObjectConfigHooked = TRUE;
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfDeviceInitHookPowerPolicyEventCallbacks(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _Inout_opt_ PWDF_POWER_POLICY_EVENT_CALLBACKS PowerPolicyEventCallbacks
    )
/*++

Routine Description:

    Registers driver's power policy event callback functions with DMF. and
    replace the driver's power policy event callback functions with DMF's callbacks.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.
    PowerPolicyEventCallbacks - A pointer to a caller-initialized WDF_POWER_POLICY_EVENT_CALLBACKS structure.

Return Value:

    None

--*/
{
    DMF_CONFIG_Bridge* bridgeModuleConfig;

    PAGED_CODE();

    // DmfDeviceInit will be set to &g_DmfDefaultDeviceInit if Allocate failed.
    // Since error checking happens only on DMF_ModulesCreate,
    // if DmfDeviceInit is set to &g_DmfDefaultDeviceInit, this function is a NOP.
    //
    if (DmfDeviceInit != &g_DmfDefaultDeviceInit)
    {
        // Hook APIs should be called only once per device instance.
        //
        ASSERT(DmfDeviceInit->PowerPolicyCallbacksHooked != TRUE);
        // DMF_DmfDeviceInitHookPowerPolicyEventCallbacks should not be called for Control device.
        //
        ASSERT(DmfDeviceInit->IsControlDevice != TRUE);

        if (PowerPolicyEventCallbacks != NULL)
        {
            bridgeModuleConfig = (DMF_CONFIG_Bridge*)DmfDeviceInit->DmfBridgeConfig;
            ASSERT(bridgeModuleConfig != NULL);

            bridgeModuleConfig->EvtDeviceArmWakeFromS0 = PowerPolicyEventCallbacks->EvtDeviceArmWakeFromS0;
            bridgeModuleConfig->EvtDeviceDisarmWakeFromS0 = PowerPolicyEventCallbacks->EvtDeviceDisarmWakeFromS0;
            bridgeModuleConfig->EvtDeviceWakeFromS0Triggered = PowerPolicyEventCallbacks->EvtDeviceWakeFromS0Triggered;
            bridgeModuleConfig->EvtDeviceArmWakeFromSxWithReason = PowerPolicyEventCallbacks->EvtDeviceArmWakeFromSxWithReason;
            bridgeModuleConfig->EvtDeviceDisarmWakeFromSx = PowerPolicyEventCallbacks->EvtDeviceDisarmWakeFromSx;
            bridgeModuleConfig->EvtDeviceWakeFromSxTriggered = PowerPolicyEventCallbacks->EvtDeviceWakeFromSxTriggered;

            DMF_ContainerPowerPolicyCallbacksInit(PowerPolicyEventCallbacks);
        }

        DmfDeviceInit->PowerPolicyCallbacksHooked = TRUE;
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfDeviceInitHookQueueConfig(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _Inout_ PWDF_IO_QUEUE_CONFIG QueueConfig
    )
/*++

Routine Description:

    Registers driver's default queue event callback functions with DMF and
    replace the driver's default queue event callback functions with DMF's callbacks.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.
    QueueConfig - A pointer to a caller-initialized WDF_IO_QUEUE_CONFIG structure.

Return Value:

    None

--*/
{
    PAGED_CODE();

    // DmfDeviceInit will be set to &g_DmfDefaultDeviceInit if Allocate failed.
    // Since error checking happens only on DMF_ModulesCreate,
    // if DmfDeviceInit is set to &g_DmfDefaultDeviceInit, this function is a NOP.
    //
    if (DmfDeviceInit != &g_DmfDefaultDeviceInit)
    {
        if (QueueConfig != NULL)
        {
            DMF_CONFIG_Bridge* bridgeModuleConfig;

            bridgeModuleConfig = (DMF_CONFIG_Bridge*)DmfDeviceInit->DmfBridgeConfig;
            ASSERT(bridgeModuleConfig != NULL);
#if !defined(DMF_USER_MODE)
            bridgeModuleConfig->EvtInternalDeviceIoControl = QueueConfig->EvtIoInternalDeviceControl;
#endif // !defined(DMF_USER_MODE)
            bridgeModuleConfig->EvtDeviceIoControl = QueueConfig->EvtIoDeviceControl;
            bridgeModuleConfig->EvtQueueIoRead = QueueConfig->EvtIoRead;
            bridgeModuleConfig->EvtQueueIoWrite = QueueConfig->EvtIoWrite;

            DMF_ContainerQueueConfigCallbacksInit(QueueConfig);
        }
        // If the Client Driver does not call this function, by default DMF assumes that
        // it should create its own default queue. Otherwise, there are two cases:
        // 1. Client Driver created the queue above so DMF should not create the queue.
        // 2. Client Driver does not want a default queue created.
        // This flag is checked later to determine if DMF should create a default queue.
        //
        DmfDeviceInit->QueueConfigHooked = TRUE;
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfFdoSetFilter(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    )
/*++

Routine Description:

    Tells DMF that the Client Driver is a Filter driver. This is necessary to enable passthru
    of requests to lower stack.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.

Return Value:

    None

--*/
{
    PAGED_CODE();

    ASSERT(! DmfDeviceInit->IsFilterDevice);
    if (DmfDeviceInit != &g_DmfDefaultDeviceInit)
    {
        DmfDeviceInit->IsFilterDevice = TRUE;
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfDeviceInitSetEventCallbacks(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _In_ DMF_EVENT_CALLBACKS* DmfEventCallbacks
    )
/*++

Routine Description:

    Registers DMF event callback functions with DMF

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.
    DmfEventCallbacks - A pointer to a caller-initialized DMF_EVENT_CALLBACKS structure.

Return Value:

    None

--*/
{
    PAGED_CODE();

    // DmfDeviceInit will be set to &g_DmfDefaultDeviceInit if Allocate failed.
    // Since error checking happens only on DMF_ModulesCreate,
    // if DmfDeviceInit is set to &g_DmfDefaultDeviceInit, this function is a NOP.
    //
    if (DmfDeviceInit != &g_DmfDefaultDeviceInit)
    {
        ASSERT(DmfEventCallbacks != NULL);

        DmfDeviceInit->DmfEventCallbacks = DmfEventCallbacks;
    }
}
#pragma code_seg()

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Feature Module Config Initialization Functions
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfDeviceInitSetBranchTrackConfig(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _In_opt_ DMF_CONFIG_BranchTrack* DmfBranchTrackModuleConfig
    )
/*++

Routine Description:

    Set BranchTrack Config.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.
    DmfBranchTrackModuleConfig - A pointer to a caller-initialized DMF_CONFIG_BranchTrack structure.

Return Value:

    None

--*/
{
    PAGED_CODE();

    // DmfDeviceInit will be set to &g_DmfDefaultDeviceInit if Allocate failed.
    // Since error checking happens only on DMF_ModulesCreate,
    // if DmfDeviceInit is set to &g_DmfDefaultDeviceInit, this function is a NOP.
    //
    if (DmfDeviceInit != &g_DmfDefaultDeviceInit)
    {
        ASSERT(DmfBranchTrackModuleConfig != NULL);

        DmfDeviceInit->DmfBranchTrackModuleConfig = DmfBranchTrackModuleConfig;
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfDeviceInitSetLiveKernelDumpConfig(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _In_ DMF_CONFIG_LiveKernelDump* DmfLiveKernelDumpModuleConfig
    )
/*++

Routine Description:

    Set LiveKernelDump Config.

Parameters Description:

    DmfDeviceInit - A pointer to a framework-allocated DMFDEVICE_INIT structure.
    DmfLiveKernelDumpModuleConfig - A pointer to a caller-initialized DMF_CONFIG_LiveKernelDump structure.

Return Value:

    None

--*/
{
    PAGED_CODE();

    // DmfDeviceInit will be set to &g_DmfDefaultDeviceInit if Allocate failed.
    // Since error checking happens only on DMF_ModulesCreate,
    // if DmfDeviceInit is set to &g_DmfDefaultDeviceInit, this function is a NOP.
    //
    if (DmfDeviceInit != &g_DmfDefaultDeviceInit)
    {
        ASSERT(DmfLiveKernelDumpModuleConfig != NULL);

        DmfDeviceInit->DmfLiveKernelDumpModuleConfig = DmfLiveKernelDumpModuleConfig;

        // Ensure ReportType is a null terminated string.
        //
        DmfDeviceInit->DmfLiveKernelDumpModuleConfig->ReportType[DMF_LIVEKERNELDUMP_MAXIMUM_REPORT_TYPE_SIZE - 1] = L'\0';
    }
}
#pragma code_seg()

// eof: DmfDeviceInit.c
//
