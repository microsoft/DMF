/*++

   Copyright (c) Microsoft Corporation. All Rights Reserved.
   Licensed under the MIT license.

Module Name:

    Dmf_ComponentFirmwareUpdateHidTransport.c

Abstract:

    This Module implements Human Interface Device (HID) Transport for Component Firmware Update. 

Environment:

    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_ComponentFirmwareUpdateHidTransport.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

//--------From Specification: Underlying HID Transport currently supported.-------//
//

typedef enum _PROTOCOL
{
    PROTOCOL_USB = 1,
    PROTOCOL_BTLE
} PROTOCOL;

// HID Timeout (Using the existing value).
//
const ULONG HIDDEVICE_RECOMMENDED_WAIT_TIMEOUT_MS = 90000;  

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Buffer Size from specification.
//
// Each time 60 bytes of Payload sent.
//
#define SizeOfPayload (60)
// Offer is 16 bytes long.
//
#define SizeOfOffer (16)
// Firmware Version is 60 bytes long.
//
#define SizeOfFirmwareVersion (60)

#define HidHeaderSize                     0x1

// Report IDs
//
#define REPORT_ID_FW_VERSION_FEATURE      0x20
#define REPORT_ID_PAYLOAD_CONTENT_OUTPUT  0x20
#define REPORT_ID_PAYLOAD_RESPONSE_INPUT  0x22
#define REPORT_ID_OFFER_CONTENT_OUTPUT    0x25
#define REPORT_ID_OFFER_RESPONSE_INPUT    0x25

//--------------------------------------------
//

// This Module's context.
//
typedef struct
{
    // HID Handle.
    //
    DMFMODULE DmfModuleHid;
} DMF_CONTEXT_ComponentFirmwareUpdateHidTransport;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(ComponentFirmwareUpdateHidTransport)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(ComponentFirmwareUpdateHidTransport)

#define MemoryTag 'TdiH'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// --------- HID layer   ------------------
//

// Interface Implementation 
//--------START ----------------
//

//-- Helper functions ---
//--------START----------
//
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_ 
_IRQL_requires_same_
#pragma code_seg("PAGED")
NTSTATUS
ComponentFirmwareUpdateHidTransport_ReportWrite(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Buffer,
    _In_ ULONG BufferLength,
    _In_ ULONG TimeoutMs
    )
/*++

Routine Description:

    Send the given fully formed output buffer to HID.

Parameters:

    DmfModule - This Module’s DMF Object.
    Buffer - Source buffer for the data write.
    BufferSize - Size of Buffer in bytes.
    TimeoutMs - Timeout value in milliseconds.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdateHidTransport* moduleContext;
    DMF_CONFIG_ComponentFirmwareUpdateHidTransport* moduleConfig;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    PAGED_CODE();

    if (moduleConfig->Protocol == PROTOCOL_USB)
    {
        ntStatus = DMF_HidTarget_BufferWrite(moduleContext->DmfModuleHid,
                                             Buffer,
                                             BufferLength,
                                             TimeoutMs);
    }
    else
    {
        ntStatus = DMF_HidTarget_OutputReportSet(moduleContext->DmfModuleHid,
                                                 (UCHAR*)Buffer,
                                                 BufferLength,
                                                 TimeoutMs);
    }

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DMF_HidTarget_BufferWrite fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

Exit:

    return ntStatus;
}
//-- Helper functions ---
//--------END------------
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
_IRQL_requires_same_
#pragma code_seg("PAGED")
NTSTATUS
ComponentFirmwareUpdateHidTransport_Intf_OfferInformationSend(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends offer information command to the device.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.
    Buffer - Header, followed by Offer Information to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the beginning of 'Buffer'.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(DmfComponentFirmwareUpdateModule);
    UNREFERENCED_PARAMETER(HeaderSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_ObjectValidate(DmfComponentFirmwareUpdateTransportModule);

    UCHAR reportId = REPORT_ID_OFFER_CONTENT_OUTPUT;
    ASSERT(HeaderSize == sizeof(reportId));
    Buffer[0] = reportId;

    ntStatus = ComponentFirmwareUpdateHidTransport_ReportWrite(DmfComponentFirmwareUpdateTransportModule,
                                                               Buffer,
                                                               (USHORT)BufferSize,
                                                               HIDDEVICE_RECOMMENDED_WAIT_TIMEOUT_MS);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "ReportWrite fails for Report 0x%x: ntStatus=%!STATUS!",
                    reportId,
                    ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
_IRQL_requires_same_
#pragma code_seg("PAGED")
NTSTATUS
ComponentFirmwareUpdateHidTransport_Intf_OfferCommandSend(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends offer command to the device.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.
    Buffer - Header followed by Offer Command to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the beginning of 'Buffer'.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(DmfComponentFirmwareUpdateModule);
    UNREFERENCED_PARAMETER(HeaderSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_ObjectValidate(DmfComponentFirmwareUpdateTransportModule);

    UCHAR reportId = REPORT_ID_OFFER_CONTENT_OUTPUT;
    ASSERT(HeaderSize == sizeof(reportId));
    Buffer[0] = reportId;

    ntStatus = ComponentFirmwareUpdateHidTransport_ReportWrite(DmfComponentFirmwareUpdateTransportModule,
                                                               Buffer,
                                                               (USHORT)BufferSize,
                                                               HIDDEVICE_RECOMMENDED_WAIT_TIMEOUT_MS);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "ReportWrite fails for Report 0x%x: ntStatus=%!STATUS!",
                    reportId,
                    ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
_IRQL_requires_same_
#pragma code_seg("PAGED")
NTSTATUS
ComponentFirmwareUpdateHidTransport_Intf_OfferSend(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends offer to the device.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.
    Buffer - Header, followed by Offer Content to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the beginning of 'Buffer'.


Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    DMF_ObjectValidate(DmfComponentFirmwareUpdateTransportModule);

    UNREFERENCED_PARAMETER(DmfComponentFirmwareUpdateModule);
    UNREFERENCED_PARAMETER(HeaderSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UCHAR reportId = REPORT_ID_OFFER_CONTENT_OUTPUT;
    ASSERT(HeaderSize == sizeof(reportId));
    Buffer[0] = reportId;

    ntStatus = ComponentFirmwareUpdateHidTransport_ReportWrite(DmfComponentFirmwareUpdateTransportModule,
                                                               Buffer,
                                                               (USHORT)BufferSize,
                                                               HIDDEVICE_RECOMMENDED_WAIT_TIMEOUT_MS);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "ReportWrite fails for Report 0x%x: ntStatus=%!STATUS!",
                    reportId,
                    ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
_IRQL_requires_same_
#pragma code_seg("PAGED")
NTSTATUS
ComponentFirmwareUpdateHidTransport_Intf_FirmwareVersionGet(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule
    )
/*++

Routine Description:

    Retrieves the firmware versions from the device.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    DMF_CONTEXT_ComponentFirmwareUpdateHidTransport* moduleContext;

    WDFMEMORY featureReportMemory;
    UCHAR* featureReportBuffer;
    size_t featureBufferLength;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_ObjectValidate(DmfComponentFirmwareUpdateTransportModule);

    moduleContext = DMF_CONTEXT_GET(DmfComponentFirmwareUpdateTransportModule);

    featureReportBuffer = NULL;
    featureBufferLength = 0;
    featureReportMemory = WDF_NO_HANDLE;
    ntStatus = STATUS_SUCCESS;

    UCHAR reportId = REPORT_ID_FW_VERSION_FEATURE;
    ntStatus = DMF_HidTarget_ReportCreate(moduleContext->DmfModuleHid,
                                          HidP_Feature,
                                          reportId,
                                          &featureReportMemory);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "DMF_HidTarget_ReportCreate fails for Report 0x%x: ntStatus=%!STATUS!", 
                    reportId,
                    ntStatus);
        goto Exit;
    }
    featureReportBuffer = (UCHAR*)WdfMemoryGetBuffer(featureReportMemory, 
                                                     &featureBufferLength);

    ntStatus = DMF_HidTarget_FeatureGet(moduleContext->DmfModuleHid,
                                        REPORT_ID_FW_VERSION_FEATURE,
                                        featureReportBuffer,
                                        (ULONG)featureBufferLength,
                                        0,
                                        (ULONG)featureBufferLength);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "DMF_HidTarget_FeatureGet fails for Feature Report ID 0x%x: ntStatus=%!STATUS!", 
                    REPORT_ID_FW_VERSION_FEATURE,
                    ntStatus);
        goto Exit;
    }

Exit:

    UCHAR* responseBuffer = NULL;
    size_t responseBufferSize = 0;

    if (featureReportBuffer && NT_SUCCESS(ntStatus))
    {
        // Skip Report ID.
        //
        responseBuffer = featureReportBuffer + HidHeaderSize;
        responseBufferSize = featureBufferLength - HidHeaderSize;
    }

    // Return the status through the callback.
    //
    INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdate* configComponentFirmwareUpdate;
    configComponentFirmwareUpdate = ComponentFirmwareUpdateProtocolConfigGet(DmfComponentFirmwareUpdateModule);
    ASSERT(configComponentFirmwareUpdate != NULL);
    configComponentFirmwareUpdate->Callbacks.Evt_ComponentFirmwareUpdate_FirmwareVersionResponse(DmfComponentFirmwareUpdateModule,
                                                                                                 DmfComponentFirmwareUpdateTransportModule,
                                                                                                 responseBuffer,
                                                                                                 responseBufferSize,
                                                                                                 ntStatus);
    if (featureReportMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(featureReportMemory);
        featureReportMemory = WDF_NO_HANDLE;
        featureReportBuffer = NULL;
    }

    // We returned the status of operation through the callback.
    //
    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
_IRQL_requires_same_
#pragma code_seg("PAGED")
NTSTATUS
ComponentFirmwareUpdateHidTransport_Intf_PayloadSend(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends Payload to the device.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.
    Buffer - Header, followed by Payload to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the beginning of 'Buffer'.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    DMF_ObjectValidate(DmfComponentFirmwareUpdateTransportModule);

    UNREFERENCED_PARAMETER(DmfComponentFirmwareUpdateModule);
    UNREFERENCED_PARAMETER(HeaderSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UCHAR reportId = REPORT_ID_PAYLOAD_CONTENT_OUTPUT;
    ASSERT(HeaderSize == sizeof(reportId));
    Buffer[0] = reportId;

    ntStatus = ComponentFirmwareUpdateHidTransport_ReportWrite(DmfComponentFirmwareUpdateTransportModule,
                                                               Buffer,
                                                               (USHORT)BufferSize,
                                                               HIDDEVICE_RECOMMENDED_WAIT_TIMEOUT_MS);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "ReportWrite fails for Report 0x%x: ntStatus=%!STATUS!",
                    reportId,
                    ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGED")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
_IRQL_requires_same_
NTSTATUS
ComponentFirmwareUpdateHidTransport_Intf_ProtocolStop(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule
    )
/*++

Routine Description:

    Clean up the transport as the protocol is being stopped.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.

Return:

    NTSTATUS

--*/
{
    UNREFERENCED_PARAMETER(DmfComponentFirmwareUpdateTransportModule);
    UNREFERENCED_PARAMETER(DmfComponentFirmwareUpdateModule);

    PAGED_CODE();

    // currently NO OP.
    //
    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGED")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
_IRQL_requires_same_
NTSTATUS
ComponentFirmwareUpdateHidTransport_Intf_ProtocolStart(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule
    )
/*++

Routine Description:

    Setup the transport for protocol transaction.

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.

Return:

    NTSTATUS

--*/
{
    UNREFERENCED_PARAMETER(DmfComponentFirmwareUpdateTransportModule);
    UNREFERENCED_PARAMETER(DmfComponentFirmwareUpdateModule);

    PAGED_CODE();

    // Currently NO OP.
    //
    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGED")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
_IRQL_requires_same_
NTSTATUS
ComponentFirmwareUpdateHidTransport_Intf_Bind(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule,
    _In_ INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdate* ComponentFirmwareUpdateConfig,
    _Out_ INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdateTransport* ComponentFirmwareUpdateTransportConfig
    )
/*++

Routine Description:

    Registers protocol Module with the transport Module. 

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.
    ComponentFirmwareUpdateConfig - Information about the callback to protocol Module.
    ComponentFirmwareUpdateTransportConfig - Transport specific configuration.

Return:

    NTSTATUS

--*/
{
    NTSTATUS  ntStatus;
    WDF_OBJECT_ATTRIBUTES attributes;
    INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdate* configComponentFirmwareUpdate;
    DMF_CONTEXT_ComponentFirmwareUpdateHidTransport* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_ObjectValidate(DmfComponentFirmwareUpdateTransportModule);

    ntStatus = STATUS_SUCCESS;

    // All the callback functions are mandatory.
    //
    if ((ComponentFirmwareUpdateConfig->Callbacks.Evt_ComponentFirmwareUpdate_FirmwareVersionResponse == NULL) ||
        (ComponentFirmwareUpdateConfig->Callbacks.Evt_ComponentFirmwareUpdate_PayloadResponse == NULL) ||
        (ComponentFirmwareUpdateConfig->Callbacks.Evt_ComponentFirmwareUpdate_OfferResponse == NULL))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Missing Callback function!");
        ASSERT(FALSE);
        ntStatus = STATUS_NOT_IMPLEMENTED;
        goto Exit;
    }

    // Allow Bind/Unbind/Bind. Context may have already been created.
    //
    configComponentFirmwareUpdate = ComponentFirmwareUpdateProtocolConfigGet(DmfComponentFirmwareUpdateModule);
    if(configComponentFirmwareUpdate == NULL)
    {
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                                INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdate);
        ntStatus = WdfObjectAllocateContext(DmfComponentFirmwareUpdateModule,
                                            &attributes,
                                            (VOID**) &configComponentFirmwareUpdate);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "WdfObjectAllocateContext fails: ntStatus=%!STATUS!", 
                        ntStatus);
            goto Exit;
        }
    }

    // Preserve the callbacks in the context.
    //
    configComponentFirmwareUpdate->Callbacks.Evt_ComponentFirmwareUpdate_FirmwareVersionResponse = 
        ComponentFirmwareUpdateConfig->Callbacks.Evt_ComponentFirmwareUpdate_FirmwareVersionResponse;

    configComponentFirmwareUpdate->Callbacks.Evt_ComponentFirmwareUpdate_PayloadResponse =
        ComponentFirmwareUpdateConfig->Callbacks.Evt_ComponentFirmwareUpdate_PayloadResponse;

    configComponentFirmwareUpdate->Callbacks.Evt_ComponentFirmwareUpdate_OfferResponse =
        ComponentFirmwareUpdateConfig->Callbacks.Evt_ComponentFirmwareUpdate_OfferResponse;

    moduleContext = DMF_CONTEXT_GET(DmfComponentFirmwareUpdateTransportModule);

    // Update this transport configurations.
    //
    // 1 byte for ReportID.
    //
    ComponentFirmwareUpdateTransportConfig->TransportHeaderSize = HidHeaderSize;
    // Set the maximum sizes for this transport from HID Capability. 
    // Both Payload and Offer are sent through output report.
    // Firmware Version is retrieved through Feature report.
    // Also don't include ReportID size when reporting the buffer sizes.
    //
    ComponentFirmwareUpdateTransportConfig->TransportFirmwarePayloadBufferRequiredSize = SizeOfPayload;
    ComponentFirmwareUpdateTransportConfig->TransportFirmwareVersionBufferRequiredSize = SizeOfFirmwareVersion;
    ComponentFirmwareUpdateTransportConfig->TransportOfferBufferRequiredSize = SizeOfOffer;
    ComponentFirmwareUpdateTransportConfig->WaitTimeout = HIDDEVICE_RECOMMENDED_WAIT_TIMEOUT_MS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
_IRQL_requires_same_
NTSTATUS
ComponentFirmwareUpdateHidTransport_Intf_Unbind(
    _In_ DMFMODULE DmfComponentFirmwareUpdateTransportModule,
    _In_ DMFMODULE DmfComponentFirmwareUpdateModule
    )
/*++

Routine Description:

    Deregisters protocol Module from the transport Module. 

Parameters:

    DmfComponentFirmwareUpdateTransportModule - Transport Module’s DMF Object.
    DmfComponentFirmwareUpdateModule - Protocol Module's DMF Object.

Return:

    NTSTATUS

--*/
{
    INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdate* configComponentFirmwareUpdate;

    UNREFERENCED_PARAMETER(DmfComponentFirmwareUpdateTransportModule);

    configComponentFirmwareUpdate = ComponentFirmwareUpdateProtocolConfigGet(DmfComponentFirmwareUpdateModule);
    ASSERT(configComponentFirmwareUpdate != NULL);

    // Clear the Preserved callbacks from the context.
    //
    configComponentFirmwareUpdate->Callbacks.Evt_ComponentFirmwareUpdate_FirmwareVersionResponse = NULL;

    configComponentFirmwareUpdate->Callbacks.Evt_ComponentFirmwareUpdate_PayloadResponse = NULL;

    configComponentFirmwareUpdate->Callbacks.Evt_ComponentFirmwareUpdate_OfferResponse = NULL;

    return STATUS_SUCCESS;
}
// Interface Implementation 
//--------END ------------------
//

VOID
ComponentFirmwareUpdateHidTransport_HidInputReportCompletionCallback(
    _In_ DMFMODULE DmfModuleHid,
    _In_reads_(BufferLength) UCHAR* Buffer,
    _In_ ULONG BufferLength
    )
/*++

Routine Description:

    Called when there is a HID input report received. This will do basic validation and report the contents 
    through the callback for further processing.

Parameters:

    DmfModuleHid - This Module’s DMF Object.
    Buffer - Buffer that has completed read.
    BufferLength - Length of the above buffer in bytes.

Return:

    None.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdateHidTransport* moduleContext;
    DMFMODULE dmfModuleComponentFirmwareUpdateTransport;
    DMFMODULE dmfModuleComponentFirmwareUpdate;

    FuncEntry(DMF_TRACE);

    dmfModuleComponentFirmwareUpdateTransport = DMF_ParentModuleGet(DmfModuleHid);
    moduleContext = DMF_CONTEXT_GET(dmfModuleComponentFirmwareUpdateTransport);

    dmfModuleComponentFirmwareUpdate = DMF_ParentModuleGet(dmfModuleComponentFirmwareUpdateTransport);

    ASSERT(Buffer != NULL);

    ntStatus = STATUS_SUCCESS;
    USHORT reportId = Buffer[0];

    // Ignore all invalid reports.
    //
    if ((reportId != REPORT_ID_OFFER_RESPONSE_INPUT) &&
        (reportId != REPORT_ID_PAYLOAD_RESPONSE_INPUT))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "Ignoring HID report with invalid report id: 0x%x", reportId);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, 
                DMF_TRACE, 
                "HidInputReportCompletionCallback length: %d", 
                BufferLength);

    switch (reportId)
    {
        case REPORT_ID_OFFER_RESPONSE_INPUT:
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, 
                        DMF_TRACE, 
                        "HidInputReportCompletionCallback: Got an OFFER RESPONSE packet ");

            INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdate* configComponentFirmwareUpdate;
            configComponentFirmwareUpdate = ComponentFirmwareUpdateProtocolConfigGet(dmfModuleComponentFirmwareUpdate);
            ASSERT(configComponentFirmwareUpdate != NULL);
            configComponentFirmwareUpdate->Callbacks.Evt_ComponentFirmwareUpdate_OfferResponse(dmfModuleComponentFirmwareUpdate,
                                                                                               dmfModuleComponentFirmwareUpdateTransport,
                                                                                               Buffer + HidHeaderSize,
                                                                                               BufferLength - HidHeaderSize,
                                                                                               ntStatus);
        }
        break;
        case REPORT_ID_PAYLOAD_RESPONSE_INPUT:
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, 
                        DMF_TRACE,
                        "HidInputReportCompletionCallback: Got an PAYLOAD RESPONSE packet ");

            INTF_TRANSPORT_CONFIG_ComponentFirmwareUpdate* configComponentFirmwareUpdate;
            configComponentFirmwareUpdate = ComponentFirmwareUpdateProtocolConfigGet(dmfModuleComponentFirmwareUpdate);
            ASSERT(configComponentFirmwareUpdate != NULL);
            configComponentFirmwareUpdate->Callbacks.Evt_ComponentFirmwareUpdate_PayloadResponse(dmfModuleComponentFirmwareUpdate,
                                                                                                 dmfModuleComponentFirmwareUpdateTransport,
                                                                                                 Buffer + HidHeaderSize,
                                                                                                 BufferLength - HidHeaderSize,
                                                                                                 ntStatus);
        }
        break;
    };

Exit:

    // Pend an input report read, even if we failed to process the read just completed.
    //
    ntStatus = DMF_HidTarget_InputRead(moduleContext->DmfModuleHid);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "DMF_HidTarget_InputRead fails: ntStatus=%!STATUS!", 
                    ntStatus);
    }

    FuncExitNoReturn(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGED")
_Must_inspect_result_
static
NTSTATUS
ComponentFirmwareUpdateHidTransport_HidReadsPend(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Pend input reads.

Parameters:

    DmfModule - This Module’s DMF Object.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdateHidTransport* moduleContext;
    DMF_CONFIG_ComponentFirmwareUpdateHidTransport* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    // Get buffers from the producer and issue the required number of input reads.
    //
    for (UINT index = 0; index < moduleConfig->NumberOfInputReportReadsPended; ++index)
    {
        // Pend an input report read.
        //
        ntStatus = DMF_HidTarget_InputRead(moduleContext->DmfModuleHid);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, 
                        DMF_TRACE, 
                        "DMF_HidTarget_InputRead fails: ntStatus=%!STATUS!", 
                        ntStatus);
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
};
#pragma code_seg()

#pragma code_seg("PAGED")
static
VOID 
ComponentFirmwareUpdateHidTransport_HidPostOpen_Callback(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This is callback to indicate that HID is opened (Post Open)

Arguments:

    DmfModule - The Module from which the callback is called.

Return Value:

    None.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdateHidTransport* moduleContext;
    DMFMODULE dmfModuleComponentFirmwareUpdateTransport;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    dmfModuleComponentFirmwareUpdateTransport = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleComponentFirmwareUpdateTransport);

    TraceEvents(TRACE_LEVEL_INFORMATION, 
                DMF_TRACE, 
                "Hid_Opened");

    // Pend Input Reads.
    //
    ntStatus = ComponentFirmwareUpdateHidTransport_HidReadsPend(dmfModuleComponentFirmwareUpdateTransport);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "HidReadsPend fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    // The target has been opened. Perform any other operation that must be done.
    // NOTE: That this causes any children to open.
    //
    ntStatus = DMF_ModuleOpen(dmfModuleComponentFirmwareUpdateTransport);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "DMF_Module_Open fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

Exit:

    FuncExitVoid(DMF_TRACE);
}

static
VOID 
ComponentFirmwareUpdateHidTransport_HidPreClose_Callback(
    _In_ DMFMODULE DmfModule
)
/*++

Routine Description:

    This is callback to indicate that HID is about to be closed (Pre Close)

Arguments:

    DmfModule - The Module from which the callback is called.

Return Value:

    None.

--*/
{

    DMF_CONTEXT_ComponentFirmwareUpdateHidTransport* moduleContext;
    DMFMODULE dmfModuleComponentFirmwareUpdateTransport;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    dmfModuleComponentFirmwareUpdateTransport = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleComponentFirmwareUpdateTransport);

    TraceEvents(TRACE_LEVEL_INFORMATION, 
                DMF_TRACE, 
                "Hid_Closed");

    // Close the Module.
    //
    DMF_ModuleClose(dmfModuleComponentFirmwareUpdateTransport);

    FuncExitVoid(DMF_TRACE);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Entry Points
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Entry Points
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ComponentFirmwareUpdateHidTransport_ChildModulesAdd(
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
    DMF_CONTEXT_ComponentFirmwareUpdateHidTransport* moduleContext;
    DMF_CONFIG_ComponentFirmwareUpdateHidTransport* moduleConfig;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_HidTarget hidTargetConfig;
    DMF_MODULE_EVENT_CALLBACKS hidCallbacks;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);
    UNREFERENCED_PARAMETER(DmfModuleInit);

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // HidTarget
    // ---------
    //
    DMF_CONFIG_HidTarget_AND_ATTRIBUTES_INIT(&hidTargetConfig,
                                             &moduleAttributes);
    // Configure the HID target as In-stack.
    //
    hidTargetConfig.HidTargetToConnect = DMF_ParentDeviceGet(DmfModule);
    hidTargetConfig.SkipHidDeviceEnumerationSearch = TRUE;

    hidTargetConfig.OpenMode = GENERIC_WRITE | GENERIC_READ;
    hidTargetConfig.ShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE;
    hidTargetConfig.EvtHidInputReport = ComponentFirmwareUpdateHidTransport_HidInputReportCompletionCallback;

    DMF_MODULE_ATTRIBUTES_EVENT_CALLBACKS_INIT(&moduleAttributes,
                                               &hidCallbacks);
    hidCallbacks.EvtModuleOnDeviceNotificationPostOpen = ComponentFirmwareUpdateHidTransport_HidPostOpen_Callback;
    hidCallbacks.EvtModuleOnDeviceNotificationPreClose = ComponentFirmwareUpdateHidTransport_HidPreClose_Callback;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleHid);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGED")
_Must_inspect_result_
static
NTSTATUS
DMF_ComponentFirmwareUpdateHidTransport_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module HID Tranport.

Arguments:

    DmfModule - This Module's DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    // Bind the underlying Implementation to the interface.
    //
    INTF_DMF_ComponentFirmwareUpdateTransport Intf_ComponentFirmwareUpdateTransport;
    Intf_ComponentFirmwareUpdateTransport.Intf_ComponentFirmwareUpdateTransport_FirmwareVersionGet = ComponentFirmwareUpdateHidTransport_Intf_FirmwareVersionGet;
    Intf_ComponentFirmwareUpdateTransport.Intf_ComponentFirmwareUpdateTransport_OfferCommandSend = ComponentFirmwareUpdateHidTransport_Intf_OfferCommandSend;
    Intf_ComponentFirmwareUpdateTransport.Intf_ComponentFirmwareUpdateTransport_OfferInformationSend = ComponentFirmwareUpdateHidTransport_Intf_OfferInformationSend;
    Intf_ComponentFirmwareUpdateTransport.Intf_ComponentFirmwareUpdateTransport_OfferSend = ComponentFirmwareUpdateHidTransport_Intf_OfferSend;
    Intf_ComponentFirmwareUpdateTransport.Intf_ComponentFirmwareUpdateTransport_PayloadSend = ComponentFirmwareUpdateHidTransport_Intf_PayloadSend;
    Intf_ComponentFirmwareUpdateTransport.Intf_ComponentFirmwareUpdateTransport_Bind = ComponentFirmwareUpdateHidTransport_Intf_Bind;
    Intf_ComponentFirmwareUpdateTransport.Intf_ComponentFirmwareUpdateTransport_Unbind = ComponentFirmwareUpdateHidTransport_Intf_Unbind;
    Intf_ComponentFirmwareUpdateTransport.Intf_ComponentFirmwareUpdateTransport_ProtocolStart = ComponentFirmwareUpdateHidTransport_Intf_ProtocolStart;
    Intf_ComponentFirmwareUpdateTransport.Intf_ComponentFirmwareUpdateTransport_ProtocolStop = ComponentFirmwareUpdateHidTransport_Intf_ProtocolStop;

    // Transport.
    //
    ntStatus = DMF_ComponentFirmwareUpdateTransport_BindInterface(DmfModule,
                                                                  &Intf_ComponentFirmwareUpdateTransport);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "DMF_ComponentFirmwareUpdateTransport_BindInterface fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGED")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdateHidTransport_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Component Firmware Update HID Transport.

Arguments:

    Device - WdfDevice associated with this instance.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_ComponentFirmwareUpdateHidTransport;
    DMF_CALLBACKS_DMF DmfEntrypointsDmf_ComponentFirmwareUpdateHidTransport;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&DmfEntrypointsDmf_ComponentFirmwareUpdateHidTransport);
    DmfEntrypointsDmf_ComponentFirmwareUpdateHidTransport.ChildModulesAdd = DMF_ComponentFirmwareUpdateHidTransport_ChildModulesAdd;
    DmfEntrypointsDmf_ComponentFirmwareUpdateHidTransport.DeviceOpen = DMF_ComponentFirmwareUpdateHidTransport_Open;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_ComponentFirmwareUpdateHidTransport,
                                            ComponentFirmwareUpdateHidTransport,
                                            DMF_CONTEXT_ComponentFirmwareUpdateHidTransport,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_NOTIFY_Create);

    dmfModuleDescriptor_ComponentFirmwareUpdateHidTransport.CallbacksDmf = &DmfEntrypointsDmf_ComponentFirmwareUpdateHidTransport;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_ComponentFirmwareUpdateHidTransport,
                                DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "DMF_ModuleCreate fails: ntStatus=%!STATUS!", 
                    ntStatus);
    }
    
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// eof: Dmf_ComponentFirmwareUpdateHidTransport.c
//
