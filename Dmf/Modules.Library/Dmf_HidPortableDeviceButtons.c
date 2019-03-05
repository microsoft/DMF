/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_HidPortableDeviceButtons.c

Abstract:

    Support for buttons (Power, Volume+ and Volume-) via Vhf.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_HidPortableDeviceButtons.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Display backlight brightness up and down codes defined by usb hid review request #41.
// https://www.usb.org/sites/default/files/hutrr41_0.pdf
//
#define DISPLAY_BACKLIGHT_BRIGHTNESS_INCREMENT    0x6F
#define DISPLAY_BACKLIGHT_BRIGHTNESS_DECREMENT    0x70

// The Input Report structure used for the child HID device
// NOTE: The actual size of this structure must match exactly with the
//       corresponding descriptor below.
//
#include <pshpack1.h>
typedef struct _BUTTONS_INPUT_REPORT
{
    UCHAR ReportId;

    union
    {
        unsigned char Data;

        struct
        {
            unsigned char Windows : 1;
            unsigned char Power : 1;
            unsigned char VolumeUp : 1;
            unsigned char VolumeDown : 1;
            unsigned char RotationLock : 1;
        } Buttons;
    } u;
} BUTTONS_INPUT_REPORT;

// Used in conjunction with Consumer usage page to send hotkeys to the OS.
//
typedef struct
{
    UCHAR ReportId;
    unsigned short HotKey;
} BUTTONS_HOTKEY_INPUT_REPORT;

#include <poppack.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Thread for processing requests for HID.
    //
    DMFMODULE DmfModuleVirtualHidDeviceVhf;
    // It is the current state of all the buttons.
    //
    HID_XFER_PACKET VhfHidReport;
    // Current state of button presses. Note that this variable 
    // stores state of multiple buttons so that combinations of buttons
    // can be pressed at the same time.
    //
    BUTTONS_INPUT_REPORT InputReportButtonState;
    // Enabled/disabled state of buttons. Buttons can be enabled/disabled
    // by higher layers. This variable maintains the enabled/disabled
    // state of each button.
    //
    BUTTONS_INPUT_REPORT InputReportEnabledState;
} DMF_CONTEXT_HidPortableDeviceButtons;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(HidPortableDeviceButtons)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(HidPortableDeviceButtons)

// MemoryTag.
//
#define MemoryTag 'BDPH'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Number of BranchTrack button presses for each button.
//
#define HidPortableDeviceButtons_ButtonPresses         10

//
// This HID Report Descriptor describes a 1-byte input report for the 5
// buttons supported on Windows 10 for desktop editions (Home, Pro, and Enterprise). Following are the buttons and
// their bit positions in the input report:
//     Bit 0 - Windows/Home Button (Unused)
//     Bit 1 - Power Button
//     Bit 2 - Volume Up Button
//     Bit 3 - Volume Down Button
//     Bit 4 - Rotation Lock Slider switch (Unused)
//     Bit 5 - Unused
//     Bit 6 - Unused
//     Bit 7 - Unused
//
// The Report Descriptor also defines a 1-byte Control Enable/Disable
// feature report of the same size and bit positions as the Input Report.
// For a Get Feature Report, each bit in the report conveys whether the
// corresponding Control (i.e. button) is currently Enabled (1) or
// Disabled (0). For a Set Feature Report, each bit in the report conveys
// whether the corresponding Control (i.e. button) should be Enabled (1)
// or Disabled (0).
//

// NOTE: This descriptor is derived from the version published in MSDN. The published
//       version was incorrect however. The modifications from that are to correct
//       the issues with the published version.
//

#define REPORTID_BUTTONS    0x01
#define REPORTID_HOTKEYS    0x02

// Report Size includes the Report Id and one byte for data.
//
#define REPORT_SIZE      2

static
const
UCHAR
g_HidPortableDeviceButtons_HidReportDescriptor[] =
{
    0x15, 0x00,                      // LOGICAL_MINIMUM (0)
    0x25, 0x01,                      // LOGICAL_MAXIMUM (1)
    0x75, 0x01,                      // REPORT_SIZE (1)

    0x05, 0x01,                      // USAGE_PAGE (Generic Desktop)
    0x09, 0x0D,                      // USAGE (Portable Device Control)
    0xA1, 0x01,                      // COLLECTION (Application)
    0x85, REPORTID_BUTTONS,          // REPORT_ID (REPORTID_BUTTONS) (For Input Report & Feature Report)

    0x05, 0x01,                      // USAGE_PAGE (Generic Desktop)
    0x09, 0x0D,                      // USAGE (Portable Device Control)
    0xA1, 0x02,                      // COLLECTION (Logical)
    0x05, 0x07,                      // USAGE_PAGE (Keyboard)
    0x09, 0xE3,                      // USAGE (Keyboard LGUI)                // Windows/Home Button
    0x95, 0x01,                      // REPORT_COUNT (1)
    0x81, 0x02,                      // INPUT (Data,Var,Abs)
    0x05, 0x01,                      // USAGE_PAGE (Generic Desktop)
    0x09, 0xCB,                      // USAGE (Control Enable)           
    0x95, 0x01,                      // REPORT_COUNT (1)
    0xB1, 0x02,                      // FEATURE (Data,Var,Abs)
    0xC0,                            // END_COLLECTION

    0x05, 0x01,                      // USAGE_PAGE (Generic Desktop)
    0x09, 0x0D,                      // USAGE (Portable Device Control)
    0xA1, 0x02,                      // COLLECTION (Logical)
    0x05, 0x01,                      // USAGE_PAGE (Generic Desktop)
    0x09, 0x81,                      // USAGE (System Power Down)            // Power Button
    0x95, 0x01,                      // REPORT_COUNT (1)
    0x81, 0x02,                      // INPUT (Data,Var,Abs)
    0x05, 0x01,                      // USAGE_PAGE (Generic Desktop)
    0x09, 0xCB,                      // USAGE (Control Enable)           
    0x95, 0x01,                      // REPORT_COUNT (1)
    0xB1, 0x02,                      // FEATURE (Data,Var,Abs)
    0xC0,                            // END_COLLECTION

    0x05, 0x01,                      // USAGE_PAGE (Generic Desktop)
    0x09, 0x0D,                      // USAGE (Portable Device Control)
    0xA1, 0x02,                      // COLLECTION (Logical)
    0x05, 0x0C,                      // USAGE_PAGE (Consumer Devices)
    0x09, 0xE9,                      // USAGE (Volume Increment)             // Volume Up Button
    0x95, 0x01,                      // REPORT_COUNT (1)
    0x81, 0x02,                      // INPUT (Data,Var,Abs)
    0x05, 0x01,                      // USAGE_PAGE (Generic Desktop)
    0x09, 0xCB,                      // USAGE (Control Enable)           
    0x95, 0x01,                      // REPORT_COUNT (1)
    0xB1, 0x02,                      // FEATURE (Data,Var,Abs)
    0xC0,                            // END_COLLECTION

    0x05, 0x01,                      // USAGE_PAGE (Generic Desktop)
    0x09, 0x0D,                      // USAGE (Portable Device Control)
    0xA1, 0x02,                      // COLLECTION (Logical)
    0x05, 0x0C,                      // USAGE_PAGE (Consumer Devices)
    0x09, 0xEA,                      // USAGE (Volume Decrement)             // Volume Down Button
    0x95, 0x01,                      // REPORT_COUNT (1)
    0x81, 0x02,                      // INPUT (Data,Var,Abs)
    0x05, 0x01,                      // USAGE_PAGE (Generic Desktop)
    0x09, 0xCB,                      // USAGE (Control Enable)           
    0x95, 0x01,                      // REPORT_COUNT (1)
    0xB1, 0x02,                      // FEATURE (Data,Var,Abs)
    0xC0,                            // END_COLLECTION

    0x05, 0x01,                      // USAGE_PAGE (Generic Desktop)
    0x09, 0x0D,                      // USAGE (Portable Device Control)
    0xA1, 0x02,                      // COLLECTION (Logical)
    0x05, 0x01,                      // USAGE_PAGE (Generic Desktop)
    0x09, 0xCA,                      // USAGE (System Display Rotation Lock Slider Switch) // Rotation Lock Button
    0x95, 0x01,                      // REPORT_COUNT (1)
    0x81, 0x02,                      // INPUT (Data,Var,Abs)
    0x95, 0x03,                      // REPORT_COUNT (3)                     // unused bits in 8-bit Input Report
    0x81, 0x03,                      // INPUT (Cnst,Var,Abs)
    0x05, 0x01,                      // USAGE_PAGE (Generic Desktop)
    0x09, 0xCB,                      // USAGE (Control Enable)           
    0x95, 0x01,                      // REPORT_COUNT (1)
    0xB1, 0x02,                      // FEATURE (Data,Var,Abs)
    0x95, 0x03,                      // REPORT_COUNT (3)                     // unused bits in 8-bit Feature Report
    0xB1, 0x03,                      // FEATURE (Cnst,Var,Abs)
    0xC0,                            // END_COLLECTION

    0xC0,                            // END_COLLECTION

    //***************************************************************
    //
    // hotkeys (consumer)
    //
    // report consists of:
    // 1 byte report ID
    // 1 word Consumer Key
    //
    //***************************************************************

    0x05, 0x0C,                      // USAGE_PAGE (Consumer Devices)
    0x09, 0x01,                      // HID_USAGE (Consumer Control)
    0xA1, 0x01,                      // COLLECTION (Application)
    0x85, REPORTID_HOTKEYS,          // REPORT_ID (REPORTID_HOTKEYS)
    0x75, 0x10,                      // REPORT_SIZE(16),
    0x95, 0x01,                      // REPORT_COUNT (1)
    0x15, 0x00,                      // LOGICAL_MIN (0)
    0x26, 0xff, 0x03,                // HID_LOGICAL_MAX (1023)
    0x19, 0x00,                      // HID_USAGE_MIN (0)
    0x2A, 0xff, 0x03,                // HID_USAGE_MAX (1023)
    0x81, 0x00,                      // HID_INPUT (Data,Arr,Abs)
    0xC0                             // END_COLLECTION
};

// HID Device Descriptor with just one report representing the Portable Device Buttons.
//
static
const
HID_DESCRIPTOR
g_HidPortableDeviceButtons_HidDescriptor =
{
    0x09,     // Length of HID descriptor
    0x21,     // Descriptor type == HID  0x21
    0x0100,   // HID spec release
    0x00,     // Country code == English
    0x01,     // Number of HID class descriptors
    {
        0x22,   // Descriptor type
        // Total length of report descriptor.
        //
        (USHORT) sizeof(g_HidPortableDeviceButtons_HidReportDescriptor)
    }
};

VOID
HidPortableDeviceButtons_GetFeature(
    _In_ VOID* VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_ VOID* VhfOperationContext,
    _In_ PHID_XFER_PACKET HidTransferPacket
    )
/*++

Routine Description:

    Handle's GET_FEATURE for buttons which allows Client to inquire about
    the enable/disable status of buttons.
    This function receives the request from upper layer and returns the enable/disable
    status of each button which has been stored in the Module Context. Upper layer
    uses this data to send back enable/disable requests for each of the buttons
    as a bit mask.

Arguments:

    VhfClientContext - This Module's handle.
    VhfOperationHandle - Vhf context for this transaction.
    VhfOperationContext - Client context for this transaction.
    HidTransferPacket - Contains SET_FEATURE report data.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_HidPortableDeviceButtons* moduleContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(VhfOperationContext);

    ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    dmfModule = DMFMODULEVOID_TO_MODULE(VhfClientContext);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    if (HidTransferPacket->reportBufferLen < REPORT_SIZE)
    {
        DMF_BRANCHTRACK_MODULE_NEVER(dmfModule, "HidPortableDeviceButtons_GetFeature.BadReportBufferSize");
        goto Exit;
    }

    if (HidTransferPacket->reportId != REPORTID_BUTTONS)
    {
        DMF_BRANCHTRACK_MODULE_NEVER(dmfModule, "HidPortableDeviceButtons_GetFeature.BadReportId");
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

    BUTTONS_INPUT_REPORT* featureReport = (BUTTONS_INPUT_REPORT*)HidTransferPacket->reportBuffer;

    DMF_ModuleLock(dmfModule);

    ASSERT(sizeof(moduleContext->InputReportEnabledState) <= HidTransferPacket->reportBufferLen);
    ASSERT(HidTransferPacket->reportBufferLen >= REPORT_SIZE);

    *featureReport = moduleContext->InputReportEnabledState;

    DMF_ModuleUnlock(dmfModule);

    DMF_BRANCHTRACK_MODULE_AT_LEAST(dmfModule, "HidPortableDeviceButtons_GetFeature{Enter connected standby without audio playing}[HidPortableDeviceButtons]", HidPortableDeviceButtons_ButtonPresses);

Exit:

    DMF_VirtualHidDeviceVhf_AsynchronousOperationComplete(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                          VhfOperationHandle,
                                                          ntStatus);

    FuncExitVoid(DMF_TRACE);
}

VOID
HidPortableDeviceButtons_SetFeature(
    _In_ VOID* VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_ VOID* VhfOperationContext,
    _In_ PHID_XFER_PACKET HidTransferPacket
    )
/*++

Routine Description:

    Handle's set feature for buttons which allows Client to enable/disable buttons.
    This function receives the request from client and stores the enable/disable
    status of each button in the Module Context. Later, if that button is pressed
    and the Module Context indicates that the button is disabled, that button
    press is never sent to the upper layer.

Arguments:

    VhfClientContext - This Module's handle.
    VhfOperationHandle - Vhf context for this transaction.
    VhfOperationContext - Client context for this transaction.
    HidTransferPacket - Contains SET_FEATURE report data.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_HidPortableDeviceButtons* moduleContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(VhfOperationContext);

    ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    dmfModule = DMFMODULEVOID_TO_MODULE(VhfClientContext);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    if (HidTransferPacket->reportBufferLen < REPORT_SIZE)
    {
        DMF_BRANCHTRACK_MODULE_NEVER(dmfModule, "HidPortableDeviceButtons_SetFeature.BadReportBufferSize");
        goto Exit;
    }

    if (HidTransferPacket->reportId != REPORTID_BUTTONS)
    {
        DMF_BRANCHTRACK_MODULE_NEVER(dmfModule, "HidPortableDeviceButtons_SetFeature.BadReportId");
        goto Exit;
    }

    BUTTONS_INPUT_REPORT* featureReport = (BUTTONS_INPUT_REPORT*)HidTransferPacket->reportBuffer;

    DMF_ModuleLock(dmfModule);

    if (! featureReport->u.Buttons.Power)
    {
        // Fail this request...Power button should never be disabled.
        //
        ASSERT(FALSE);
        DMF_BRANCHTRACK_MODULE_NEVER(dmfModule, "HidPortableDeviceButtons_SetFeature.DisablePowerButton");
    }
    else
    {
        moduleContext->InputReportEnabledState = *featureReport;
        ntStatus = STATUS_SUCCESS;
        DMF_BRANCHTRACK_MODULE_AT_LEAST(dmfModule, "HidPortableDeviceButtons_SetFeature{Enter connected standby without audio playing}[HidPortableDeviceButtons]", HidPortableDeviceButtons_ButtonPresses);
    }

    DMF_ModuleUnlock(dmfModule);

Exit:

    DMF_VirtualHidDeviceVhf_AsynchronousOperationComplete(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                          VhfOperationHandle,
                                                          ntStatus);

    FuncExitVoid(DMF_TRACE);
}

VOID
HidPortableDeviceButtons_GetInputReport(
    _In_ VOID* VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_ VOID* VhfOperationContext,
    _In_ PHID_XFER_PACKET HidTransferPacket
    )
/*++

Routine Description:

    Handle's GET_INPUT_REPORT for buttons which allows Client to inquire about
    the current pressed/unpressed status of buttons.
    This function receives the request from upper layer and returns the pressed/unpressed
    status of each button which has been stored in the Module Context.

Arguments:

    VhfClientContext - This Module's handle.
    VhfOperationHandle - Vhf context for this transaction.
    VhfOperationContext - Client context for this transaction.
    HidTransferPacket - Contains GET_INPUT_REPORT report data.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_HidPortableDeviceButtons* moduleContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(VhfOperationContext);

    ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    dmfModule = DMFMODULEVOID_TO_MODULE(VhfClientContext);
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    if (HidTransferPacket->reportBufferLen < REPORT_SIZE)
    {
        DMF_BRANCHTRACK_MODULE_NEVER(dmfModule, "HidPortableDeviceButtons_GetInputReport.BadReportBufferSize");
        goto Exit;
    }

    if (HidTransferPacket->reportId != REPORTID_BUTTONS)
    {
        DMF_BRANCHTRACK_MODULE_NEVER(dmfModule, "HidPortableDeviceButtons_GetInputReport.BadReportId");
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

    BUTTONS_INPUT_REPORT* inputReport = (BUTTONS_INPUT_REPORT*)HidTransferPacket->reportBuffer;

    DMF_ModuleLock(dmfModule);

    ASSERT(sizeof(moduleContext->InputReportEnabledState) <= HidTransferPacket->reportBufferLen);
    ASSERT(HidTransferPacket->reportBufferLen >= REPORT_SIZE);

    *inputReport = moduleContext->InputReportButtonState;

    DMF_ModuleUnlock(dmfModule);

Exit:

    DMF_VirtualHidDeviceVhf_AsynchronousOperationComplete(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                          VhfOperationHandle,
                                                          ntStatus);

    FuncExitVoid(DMF_TRACE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Wdf Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_HidPortableDeviceButtons_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    On the way up clear the state of the buttons in case they were held during hibernate.

Arguments:

    DmfModule - The given DMF Module.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

   NTSTATUS of either the given DMF Module's Open Callback or STATUS_SUCCESS.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidPortableDeviceButtons* moduleContext;

    UNREFERENCED_PARAMETER(PreviousState);

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Clear the state of buttons in case they are held down during power transitions.
    //
    DMF_HidPortableDeviceButtons_ButtonStateChange(DmfModule,
                                                   HidPortableDeviceButtons_ButtonId_Power,
                                                   FALSE);
    DMF_HidPortableDeviceButtons_ButtonStateChange(DmfModule,
                                                   HidPortableDeviceButtons_ButtonId_VolumePlus,
                                                   FALSE);
    DMF_HidPortableDeviceButtons_ButtonStateChange(DmfModule,
                                                   HidPortableDeviceButtons_ButtonId_VolumeMinus,
                                                   FALSE);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_HidPortableDeviceButtons_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type HidPortableDeviceButtons.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_HidPortableDeviceButtons* moduleConfig;
    DMF_CONTEXT_HidPortableDeviceButtons* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    // Set these static values now because they don't change.
    //
    moduleContext->VhfHidReport.reportBuffer = (UCHAR*)&moduleContext->InputReportButtonState;
    moduleContext->VhfHidReport.reportBufferLen = REPORT_SIZE;

    // Only one type of report is used. Set it now.
    //
    moduleContext->InputReportButtonState.ReportId = REPORTID_BUTTONS;
    moduleContext->InputReportButtonState.u.Data = 0;
    moduleContext->VhfHidReport.reportId = moduleContext->InputReportButtonState.ReportId;

    // Enable buttons by default.
    // NOTE: Unused buttons are left disabled.
    //
    moduleContext->InputReportEnabledState.u.Data = 0;
    moduleContext->InputReportEnabledState.u.Buttons.Power = 1;
    moduleContext->InputReportEnabledState.u.Buttons.VolumeDown = 1;
    moduleContext->InputReportEnabledState.u.Buttons.VolumeUp = 1;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_HidPortableDeviceButtons_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type HidPortableDeviceButtons.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_HidPortableDeviceButtons* moduleContext;
    DMF_CONFIG_HidPortableDeviceButtons* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_HidPortableDeviceButtons_ChildModulesAdd(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Configure and add the required Child Modules to the given Parent Module.

Arguments:

    DmfModule - The given Parent Module.
    DmfParentModuleAttributes - Pointer to the parent DMF_MODULE_ATTRIBUTES structure.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    None

--*/
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_HidPortableDeviceButtons* moduleConfig;
    DMF_CONTEXT_HidPortableDeviceButtons* moduleContext;
    DMF_CONFIG_VirtualHidDeviceVhf virtualHidDeviceMsModuleConfig;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // VirtualHidDeviceVhf
    // -------------------
    //
    DMF_CONFIG_VirtualHidDeviceVhf_AND_ATTRIBUTES_INIT(&virtualHidDeviceMsModuleConfig,
                                                       &moduleAttributes);

    virtualHidDeviceMsModuleConfig.VendorId = moduleConfig->VendorId;
    virtualHidDeviceMsModuleConfig.ProductId = moduleConfig->ProductId;
    virtualHidDeviceMsModuleConfig.VersionNumber = 0x0001;

    virtualHidDeviceMsModuleConfig.HidDescriptor = &g_HidPortableDeviceButtons_HidDescriptor;
    virtualHidDeviceMsModuleConfig.HidDescriptorLength = sizeof(g_HidPortableDeviceButtons_HidDescriptor);
    virtualHidDeviceMsModuleConfig.HidReportDescriptor = g_HidPortableDeviceButtons_HidReportDescriptor;
    virtualHidDeviceMsModuleConfig.HidReportDescriptorLength = sizeof(g_HidPortableDeviceButtons_HidReportDescriptor);

    // Set virtual device attributes.
    //
    virtualHidDeviceMsModuleConfig.HidDeviceAttributes.VendorID = moduleConfig->VendorId;
    virtualHidDeviceMsModuleConfig.HidDeviceAttributes.ProductID = moduleConfig->ProductId;
    virtualHidDeviceMsModuleConfig.HidDeviceAttributes.VersionNumber = moduleConfig->VersionNumber;
    virtualHidDeviceMsModuleConfig.HidDeviceAttributes.Size = sizeof(virtualHidDeviceMsModuleConfig.HidDeviceAttributes);

    virtualHidDeviceMsModuleConfig.StartOnOpen = TRUE;
    virtualHidDeviceMsModuleConfig.VhfClientContext = DmfModule;

    // Set callbacks from upper layer.
    //
    virtualHidDeviceMsModuleConfig.IoctlCallback_IOCTL_HID_GET_FEATURE = HidPortableDeviceButtons_GetFeature;
    virtualHidDeviceMsModuleConfig.IoctlCallback_IOCTL_HID_SET_FEATURE = HidPortableDeviceButtons_SetFeature;
    virtualHidDeviceMsModuleConfig.IoctlCallback_IOCTL_HID_GET_INPUT_REPORT = HidPortableDeviceButtons_GetInputReport;

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleVirtualHidDeviceVhf);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

VOID
DMF_HidPortableDeviceButtons_BranchTrackInitialize(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Create the BranchTrack table. These entries are necessary to allow the consumer of this data
    to know what code paths did not execute that *should* have executed.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
   // It not used in RELEASE build.
    //
    UNREFERENCED_PARAMETER(DmfModule);

    DMF_BRANCHTRACK_MODULE_NEVER_CREATE(DmfModule, "HidPortableDeviceButtons_GetFeature.BadReportBufferSize");
    DMF_BRANCHTRACK_MODULE_NEVER_CREATE(DmfModule, "HidPortableDeviceButtons_GetFeature.BadReportId");
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "HidPortableDeviceButtons_GetFeature{Enter connected standby without audio playing}[HidPortableDeviceButtons]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_NEVER_CREATE(DmfModule, "HidPortableDeviceButtons_SetFeature.BadReportBufferSize");
    DMF_BRANCHTRACK_MODULE_NEVER_CREATE(DmfModule, "HidPortableDeviceButtons_SetFeature.BadReportId");
    DMF_BRANCHTRACK_MODULE_NEVER_CREATE(DmfModule, "HidPortableDeviceButtons_SetFeature.DisablePowerButton");
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "HidPortableDeviceButtons_SetFeature{Enter connected standby without audio playing}[HidPortableDeviceButtons]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_NEVER_CREATE(DmfModule, "HidPortableDeviceButtons_GetInputReport.BadReportBufferSize");
    DMF_BRANCHTRACK_MODULE_NEVER_CREATE(DmfModule, "HidPortableDeviceButtons_GetInputReport.BadReportId");
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "ButtonIsEnabled.HidPortableDeviceButtons_ButtonId_Power.True{Press or release power}[HidPortableDeviceButtons]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_NEVER_CREATE(DmfModule, "ButtonIsEnabled.HidPortableDeviceButtons_ButtonId_Power.False");
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "ButtonIsEnabled.HidPortableDeviceButtons_ButtonId_VolumePlus.True{Play audio during connected standby}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "ButtonIsEnabled.HidPortableDeviceButtons_ButtonId_VolumePlus.False{Don't play audio during connected standby}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "ButtonIsEnabled.HidPortableDeviceButtons_ButtonId_VolumeMinus.False{Play audio during connected standby}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "ButtonIsEnabled.HidPortableDeviceButtons_ButtonId_VolumeMinus.False{Don't play audio during connected standby}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_NEVER_CREATE(DmfModule, "ButtonIsEnabled.BadButton");
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_Power.Down{Power press}[HidPortableDeviceButtons]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_Power.Up{Power release}[HidPortableDeviceButtons]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_VolumePlus.Down{Vol+ press}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_Power.ScreenCapture{Press Power and Vol+}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_VolumePlus.Up{Vol+ release}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_VolumeMinus.Down{Vol- press}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_Power.SAS{Press Power and Vol-}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_VolumeMinus.Up{Vol- release}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_NEVER_CREATE(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_Power");
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "HotkeyStateChange.HidPortableDeviceButtons_Hotkey_BrightnessUp.Down{Backlight+ press}[SshKeypad]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "HotkeyStateChange.HidPortableDeviceButtons_Hotkey_BrightnessUp.Up{Backlight+ release}[SshKeypad]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "HotkeyStateChange.HidPortableDeviceButtons_Hotkey_BrightnessDown.Down{BacklightDown- press}[SshKeypad]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfModule, "HotkeyStateChange.HidPortableDeviceButtons_Hotkey_BrightnessDown.Up{BacklightDown- release}[SshKeypad]", HidPortableDeviceButtons_ButtonPresses);
    DMF_BRANCHTRACK_MODULE_NEVER_CREATE(DmfModule, "HotkeyStateChange.DMF_HidPortableDeviceButtons_HotkeyStateChange");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_HidPortableDeviceButtons;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_HidPortableDeviceButtons;
static DMF_CALLBACKS_WDF DmfCallbacksWdf_HidPortableDeviceButtons;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HidPortableDeviceButtons_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type HidPortableDeviceButtons.

Arguments:

    Device - Client driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_HidPortableDeviceButtons);
    DmfCallbacksDmf_HidPortableDeviceButtons.DeviceOpen = DMF_HidPortableDeviceButtons_Open;
    DmfCallbacksDmf_HidPortableDeviceButtons.DeviceClose = DMF_HidPortableDeviceButtons_Close;
    DmfCallbacksDmf_HidPortableDeviceButtons.ChildModulesAdd = DMF_HidPortableDeviceButtons_ChildModulesAdd;

    DMF_CALLBACKS_WDF_INIT(&DmfCallbacksWdf_HidPortableDeviceButtons);
    DmfCallbacksWdf_HidPortableDeviceButtons.ModuleD0Entry = DMF_HidPortableDeviceButtons_ModuleD0Entry;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_HidPortableDeviceButtons,
                                            HidPortableDeviceButtons,
                                            DMF_CONTEXT_HidPortableDeviceButtons,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    DmfModuleDescriptor_HidPortableDeviceButtons.CallbacksDmf = &DmfCallbacksDmf_HidPortableDeviceButtons;
    DmfModuleDescriptor_HidPortableDeviceButtons.CallbacksWdf = &DmfCallbacksWdf_HidPortableDeviceButtons;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_HidPortableDeviceButtons,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_HidPortableDeviceButtons_ButtonIsEnabled(
    _In_ DMFMODULE DmfModule,
    _In_ HidPortableDeviceButtons_ButtonIdType ButtonId
    )
/*++

Routine Description:

    Determines if a given button is enabled or disabled.

Arguments:

    DmfModule - This Module's handle.
    ButtonId - The given button.

Return Value:

    TRUE if given button is enabled.
    FALSE if given button is disabled.

--*/
{
    BOOLEAN returnValue;
    DMF_CONTEXT_HidPortableDeviceButtons* moduleContext;

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_HidPortableDeviceButtons);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    returnValue = FALSE;

    DMF_ModuleLock(DmfModule);

    // If statements are used below for clarity and ease of debugging. Also, it prevents
    // need to cast and allows for possible different states later.
    //
    switch (ButtonId)
    {
        case HidPortableDeviceButtons_ButtonId_Power:
        {
            if (moduleContext->InputReportEnabledState.u.Buttons.Power)
            {
                returnValue = TRUE;
                DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "ButtonIsEnabled.HidPortableDeviceButtons_ButtonId_Power.True{Press or release power}[HidPortableDeviceButtons]", HidPortableDeviceButtons_ButtonPresses);
            }
            else
            {
                DMF_BRANCHTRACK_MODULE_NEVER(DmfModule, "ButtonIsEnabled.HidPortableDeviceButtons_ButtonId_Power.False");
            }
            break;
        }
        case HidPortableDeviceButtons_ButtonId_VolumePlus:
        {
            if (moduleContext->InputReportEnabledState.u.Buttons.VolumeUp)
            {
                returnValue = TRUE;
                DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "ButtonIsEnabled.HidPortableDeviceButtons_ButtonId_VolumePlus.True{Play audio during connected standby}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
            }
            else
            {
                DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "ButtonIsEnabled.HidPortableDeviceButtons_ButtonId_VolumePlus.False{Don't play audio during connected standby}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
            }
            break;
        }
        case HidPortableDeviceButtons_ButtonId_VolumeMinus:
        {
            if (moduleContext->InputReportEnabledState.u.Buttons.VolumeDown)
            {
                returnValue = TRUE;
                DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "ButtonIsEnabled.HidPortableDeviceButtons_ButtonId_VolumeMinus.False{Play audio during connected standby}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
            }
            else
            {
                DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "ButtonIsEnabled.HidPortableDeviceButtons_ButtonId_VolumeMinus.False{Don't play audio during connected standby}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
            }
            break;
        }
        default:
        {
            ASSERT(FALSE);
            DMF_BRANCHTRACK_MODULE_NEVER(DmfModule, "ButtonIsEnabled.BadButton");
            break;
        }
    }

    DMF_ModuleUnlock(DmfModule);

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_HidPortableDeviceButtons_ButtonStateChange(
    _In_ DMFMODULE DmfModule,
    _In_ HidPortableDeviceButtons_ButtonIdType ButtonId,
    _In_ ULONG ButtonStateDown
    )
/*++

Routine Description:

    Updates the state of a given button.

Arguments:

    DmfModule - This Module's handle.
    ButtonId - The given button.
    ButtonStateDown - Indicates if the button is pressed DOWN.

Return Value:

    STATUS_SUCCESS means the updated state was successfully sent to HID stack.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidPortableDeviceButtons* moduleContext;

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_HidPortableDeviceButtons);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Lock the Module context because the Client Driver may call from different threads 
    // (e.g. button press thread is different than rotation lock thread).
    //
    DMF_ModuleLock(DmfModule);

    ASSERT(moduleContext->InputReportButtonState.ReportId == REPORTID_BUTTONS);
    ASSERT(moduleContext->VhfHidReport.reportId == moduleContext->InputReportButtonState.ReportId);

    // If statements are used below for clarity and ease of debugging. Also, it prevents
    // need to cast and allows for possible different states later.
    //
    switch (ButtonId)
    {
        case HidPortableDeviceButtons_ButtonId_Power:
        {
            if (ButtonStateDown)
            {
                moduleContext->InputReportButtonState.u.Buttons.Power = 1;
                DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_Power.Down{Power press}[HidPortableDeviceButtons]", HidPortableDeviceButtons_ButtonPresses);
            }
            else
            {
                moduleContext->InputReportButtonState.u.Buttons.Power = 0;
                DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_Power.Up{Power release}[HidPortableDeviceButtons]", HidPortableDeviceButtons_ButtonPresses);
            }
            break;
        }
        case HidPortableDeviceButtons_ButtonId_VolumePlus:
        {
            if (ButtonStateDown)
            {
                moduleContext->InputReportButtonState.u.Buttons.VolumeUp = 1;
                DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_VolumePlus.Down{Vol+ press}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
                if (moduleContext->InputReportButtonState.u.Buttons.Power)
                {
                    // Verify Screen Capture runs.
                    //
                    DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_Power.ScreenCapture{Press Power and Vol+}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
                }
            }
            else
            {
                moduleContext->InputReportButtonState.u.Buttons.VolumeUp = 0;
                DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_VolumePlus.Up{Vol+ release}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
            }
            break;
        }
        case HidPortableDeviceButtons_ButtonId_VolumeMinus:
        {
            if (ButtonStateDown)
            {
                moduleContext->InputReportButtonState.u.Buttons.VolumeDown = 1;
                DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_VolumeMinus.Down{Vol- press}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
                if (moduleContext->InputReportButtonState.u.Buttons.Power)
                {
                    // Verify SAS runs.
                    //
                    DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_Power.SAS{Press Power and Vol-}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
                }
            }
            else
            {
                moduleContext->InputReportButtonState.u.Buttons.VolumeDown = 0;
                DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_VolumeMinus.Up{Vol- release}[HidPortableDeviceButtons,Volume]", HidPortableDeviceButtons_ButtonPresses);
            }
            break;
        }
        default:
        {
            ASSERT(FALSE);
            ntStatus = STATUS_NOT_SUPPORTED;
            DMF_BRANCHTRACK_MODULE_NEVER(DmfModule, "ButtonStateChange.HidPortableDeviceButtons_ButtonId_Power");
            DMF_ModuleUnlock(DmfModule);
            goto Exit;
        }
    }

    // Don't send requests with lock held. Copy the data to send to local variable,
    // unlock and send.
    //
    BUTTONS_INPUT_REPORT inputReportButtonState;
    HID_XFER_PACKET hidXferPacket;

    inputReportButtonState = moduleContext->InputReportButtonState;
    hidXferPacket.reportBuffer = (UCHAR*)&inputReportButtonState;
    hidXferPacket.reportBufferLen = moduleContext->VhfHidReport.reportBufferLen;
    hidXferPacket.reportId = moduleContext->VhfHidReport.reportId;
    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Buttons state=0x%02x", moduleContext->InputReportButtonState.u.Data);

    DMF_ModuleUnlock(DmfModule);

    // This function actually populates the upper layer's input report with expected button data.
    //
    ntStatus = DMF_VirtualHidDeviceVhf_ReadReportSend(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                      &hidXferPacket);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_HidPortableDeviceButtons_HotkeyStateChange(
    _In_ DMFMODULE DmfModule,
    _In_ HidPortableDeviceButtons_ButtonIdType Hotkey,
    _In_ ULONG HotkeyStateDown
    )
/*++

Routine Description:

    Updates the state of a given hotkey.

Arguments:

    DmfModule - This Module's handle.
    ButtonId - The given hotkey.
    ButtonStateDown - Indicates if the hotkey is pressed DOWN.

Return Value:

    STATUS_SUCCESS means the updated state was successfully sent to HID stack.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HidPortableDeviceButtons* moduleContext;
    BUTTONS_HOTKEY_INPUT_REPORT hotkeyInputReport;
    HID_XFER_PACKET hidXferPacket;

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_HidPortableDeviceButtons);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    hotkeyInputReport.ReportId = REPORTID_HOTKEYS;
    hotkeyInputReport.HotKey = 0;

    // If statements are used below for clarity and ease of debugging. Also, it prevents
    // need to cast and allows for possible different states later.
    //
    switch (Hotkey)
    {
        case HidPortableDeviceButtons_Hotkey_BrightnessUp:
            if (HotkeyStateDown)
            {
                hotkeyInputReport.HotKey = DISPLAY_BACKLIGHT_BRIGHTNESS_INCREMENT;
                DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "HotkeyStateChange.HidPortableDeviceButtons_Hotkey_BrightnessUp.Down{Backlight+ press}[SshKeypad]", HidPortableDeviceButtons_ButtonPresses);
            }
            else
            {
                DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "HotkeyStateChange.HidPortableDeviceButtons_Hotkey_BrightnessUp.Up{Backlight+ release}[SshKeypad]", HidPortableDeviceButtons_ButtonPresses);
            }
            break;

        case HidPortableDeviceButtons_Hotkey_BrightnessDown:
            if (HotkeyStateDown)
            {
                hotkeyInputReport.HotKey = DISPLAY_BACKLIGHT_BRIGHTNESS_DECREMENT;
                DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "HotkeyStateChange.HidPortableDeviceButtons_Hotkey_BrightnessDown.Down{BacklightDown- press}[SshKeypad]", HidPortableDeviceButtons_ButtonPresses);
            }
            else
            {
                DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfModule, "HotkeyStateChange.HidPortableDeviceButtons_Hotkey_BrightnessDown.Up{BacklightDown- release}[SshKeypad]", HidPortableDeviceButtons_ButtonPresses);
            }
            break;

        default:
            ASSERT(FALSE);
            ntStatus = STATUS_NOT_SUPPORTED;
            DMF_BRANCHTRACK_MODULE_NEVER(DmfModule, "HotkeyStateChange.DMF_HidPortableDeviceButtons_HotkeyStateChange");
            DMF_ModuleUnlock(DmfModule);
            goto Exit;
    }

    hidXferPacket.reportBuffer = (UCHAR*)&hotkeyInputReport;
    hidXferPacket.reportBufferLen = sizeof(BUTTONS_HOTKEY_INPUT_REPORT);
    hidXferPacket.reportId = REPORTID_HOTKEYS;
    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Hotkey input report hotkey=0x%02x", hotkeyInputReport.HotKey);

    // This function actually populates the upper layer's input report with expected button data.
    //
    ntStatus = DMF_VirtualHidDeviceVhf_ReadReportSend(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                      &hidXferPacket);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// eof: Dmf_HidPortableDeviceButtons.c
//
