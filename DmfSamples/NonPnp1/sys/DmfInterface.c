/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    DmfInterface.c

Abstract:

    NonPnp Sample: This sample shows how to write a Non-Pnp (Control) driver. This type of driver is usually
                   loaded by an application that needs to perform an action in Kernel-mode. Such drivers do
                   not receive DeviceAdd() nor any PnP callbacks. However, it is possible to write such a 
                   driver using DMF as this sample shows. This driver simply instantiates a Module that
                   creates a symbolic link. Then, an application can open the driver and send/receive data
                   using an IOCTL interface.

Environment:

    Kernel-mode

--*/

// The DMF Library and the DMF Library Modules this driver uses.
// In this sample, the driver uses the template library.
//
#include "DmfModules.Template.h"

#include "Trace.h"
#include "DmfInterface.tmh"

// This driver does not use the default DMF driver/device macros because special code is needed in
// those functions for Non-PnP drivers.
//
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_OBJECT_CONTEXT_CLEANUP NonPnp_EvtDriverContextCleanup;
EVT_WDF_DRIVER_UNLOAD NonPnp_EvtDriverUnload;
EVT_DMF_DEVICE_MODULES_ADD NonPnp_DmfModulesAdd;
NTSTATUS NonPnp_ControlDeviceCreate(_In_ PWDFDEVICE_INIT deviceInit);

/*WPP_INIT_TRACING(); (This comment is necessary for WPP Scanner.)*/

#define MemoryTag 'pnPN'
#define NONPNP_DEVICE_NAME  L"\\Device\\NonPnp1"

typedef struct
{
    WDFDEVICE WdfDevice;
    DMFMODULE DmfModuleNonPnp;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceContextGet)

#pragma code_seg("INIT")
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject, 
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    DriverEntry initializes the driver and is the first routine called by the
    system after the driver is loaded.

Arguments:

    DriverObject: Represents the instance of the function driver that is loaded
                  into memory. DriverEntry must initialize members of DriverObject before it
                  returns to the caller. DriverObject is allocated by the system before the
                  driver is loaded, and it is released by the system after the system unloads
                  the function driver from memory.
    RegistryPath: Represents the driver specific path in the Registry.
                  The function driver can use the path to store driver related data between
                  reboots. The path does not store hardware instance specific data.

Returns:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL or another NTSTATUS error code otherwise.

--*/
{
    WDF_DRIVER_CONFIG driverConfigDetails;
    NTSTATUS  ntStatus;
    WDF_OBJECT_ATTRIBUTES driverAttributes;
    WDFDRIVER driver;
    PWDFDEVICE_INIT deviceInit;

    PAGED_CODE();

    driver = NULL;
    deviceInit = NULL;

    WDF_DRIVER_CONFIG_INIT(&driverConfigDetails,
                           WDF_NO_EVENT_CALLBACK);

    // This flag tells WDF this driver is a non-Pnp driver. 
    //
    driverConfigDetails.DriverInitFlags |= WdfDriverInitNonPnpDriver;

    // NOTE: Non-PnP drivers must register for this callback. Otherwise, the driver
    //       will fail to unload properly. (The callback does not need to do anything.)
    //
    driverConfigDetails.EvtDriverUnload = NonPnp_EvtDriverUnload;
    driverConfigDetails.DriverPoolTag = MemoryTag;

    // Register a cleanup callback so that we can call WPP_CLEANUP when the framework
    // driver object is deleted during driver unload.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&driverAttributes);
    driverAttributes.EvtCleanupCallback = NonPnp_EvtDriverContextCleanup;

    ntStatus = WdfDriverCreate(DriverObject,
                                RegistryPath,
                                &driverAttributes,
                                &driverConfigDetails,
                                &driver);
    if ( !NT_SUCCESS ( ntStatus ) )
    {
        goto Exit;
    }

    WPP_INIT_TRACING(DriverObject,
                     RegistryPath);

    // Create opaque WDFDEVICE_INIT structure.
    //
    deviceInit = WdfControlDeviceInitAllocate(driver, 
                                              &SDDL_DEVOBJ_SYS_ALL_ADM_ALL);
    if (NULL == deviceInit)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    ntStatus = NonPnp_ControlDeviceCreate(deviceInit);
    if ( !NT_SUCCESS ( ntStatus ) )
    {
        goto Exit;
    }

Exit:

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
#pragma code_seg("PAGE")
VOID
NonPnp_EvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
    )
/*++

Routine Description:

    Called when the driver unloads. This driver uses this callback to uninitialize
    WPP tracing.

Arguments:

    DriverObject: The WDFOBJECT passed to DriverEntry.

Returns:

    NTSTATUS

--*/
{
    PAGED_CODE();

    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}
#pragma code_seg()

#pragma code_seg("PAGE")
VOID
NonPnp_EvtDriverUnload(
    _In_ WDFDRIVER Driver
    )
/*++

Routine Description:

    Called by WDF when the driver unloads.
    NOTE: Non-PnP drivers must register for this callback; otherwise, the
          driver will fail to unload.
    NOTE: If, by design, the driver should remain loaded always, then do 
          not register for this callback.

Arguments:

    Driver: WDFDRIVER object passed in DriverEntry.

Returns:

    None

--*/
{
    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    // NOTE: Control Device will be deleted when its parent Driver object
    //       is deleted by WDF.
    //
}
#pragma code_seg()

NTSTATUS
NonPnp_ControlDeviceCreate(
    _In_ PWDFDEVICE_INIT DeviceInit
    )
/*++

Routine Description:

    Creates a WDFDEVICE and instantiates DMF Modules for a Non-PnP driver.

Arguments:

    DeviceInit - Allocated by the caller so that a WDFDEVICE can be created.

Returns:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDFDEVICE device;
    DEVICE_CONTEXT* deviceContext;
    DMF_EVENT_CALLBACKS dmfEventCallbacks;
    PDMFDEVICE_INIT dmfDeviceInit;
    DECLARE_CONST_UNICODE_STRING(deviceName,
                                 NONPNP_DEVICE_NAME);

    // Non-PnP drivers that use DMF must call DMF_DmfControlDeviceInitAllocate passing
    // a valid PDEVICE_INIT structure (allocated in DriverEntry).
    // This allows DMF to perform specific functions associated with Non-PnP drivers.
    //
    dmfDeviceInit = DMF_DmfControlDeviceInitAllocate(DeviceInit);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes,
                                            DEVICE_CONTEXT);
    deviceAttributes.ExecutionLevel = WdfExecutionLevelPassive;

    // Assign the device name to the device.
    //
    ntStatus = WdfDeviceInitAssignName(DeviceInit,
                                       &deviceName);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    WdfDeviceInitSetIoType(DeviceInit, 
                           WdfDeviceIoBuffered);

    WdfDeviceInitSetDeviceType(DeviceInit, 
                               FILE_DEVICE_UNKNOWN);

    ntStatus = WdfDeviceCreate(&DeviceInit,
                               &deviceAttributes,
                               &device );
    if ( !NT_SUCCESS ( ntStatus ) )
    {
        goto Exit;
    }

    deviceContext = DeviceContextGet(device);

    deviceContext->WdfDevice = device; 

    // It is important that NonPnp driver tell DMF they are not PnP drivers
    // before calling DMF_ModulesCreate.
    //
    DMF_DmfControlDeviceInitSetClientDriverDevice(dmfDeviceInit,
                                                  device);

    // Initialize DMF and the Modules it will use.
    //
    DMF_EVENT_CALLBACKS_INIT(&dmfEventCallbacks);
    dmfEventCallbacks.EvtDmfDeviceModulesAdd = NonPnp_DmfModulesAdd;
    DMF_DmfDeviceInitSetEventCallbacks(dmfDeviceInit,
                                       &dmfEventCallbacks);
    ntStatus = DMF_ModulesCreate(device,
                                 &dmfDeviceInit);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Control devices must notify WDF when they are done initializing. I/O is
    // rejected until this call is made.
    //
    WdfControlFinishInitializing(device);

Exit:

    if (!NT_SUCCESS(ntStatus))
    {
        // Only free deviceInit if the driver fails to load. Otherwise, DMF will do so.
        //
        if (DeviceInit != NULL)
        {
            WdfDeviceInitFree(DeviceInit);
        }
        DMF_DmfDeviceInitFree(&dmfDeviceInit);
    }

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
#pragma code_seg("PAGED")
VOID
NonPnp_DmfModulesAdd(
    _In_ WDFDEVICE Device,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Add all the DMF Modules used by this driver. In this driver a specific Module for 
    written for this sample is instantiated. However, any Module(s) that does not need
    resources can be instantiated. 

Arguments:

    Device - WDFDEVICE handle.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    NTSTATUS

--*/
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DEVICE_CONTEXT* deviceContext;

    UNREFERENCED_PARAMETER(DmfModuleInit);
    
    PAGED_CODE();
    
    deviceContext = DeviceContextGet(Device);

    DMF_NonPnp_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &deviceContext->DmfModuleNonPnp);
}
#pragma code_seg()

// eof: DmfInterface.c
//

