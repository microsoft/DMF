/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    DmfInterface.c

Abstract:

Environment:

    Kernel mode only

--*/

// The DMF Library and the DMF Library Modules this driver uses.
// In this sample, the driver uses the default library, unlike the earlier sample drivers that
// use the Template library.
//
#include <initguid.h>
#include "DmfModules.Library.h"

#include "Trace.h"
#include "DmfInterface.tmh"

// DMF: These lines provide default DriverEntry/AddDevice/DriverCleanup functions.
//
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD EyeGazeIoctlEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP EyeGazeIoctlEvtDriverContextCleanup;
EVT_DMF_DEVICE_MODULES_ADD DmfDeviceModulesAdd;

/*WPP_INIT_TRACING(); (This comment is necessary for WPP Scanner.)*/
#pragma code_seg("INIT")
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        EyeGazeIoctlEvtDriverContextCleanup,
                        EyeGazeIoctlEvtDeviceAdd)
#pragma code_seg()

#pragma code_seg("PAGED")
DMF_DEFAULT_DRIVERCLEANUP(EyeGazeIoctlEvtDriverContextCleanup)
DMF_DEFAULT_DEVICEADD(EyeGazeIoctlEvtDeviceAdd,
                      DmfDeviceModulesAdd)
#pragma code_seg()

#define HIDMINI_PRODUCT_ID      0xFEED
#define HIDMINI_VENDOR_ID       0xDEED
#define HIDMINI_VERSION         0x0101

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
    DMF_CONFIG_EyeGazeIoctl moduleConfigEyeGazeGhost;

    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "-->%!FUNC!");

    // EyeGazeIoctl
    // ------------
    //
    DMF_CONFIG_EyeGazeIoctl_AND_ATTRIBUTES_INIT(&moduleConfigEyeGazeGhost,
                                                &moduleAttributes);
    moduleConfigEyeGazeGhost.ProductId = HIDMINI_PRODUCT_ID;
    moduleConfigEyeGazeGhost.VendorId = HIDMINI_VENDOR_ID;
    moduleConfigEyeGazeGhost.VersionId = HIDMINI_VERSION;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "<--%!FUNC!");
}

