/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_VirtualHidAmbientLightSensor.c

Abstract:

    Exposes a virtual HID Ambient Light Sensor (ALS) and methods to send lux data up the HID stack.

Environment:

    Kernel-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_VirtualHidAmbientLightSensor.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Input Report
//
#pragma pack(1)
typedef struct
{
    UCHAR ReportId;
    VirtualHidAmbientLightSensor_ALS_INPUT_REPORT_DATA InputReportData;
} ALS_INPUT_REPORT;
#pragma pack()

// Feature Report
//
#pragma pack(1)
typedef struct
{
    UCHAR ReportId;
    VirtualHidAmbientLightSensor_ALS_FEATURE_REPORT_DATA FeatureReportData;
} ALS_FEATURE_REPORT;
#pragma pack()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Virtual Hid Device via Vhf.
    //
    DMFMODULE DmfModuleVirtualHidDeviceVhf;

    // ALS Input Report.
    //
    ALS_INPUT_REPORT InputReport;
    // ALS Feature Report.
    //
    ALS_FEATURE_REPORT FeatureReport;
} DMF_CONTEXT_VirtualHidAmbientLightSensor;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(VirtualHidAmbientLightSensor)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(VirtualHidAmbientLightSensor)

// MemoryTag.
//
#define MemoryTag 'SLAV'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define REPORT_ID_ALS 1

static
const
UCHAR
g_VirtualHidAmbientLightSensor_HidReportDescriptor[] =
{
    0x05, HID_USAGE_PAGE_SENSOR,
    HID_USAGE_SENSOR_TYPE_LIGHT_AMBIENTLIGHT,
    HID_COLLECTION(HID_FLAGS_COLLECTION_Physical),

    HID_REPORT_ID(REPORT_ID_ALS),

    // Feature Report
    // --------------
    //

    // Connection Type
    //
    HID_USAGE_SENSOR_PROPERTY_SENSOR_CONNECTION_TYPE,
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_8(2),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(1),
    HID_COLLECTION(HID_FLAGS_COLLECTION_Logical),
        HID_USAGE_SENSOR_PROPERTY_CONNECTION_TYPE_PC_INTEGRATED_SEL,
        HID_USAGE_SENSOR_PROPERTY_CONNECTION_TYPE_PC_ATTACHED_SEL,
        HID_USAGE_SENSOR_PROPERTY_CONNECTION_TYPE_PC_EXTERNAL_SEL,
        HID_FEATURE(Data_Arr_Abs),
        HID_END_COLLECTION,

    // Reporting State
    //
    HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE,
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_8(5),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(1),
    HID_COLLECTION(HID_FLAGS_COLLECTION_Logical),
        HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_NO_EVENTS_SEL,
        HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_ALL_EVENTS_SEL,
        HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_THRESHOLD_EVENTS_SEL,
        HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_NO_EVENTS_WAKE_SEL,
        HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_ALL_EVENTS_WAKE_SEL,
        HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_THRESHOLD_EVENTS_WAKE_SEL,
        HID_FEATURE(Data_Arr_Abs),
        HID_END_COLLECTION,

    // Power State
    //
    HID_USAGE_SENSOR_PROPERTY_POWER_STATE,
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_8(5),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(1),
        HID_COLLECTION(HID_FLAGS_COLLECTION_Logical),
        HID_USAGE_SENSOR_PROPERTY_POWER_STATE_UNDEFINED_SEL,
        HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D0_FULL_POWER_SEL,
        HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D1_LOW_POWER_SEL,
        HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D2_STANDBY_WITH_WAKE_SEL,
        HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D3_SLEEP_WITH_WAKE_SEL,
        HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D4_POWER_OFF_SEL,
        HID_FEATURE(Data_Arr_Abs),
        HID_END_COLLECTION,

    // Sensor State
    //
    HID_USAGE_SENSOR_STATE,
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_8(6),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(1),
    HID_COLLECTION(HID_FLAGS_COLLECTION_Logical),
        HID_USAGE_SENSOR_STATE_UNKNOWN_SEL,
        HID_USAGE_SENSOR_STATE_READY_SEL,
        HID_USAGE_SENSOR_STATE_NOT_AVAILABLE_SEL,
        HID_USAGE_SENSOR_STATE_NO_DATA_SEL,
        HID_USAGE_SENSOR_STATE_INITIALIZING_SEL,
        HID_USAGE_SENSOR_STATE_ACCESS_DENIED_SEL,
        HID_USAGE_SENSOR_STATE_ERROR_SEL,
        HID_FEATURE(Data_Arr_Abs),
        HID_END_COLLECTION,

    // Change Sensitivity Relative Percentage
    // (Divide by 100 to get actual value.)
    //
    HID_USAGE_SENSOR_DATA(HID_USAGE_SENSOR_DATA_LIGHT_ILLUMINANCE,HID_USAGE_SENSOR_DATA_MOD_CHANGE_SENSITIVITY_REL_PCT),
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_16(0xFF,0xFF),
    HID_REPORT_SIZE(16),
    HID_REPORT_COUNT(1),
    HID_UNIT_EXPONENT(0x0E),
    HID_FEATURE(Data_Var_Abs),

    // Change Sensitivity Absolute
    //
    HID_USAGE_SENSOR_DATA(HID_USAGE_SENSOR_DATA_LIGHT_ILLUMINANCE, HID_USAGE_SENSOR_DATA_MOD_CHANGE_SENSITIVITY_ABS),
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_16(0xFF, 0xFF),
    HID_REPORT_SIZE(16),
    HID_REPORT_COUNT(1),
    HID_UNIT_EXPONENT(0x0E),
    HID_FEATURE(Data_Var_Abs),

    // Report Interval
    //
    HID_USAGE_SENSOR_PROPERTY_REPORT_INTERVAL,
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_32(0xFF,0xFF,0xFF,0xFF),
    HID_REPORT_SIZE(32),
    HID_REPORT_COUNT(1),
    HID_UNIT_EXPONENT(0),
    HID_FEATURE(Data_Var_Abs),

    // Minimum Report Interval
    // NOTE: This value must be correct for SDT to run.
    // (Do not use zero.)
    //
    HID_USAGE_SENSOR_PROPERTY_MINIMUM_REPORT_INTERVAL,
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_32(0xFF, 0xFF, 0xFF, 0xFF),
    HID_REPORT_SIZE(32),
    HID_REPORT_COUNT(1),
    HID_UNIT_EXPONENT(0),
    HID_FEATURE(Data_Var_Abs),

    // Response Curve
    //
    HID_USAGE_SENSOR_PROPERTY_RESPONSE_CURVE,
    HID_LOGICAL_MIN_16(0x01,0x80),
    HID_LOGICAL_MAX_16(0xFF,0x7F),
    HID_REPORT_SIZE(16),
    HID_REPORT_COUNT(VirtualHidAmbientLightSensor_MAXIMUM_NUMBER_OF_ALR_CURVE_RECORDS * 2),
    HID_UNIT_EXPONENT(0x0),
    HID_FEATURE(Data_Var_Abs),

    // Input Report
    // ------------
    //

    // Lux
    //
    HID_USAGE_SENSOR_DATA_LIGHT_ILLUMINANCE,
    HID_LOGICAL_MIN_32(0x01U,0x00U,0x00U,0x80U),
    HID_LOGICAL_MAX_32(0xFFU,0xFFU,0xFFU,0x7FU),
    HID_REPORT_SIZE(32U),
    HID_REPORT_COUNT(1U),
    HID_UNIT_EXPONENT(0x0U),
    HID_INPUT(Data_Var_Abs),

    // Sensor State
    //
    0x05, HID_USAGE_PAGE_SENSOR,
    HID_USAGE_SENSOR_STATE,
    HID_LOGICAL_MIN_8(0U),
    HID_LOGICAL_MAX_8(6U),
    HID_REPORT_SIZE(8U),
    HID_REPORT_COUNT(1U),
    HID_COLLECTION(HID_FLAGS_COLLECTION_Logical),
        HID_USAGE_SENSOR_STATE_UNKNOWN_SEL,
        HID_USAGE_SENSOR_STATE_READY_SEL,
        HID_USAGE_SENSOR_STATE_NOT_AVAILABLE_SEL,
        HID_USAGE_SENSOR_STATE_NO_DATA_SEL,
        HID_USAGE_SENSOR_STATE_INITIALIZING_SEL,
        HID_USAGE_SENSOR_STATE_ACCESS_DENIED_SEL,
        HID_USAGE_SENSOR_STATE_ERROR_SEL,
        HID_INPUT(Data_Arr_Abs),
        HID_END_COLLECTION,

    // Sensor Event
    //
    HID_USAGE_SENSOR_EVENT,
    HID_LOGICAL_MIN_8(0U),
    HID_LOGICAL_MAX_8(5U),
    HID_REPORT_SIZE(8U),
    HID_REPORT_COUNT(1U),
    HID_COLLECTION(HID_FLAGS_COLLECTION_Logical),
        HID_USAGE_SENSOR_EVENT_UNKNOWN_SEL,
        HID_USAGE_SENSOR_EVENT_STATE_CHANGED_SEL,
        HID_USAGE_SENSOR_EVENT_PROPERTY_CHANGED_SEL,
        HID_USAGE_SENSOR_EVENT_DATA_UPDATED_SEL,
        HID_USAGE_SENSOR_EVENT_POLL_RESPONSE_SEL,
        HID_USAGE_SENSOR_EVENT_CHANGE_SENSITIVITY_SEL,
        HID_INPUT(Data_Arr_Abs),
        HID_END_COLLECTION,

    // End of Collection
    // -----------------
    //
    HID_END_COLLECTION,
};

// HID Device Descriptor with just one report representing the keyboard.
//
static
const
HID_DESCRIPTOR
g_VirtualHidAmbientLightSensor_HidDescriptor =
{
    0x09,     // Length of HID descriptor
    0x21,     // Descriptor type == HID  0x21
    0x0100,   // HID spec release
    0x33,     // Country code == English
    0x01,     // Number of HID class descriptors
    {
      0x22,   // Descriptor type
      // Total length of report descriptor.
      //
      (USHORT) sizeof(g_VirtualHidAmbientLightSensor_HidReportDescriptor)
    }
};

#include <pshpack1.h>
typedef struct _VirtualHidAmbientLightSensor_INPUT_REPORT
{
#pragma pack(1)
    UCHAR ReportId;
    LONG Lux;
    UCHAR AlsSensorState;
    UCHAR AlsSensorEvent;
} VirtualHidAmbientLightSensor_INPUT_REPORT;
#include <poppack.h>

_Function_class_(EVT_VHF_ASYNC_OPERATION)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
VirtualHidAmbientLightSensor_EvtVhfAsyncOperationGetInputReport(
    _In_ PVOID VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_opt_ PVOID VhfOperationContext,
    _In_ PHID_XFER_PACKET HidTransferPacket
    )
/*++

Routine Description:

    VHF Input Report Callback. Client writes data to given buffer.

Arguments:

    VhfClientContext - This Module's handle.
    VhfOperationHandle - Handle for VHF for this transaction.
    VhfOperationContext - Context for VHF for this transaction.
    HidTransferPacket - Where to write the data.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_VirtualHidAmbientLightSensor* moduleContext;
    DMF_CONFIG_VirtualHidAmbientLightSensor* moduleConfig;
    ALS_INPUT_REPORT* alsInputReport;
    NTSTATUS ntStatus;
    
    UNREFERENCED_PARAMETER(VhfOperationContext);

    dmfModule = (DMFMODULE)VhfClientContext;
    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    alsInputReport = (ALS_INPUT_REPORT*)(HidTransferPacket->reportBuffer);

    if (HidTransferPacket->reportBufferLen < sizeof(moduleContext->InputReport))
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    DMF_ModuleLock(dmfModule);

    RtlZeroMemory(alsInputReport,
                  HidTransferPacket->reportBufferLen);

    // Get data from Client.
    //
    moduleConfig->InputReportDataGet(dmfModule,
                                     &moduleContext->InputReport.InputReportData);

    // Copy to HID packet.
    //
    RtlCopyMemory(alsInputReport,
                  &moduleContext->InputReport,
                  sizeof(ALS_INPUT_REPORT));

    DMF_ModuleUnlock(dmfModule);

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
VirtualHidAmbientLightSensor_EvtVhfAsyncOperationGetFeature(
    _In_ PVOID VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_opt_ PVOID VhfOperationContext,
    _In_ PHID_XFER_PACKET HidTransferPacket
    )
/*++

Routine Description:

    VHF Get Feature Report Callback. Client writes data to given buffer.

Arguments:

    VhfClientContext - This Module's handle.
    VhfOperationHandle - Handle for VHF for this transaction.
    VhfOperationContext - Context for VHF for this transaction.
    HidTransferPacket - Where to write the data.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_VirtualHidAmbientLightSensor* moduleContext;
    DMF_CONFIG_VirtualHidAmbientLightSensor* moduleConfig;
    ALS_FEATURE_REPORT* alsFeatureReport;
    NTSTATUS ntStatus;
    
    UNREFERENCED_PARAMETER(VhfOperationContext);

    dmfModule = (DMFMODULE)VhfClientContext;
    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    alsFeatureReport = (ALS_FEATURE_REPORT*)(HidTransferPacket->reportBuffer);

    if (HidTransferPacket->reportBufferLen < sizeof(moduleContext->FeatureReport))
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    DMF_ModuleLock(dmfModule);

    RtlZeroMemory(alsFeatureReport,
                  HidTransferPacket->reportBufferLen);

    moduleConfig->FeatureReportDataGet(dmfModule,
                                       &moduleContext->FeatureReport.FeatureReportData);

    // Copy to HID packet.
    //
    RtlCopyMemory(alsFeatureReport,
                  &moduleContext->FeatureReport,
                  sizeof(ALS_FEATURE_REPORT));

    DMF_ModuleUnlock(dmfModule);

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
VirtualHidAmbientLightSensor_EvtVhfAsyncOperationSetFeature(
    _In_ PVOID VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_opt_ PVOID VhfOperationContext,
    _In_ PHID_XFER_PACKET HidTransferPacket
    )
/*++

Routine Description:

    VHF Set Feature Callback. Client reads data from given buffer.

Arguments:

    VhfClientContext - This Module's handle.
    VhfOperationHandle - Handle for VHF for this transaction.
    VhfOperationContext - Context for VHF for this transaction.
    HidTransferPacket - Where to read the data from.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_VirtualHidAmbientLightSensor* moduleContext;
    DMF_CONFIG_VirtualHidAmbientLightSensor* moduleConfig;
    ALS_FEATURE_REPORT* alsFeatureReport;
    NTSTATUS ntStatus;
    
    UNREFERENCED_PARAMETER(VhfOperationContext);

    dmfModule = (DMFMODULE)VhfClientContext;
    moduleContext = DMF_CONTEXT_GET(dmfModule);
    moduleConfig = DMF_CONFIG_GET(dmfModule);

    alsFeatureReport = (ALS_FEATURE_REPORT*)(HidTransferPacket->reportBuffer);

    if (HidTransferPacket->reportBufferLen < sizeof(moduleContext->FeatureReport))
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    DMF_ModuleLock(dmfModule);

    // Copy from HID packet.
    //
    RtlCopyMemory(&moduleContext->FeatureReport,
                  alsFeatureReport,
                  sizeof(ALS_FEATURE_REPORT));

    // Copy to Client.
    //
    moduleConfig->FeatureReportDataSet(dmfModule,
                                       &moduleContext->FeatureReport.FeatureReportData);

    DMF_ModuleUnlock(dmfModule);

    ntStatus = STATUS_SUCCESS;

Exit:

    DMF_VirtualHidDeviceVhf_AsynchronousOperationComplete(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                          VhfOperationHandle, 
                                                          ntStatus);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_VirtualHidAmbientLightSensor_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type VirtualHidAmbientLightSensor.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_VirtualHidAmbientLightSensor* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Set it and never change it again. (Client just deals with payload.)
    //
    moduleContext->FeatureReport.ReportId = REPORT_ID_ALS;
    moduleContext->InputReport.ReportId = REPORT_ID_ALS;

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_VirtualHidAmbientLightSensor_ChildModulesAdd(
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
    DMF_CONFIG_VirtualHidAmbientLightSensor* moduleConfig;
    DMF_CONTEXT_VirtualHidAmbientLightSensor* moduleContext;
    DMF_CONFIG_VirtualHidDeviceVhf virtualHidDeviceVhfModuleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // VirtualHidDeviceVhf
    // -------------------
    //
    DMF_CONFIG_VirtualHidDeviceVhf_AND_ATTRIBUTES_INIT(&virtualHidDeviceVhfModuleConfig,
                                                        &moduleAttributes);

    virtualHidDeviceVhfModuleConfig.VendorId = moduleConfig->VendorId;
    virtualHidDeviceVhfModuleConfig.ProductId = moduleConfig->ProductId;
    virtualHidDeviceVhfModuleConfig.VersionNumber = 0x0001;

    virtualHidDeviceVhfModuleConfig.HidDescriptor = &g_VirtualHidAmbientLightSensor_HidDescriptor;
    virtualHidDeviceVhfModuleConfig.HidDescriptorLength = sizeof(g_VirtualHidAmbientLightSensor_HidDescriptor);
    virtualHidDeviceVhfModuleConfig.HidReportDescriptor = g_VirtualHidAmbientLightSensor_HidReportDescriptor;
    virtualHidDeviceVhfModuleConfig.HidReportDescriptorLength = sizeof(g_VirtualHidAmbientLightSensor_HidReportDescriptor);

    // Set virtual device attributes.
    //
    virtualHidDeviceVhfModuleConfig.HidDeviceAttributes.VendorID = moduleConfig->VendorId;
    virtualHidDeviceVhfModuleConfig.HidDeviceAttributes.ProductID = moduleConfig->ProductId;
    virtualHidDeviceVhfModuleConfig.HidDeviceAttributes.VersionNumber = moduleConfig->VersionNumber;
    virtualHidDeviceVhfModuleConfig.HidDeviceAttributes.Size = sizeof(virtualHidDeviceVhfModuleConfig.HidDeviceAttributes);

    virtualHidDeviceVhfModuleConfig.StartOnOpen = TRUE;
    virtualHidDeviceVhfModuleConfig.VhfClientContext = DmfModule;

    virtualHidDeviceVhfModuleConfig.IoctlCallback_IOCTL_HID_GET_INPUT_REPORT = VirtualHidAmbientLightSensor_EvtVhfAsyncOperationGetInputReport;
    virtualHidDeviceVhfModuleConfig.IoctlCallback_IOCTL_HID_GET_FEATURE = VirtualHidAmbientLightSensor_EvtVhfAsyncOperationGetFeature;
    virtualHidDeviceVhfModuleConfig.IoctlCallback_IOCTL_HID_SET_FEATURE = VirtualHidAmbientLightSensor_EvtVhfAsyncOperationSetFeature;

    DMF_DmfModuleAdd(DmfModuleInit,
                    &moduleAttributes,
                    WDF_NO_OBJECT_ATTRIBUTES,
                    &moduleContext->DmfModuleVirtualHidDeviceVhf);

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
DMF_VirtualHidAmbientLightSensor_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type VirtualHidAmbientLightSensor.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_VirtualHidAmbientLightSensor;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_VirtualHidAmbientLightSensor;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_VirtualHidAmbientLightSensor);
    dmfCallbacksDmf_VirtualHidAmbientLightSensor.DeviceOpen = DMF_VirtualHidAmbientLightSensor_Open;
    dmfCallbacksDmf_VirtualHidAmbientLightSensor.ChildModulesAdd = DMF_VirtualHidAmbientLightSensor_ChildModulesAdd;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_VirtualHidAmbientLightSensor,
                                            VirtualHidAmbientLightSensor,
                                            DMF_CONTEXT_VirtualHidAmbientLightSensor,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_VirtualHidAmbientLightSensor.CallbacksDmf = &dmfCallbacksDmf_VirtualHidAmbientLightSensor;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_VirtualHidAmbientLightSensor,
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
NTSTATUS
DMF_VirtualHidAmbientLightSensor_LuxValueSend(
    _In_ DMFMODULE DmfModule,
    _In_ float LuxValue
    )
/*++

Routine Description:

    Sends a given Lux value up the stack.

Arguments:

    DmfModule - This Module's handle.
    LuxValue - The given Lux value.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_VirtualHidAmbientLightSensor* moduleContext;
    HID_XFER_PACKET hidXferPacket;
    ALS_INPUT_REPORT alsInputReport;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 VirtualHidAmbientLightSensor);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    alsInputReport.ReportId = REPORT_ID_ALS;
    alsInputReport.InputReportData.Lux = (INT32)LuxValue;
    alsInputReport.InputReportData.AlsSensorState = HID_USAGE_SENSOR_STATE_READY_ENUM;
    alsInputReport.InputReportData.AlsSensorEvent = HID_USAGE_SENSOR_EVENT_STATE_CHANGED_ENUM;

    hidXferPacket.reportBuffer = (UCHAR*)&alsInputReport;
    hidXferPacket.reportBufferLen = sizeof(alsInputReport);
    hidXferPacket.reportId = REPORT_ID_ALS;

    ntStatus = DMF_VirtualHidDeviceVhf_ReadReportSend(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                      &hidXferPacket);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// eof: Dmf_VirtualHidAmbientLightSensor.c
//
