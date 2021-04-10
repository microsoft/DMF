/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    DmfInterface.c

Abstract:

    InterfaceClientTransport1 Sample: Demonstrates how to use the DMF (Protocol/Transport) Interface to dynamically 
                                      bind (attach) a Child Module to a Parent Module at runtime instead of compile time
                                      as is normally done. This feature allows a driver to implement a generic
                                      Protocol. Then, the driver chooses an Transport specific implementation at runtime.
                                      For example, a Protocol could implement a Latch that opens and closes. Then, the
                                      Transport might allow that Protocol to execute over HID, USB, or PCI.

Environment:

    Kernel mode only

--*/

// The DMF Library and the DMF Library Modules this driver uses.
// NOTE: The Template Library automatically includes the DMF Library.
//
#include <initguid.h>
#include "..\Modules.Template\DmfModules.Template.h"

#include "Trace.h"
#include "DmfInterface.tmh"

UCHAR g_TestTriageData1[] = "SampleInterface1 driver triage data";
UCHAR g_TestSecondaryData1[] = "SampleInterface1 secondary data";

// {B5953C99-F12A-45A4-AC13-129A11B35BC0}
DEFINE_GUID(Interface1_CrashDataGuid,
            0xb5953c99, 0xf12a, 0x45a4, 0xac, 0x13, 0x12, 0x9a, 0x11, 0xb3, 0x5b, 0xc0);

// Forward declarations.
//
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD InterfaceClientServerEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP InterfaceClientServerEvtDriverContextCleanup;
EVT_DMF_DEVICE_MODULES_ADD DmfDeviceModulesAdd;
EVT_WDF_DEVICE_D0_ENTRY InterfaceClientServerEvtDeviceD0Entry;
EVT_WDF_DEVICE_PREPARE_HARDWARE InterfaceClientServerEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE InterfaceClientServerEvtDeviceReleaseHardware;

/*WPP_INIT_TRACING(); (This comment is necessary for WPP Scanner.)*/
#pragma code_seg("INIT")
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        InterfaceClientServerEvtDriverContextCleanup,
                        InterfaceClientServerEvtDeviceAdd)
#pragma code_seg()

typedef struct
{
    // The Interface's Protocol Module.
    //
    DMFMODULE DmfModuleProtocol;
    // The Interface's Transport Module.
    //
    DMFMODULE DmfModuleTransport;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceContextGet)

//Nonpaged segments for Bug Check callbacks
//

// Callback function for client driver to inform OS how much space Client Driver needs 
// to write its data.  This is called during BugCheck at IRQL = HIGH_LEVEL so must 
// be nonpaged and has restrictions on what it may do.
//
_Function_class_(EVT_DMF_CrashDump_Query)
_IRQL_requires_same_
VOID
Interface1_CrashDump_Query(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** OutputBuffer,
    _Out_ ULONG* SizeNeededBytes)
{
    UNREFERENCED_PARAMETER(DmfModule);

    DmfAssert(OutputBuffer != NULL);
    DmfAssert(SizeNeededBytes != NULL);
    
    // Return the size of our sample global data
    //
    *SizeNeededBytes = sizeof(g_TestSecondaryData1);
    *OutputBuffer = (VOID*)g_TestSecondaryData1;

}

// Callback function for client driver to write its own data after the system is crashed.
// Note that this callback is only applicable to the RINGBUFFER_INDEX_SELF instance. Other instances
// are used by User-mode and cannot use this callback. This is called during BugCheck 
// at IRQL = HIGH_LEVEL so must be nonpaged and has restrictions on what it may do.
//
_Function_class_(EVT_DMF_CrashDump_Write)
_IRQL_requires_same_
VOID
Interface1_CrashDump_Write(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** OutputBuffer,
    _In_ ULONG* OutputBufferLength)
{
    UNREFERENCED_PARAMETER(DmfModule);

    DmfAssert(OutputBuffer != NULL);
    DmfAssert(OutputBufferLength != NULL);

    // Return the size of our sample global data
    //
    *OutputBufferLength = sizeof(g_TestSecondaryData1);
    *OutputBuffer = (VOID*)g_TestSecondaryData1;
}
// Callback for marking memory regions which should be included in the kernel minidump.
// This is called during BugCheck at IRQL = HIGH_LEVEL so must be nonpaged and
// has restrictions on what it may do.  The bugcheck code and parameters
// are provided so the callback may choose to only add data when certain Bug Checks occur.
//
_Function_class_(EVT_DMF_CrashDump_StoreTriageDumpData)
_IRQL_requires_same_
VOID
Interface1_CrashDump_StoreTriageDumpData(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG BugCheckCode,
    _In_ ULONG_PTR BugCheckParameter1,
    _In_ ULONG_PTR BugCheckParameter2,
    _In_ ULONG_PTR BugCheckParameter3,
    _In_ ULONG_PTR BugCheckParameter4)
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(BugCheckCode);
    UNREFERENCED_PARAMETER(BugCheckParameter1);
    UNREFERENCED_PARAMETER(BugCheckParameter2);
    UNREFERENCED_PARAMETER(BugCheckParameter3);
    UNREFERENCED_PARAMETER(BugCheckParameter4);

    // Add sample data via triage dump data callback so it is available
    // as memory in the crash minidump.

    ntStatus = DMF_CrashDump_TriageDumpDataAdd(DmfModule,
                                               g_TestTriageData1,
                                               sizeof(g_TestTriageData1));
}

#pragma code_seg("PAGED")
DMF_DEFAULT_DRIVERCLEANUP(InterfaceClientServerEvtDriverContextCleanup)

_Use_decl_annotations_
NTSTATUS
InterfaceClientServerEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    PDMFDEVICE_INIT dmfDeviceInit;
    DMF_EVENT_CALLBACKS dmfCallbacks;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    dmfDeviceInit = DMF_DmfDeviceInitAllocate(DeviceInit);

    // Tell WDF this callback should be called.
    //
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDeviceD0Entry = InterfaceClientServerEvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDevicePrepareHardware = InterfaceClientServerEvtDevicePrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware = InterfaceClientServerEvtDeviceReleaseHardware;

    // All DMF drivers must call this function even if they do not support PnP Power callbacks.
    // (In this case, this driver does support a PnP Power callback.)
    //
    DMF_DmfDeviceInitHookPnpPowerEventCallbacks(dmfDeviceInit,
                                                &pnpPowerCallbacks);
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit,
                                           &pnpPowerCallbacks);

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
_Check_return_
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
static
ULONG
InterfaceTransportIdGet(
    _In_ WDFDEVICE WdfDevice
    )
/*++

Routine Description:

    Determines which Transport Module should be bound to the Protocol Module.

Arguments:

    WdfDevice - Client driver Wdf device.

Return Value:

    Identifier of the Transport Module to bind to the Protocol Module.

--*/
{
    NTSTATUS ntStatus;
    WDFKEY wdfSoftwareKey;
    ULONG transportId;
    ULONG valueData;
    DECLARE_CONST_UNICODE_STRING(valueName, L"TransportSelect");

    PAGED_CODE();

    transportId = 1;

    ntStatus = WdfDeviceOpenRegistryKey(WdfDevice,
                                        PLUGPLAY_REGKEY_DRIVER,
                                        KEY_READ,
                                        WDF_NO_OBJECT_ATTRIBUTES,
                                        &wdfSoftwareKey);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfDeviceOpenRegistryKey fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = WdfRegistryQueryValue(wdfSoftwareKey,
                                     &valueName,
                                     sizeof(valueData),
                                     &valueData,
                                     NULL,
                                     NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfRegistryQueryValue fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if ((valueData > 2) ||
        (valueData < 1))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Invalid Server Id=%u", valueData);
        goto Exit;
    }

    transportId = (ULONG)valueData;
    
Exit:

    return transportId;
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
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_SampleInterfaceProtocol1 moduleConfigInterfaceProtocol1;
    DMF_CONFIG_SampleInterfaceTransport1 moduleConfigInterfaceTransport1;
    DMF_CONFIG_SampleInterfaceTransport2 moduleConfigInterfaceTransport2;
    DMF_CONFIG_CrashDump moduleConfigCrashDump;
    DEVICE_CONTEXT* deviceContext;
    ULONG transportId;

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    transportId = InterfaceTransportIdGet(Device);

    // SampleInterfaceProtocol1
    // ------------------------
    //
    DMF_CONFIG_SampleInterfaceProtocol1_AND_ATTRIBUTES_INIT(&moduleConfigInterfaceProtocol1,
                                                            &moduleAttributes);
    moduleConfigInterfaceProtocol1.ModuleId = 1;
    moduleConfigInterfaceProtocol1.ModuleName = "SampleInterfaceProtocol1";
 
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &(deviceContext->DmfModuleProtocol));

    if (transportId == 1)
    {
        // SampleInterfaceTransport1
        // -------------------------
        //
        DMF_CONFIG_SampleInterfaceTransport1_AND_ATTRIBUTES_INIT(&moduleConfigInterfaceTransport1,
                                                                 &moduleAttributes);
        moduleConfigInterfaceTransport1.ModuleId = 1;
        moduleConfigInterfaceTransport1.ModuleName = "SampleInterfaceTransport1";

        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &(deviceContext->DmfModuleTransport));
    }

    else if (transportId == 2)
    {
        // SampleInterfaceTransport2
        // -------------------------
        //
        DMF_CONFIG_SampleInterfaceTransport2_AND_ATTRIBUTES_INIT(&moduleConfigInterfaceTransport2,
                                                              &moduleAttributes);
        moduleConfigInterfaceTransport2.ModuleId = 2;
        moduleConfigInterfaceTransport2.ModuleName = "SampleInterfaceTransport2";

        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &(deviceContext->DmfModuleTransport));
    }
    else
    {
        ASSERT(0);
    }

    // Set up CrashDump callbacks
    //

    DMF_CONFIG_CrashDump_AND_ATTRIBUTES_INIT(&moduleConfigCrashDump,
                                             &moduleAttributes);

    // Note: ComponentName MUST be set for triage dump data callback to succeed.
    //
    moduleConfigCrashDump.ComponentName = (UCHAR*)"DmfIFSamp1";
    // Secondary dump data callbacks for Dmf Ring buffer.
    //
    moduleConfigCrashDump.AdditionalDataGuid = Interface1_CrashDataGuid;
    moduleConfigCrashDump.EvtCrashDumpQuery = Interface1_CrashDump_Query;
    moduleConfigCrashDump.EvtCrashDumpWrite = Interface1_CrashDump_Write;
    // Triage Dump Data callback.
    // Allow up to 10 data ranges (added via DMF_CrashDump_TriageDumpDataAdd())
    //
    moduleConfigCrashDump.TriageDumpDataArraySize = 10;
    moduleConfigCrashDump.EvtCrashDumpStoreTriageDumpData = Interface1_CrashDump_StoreTriageDumpData;

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");
}
#pragma code_seg()

#pragma code_seg("PAGED")
_Use_decl_annotations_
NTSTATUS
InterfaceClientServerEvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    When the driver starts bind the Client and Server Modules.
 
Arguments:

    Device - Handle to a framework device object.
    PreviousState - Device power state which the device was in most recently.
                    If the device is being newly started, this will be
                    PowerDeviceUnspecified.

Return Value:

    NTSTATUS

--*/
{
    DEVICE_CONTEXT* deviceContext;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    // Bind the Modules using SampleInterface Interface.
    // The decision about which Transport to bind has already been made and the
    // Transport Module has already been created.
    //
    ntStatus = DMF_INTERFACE_BIND(deviceContext->DmfModuleProtocol,
                                  deviceContext->DmfModuleTransport,
                                  SampleInterface);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "DMF_INTERFACE_BIND fails: ntStatus=%!STATUS!", ntStatus);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "DMF_INTERFACE_BIND succeeds: ntStatus=%!STATUS!", ntStatus);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}
#pragma code_seg()

NTSTATUS
InterfaceClientServerEvtDeviceD0Entry(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    When the driver powers up call a Client (Protocol) Method that calls 
    the corresponding bound (Server) Transport Method.
 
Arguments:

    Device - Handle to a framework device object.
    PreviousState - Device power state which the device was in most recently.
                    If the device is being newly started, this will be
                    PowerDeviceUnspecified.

Return Value:

    NTSTATUS

--*/
{
    DEVICE_CONTEXT* deviceContext;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(PreviousState);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    // Call a Test Method exposed by the Client Module.
    //
    ntStatus = DMF_SampleInterfaceProtocol1_TestMethod(deviceContext->DmfModuleProtocol);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "DMF_SampleInterfaceProtocol1_TestMethod fails: ntStatus=%!STATUS!", ntStatus);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}

#pragma code_seg("PAGED")
_Use_decl_annotations_
NTSTATUS
InterfaceClientServerEvtDeviceReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    When the driver stops unbind the Client and Server Modules.
 
Arguments:

    Device - Handle to a framework device object.
    PreviousState - Device power state which the device was in most recently.
                    If the device is being newly started, this will be
                    PowerDeviceUnspecified.

Return Value:

    NTSTATUS

--*/
{
    DEVICE_CONTEXT* deviceContext;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    deviceContext = DeviceContextGet(Device);

    ntStatus = STATUS_SUCCESS;

    // Unbind the Modules using SampleInterface Interface.
    //
    DMF_INTERFACE_UNBIND(deviceContext->DmfModuleProtocol,
                         deviceContext->DmfModuleTransport,
                         SampleInterface);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");

    return ntStatus;
}
#pragma code_seg()

// eof: DmfInterface.c
//
