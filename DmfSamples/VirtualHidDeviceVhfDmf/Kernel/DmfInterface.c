/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    DmfInterface.c

Abstract:

    VirtualHidDeviceVhfDmfK Sample: DMF version of VHIDMINI Sample (Kernel-mode).

Environment:

    Kernel mode

--*/

#include "DmfModules.Template.h"

#include "Trace.h"
#include "DmfInterface.tmh"

// DMF: These lines provide default DriverEntry/AddDevice/DriverCleanup functions.
//
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD VirtualHidDeviceVhfDmfEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP VirtualHidDeviceVhfDmfEvtDriverContextCleanup;
EVT_DMF_DEVICE_MODULES_ADD DmfDeviceModulesAdd;

/*WPP_INIT_TRACING(); (This comment is necessary for WPP Scanner.)*/
#pragma code_seg("INIT")
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        VirtualHidDeviceVhfDmfEvtDriverContextCleanup,
                        VirtualHidDeviceVhfDmfEvtDeviceAdd)
#pragma code_seg()

typedef struct
{
    DMFMODULE DmfModuleVirtualHidDeviceVhfSample;
    DMFMODULE DmfModuleVirtualHidKeyboard;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceContextGet)

#pragma code_seg("PAGED")
DMF_DEFAULT_DRIVERCLEANUP(VirtualHidDeviceVhfDmfEvtDriverContextCleanup)

// NOTE: It seems not possible to uninstall the driver when keyboard is exposed.
//
#define NO_EXPOSE_KEYBOARD_DEVICE

#if defined(EXPOSE_KEYBOARD_DEVICE)

#pragma code_seg("PAGE")
_Function_class_(EVT_WDF_TIMER)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
KeyStrokeTimerCallback(
    _In_ WDFTIMER WdfTimer
    )
{
    PDEVICE_CONTEXT deviceContext;
    WDFDEVICE device;
    static int numberOfTimesTyped = 0;

    PAGED_CODE();

    device = (WDFDEVICE)WdfTimerGetParentObject(WdfTimer);
    deviceContext = DeviceContextGet(device);

    // Letters 'abc'.
    //
    USHORT keysToType[] = {0x0004, 0x0005, 0x0006};
    NTSTATUS ntStatus = DMF_VirtualHidKeyboard_Type(deviceContext->DmfModuleVirtualHidKeyboard,
                                                    keysToType,
                                                    sizeof(keysToType),
                                                    HID_USAGE_PAGE_KEYBOARD);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_VirtualHidKeyboard_Type fails: ntStatus=%!STATUS!", ntStatus);
    }

    numberOfTimesTyped++;
    if (numberOfTimesTyped < 3)
    {
        WdfTimerStart(WdfTimer,
                      WDF_REL_TIMEOUT_IN_SEC(10));
    }
}
#pragma code_seg()

#endif

_Use_decl_annotations_
NTSTATUS
VirtualHidDeviceVhfDmfEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    PDMFDEVICE_INIT dmfDeviceInit;
    DMF_EVENT_CALLBACKS dmfCallbacks;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    dmfDeviceInit = DMF_DmfDeviceInitAllocate(DeviceInit);

    // All DMF drivers must call this function even if they do not support PnP Power callbacks.
    // (In this case, this driver does support a PnP Power callback.)
    //
    DMF_DmfDeviceInitHookPnpPowerEventCallbacks(dmfDeviceInit,
                                                NULL);

    // All DMF drivers must call this function even if they do not support File Object callbacks.
    //
    DMF_DmfDeviceInitHookFileObjectConfig(dmfDeviceInit,
                                          NULL);

    // All DMF drivers must call this function even if they do not support Power Policy callbacks.
    //
    DMF_DmfDeviceInitHookPowerPolicyEventCallbacks(dmfDeviceInit,
                                                   NULL);

    // Set any device attributes needed.
    //
    WdfDeviceInitSetDeviceType(DeviceInit,
                               FILE_DEVICE_UNKNOWN);
    WdfDeviceInitSetExclusive(DeviceInit,
                              FALSE);

    // NOTE: Vhf clients do not need to be filter drivers.
    //

    // Define a device context type.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes, 
                                            DEVICE_CONTEXT);

    // Create the Client driver's WDFDEVICE.
    //
    ntStatus = WdfDeviceCreate(&DeviceInit,
                               &objectAttributes,
                               &device);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Create the DMF Modules this Client driver will use.
    //
    dmfCallbacks.EvtDmfDeviceModulesAdd = DmfDeviceModulesAdd;
    DMF_DmfDeviceInitSetEventCallbacks(dmfDeviceInit,
                                       &dmfCallbacks);

    ntStatus = DMF_ModulesCreate(device,
                                 &dmfDeviceInit);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

#if defined(EXPOSE_KEYBOARD_DEVICE)
    WDF_TIMER_CONFIG timerConfig;
    WDFTIMER timer;
    // Create a timer object to validate if tasks were executed properly.
    //
    WDF_TIMER_CONFIG_INIT(&timerConfig,
                          KeyStrokeTimerCallback);
    timerConfig.AutomaticSerialization = TRUE;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = device;
    objectAttributes.ExecutionLevel = WdfExecutionLevelPassive;
    ntStatus = WdfTimerCreate(&timerConfig,
                              &objectAttributes,
                              &timer);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfTimerCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }
    // "Press" the letter 'a' every ten seconds.
    //
    WdfTimerStart(timer,
                  WDF_REL_TIMEOUT_IN_SEC(10));

    // Set this status to error it cause driver to not start so that you can uninstall it.
    //
    // ntStatus = STATUS_INVALID_PARAMETER;
#endif

Exit:

    if (dmfDeviceInit != NULL)
    {
        DMF_DmfDeviceInitFree(&dmfDeviceInit);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CALLBACK, "<--%!FUNC! ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGED")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DmfDeviceModulesAdd(
    _In_ WDFDEVICE Device,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Add all the DMF Modules used by this driver.

Arguments:

    Device - WDFDEVICE handle.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    NTSTATUS

--*/
{
    PDEVICE_CONTEXT deviceContext;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_VirtualHidDeviceVhfSample moduleConfigVirtualHidDeviceVhfSample;
#if defined(EXPOSE_KEYBOARD_DEVICE)
    DMF_CONFIG_VirtualHidKeyboard moduleConfigVirtualHidKeyboard;
#endif

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    deviceContext = DeviceContextGet(Device);

    // VirtualHidDeviceVhfSample
    // -------------------------
    //
    DMF_CONFIG_VirtualHidDeviceVhfSample_AND_ATTRIBUTES_INIT(&moduleConfigVirtualHidDeviceVhfSample,
                                                             &moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &deviceContext->DmfModuleVirtualHidDeviceVhfSample);

#if defined(EXPOSE_KEYBOARD_DEVICE)
    // VirtualKeyboard
    // ---------------
    //
    DMF_CONFIG_VirtualHidKeyboard_AND_ATTRIBUTES_INIT(&moduleConfigVirtualHidKeyboard,
                                                      &moduleAttributes);
    moduleConfigVirtualHidKeyboard.VirtualHidKeyboardMode = VirtualHidKeyboardMode_Standalone;
    moduleConfigVirtualHidKeyboard.VendorId = 0xFEED;
    moduleConfigVirtualHidKeyboard.ProductId = 0xDEED;
    moduleConfigVirtualHidKeyboard.VersionNumber = 0x0001;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &deviceContext->DmfModuleVirtualHidKeyboard);
#endif
}
#pragma code_seg()

// eof: DmfInterface.c
//

