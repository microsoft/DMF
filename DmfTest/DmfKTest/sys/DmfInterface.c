/*++

    Copyright (c) 2018 Microsoft Corporation. All rights reserved

Module Name:

    DmfInterface.c

Abstract:

   Instantiate Dmf Library Modules used by THIS driver.
   
   (This is the only code file in the driver that contains unique code for this driver. All other code the
   driver executes is in the Dmf Framework.)

Environment:

    Kernel-mode Driver Framework

--*/

// The Dmf Library and the Dmf Library Modules this driver uses.
//
#include <initguid.h>
#include "DmfModules.Library.h"
#include "DmfModules.Library.Tests.h"

// Event logging
//
#include "DmfKTestEventLog.h"

#include "Trace.h"
#include "DmfInterface.tmh"

///////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE
///////////////////////////////////////////////////////////////////////////////////////////
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD DmfKTestEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP DmfKTestEvtDriverContextCleanup;
EVT_DMF_DEVICE_MODULES_ADD DmfDeviceModulesAdd;

// BranchTrack name.
//
#define BRANCHTRACK_NAME               "DmfKTest"

// BranchTrack Initialize routine.
//
VOID
DmfKTestBranchTrackInitialize(
    _In_ DMFMODULE DmfModuleBranchTrack
    );

/*WPP_INIT_TRACING(); (This comment is necessary for WPP Scanner.)*/
#pragma code_seg("INIT")
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        DmfKTestEvtDriverContextCleanup,
                        DmfKTestEvtDeviceAdd)
#pragma code_seg()

#pragma code_seg("PAGED")
DMF_DEFAULT_DRIVERCLEANUP(DmfKTestEvtDriverContextCleanup)
DMF_DEFAULT_DEVICEADD_WITH_BRANCHTRACK(DmfKTestEvtDeviceAdd,
                                       DmfDeviceModulesAdd,
                                       DmfKTestBranchTrackInitialize,
                                       BRANCHTRACK_NAME,
                                       BRANCHTRACK_DEFAULT_MAXIMUM_BRANCHES)
#pragma code_seg()

#pragma code_seg("PAGED")
_Check_return_
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
static
BOOLEAN
DriverModeGet(
    _In_ WDFDEVICE WdfDevice
    )
/*++

Routine Description:

    Determines the mode the driver is running in (either bus or function).

Arguments:

    WdfDevice - Client driver Wdf device.

Return Value:

    0 - Bus.
    Otherwise - Function.

--*/
{
    NTSTATUS ntStatus;
    WDFKEY wdfSoftwareKey;
    BOOLEAN driverMode;
    ULONG valueData;
    DECLARE_CONST_UNICODE_STRING(valueName, L"FunctionDriver");

    PAGED_CODE();

    driverMode = 0;

    ntStatus = WdfDeviceOpenRegistryKey(WdfDevice,
                                        PLUGPLAY_REGKEY_DRIVER,
                                        KEY_READ,
                                        WDF_NO_OBJECT_ATTRIBUTES,
                                        &wdfSoftwareKey);
    if (!NT_SUCCESS(ntStatus))
    {
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
        goto Exit;
    }

    driverMode = (BOOLEAN)valueData;
    
Exit:

    return driverMode;
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

    Add all the Dmf Modules used by this driver.

Arguments:

    Device - WDFDEVICE handle.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    NTSTATUS

--*/
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    BOOLEAN isFunctionDriver;
    DMF_CONFIG_Tests_IoctlHandler moduleConfigTests_IoctlHandler;

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    isFunctionDriver = DriverModeGet(Device);

    /////////////////////////////////////////////////////////////////////////////////
    // These tests can be in both bus and function drivers. To reduce CPU usage, they
    // can be placed in just the bus driver.
    /////////////////////////////////////////////////////////////////////////////////
    //

    // Tests_BufferPool
    // ----------------
    //
    DMF_Tests_BufferPool_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        NULL);

    // Tests_BufferQueue
    // ----------------
    //
    DMF_Tests_BufferQueue_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        NULL);

    // Tests_RingBuffer
    // ----------------
    //
    DMF_Tests_RingBuffer_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        NULL);

    // Tests_PingPongBuffer
    // --------------------
    //
    DMF_Tests_PingPongBuffer_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        NULL);

    // Tests_HashTable
    // ---------------
    //
    DMF_Tests_HashTable_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        NULL);

    // Tests_String
    // -------------
    //
    DMF_Tests_String_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        NULL);

    if (isFunctionDriver)
    {
        // Tests_DefaultTarget
        // -------------------
        //
        DMF_Tests_DefaultTarget_ATTRIBUTES_INIT(&moduleAttributes);
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         NULL);

        // Tests_DeviceInterfaceTarget
        // ---------------------------
        //
        DMF_Tests_DeviceInterfaceTarget_ATTRIBUTES_INIT(&moduleAttributes);
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         NULL);
    }
    else
    {
        // Tests_Registry
        // --------------
        // Only run these in a single driver since they add/delete from a single resource
        // (the registry). Running from multiple drivers will cause sporadic errors.
        //
        DMF_Tests_Registry_ATTRIBUTES_INIT(&moduleAttributes);
        DMF_DmfModuleAdd(DmfModuleInit,
                            &moduleAttributes,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            NULL);

        // Tests_ScheduledTask
        // --------------------
        // Only run these in a single driver since they add/delete from a single resource
        // (the registry). Running from multiple drivers will cause sporadic errors.
        //
        DMF_Tests_ScheduledTask_ATTRIBUTES_INIT(&moduleAttributes);
        DMF_DmfModuleAdd(DmfModuleInit,
                            &moduleAttributes,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            NULL);

        // Tests_IoctlHandler
        // ------------------
        //
        DMF_CONFIG_Tests_IoctlHandler_AND_ATTRIBUTES_INIT(&moduleConfigTests_IoctlHandler,
                                                          &moduleAttributes);
        // This instance will be accessed by SelfTarget and remote targets.
        // Create a device interface.
        //
        moduleConfigTests_IoctlHandler.CreateDeviceInterface = TRUE;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         NULL);

        // Tests_SelfTarget
        // ----------------
        //
        DMF_Tests_SelfTarget_ATTRIBUTES_INIT(&moduleAttributes);
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         NULL);

        // Tests_Pdo
        // ---------
        //
        DMF_Tests_Pdo_ATTRIBUTES_INIT(&moduleAttributes);
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         NULL);
    }
}
#pragma code_seg()

VOID
DmfKTestBranchTrackInitialize(
    _In_ DMFMODULE DmfModuleBranchTrack
    )
{
    UNREFERENCED_PARAMETER(DmfModuleBranchTrack);
}

// eof: DmfInterface.c
//
