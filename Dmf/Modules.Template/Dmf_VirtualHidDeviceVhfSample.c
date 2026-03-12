/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_VirtualHidDeviceVhfSample.c

Abstract:

    This Module provides the functionality exposed by the original VHIDMINI2 sample. It creates
    a sample device with some HID features, input and output reports. It is a template upon which
    other virtual HID devices for Kernel/User-mode can be built. Kernel-mode only devices should
    use DMF_VirtualHidDeviceVhf Module instead.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Template.h"
#include "DmfModules.Template.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_VirtualHidDeviceVhfSample.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Input from device to system.
//
typedef struct _HIDMINI_INPUT_REPORT
{
    // Report Id.
    //
    UCHAR ReportId;
    // Data in the Read Report.
    //
    UCHAR Data; 
} HIDMINI_INPUT_REPORT;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_VirtualHidDeviceVhfSample
{
    // Underlying VHIDMINI2 support.
    //
    DMFMODULE DmfModuleVirtualHidDeviceVhf;
    // Private data for this device.
    //
    BYTE DeviceData;
    HID_DEVICE_ATTRIBUTES HidDeviceAttributes;
    HID_DESCRIPTOR HidDescriptor;
    HIDMINI_INPUT_REPORT ReadReport;
    WDFTIMER Timer;
    BOOLEAN ReadReportReady;
} DMF_CONTEXT_VirtualHidDeviceVhfSample;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(VirtualHidDeviceVhfSample)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(VirtualHidDeviceVhfSample)

// MemoryTag.
//
#define MemoryTag 'mDHV'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// These are the default device attributes set in the driver
// which are used to identify the device.
//
#define HIDMINI_DEFAULT_PID         0xFEED
#define HIDMINI_DEFAULT_VID         0xDEED

// These are the device attributes returned by the mini driver in response
// to IOCTL_HID_GET_DEVICE_ATTRIBUTES.
//
#define HIDMINI_TEST_PID            0xDEEF
#define HIDMINI_TEST_VID            0xFEED
#define HIDMINI_TEST_VERSION        0x0505

// Custom control codes are defined here. They are to be used for sideband 
// communication with the hid minidriver. These control codes are sent to 
// the hid minidriver using Hid_SetFeature() API to a custom collection 
// defined especially to handle such requests.
//
#define  HIDMINI_CONTROL_CODE_SET_ATTRIBUTES              0x00
#define  HIDMINI_CONTROL_CODE_DUMMY1                      0x01
#define  HIDMINI_CONTROL_CODE_DUMMY2                      0x02

// This is the report id of the collection to which the control codes are sent.
//
#define CONTROL_COLLECTION_REPORT_ID                      0x01
#define TEST_COLLECTION_REPORT_ID                         0x02

// NOTE: Device strings are not supported in VHF.
//

// Data pump interval from device.
//
#define TIMER_PERIOD_IN_SECONDS 5

#include <pshpack1.h>

typedef struct _MY_DEVICE_ATTRIBUTES
{
    USHORT VendorID;
    USHORT ProductID;
    USHORT VersionNumber;
} MY_DEVICE_ATTRIBUTES;

typedef struct _HIDMINI_CONTROL_INFO
{
    // Report ID of the collection to which the control request is sent
    //
    UCHAR ReportId;

    // One byte control code (user-defined) for communication with hid 
    // mini driver.
    //
    UCHAR ControlCode;

    //
    // This union contains input data for the control request.
    //
    union 
    {
        MY_DEVICE_ATTRIBUTES Attributes;
        struct 
        {
            ULONG Dummy1;
            ULONG Dummy2;
        } Dummy;
    } u;
} HIDMINI_CONTROL_INFO;

// Output to device from system.
//
typedef struct _HIDMINI_OUTPUT_REPORT
{
    UCHAR ReportId;

    UCHAR Data; 

    USHORT Pad1;

    ULONG Pad2;
} HIDMINI_OUTPUT_REPORT;

#include <poppack.h>

// SetFeature request requires that the feature report buffer size be exactly 
// same as the size of report described in the hid report descriptor (
// excluding the report ID). Since HIDMINI_CONTROL_INFO includes report ID,
// we subtract one from the size.
//
#define FEATURE_REPORT_SIZE_CB      ((USHORT)(sizeof(HIDMINI_CONTROL_INFO) - 1))
#define INPUT_REPORT_SIZE_CB        ((USHORT)(sizeof(HIDMINI_INPUT_REPORT) - 1))
#define OUTPUT_REPORT_SIZE_CB       ((USHORT)(sizeof(HIDMINI_OUTPUT_REPORT) - 1))

#define CONTROL_FEATURE_REPORT_ID   0x01

typedef UCHAR HID_REPORT_DESCRIPTOR;

// This is the default report descriptor for the virtual Hid device returned
// by the mini driver in response to IOCTL_HID_GET_REPORT_DESCRIPTOR.
//
HID_REPORT_DESCRIPTOR
g_VirtualHidDeviceVhfSample_DefaultReportDescriptor[] = 
{
    0x06,0x00, 0xFF,                                                        // USAGE_PAGE (Vendor Defined Usage Page)
    0x09,0x01,                                                              // USAGE (Vendor Usage 0x01)
    0xA1,0x01,                                                              // COLLECTION (HID_FLAGS_COLLECTION_Application)
    0x85,CONTROL_FEATURE_REPORT_ID,                                         // REPORT_ID (1)
    0x09,0x01,                                                              // USAGE (Vendor Usage 0x01)
    0x15,0x00,                                                              // LOGICAL_MINIMUM(0)
    0x26,0xff, 0x00,                                                        // LOGICAL_MAXIMUM(255)
    0x75,0x08,                                                              // REPORT_SIZE (0x08)
    0x96,(FEATURE_REPORT_SIZE_CB & 0xff), (FEATURE_REPORT_SIZE_CB >> 8),    // REPORT_COUNT
    0xB1,0x00,                                                              // FEATURE (Data,Ary,Abs)
    0x09,0x01,                                                              // USAGE (Vendor Usage 0x01)
    0x75,0x08,                                                              // REPORT_SIZE (0x08)
    0x96,(INPUT_REPORT_SIZE_CB & 0xff), (INPUT_REPORT_SIZE_CB >> 8),        // REPORT_COUNT
    0x81,0x00,                                                              // INPUT (Data,Ary,Abs)
    0x09,0x01,                                                              // USAGE (Vendor Usage 0x01)
    0x75,0x08,                                                              // REPORT_SIZE (0x08)
    0x96,(OUTPUT_REPORT_SIZE_CB & 0xff), (OUTPUT_REPORT_SIZE_CB >> 8),      // REPORT_COUNT
    0x91,0x00,                                                              // OUTPUT (Data,Ary,Abs)
    0xC0,                                                                   // END_COLLECTION
};

_Function_class_(EVT_VHF_ASYNC_OPERATION)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
VirtualHidDeviceVhfSample_WriteReport(
    _In_ VOID* VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_opt_ VOID* VhfOperationContext,
    _In_ HID_XFER_PACKET* HidTransferPacket
    )
/*++

Routine Description:

    VHF Write Report callback. Client reads data from given buffer and processes it.
    IMPORTANT: Please read MSDN VHF documentation for more details on this callback and its usage.

Arguments:

    VhfClientContext - This Module's handle is passed as context by VHF.
    VhfOperationHandle - Handle for VHF for this transaction.
    VhfOperationContext - Context for VHF for this transaction.
    HidTransferPacket - Where to write the data to.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModule;
    DMF_CONTEXT_VirtualHidDeviceVhfSample* moduleContext;
    ULONG reportSize;
    HIDMINI_OUTPUT_REPORT* outputReport;

    UNREFERENCED_PARAMETER(VhfOperationContext);

    dmfModule = (DMFMODULE)VhfClientContext;
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    if (HidTransferPacket->reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        // Return error for unknown collection
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualHidDeviceVhfSample_WriteReport: unknown report id %d", HidTransferPacket->reportId);
        goto Exit;
    }

    // Before touching buffer make sure buffer is big enough.
    //
    reportSize = sizeof(HIDMINI_OUTPUT_REPORT);

    if (HidTransferPacket->reportBufferLen < reportSize)
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualHidDeviceVhfSample_WriteReport: invalid input buffer. size %d, expect %d", HidTransferPacket->reportBufferLen, reportSize);
        goto Exit;
    }

    outputReport = (HIDMINI_OUTPUT_REPORT*)HidTransferPacket->reportBuffer;

    // Store the device data in the Module Context.
    //
    moduleContext->DeviceData = outputReport->Data;

    ntStatus = STATUS_SUCCESS;

Exit:

    DMF_VirtualHidDeviceVhf_AsynchronousOperationComplete(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                          VhfOperationHandle,
                                                          ntStatus);
}

_Function_class_(EVT_VHF_ASYNC_OPERATION)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
VirtualHidDeviceVhfSample_GetFeature(
    _In_ VOID* VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_opt_ VOID* VhfOperationContext,
    _In_ HID_XFER_PACKET* HidTransferPacket
    )
/*++

Routine Description:

    VHF Get Feature Report Callback. Client writes data to given buffer.
    IMPORTANT: Please read MSDN VHF documentation for more details on this callback and its usage.

Arguments:

    VhfClientContext - This Module's handle is passed as context by VHF.
    VhfOperationHandle - Handle for VHF for this transaction.
    VhfOperationContext - Context for VHF for this transaction.
    HidTransferPacket - Where to write the data to.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModule;
    DMF_CONTEXT_VirtualHidDeviceVhfSample* moduleContext;
    ULONG reportSize;
    MY_DEVICE_ATTRIBUTES* myAttributes;
    HID_DEVICE_ATTRIBUTES*  hidAttributes;

    UNREFERENCED_PARAMETER(VhfOperationContext);

    dmfModule = (DMFMODULE)VhfClientContext;
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    hidAttributes = &moduleContext->HidDeviceAttributes;

    if (HidTransferPacket->reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualHidDeviceVhfSample_GetFeature fails: invalid report id %d", HidTransferPacket->reportId);
        goto Exit;
    }

    // Ensure uninitialized data is not returned.
    //
    if (HidTransferPacket->reportBufferLen < sizeof(HIDMINI_CONTROL_INFO))
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualHidDeviceVhfSample_GetFeature fails: reportBufferLen=%d expected=%d",
                    HidTransferPacket->reportBufferLen,
                    sizeof(HIDMINI_CONTROL_INFO));
        goto Exit;

    }
    RtlZeroMemory(HidTransferPacket->reportBuffer,
                  sizeof(HIDMINI_CONTROL_INFO));

    reportSize = sizeof(MY_DEVICE_ATTRIBUTES) + sizeof(HidTransferPacket->reportId);
    if (HidTransferPacket->reportBufferLen < reportSize) 
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "VirtualHidDeviceVhfSample_GetFeature fails: output buffer too small. Size %d, expect %d",
                    HidTransferPacket->reportBufferLen,
                    reportSize);
        goto Exit;
    }

    // Since this device has one report ID, hidclass would pass on the report
    // ID in the buffer (it wouldn't if report descriptor did not have any report
    // ID). However, since UMDF allows only writes to an output buffer, we can't
    // "read" the report ID from "output" buffer. There is no need to read the
    // report ID since we get it other way as shown above, however this is
    // something to keep in mind.
    //
    myAttributes = (MY_DEVICE_ATTRIBUTES*)(HidTransferPacket->reportBuffer + sizeof(HidTransferPacket->reportId));
    myAttributes->ProductID     = hidAttributes->ProductID;
    myAttributes->VendorID      = hidAttributes->VendorID;
    myAttributes->VersionNumber = hidAttributes->VersionNumber;

    ntStatus = STATUS_SUCCESS;

Exit:

    DMF_VirtualHidDeviceVhf_AsynchronousOperationComplete(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                          VhfOperationHandle,
                                                          ntStatus);
}

_Function_class_(EVT_VHF_ASYNC_OPERATION)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
VirtualHidDeviceVhfSample_SetFeature(
    _In_ VOID* VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_opt_ VOID* VhfOperationContext,
    _In_ HID_XFER_PACKET* HidTransferPacket
    )
/*++

Routine Description:

    VHF Set Feature Callback. Client reads data from given buffer.
    IMPORTANT: Please read MSDN VHF documentation for more details on this callback and its usage.

Arguments:

    VhfClientContext - This Module's handle is passed as context by VHF.
    VhfOperationHandle - Handle for VHF for this transaction.
    VhfOperationContext - Context for VHF for this transaction.
    HidTransferPacket - Where to read the data from.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModule;
    DMF_CONTEXT_VirtualHidDeviceVhfSample* moduleContext;
    ULONG reportSize;
    HIDMINI_CONTROL_INFO* controlInfo;
    PHID_DEVICE_ATTRIBUTES  hidAttributes;

    UNREFERENCED_PARAMETER(VhfOperationContext);

    dmfModule = (DMFMODULE)VhfClientContext;
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    hidAttributes = &moduleContext->HidDeviceAttributes;

    if (HidTransferPacket->reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualHidDeviceVhfSample_SetFeature fails: invalid report id %d", HidTransferPacket->reportId);
        goto Exit;
    }

    // Before touching control code make sure buffer is big enough.
    //
    reportSize = sizeof(HIDMINI_CONTROL_INFO);

    if (HidTransferPacket->reportBufferLen < reportSize) 
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "VirtualHidDeviceVhfSample_SetFeature fails: invalid input buffer. size %d, expect %d",
                    HidTransferPacket->reportBufferLen, reportSize);
        goto Exit;
    }

    controlInfo = (HIDMINI_CONTROL_INFO*)HidTransferPacket->reportBuffer;
    switch (controlInfo->ControlCode)
    {
        case HIDMINI_CONTROL_CODE_SET_ATTRIBUTES:
            // Store the device attributes in device extension.
            //
            hidAttributes->ProductID     = controlInfo->u.Attributes.ProductID;
            hidAttributes->VendorID      = controlInfo->u.Attributes.VendorID;
            hidAttributes->VersionNumber = controlInfo->u.Attributes.VersionNumber;

            // Set ntStatus and information.
            //
            ntStatus = STATUS_SUCCESS;
            break;

        case HIDMINI_CONTROL_CODE_DUMMY1:
            ntStatus = STATUS_NOT_IMPLEMENTED;
            break;

        case HIDMINI_CONTROL_CODE_DUMMY2:
            ntStatus = STATUS_NOT_IMPLEMENTED;
            break;

        default:
            ntStatus = STATUS_NOT_IMPLEMENTED;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualHidDeviceVhfSample_SetFeature fails: Unknown control Code 0x%x", controlInfo->ControlCode);
            break;
    }

Exit:

    DMF_VirtualHidDeviceVhf_AsynchronousOperationComplete(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                          VhfOperationHandle,
                                                          ntStatus);
}

_Function_class_(EVT_VHF_ASYNC_OPERATION)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
VirtualHidDeviceVhfSample_GetInputReport(
    _In_ VOID* VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_opt_ VOID* VhfOperationContext,
    _In_ HID_XFER_PACKET* HidTransferPacket
    )
/*++

Routine Description:

    VHF Input Report Callback. Client writes data to given buffer.
    IMPORTANT: Please read MSDN VHF documentation for more details on this callback and its usage.

Arguments:

    VhfClientContext - This Module's handle is passed as context by VHF.
    VhfOperationHandle - Handle for VHF for this transaction.
    VhfOperationContext - Context for VHF for this transaction.
    HidTransferPacket - Where to write the data to.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_VirtualHidDeviceVhfSample* moduleContext;
    HIDMINI_INPUT_REPORT* reportBuffer;
    NTSTATUS ntStatus;
    ULONG reportSize;

    UNREFERENCED_PARAMETER(VhfOperationContext);

    dmfModule = (DMFMODULE)VhfClientContext;
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    if (HidTransferPacket->reportId != CONTROL_COLLECTION_REPORT_ID)
    {
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        //
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VirtualHidDeviceVhfSample_GetInputReport fails: invalid report id %d", HidTransferPacket->reportId);
        goto Exit;
    }

    reportSize = sizeof(HIDMINI_INPUT_REPORT);
    if (HidTransferPacket->reportBufferLen < reportSize)
    {
        ntStatus = STATUS_INVALID_BUFFER_SIZE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "VirtualHidDeviceVhfSample_GetInputReport fails: output buffer too small. Size %d, expect %d",
                    HidTransferPacket->reportBufferLen,
                    reportSize);
        goto Exit;
    }

    reportBuffer = (HIDMINI_INPUT_REPORT*)(HidTransferPacket->reportBuffer);

    reportBuffer->ReportId = CONTROL_COLLECTION_REPORT_ID;
    reportBuffer->Data     = moduleContext->DeviceData;

    ntStatus = STATUS_SUCCESS;

Exit:

    DMF_VirtualHidDeviceVhf_AsynchronousOperationComplete(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                          VhfOperationHandle,
                                                          ntStatus);

}

// NOTE: VHF does not support HID_SET_OUTPUT_REPORT.
// 

_Function_class_(EVT_VHF_READY_FOR_NEXT_READ_REPORT)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
VirtualHidDeviceVhfSample_ReadReport(
    _In_ VOID* VhfClientContext
    )
/*++

Routine Description:

    VHF Read Report Callback. Client writes data to given buffer.
    IMPORTANT: Please read MSDN VHF documentation for more details on this callback and its usage.

Arguments:

    VhfClientContext - This Module's handle is passed as context by VHF.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_VirtualHidDeviceVhfSample* moduleContext;

    dmfModule = (DMFMODULE)VhfClientContext;
    moduleContext = DMF_CONTEXT_GET(dmfModule);

    DMF_ModuleLock(dmfModule);
    moduleContext->ReadReportReady = TRUE;
    DMF_ModuleUnlock(dmfModule);
}

EVT_WDF_TIMER VirtualHidDeviceVhfSample_EvtTimerHandler;

VOID
VirtualHidDeviceVhfSample_EvtTimerHandler(
    _In_ WDFTIMER Timer
    )
/*++
Routine Description:

    This periodic timer callback routine checks the device's manual queue and
    completes any pending request with data from the device.

Arguments:

    Timer - Handle to a timer object that was obtained from WdfTimerCreate.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModule;
    DMF_CONTEXT_VirtualHidDeviceVhfSample* moduleContext;
    HIDMINI_INPUT_REPORT* readReport;

    dmfModule = (DMFMODULE)WdfTimerGetParentObject(Timer);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    BOOLEAN readyForNextReport = FALSE;
    DMF_ModuleLock(dmfModule);
    if (moduleContext->ReadReportReady)
    {
        readyForNextReport = TRUE;
        moduleContext->ReadReportReady = FALSE;
    }
    DMF_ModuleUnlock(dmfModule);

    if (!readyForNextReport)
    {
        // No read report is pending, so no need to send a report.
        //
        goto Exit;
    }

    readReport = &moduleContext->ReadReport;

    // Populate data to return to caller.
    //
    readReport->ReportId = CONTROL_FEATURE_REPORT_ID;
    readReport->Data = moduleContext->DeviceData;

    HID_XFER_PACKET hidXferPacket;

    hidXferPacket.reportBuffer = (UCHAR*)readReport;
    hidXferPacket.reportBufferLen = sizeof(HIDMINI_INPUT_REPORT);
    hidXferPacket.reportId = readReport->ReportId;
    ntStatus = DMF_VirtualHidDeviceVhf_ReadReportSend(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                      &hidXferPacket);
    if ( !NT_SUCCESS(ntStatus) )
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_VirtualHidDeviceVhf_ReadReportSend fails: ntStatus=%!STATUS!", ntStatus);
    }

Exit:

    WdfTimerStart(moduleContext->Timer,
                  WDF_REL_TIMEOUT_IN_SEC(TIMER_PERIOD_IN_SECONDS));

    return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// NOTE: Devices strings are not supported in VHF.
//

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_VirtualHidDeviceVhfSample_ChildModulesAdd(
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
    DMF_CONFIG_VirtualHidDeviceVhfSample* moduleConfig;
    DMF_CONTEXT_VirtualHidDeviceVhfSample* moduleContext;
    DMF_CONFIG_VirtualHidDeviceVhf moduleConfigVirtualHidDeviceVhf;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleConfig->ReadFromRegistry)
    {
        // TODO: Read HID descriptors from Registry.
        //
    }

    // VirtualHidDeviceVhf
    // -------------------
    //
    DMF_CONFIG_VirtualHidDeviceVhf_AND_ATTRIBUTES_INIT(&moduleConfigVirtualHidDeviceVhf,
                                                       &moduleAttributes);

    moduleConfigVirtualHidDeviceVhf.VendorId = HIDMINI_DEFAULT_VID;
    moduleConfigVirtualHidDeviceVhf.ProductId = HIDMINI_DEFAULT_PID;
    moduleConfigVirtualHidDeviceVhf.VersionNumber = HIDMINI_TEST_VERSION;

    moduleConfigVirtualHidDeviceVhf.HidReportDescriptor = g_VirtualHidDeviceVhfSample_DefaultReportDescriptor;
    moduleConfigVirtualHidDeviceVhf.HidReportDescriptorLength = sizeof(g_VirtualHidDeviceVhfSample_DefaultReportDescriptor);

    moduleConfigVirtualHidDeviceVhf.StartOnOpen = TRUE;
    moduleConfigVirtualHidDeviceVhf.VhfClientContext = DmfModule;

    moduleConfigVirtualHidDeviceVhf.IoctlCallback_IOCTL_HID_GET_INPUT_REPORT = VirtualHidDeviceVhfSample_GetInputReport;
    moduleConfigVirtualHidDeviceVhf.IoctlCallback_IOCTL_HID_GET_FEATURE = VirtualHidDeviceVhfSample_GetFeature;
    moduleConfigVirtualHidDeviceVhf.IoctlCallback_IOCTL_HID_SET_FEATURE = VirtualHidDeviceVhfSample_SetFeature;
    moduleConfigVirtualHidDeviceVhf.IoctlCallback_IOCTL_HID_WRITE_REPORT = VirtualHidDeviceVhfSample_WriteReport;
    moduleConfigVirtualHidDeviceVhf.IoctlCallback_IOCTL_HID_READ_REPORT = VirtualHidDeviceVhfSample_ReadReport;

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleVirtualHidDeviceVhf);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_VirtualHidDeviceVhfSample_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type VirtualHidDeviceVhfSample.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_VirtualHidDeviceVhfSample* moduleContext;
    WDF_TIMER_CONFIG timerConfig;
    WDF_OBJECT_ATTRIBUTES timerAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Initialize the device's data.
    //
    moduleContext->DeviceData = 0;
    moduleContext->ReadReport.ReportId = CONTROL_FEATURE_REPORT_ID;
    moduleContext->ReadReport.Data = moduleContext->DeviceData;

    // Intentionally set to zero to show it can be changed by client through control code. 
    //
    moduleContext->HidDeviceAttributes.ProductID = 0;
    moduleContext->HidDeviceAttributes.VendorID = 0;
    moduleContext->HidDeviceAttributes.VersionNumber = 0;

    // This timer simulates data coming from the device asynchrnously.
    //
    WDF_TIMER_CONFIG_INIT(&timerConfig,
                          VirtualHidDeviceVhfSample_EvtTimerHandler);
    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = DmfModule;
    timerAttributes.ExecutionLevel = WdfExecutionLevelPassive;
    ntStatus = WdfTimerCreate(&timerConfig,
                              &timerAttributes,
                              &moduleContext->Timer);
    if ( !NT_SUCCESS(ntStatus) )
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfTimerCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Start timer.
    //
    WdfTimerStart(moduleContext->Timer,
                  WDF_REL_TIMEOUT_IN_SEC(TIMER_PERIOD_IN_SECONDS));

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_VirtualHidDeviceVhfSample_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type VirtualHidDeviceVhfSample.

Arguments:

    DmfModule - This MOdule's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_VirtualHidDeviceVhfSample* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    WdfTimerStop(moduleContext->Timer,
                 TRUE);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_VirtualHidDeviceVhfSample_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type VirtualHidDeviceVhfSample.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_VirtualHidDeviceVhfSample;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_VirtualHidDeviceVhfSample;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_VirtualHidDeviceVhfSample);
    dmfCallbacksDmf_VirtualHidDeviceVhfSample.ChildModulesAdd = DMF_VirtualHidDeviceVhfSample_ChildModulesAdd;
    dmfCallbacksDmf_VirtualHidDeviceVhfSample.DeviceOpen = DMF_VirtualHidDeviceVhfSample_Open;
    dmfCallbacksDmf_VirtualHidDeviceVhfSample.DeviceClose = DMF_VirtualHidDeviceVhfSample_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_VirtualHidDeviceVhfSample,
                                            VirtualHidDeviceVhfSample,
                                            DMF_CONTEXT_VirtualHidDeviceVhfSample,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_VirtualHidDeviceVhfSample.CallbacksDmf = &dmfCallbacksDmf_VirtualHidDeviceVhfSample;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_VirtualHidDeviceVhfSample,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_VirtualHidDeviceVhfSample.c
//
