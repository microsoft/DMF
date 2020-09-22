/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_EyeGazeIoctl.c

Abstract:

    Exposes an IOCTL interface that allows other components to send Eye Gaze data. This Module
    is a template that can be used to retrieve and send Eye Gaze data from other sources.

Environment:

    Kernel-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_EyeGazeIoctl.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_EyeGazeIoctl
{
    // Underlying VHF support.
    //
    DMFMODULE DmfModuleVirtualEyeGaze;
} DMF_CONTEXT_EyeGazeIoctl;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(EyeGazeIoctl)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(EyeGazeIoctl)

// MemoryTag.
//
#define MemoryTag 'mDHV'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
NTSTATUS
EyeGazeIoctl_IoctlHandler_GazeReport(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoControlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    Process GAZE_DATA Ioctl.

Arguments:

    DmfModule - DMF_IoctlHandler Module handle.
    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.
    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.
    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

    STATUS_PENDING - DMF does not complete the WDFREQUEST and Client Driver owns the WDFREQUEST.
    Other - DMF completes the WDFREQUEST with that NTSTATUS.

--*/
{
    NTSTATUS ntStatus;
    GAZE_DATA* gazeData;
    DMFMODULE dmfModuleEyeGazeIoctl;
    DMF_CONTEXT_EyeGazeIoctl* moduleContext;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(IoControlCode);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);

    PAGED_CODE();

    *BytesReturned = 0;
    dmfModuleEyeGazeIoctl = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleEyeGazeIoctl);

    gazeData = (GAZE_DATA*)InputBuffer;

    ntStatus = DMF_VirtualEyeGaze_GazeReportSend(moduleContext->DmfModuleVirtualEyeGaze,
                                                 gazeData);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Tell application this driver read the input buffer.
    //
    *BytesReturned = InputBufferSize;

Exit:

    // Causes DMF to complete the WDFREQUEST.
    //
    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
NTSTATUS
EyeGazeIoctl_IoctlHandler_ConfigurationData(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoControlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    Processes Configuration Data IOCTL.

Arguments:

    DmfModule - DMF_IoctlHandler Module handle.
    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.
    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.
    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

    STATUS_PENDING - DMF does not complete the WDFREQUEST and Client Driver owns the WDFREQUEST.
    Other - DMF completes the WDFREQUEST with that NTSTATUS.

--*/
{
    NTSTATUS ntStatus;
    CONFIGURATION_DATA* configurationData;
    DMFMODULE dmfModuleEyeGazeIoctl;
    DMF_CONTEXT_EyeGazeIoctl* moduleContext;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(IoControlCode);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);

    PAGED_CODE();

    *BytesReturned = 0;
    dmfModuleEyeGazeIoctl = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleEyeGazeIoctl);

    configurationData = (CONFIGURATION_DATA*)InputBuffer;

    ntStatus = DMF_VirtualEyeGaze_ConfigurationDataSet(moduleContext->DmfModuleVirtualEyeGaze,
                                                       configurationData);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Tell application this driver read the input buffer.
    //
    *BytesReturned = InputBufferSize;

Exit:

    // Causes DMF to complete the WDFREQUEST.
    //
    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
NTSTATUS
EyeGazeIoctl_IoctlHandler_CapabilitiesData(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoControlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
)
/*++

Routine Description:

    Processes Capabilities Data IOCTL.

Arguments:

    DmfModule - DMF_IoctlHandler Module handle.
    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.
    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.
    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

    STATUS_PENDING - DMF does not complete the WDFREQUEST and Client Driver owns the WDFREQUEST.
    Other - DMF completes the WDFREQUEST with that NTSTATUS.

--*/
{
    NTSTATUS ntStatus;
    CAPABILITIES_DATA* capabilitiesData;
    DMFMODULE dmfModuleEyeGazeIoctl;
    DMF_CONTEXT_EyeGazeIoctl* moduleContext;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(IoControlCode);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);

    PAGED_CODE();

    *BytesReturned = 0;
    dmfModuleEyeGazeIoctl = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleEyeGazeIoctl);

    capabilitiesData = (CAPABILITIES_DATA*)InputBuffer;

    ntStatus = DMF_VirtualEyeGaze_CapabilitiesDataSet(moduleContext->DmfModuleVirtualEyeGaze,
                                                      capabilitiesData);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Tell application this driver read the input buffer.
    //
    *BytesReturned = InputBufferSize;

Exit:

    // Causes DMF to complete the WDFREQUEST.
    //
    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
NTSTATUS
EyeGazeIoctl_IoctlHandler_ControlMode(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoControlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    Processes Control Mode IOCTL.

Arguments:

    DmfModule - DMF_IoctlHandler Module handle.
    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.
    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.
    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

    STATUS_PENDING - DMF does not complete the WDFREQUEST and Client Driver owns the WDFREQUEST.
    Other - DMF completes the WDFREQUEST with that NTSTATUS.

--*/
{
    NTSTATUS ntStatus;
    UCHAR controlMode;
    DMFMODULE dmfModuleEyeGazeIoctl;
    DMF_CONTEXT_EyeGazeIoctl* moduleContext;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(IoControlCode);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferSize);

    PAGED_CODE();

    *BytesReturned = 0;
    dmfModuleEyeGazeIoctl = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleEyeGazeIoctl);

    ntStatus = DMF_VirtualEyeGaze_TrackerControlModeGet(moduleContext->DmfModuleVirtualEyeGaze,
                                                        &controlMode);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    *BytesReturned = sizeof(UCHAR);
    if (OutputBufferSize < *BytesReturned)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,"OutputBufferSize too small: OutputBufferSize=%lld Expected=%lld", OutputBufferSize, *BytesReturned);
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    RtlCopyMemory(OutputBuffer,
                  &controlMode,
                  *BytesReturned);

Exit:

    // Causes DMF to complete the WDFREQUEST.
    //
    return ntStatus;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// All IOCTLs are automatically forwarded down the stack except for those in this table.
//
IoctlHandler_IoctlRecord EyeGazeIoctl_IoctlHandlerTable[] =
{
    { (LONG)IOCTL_EYEGAZE_GAZE_DATA,            sizeof(GAZE_DATA),          0, EyeGazeIoctl_IoctlHandler_GazeReport,        FALSE },
    { (LONG)IOCTL_EYEGAZE_CONFIGURATION_REPORT, sizeof(CONFIGURATION_DATA), 0, EyeGazeIoctl_IoctlHandler_ConfigurationData, FALSE },
    { (LONG)IOCTL_EYEGAZE_CAPABILITIES_REPORT,  sizeof(CAPABILITIES_DATA),  0, EyeGazeIoctl_IoctlHandler_CapabilitiesData,  FALSE },
    { (LONG)IOCTL_EYEGAZE_CONTROL_REPORT,       sizeof(UCHAR),              0, EyeGazeIoctl_IoctlHandler_ControlMode,       FALSE },
};

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_EyeGazeIoctl_ChildModulesAdd(
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
    DMF_CONTEXT_EyeGazeIoctl* moduleContext;
    DMF_CONFIG_EyeGazeIoctl* moduleConfig;
    DMF_CONFIG_VirtualEyeGaze moduleConfigVirtualEyeGaze;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // VirtualEyeGaze
    // --------------
    //
    DMF_CONFIG_VirtualEyeGaze_AND_ATTRIBUTES_INIT(&moduleConfigVirtualEyeGaze,
                                                  &moduleAttributes);
    moduleConfigVirtualEyeGaze.ProductId = moduleConfig->ProductId;
    moduleConfigVirtualEyeGaze.VendorId = moduleConfig->VendorId;
    moduleConfigVirtualEyeGaze.VersionNumber = moduleConfig->VersionId;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleVirtualEyeGaze);

    // IoctlHandler
    // ------------
    //
    DMF_CONFIG_IoctlHandler moduleConfigIoctlHandler;
    DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(&moduleConfigIoctlHandler,
                                                &moduleAttributes);
    moduleConfigIoctlHandler.DeviceInterfaceGuid = VirtualEyeGaze_GUID;
    moduleConfigIoctlHandler.IoctlRecords = EyeGazeIoctl_IoctlHandlerTable;
    moduleConfigIoctlHandler.IoctlRecordCount = _countof(EyeGazeIoctl_IoctlHandlerTable);
    moduleConfigIoctlHandler.AccessModeFilter = IoctlHandler_AccessModeFilterAdministratorOnly;
    DMF_DmfModuleAdd(DmfModuleInit, 
                     &moduleAttributes, 
                     WDF_NO_OBJECT_ATTRIBUTES, 
                     NULL);
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
DMF_EyeGazeIoctl_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type EyeGazeIoctl.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_EyeGazeIoctl;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_EyeGazeIoctl;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_EyeGazeIoctl);
    dmfCallbacksDmf_EyeGazeIoctl.ChildModulesAdd = DMF_EyeGazeIoctl_ChildModulesAdd;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_EyeGazeIoctl,
                                            EyeGazeIoctl,
                                            DMF_CONTEXT_EyeGazeIoctl,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_EyeGazeIoctl.CallbacksDmf = &dmfCallbacksDmf_EyeGazeIoctl;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_EyeGazeIoctl,
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

// eof: Dmf_EyeGazeIoctl.c
//
