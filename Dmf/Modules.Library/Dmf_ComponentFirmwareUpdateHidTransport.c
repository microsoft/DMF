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

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_ComponentFirmwareUpdateHidTransport.tmh"
#endif

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
typedef struct _DMF_CONTEXT_ComponentFirmwareUpdateHidTransport
{
    // HID Handle.
    //
    DMFMODULE DmfModuleHid;
    // Interface Handle.
    //
    DMFINTERFACE DmfInterfaceComponentFirmwareUpdate;
    // Timeout to be used for transport operations.
    //
    ULONG HidDeviceWaitTimeoutMs;
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

//-- Helper functions ---
//--------START----------
//
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_ 
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

    FuncEntry(DMF_TRACE);

    dmfModuleComponentFirmwareUpdateTransport = DMF_ParentModuleGet(DmfModuleHid);
    moduleContext = DMF_CONTEXT_GET(dmfModuleComponentFirmwareUpdateTransport);

    DmfAssert(Buffer != NULL);

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

            EVT_ComponentFirmwareUpdate_OfferResponse(moduleContext->DmfInterfaceComponentFirmwareUpdate,
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

            EVT_ComponentFirmwareUpdate_PayloadResponse(moduleContext->DmfInterfaceComponentFirmwareUpdate,
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

#pragma code_seg("PAGE")
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

#pragma code_seg("PAGE")
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

// Transport Generic Callbacks.
// (Implementation of publicly accessible callbacks required by the Interface.)
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_ComponentFirmwareUpdateHidTransport_PostBind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    This callback tells the given Transport Module that it is bound to the given
    Protocol Module.

Arguments:

    DmfInterface - Interface handle.

Return Value:

    NTSTATUS

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfInterface);

    FuncEntry(DMF_TRACE);

    // Currently NOP.
    //
    
    // It is now possible to use Methods provided by the Protocol.
    //

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_ComponentFirmwareUpdateHidTransport_PreUnbind(
    _In_ DMFINTERFACE DmfInterface
)
/*++

Routine Description:

    This callback tells the given Transport Module that it is about to be unbound from
    the given Protocol Module.

Arguments:

    DmfInterface - Interface handle.

Return Value:

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfInterface);

    FuncEntry(DMF_TRACE);

    // Currently NOP.
    //

    // Stop using Methods provided by Protocol after this callback completes (except for Unbind).
    //

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdateHidTransport_OfferInformationSend(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends offer information command to the device.

Parameters:

    DmfInterface - Interface handle.
    Buffer - Header, followed by Offer Information to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the beginning of 'Buffer'.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE DmfComponentFirmwareUpdateTransportModule;
    DMF_CONTEXT_ComponentFirmwareUpdateHidTransport* moduleContext;

    UNREFERENCED_PARAMETER(HeaderSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfComponentFirmwareUpdateTransportModule = DMF_InterfaceTransportModuleGet(DmfInterface);
    DMF_ObjectValidate(DmfComponentFirmwareUpdateTransportModule);
    moduleContext = DMF_CONTEXT_GET(DmfComponentFirmwareUpdateTransportModule);

    UCHAR reportId = REPORT_ID_OFFER_CONTENT_OUTPUT;
    DmfAssert(HeaderSize == sizeof(reportId));
    Buffer[0] = reportId;

    ntStatus = ComponentFirmwareUpdateHidTransport_ReportWrite(DmfComponentFirmwareUpdateTransportModule,
                                                               Buffer,
                                                               (USHORT)BufferSize,
                                                               moduleContext->HidDeviceWaitTimeoutMs);
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

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdateHidTransport_OfferCommandSend(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends offer command to the device.

Parameters:

    DmfInterface - Interface handle.
    Buffer - Header followed by Offer Command to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the begining of 'Buffer'.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE DmfComponentFirmwareUpdateTransportModule;
    DMF_CONTEXT_ComponentFirmwareUpdateHidTransport* moduleContext;

    UNREFERENCED_PARAMETER(HeaderSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfComponentFirmwareUpdateTransportModule = DMF_InterfaceTransportModuleGet(DmfInterface);
    DMF_ObjectValidate(DmfComponentFirmwareUpdateTransportModule);
    moduleContext = DMF_CONTEXT_GET(DmfComponentFirmwareUpdateTransportModule);

    UCHAR reportId = REPORT_ID_OFFER_CONTENT_OUTPUT;
    DmfAssert(HeaderSize == sizeof(reportId));
    Buffer[0] = reportId;

    ntStatus = ComponentFirmwareUpdateHidTransport_ReportWrite(DmfComponentFirmwareUpdateTransportModule,
                                                               Buffer,
                                                               (USHORT)BufferSize,
                                                               moduleContext->HidDeviceWaitTimeoutMs);
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

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdateHidTransport_OfferSend(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends offer to the device.

Parameters:

    DmfInterface - Interface handle.
    Buffer - Header, followed by Offer Content to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the beginning of 'Buffer'.


Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE DmfComponentFirmwareUpdateTransportModule;
    DMF_CONTEXT_ComponentFirmwareUpdateHidTransport* moduleContext;

    UNREFERENCED_PARAMETER(HeaderSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfComponentFirmwareUpdateTransportModule = DMF_InterfaceTransportModuleGet(DmfInterface);
    DMF_ObjectValidate(DmfComponentFirmwareUpdateTransportModule);
    moduleContext = DMF_CONTEXT_GET(DmfComponentFirmwareUpdateTransportModule);

    UCHAR reportId = REPORT_ID_OFFER_CONTENT_OUTPUT;
    DmfAssert(HeaderSize == sizeof(reportId));
    Buffer[0] = reportId;

    ntStatus = ComponentFirmwareUpdateHidTransport_ReportWrite(DmfComponentFirmwareUpdateTransportModule,
                                                               Buffer,
                                                               (USHORT)BufferSize,
                                                               moduleContext->HidDeviceWaitTimeoutMs);
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

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdateHidTransport_FirmwareVersionGet(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Retrieves the firmware versions from the device.

Parameters:

    DmfInterface - Interface handle.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE DmfComponentFirmwareUpdateTransportModule;
    DMF_CONTEXT_ComponentFirmwareUpdateHidTransport* moduleContext;

    WDFMEMORY featureReportMemory;
    UCHAR* featureReportBuffer;
    size_t featureBufferLength;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfComponentFirmwareUpdateTransportModule = DMF_InterfaceTransportModuleGet(DmfInterface);
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
    EVT_ComponentFirmwareUpdate_FirmwareVersionResponse(DmfInterface,
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

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdateHidTransport_PayloadSend(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
/*++

Routine Description:

    Sends Payload to the device.

Parameters:

    DmfInterface - Interface handle.
    Buffer - Header, followed by Payload to Send.
    BufferSize - Size of the above in bytes.
    HeaderSize - Size of the header. Header is at the beginning of 'Buffer'.

Return:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE DmfComponentFirmwareUpdateTransportModule;
    DMF_CONTEXT_ComponentFirmwareUpdateHidTransport* moduleContext;

    UNREFERENCED_PARAMETER(HeaderSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);
    
    DmfComponentFirmwareUpdateTransportModule = DMF_InterfaceTransportModuleGet(DmfInterface);
    DMF_ObjectValidate(DmfComponentFirmwareUpdateTransportModule);
    moduleContext = DMF_CONTEXT_GET(DmfComponentFirmwareUpdateTransportModule);

    UCHAR reportId = REPORT_ID_PAYLOAD_CONTENT_OUTPUT;
    DmfAssert(HeaderSize == sizeof(reportId));
    Buffer[0] = reportId;

    ntStatus = ComponentFirmwareUpdateHidTransport_ReportWrite(DmfComponentFirmwareUpdateTransportModule,
                                                               Buffer,
                                                               (USHORT)BufferSize,
                                                               moduleContext->HidDeviceWaitTimeoutMs);
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

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdateHidTransport_ProtocolStop(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Clean up the transport as the protocol is being stopped.

Parameters:

    DmfInterface - Interface handle.

Return:

    NTSTATUS

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);

    PAGED_CODE();

    // currently NOP.
    //
    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdateHidTransport_ProtocolStart(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Setup the transport for protocol transaction.

Parameters:

    DmfInterface - Interface handle.

Return:

    NTSTATUS

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);

    PAGED_CODE();

    // Currently NOP.
    //
    return STATUS_SUCCESS;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdateHidTransport_Bind(
    _In_ DMFINTERFACE DmfInterface,
    _In_ DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_BIND_DATA* ProtocolBindData,
    _Out_ DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_BIND_DATA* TransportBindData
    )
/*++

Routine Description:

    Binds the given Transport Module to the given Protocol Module.

Parameters:

    DmfInterface - Interface handle.
    ProtocolBindData - Bind data provided by Protocol for the Transport.
    TransportBindData - Bind data provided by Transport for the Protocol.

Return:

    NTSTATUS

--*/
{
    NTSTATUS  ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdateHidTransport* moduleContext;
    DMF_CONFIG_ComponentFirmwareUpdateHidTransport* moduleConfig;
    DMFMODULE DmfComponentFirmwareUpdateTransportModule;

    UNREFERENCED_PARAMETER(ProtocolBindData);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    DmfComponentFirmwareUpdateTransportModule = DMF_InterfaceTransportModuleGet(DmfInterface);
    moduleContext = DMF_CONTEXT_GET(DmfComponentFirmwareUpdateTransportModule);
    moduleConfig = DMF_CONFIG_GET(DmfComponentFirmwareUpdateTransportModule);

    // Save the Interface Handle representing the Interface binding.
    //
    moduleContext->DmfInterfaceComponentFirmwareUpdate = DmfInterface;

    // Update this transport configurations.
    //
    // 1 byte for ReportID.
    //
    TransportBindData->TransportHeaderSize = HidHeaderSize;
    // Set the maximum sizes for this transport from HID Capability. 
    // Both Payload and Offer are sent through output report.
    // Firmware Version is retrieved through Feature report.
    // Also don't include ReportID size when reporting the buffer sizes.
    //
    TransportBindData->TransportFirmwarePayloadBufferRequiredSize = SizeOfPayload;
    TransportBindData->TransportFirmwareVersionBufferRequiredSize = SizeOfFirmwareVersion;
    TransportBindData->TransportOfferBufferRequiredSize = SizeOfOffer;
    TransportBindData->TransportWaitTimeout = moduleContext->HidDeviceWaitTimeoutMs;
    TransportBindData->TransportPayloadFillAlignment = moduleConfig->PayloadFillAlignment;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_ComponentFirmwareUpdateHidTransport_Unbind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Deregisters protocol Module from the transport Module. 

Parameters:

    DmfInterface - Interface handle.

Return:

    NTSTATUS

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);
}

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
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

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_Must_inspect_result_
static
NTSTATUS
DMF_ComponentFirmwareUpdateHidTransport_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module HID Transport.

Arguments:

    DmfModule - This Module's DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ComponentFirmwareUpdateHidTransport* moduleContext;
    DMF_CONFIG_ComponentFirmwareUpdateHidTransport* moduleConfig;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Set the timeout. Use the default if none was specified.
    //
    if (moduleConfig->HidDeviceWaitTimeoutMs == 0)
    {
        moduleContext->HidDeviceWaitTimeoutMs = HIDDEVICE_RECOMMENDED_WAIT_TIMEOUT_MS;
    }
    else
    {
        moduleContext->HidDeviceWaitTimeoutMs = moduleConfig->HidDeviceWaitTimeoutMs;
    }
    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
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
    DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA transportDeclarationData;

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

    // Initialize the Transport Declaration Data.
    //
    DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DESCRIPTOR_INIT(&transportDeclarationData,
                                                                    DMF_ComponentFirmwareUpdateHidTransport_PostBind,
                                                                    DMF_ComponentFirmwareUpdateHidTransport_PreUnbind,
                                                                    DMF_ComponentFirmwareUpdateHidTransport_Bind,
                                                                    DMF_ComponentFirmwareUpdateHidTransport_Unbind,
                                                                    DMF_ComponentFirmwareUpdateHidTransport_FirmwareVersionGet,
                                                                    DMF_ComponentFirmwareUpdateHidTransport_OfferInformationSend,
                                                                    DMF_ComponentFirmwareUpdateHidTransport_OfferCommandSend,
                                                                    DMF_ComponentFirmwareUpdateHidTransport_OfferSend,
                                                                    DMF_ComponentFirmwareUpdateHidTransport_PayloadSend,
                                                                    DMF_ComponentFirmwareUpdateHidTransport_ProtocolStart,
                                                                    DMF_ComponentFirmwareUpdateHidTransport_ProtocolStop);

    // Add the interface to the Transport Module.
    //
    ntStatus = DMF_ModuleInterfaceDescriptorAdd(*DmfModule,
                                                (DMF_INTERFACE_DESCRIPTOR*)&transportDeclarationData);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleInterfaceDescriptorAdd fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// eof: Dmf_ComponentFirmwareUpdateHidTransport.c
//
