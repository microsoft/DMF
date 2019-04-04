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

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

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

    // Tests_Registry
    // --------------
    //
    DMF_Tests_Registry_ATTRIBUTES_INIT(&moduleAttributes);
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

    // Tests_ScheduledTask
    // --------------------
    //
    DMF_Tests_ScheduledTask_ATTRIBUTES_INIT(&moduleAttributes);
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

    // Tests_IoctlHandler
    // ------------------
    //
    DMF_Tests_IoctlHandler_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    // Tests_SelfTarget
    // ------------------
    //
    DMF_Tests_SelfTarget_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);
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
