/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_SerialTarget.c

Abstract:

    Creates a stream of asynchronous requests to a Serial IO Target. Also, there is support
    for sending synchronous requests to the same IO Target.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_SerialTarget.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Underlying Device Target.
    //
    WDFIOTARGET IoTarget;
    // Indicates the mode of ContinuousRequestTarget.
    //
    ContinuousRequestTarget_ModeType ContinuousRequestTargetMode;
    // Connection ID for serial peripheral.
    //
    LARGE_INTEGER PeripheralId;
    // Child ContinuousRequestTarget DMF Module.
    //
    DMFMODULE DmfModuleContinuousRequestTarget;
    // Redirect Input buffer callback from ContinuousRequestTarget to this callback.
    //
    EVT_DMF_ContinuousRequestTarget_BufferInput* EvtContinuousRequestTargetBufferInput;
    // Redirect Output buffer callback from ContinuousRequestTarget to this callback.
    //
    EVT_DMF_ContinuousRequestTarget_BufferOutput* EvtContinuousRequestTargetBufferOutput;
} DMF_CONTEXT_SerialTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(SerialTarget)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(SerialTarget)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define RESHUB_USE_HELPER_ROUTINES
#include "reshub.h"

_Function_class_(EVT_DMF_ContinuousRequestTarget_BufferInput)
VOID
SerialTarget_StreamAsynchronousBufferInput(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(*InputBufferSize) VOID* InputBuffer,
    _Out_ size_t* InputBufferSize,
    _In_ VOID* ClientBuferContextInput
    )
/*++

Routine Description:

    Redirect input buffer callback from Request Stream to Parent Module/Device.

Arguments:

    DmfModule - ContinuousRequestTarget DMFMODULE.
    InputBuffer - The given input buffer.
    InputBufferSize - Size of the given input buffer.
    ClientBuferContextInput - Context associated with the given input buffer.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_SerialTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    dmfModule = DMF_ParentModuleGet(DmfModule);
    DmfAssert(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    DmfAssert(moduleContext->EvtContinuousRequestTargetBufferInput != NULL);
    // 'Using uninitialized memory '*InputBufferSize'.'.
    //
    #pragma warning(suppress:6001)
    moduleContext->EvtContinuousRequestTargetBufferInput(dmfModule,
                                                         InputBuffer,
                                                         InputBufferSize,
                                                         ClientBuferContextInput);

    FuncExitVoid(DMF_TRACE);
}

_Function_class_(EVT_DMF_ContinuousRequestTarget_BufferOutput)
ContinuousRequestTarget_BufferDisposition
SerialTarget_StreamAsynchronousBufferOutput(
    _In_ DMFMODULE DmfModule,
    _In_reads_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _In_ VOID* ClientBufferContextOutput,
    _In_ NTSTATUS CompletionStatus
    )
/*++

Routine Description:

    Redirect output buffer callback from Request Stream to Parent Module/Device.

Arguments:

    DmfModule - ContinuousRequestTarget DMFMODULE.
    OutputBuffer - The given output buffer.
    OutputBufferSize - Size of the given output buffer.
    ClientBufferContextOutput - Context associated with the given output buffer.
    CompletionStatus - Request completion status.

Return Value:

    ContinuousRequestTarget_BufferDisposition.

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_SerialTarget* moduleContext;
    ContinuousRequestTarget_BufferDisposition bufferDisposition;

    FuncEntry(DMF_TRACE);

    dmfModule = DMF_ParentModuleGet(DmfModule);
    DmfAssert(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    if (moduleContext->EvtContinuousRequestTargetBufferOutput != NULL)
    {
        bufferDisposition = moduleContext->EvtContinuousRequestTargetBufferOutput(dmfModule,
                                                                                  OutputBuffer,
                                                                                  OutputBufferSize,
                                                                                  ClientBufferContextOutput,
                                                                                  CompletionStatus);
    }
    else
    {
        bufferDisposition = ContinuousRequestTarget_BufferDisposition_ContinuousRequestTargetAndContinueStreaming;
    }

    FuncExit(DMF_TRACE, "bufferDisposition=%d", bufferDisposition);

    return bufferDisposition;
}

EVT_WDF_IO_TARGET_QUERY_REMOVE SerialTarget_EvtIoTargetQueryRemove;

_Use_decl_annotations_
NTSTATUS
SerialTarget_EvtIoTargetQueryRemove(
    _In_ WDFIOTARGET IoTarget
    )
{
    NTSTATUS ntStatus;
    DMFMODULE* dmfModuleAddress;
    DMF_CONTEXT_SerialTarget* moduleContext;

    ntStatus = STATUS_SUCCESS;

    FuncEntry(DMF_TRACE);

    dmfModuleAddress = WdfObjectGet_DMFMODULE(IoTarget);
    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    DmfAssert(moduleContext != NULL);

    DMF_SerialTarget_StreamStop(*dmfModuleAddress);

    WdfIoTargetCloseForQueryRemove(IoTarget);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

EVT_WDF_IO_TARGET_REMOVE_CANCELED SerialTarget_EvtIoTargetRemoveCanceled;

VOID
SerialTarget_EvtIoTargetRemoveCanceled(
    _In_ WDFIOTARGET IoTarget
    )
/*++

Routine Description:

    Performs operations when the removal of a specified remote I/O target is canceled.

Arguments:

    IoTarget - A handle to an I/O target object.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE* dmfModuleAddress;
    DMF_CONTEXT_SerialTarget* moduleContext;
    WDF_IO_TARGET_OPEN_PARAMS openParams;

    FuncEntry(DMF_TRACE);

    dmfModuleAddress = WdfObjectGet_DMFMODULE(IoTarget);
    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    DmfAssert(moduleContext != NULL);

    WDF_IO_TARGET_OPEN_PARAMS_INIT_REOPEN(&openParams);

    ntStatus = WdfIoTargetOpen(IoTarget,
                               &openParams);

    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to re-open serial target - %!STATUS!", ntStatus);
        WdfObjectDelete(IoTarget);
        goto Exit;
    }

    ntStatus = DMF_SerialTarget_StreamStart(*dmfModuleAddress);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_SerialTarget_StreamStart fails: ntStatus=%!STATUS!", ntStatus);
    }

Exit:
    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_Must_inspect_result_
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
SerialTarget_InitializeSerialPort(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This function initializes the PCH UART0 port.

Arguments:

    Device - A handle to a framework device object.
    PchUartConfigurationMode - Setting that indicates if the PCH UART mode was configured in Legacy or Pci mode in UEFI.

Return Value:

    STATUS_SUCCESS - always returned

Environment:

    PASSIVE_LEVEL

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    DMF_CONTEXT_SerialTarget* moduleContext;
    DMF_CONFIG_SerialTarget* moduleConfig;
    WDF_IO_TARGET_OPEN_PARAMS  targetOpenParams;
    SerialTarget_Configuration serialIoConfigurationParameters;
    WDF_OBJECT_ATTRIBUTES targetAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    FuncEntry(DMF_TRACE);

    // Create the device path using the connection ID.
    //
    DECLARE_UNICODE_STRING_SIZE(DevicePath, RESOURCE_HUB_PATH_SIZE);

    WDF_MEMORY_DESCRIPTOR   InputMemoryDescriptor;

    // Create the serial target.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&targetAttributes);
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&targetAttributes,
                                           DMFMODULE);
    targetAttributes.ParentObject = DmfModule;

    ntStatus = WdfIoTargetCreate(DMF_ParentDeviceGet(DmfModule),
                                 &targetAttributes,
                                 &moduleContext->IoTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to create IO target - %!STATUS!", ntStatus);
        goto Exit;
    }

    // NOTE: It is not possible to get the parent of a WDFIOTARGET.
    // Therefore, it is necessary to save the DmfModule in its context area.
    //
    DMF_ModuleInContextSave(moduleContext->IoTarget,
                            DmfModule);

    RESOURCE_HUB_CREATE_PATH_FROM_ID(&DevicePath,
                                     moduleContext->PeripheralId.LowPart,
                                     moduleContext->PeripheralId.HighPart);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Opening handle to serial target via %wZ", &DevicePath);

    // Open a handle to the serial controller.
    //
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&targetOpenParams,
                                                &DevicePath,
                                                moduleConfig->OpenMode);

    targetOpenParams.ShareAccess = moduleConfig->ShareAccess;
    targetOpenParams.CreateDisposition = FILE_OPEN;
    targetOpenParams.FileAttributes = FILE_ATTRIBUTE_NORMAL;
    targetOpenParams.EvtIoTargetQueryRemove = SerialTarget_EvtIoTargetQueryRemove;
    targetOpenParams.EvtIoTargetRemoveCanceled = SerialTarget_EvtIoTargetRemoveCanceled;

    ntStatus = WdfIoTargetOpen(moduleContext->IoTarget,
                               &targetOpenParams);

    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to open serial target - %!STATUS!", ntStatus);
        goto Exit;
    }

    DMF_ContinuousRequestTarget_IoTargetSet(moduleContext->DmfModuleContinuousRequestTarget,
                                            moduleContext->IoTarget);

    ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                 NULL,
                                                 IOCTL_SERIAL_APPLY_DEFAULT_CONFIGURATION,
                                                 NULL,
                                                 NULL,
                                                 NULL,
                                                 NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to apply default configuration ");
        goto Exit;
    }

    if (moduleConfig->EvtSerialTargetCustomConfiguration == NULL)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "EvtSerialTargetCustomConfiguration not set");
        goto Exit;
    }

    if (! moduleConfig->EvtSerialTargetCustomConfiguration(DmfModule,
                                                           &serialIoConfigurationParameters))
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No override of configuration parameters required.");
        goto Exit;
    }

    // Check to see if the right bits are set in Flags.
    //
    if ((serialIoConfigurationParameters.Flags & ~ConfigurationParametersValidFlags) != 0)
    {
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Wrong serialIoConfigurationParameters.Flags = 0x%x", serialIoConfigurationParameters.Flags);
        goto Exit;
    }

    if (serialIoConfigurationParameters.Flags & SerialBaudRateFlag)
    {
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&InputMemoryDescriptor,
                                          &serialIoConfigurationParameters.BaudRate,
                                          sizeof(SERIAL_BAUD_RATE));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_SET_BAUD_RATE,
                                                     &InputMemoryDescriptor,
                                                     NULL,
                                                     NULL,
                                                     NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to set baudrate ");
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "successfully set baudrate");
    }

    if (serialIoConfigurationParameters.Flags & SerialClearRtsFlag)
    {
        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_CLR_RTS,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to CLR_RTS");
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "successfully CLR_RTS");
    }

    if (serialIoConfigurationParameters.Flags & SerialClearDtrFlag)
    {
        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_CLR_DTR,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to CLR_DTR");
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "successfully CLR_DTR");
    }

    if (serialIoConfigurationParameters.Flags & SerialHandflowFlag)
    {
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&InputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialHandflow,
                                          sizeof(SERIAL_HANDFLOW));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_SET_HANDFLOW,
                                                     &InputMemoryDescriptor,
                                                     NULL,
                                                     NULL,
                                                     NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to SET_HANDFLOW - %!STATUS!", ntStatus);
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "successfully SET_HANDFLOW");

        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&InputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialHandflow,
                                          sizeof(SERIAL_HANDFLOW));

        RtlZeroMemory(&serialIoConfigurationParameters.SerialHandflow, sizeof(SERIAL_HANDFLOW));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_GET_HANDFLOW,
                                                     NULL,
                                                     &InputMemoryDescriptor,
                                                     NULL,
                                                     NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to GET_HANDFLOW - %!STATUS!", ntStatus);
            goto Exit;
        }
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "successfully GET_HANDFLOW ControlHandShake=0x%x FlowReplace=0x%x XonLimit=0x%x XoffLimit=0x%x",
                    serialIoConfigurationParameters.SerialHandflow.ControlHandShake,
                    serialIoConfigurationParameters.SerialHandflow.FlowReplace,
                    serialIoConfigurationParameters.SerialHandflow.XonLimit,
                    serialIoConfigurationParameters.SerialHandflow.XoffLimit);
    }

    if (serialIoConfigurationParameters.Flags & SerialWaitMaskFlag)
    {
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&InputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialWaitMask,
                                          sizeof(ULONG));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_SET_WAIT_MASK,
                                                     &InputMemoryDescriptor,
                                                     NULL,
                                                     NULL,
                                                     NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to SET_WAIT_MASK ");
            goto Exit;
        }
        else
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "successfully SET_WAIT_MASK to 0x%X", serialIoConfigurationParameters.SerialWaitMask);
        }

        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&InputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialWaitMask,
                                          sizeof(ULONG));

        serialIoConfigurationParameters.SerialWaitMask = 0;

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_GET_WAIT_MASK,
                                                     NULL,
                                                     &InputMemoryDescriptor,
                                                     NULL,
                                                     NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to GET_WAIT_MASK ");
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "successfully GET_WAIT_MASK: 0x%X", serialIoConfigurationParameters.SerialWaitMask);
    }

    if (serialIoConfigurationParameters.Flags & SerialLineControlFlag)
    {

        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&InputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialLineControl,
                                          sizeof(SERIAL_LINE_CONTROL));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_SET_LINE_CONTROL,
                                                     &InputMemoryDescriptor,
                                                     NULL,
                                                     NULL,
                                                     NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to set SERIAL_LINE_CONTROL - %!STATUS!", ntStatus);
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "successfully set SERIAL_LINE_CONTROL");
    }

    if (serialIoConfigurationParameters.Flags & SerialCharsFlag)
    {
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&InputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialChars,
                                          sizeof(SERIAL_CHARS));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_SET_CHARS,
                                                     &InputMemoryDescriptor,
                                                     NULL,
                                                     NULL,
                                                     NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to SET_CHARS - %!STATUS!", ntStatus);
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "successfully SET_CHARS");

        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&InputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialChars,
                                          sizeof(SERIAL_CHARS));

        RtlZeroMemory(&serialIoConfigurationParameters.SerialChars, sizeof(SERIAL_CHARS));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_GET_CHARS,
                                                     NULL,
                                                     &InputMemoryDescriptor,
                                                     NULL,
                                                     NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to GET_CHARS - %!STATUS!", ntStatus);
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "successfully GET_CHARS: EofChar=0x%x ErrorChar=0x%x BreakChar = 0x%x EventChar=0x%x XonChar=0x%x XoffChar=0x%x ",
                    serialIoConfigurationParameters.SerialChars.EofChar,
                    serialIoConfigurationParameters.SerialChars.ErrorChar,
                    serialIoConfigurationParameters.SerialChars.BreakChar,
                    serialIoConfigurationParameters.SerialChars.EventChar,
                    serialIoConfigurationParameters.SerialChars.XonChar,
                    serialIoConfigurationParameters.SerialChars.XoffChar);
    }

    if (serialIoConfigurationParameters.Flags & SerialTimeoutsFlag)
    {
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&InputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialTimeouts,
                                          sizeof(SERIAL_TIMEOUTS));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_SET_TIMEOUTS,
                                                     &InputMemoryDescriptor,
                                                     NULL,
                                                     NULL,
                                                     NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to SET_TIMEOUTS ");
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "successfully SET_TIMEOUTS");
    }

    if (serialIoConfigurationParameters.Flags & SerialQueueSizeFlag)
    {
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&InputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.QueueSize,
                                          sizeof(SERIAL_QUEUE_SIZE));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_SET_QUEUE_SIZE,
                                                     &InputMemoryDescriptor,
                                                     NULL,
                                                     NULL,
                                                     NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to SET_QUEUE_SIZE ");
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "successfully SET_QUEUE_SIZE");
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// Destroy the Device IoTarget.
//
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
SerialTarget_IoTargetDestroy(
    _Inout_ DMF_CONTEXT_SerialTarget* ModuleContext
    )
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    if (ModuleContext->IoTarget != NULL)
    {
        WdfIoTargetClose(ModuleContext->IoTarget);
        DMF_ContinuousRequestTarget_IoTargetClear(ModuleContext->DmfModuleContinuousRequestTarget);
        WdfObjectDelete(ModuleContext->IoTarget);
        ModuleContext->IoTarget = NULL;
    }

    FuncExitVoid(DMF_TRACE);
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

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_SerialTarget_Open(
    _In_ DMFMODULE DmfModule
    )
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SerialTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = SerialTarget_InitializeSerialPort(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "SerialTarget_InitializeSerialPort fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (moduleContext->ContinuousRequestTargetMode == ContinuousRequestTarget_Mode_Automatic)
    {
        // By calling this function here, callbacks at the Client will happen only after the Module is open.
        //
        DmfAssert(moduleContext->DmfModuleContinuousRequestTarget != NULL);
        ntStatus = DMF_ContinuousRequestTarget_Start(moduleContext->DmfModuleContinuousRequestTarget);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ContinuousRequestTarget_Start fails: ntStatus=%!STATUS!", ntStatus);
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_SerialTarget_Close(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_SerialTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->IoTarget != NULL)
    {
        if (moduleContext->ContinuousRequestTargetMode == ContinuousRequestTarget_Mode_Automatic)
        {
            // By calling this function here, callbacks at the Client will happen only before the Module is closed.
            //
            DmfAssert(moduleContext->DmfModuleContinuousRequestTarget != NULL);
            DMF_ContinuousRequestTarget_StopAndWait(moduleContext->DmfModuleContinuousRequestTarget);
        }

        // Close the associated target.
        //
        SerialTarget_IoTargetDestroy(moduleContext);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ResourcesAssign)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_SerialTarget_ResourcesAssign(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
{
    DMF_CONTEXT_SerialTarget* moduleContext;
    ULONG resourceCount;
    ULONG resourceIndex;
    NTSTATUS ntStatus;
    WDFDEVICE device;
    BOOLEAN serialResourceFound;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(ResourcesRaw);

    FuncEntry(DMF_TRACE);

    DmfAssert(ResourcesRaw != NULL);
    DmfAssert(ResourcesTranslated != NULL);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    serialResourceFound = FALSE;

    // Check the number of resources for the button device.
    //
    resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);

    // Parse the resources. This Module cares about GPIO and Interrupt resources.
    //
    for (resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++)
    {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR resource;

        resource = WdfCmResourceListGetDescriptor(ResourcesTranslated,
                                                  resourceIndex);
        if (NULL == resource)
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No resources found");
            goto Exit;
        }

        if (CmResourceTypeConnection == resource->Type)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "CmResourceTypeConnection %08X %08X %08X %08X\r\n",
                        resource->u.Connection.Class,
                        resource->u.Connection.IdHighPart,
                        resource->u.Connection.IdLowPart,
                        resource->u.Connection.Type);

            switch (resource->u.Connection.Class)
            {
            case CM_RESOURCE_CONNECTION_CLASS_SERIAL:
                TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Connection Class Serial (SPB)\r\n");
                switch (resource->u.Connection.Type)
                {
                case CM_RESOURCE_CONNECTION_TYPE_SERIAL_UART:
                {
                    moduleContext->PeripheralId.LowPart = resource->u.Connection.IdLowPart;
                    moduleContext->PeripheralId.HighPart = resource->u.Connection.IdHighPart;

                    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Connection Class SPB Type UART = 0x%llx\r\n", moduleContext->PeripheralId.QuadPart);

                    serialResourceFound = TRUE;
                    break;
                }
                default:
                    TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Unexpected Connection Class SPB Type %08X\r\n", resource->u.Connection.Type);
                    break;
                }
                break;
            default:
                TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Unexpected Connection Class %08X\r\n", resource->u.Connection.Class);
                break;
            }
            break;
        }
    }

    //  Validate if Serial IO resource has been found.
    //
    if (! serialResourceFound)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No Serial IO resources found");
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
        NT_ASSERT(FALSE);
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_SerialTarget_ChildModulesAdd(
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
    DMF_CONFIG_SerialTarget* moduleConfig;
    DMF_CONTEXT_SerialTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // ContinuousRequestTarget
    // -----------------------
    //

    // Store ContinuousRequestTarget callbacks from config into SerialTarget context for redirection.
    //
    moduleContext->EvtContinuousRequestTargetBufferInput = moduleConfig->ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput;
    moduleContext->EvtContinuousRequestTargetBufferOutput = moduleConfig->ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput;

    // Replace ContinuousRequestTarget callbacks in config with SerialTarget callbacks.
    //
    moduleConfig->ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput = SerialTarget_StreamAsynchronousBufferInput;
    moduleConfig->ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput = SerialTarget_StreamAsynchronousBufferOutput;

    DMF_ContinuousRequestTarget_ATTRIBUTES_INIT(&moduleAttributes);
    moduleAttributes.ModuleConfigPointer = &moduleConfig->ContinuousRequestTargetModuleConfig;
    moduleAttributes.SizeOfModuleSpecificConfig = sizeof(moduleConfig->ContinuousRequestTargetModuleConfig);
    moduleAttributes.PassiveLevel = DmfParentModuleAttributes->PassiveLevel;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleContinuousRequestTarget);
    
    // Remember Client's choice so this Module can start/stop streaming appropriately.
    //
    moduleContext->ContinuousRequestTargetMode = moduleConfig->ContinuousRequestTargetModuleConfig.ContinuousRequestTargetMode;

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
DMF_SerialTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type SerialTarget.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_SerialTarget;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_SerialTarget;
    DMF_CONFIG_SerialTarget* moduleConfig;
    DmfModuleOpenOption openOption;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = (DMF_CONFIG_SerialTarget*)DmfModuleAttributes->ModuleConfigPointer;

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_SerialTarget);
    dmfCallbacksDmf_SerialTarget.DeviceOpen = DMF_SerialTarget_Open;
    dmfCallbacksDmf_SerialTarget.DeviceClose = DMF_SerialTarget_Close;
    dmfCallbacksDmf_SerialTarget.DeviceResourcesAssign = DMF_SerialTarget_ResourcesAssign;
    dmfCallbacksDmf_SerialTarget.ChildModulesAdd = DMF_SerialTarget_ChildModulesAdd;

    // SerialTarget support multiple open option configurations. 
    // Choose the open option based on Module config. 
    //
    switch (moduleConfig->ModuleOpenOption)
    {
    case SerialTarget_OpenOption_PrepareHardware:
        openOption = DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware;
        break;
    case SerialTarget_OpenOption_D0EntrySystemPowerUp:
        openOption = DMF_MODULE_OPEN_OPTION_OPEN_D0EntrySystemPowerUp;
        break;
    case SerialTarget_OpenOption_D0Entry:
        openOption = DMF_MODULE_OPEN_OPTION_OPEN_D0Entry;
        break;
    default:
        DmfAssert(FALSE);
        openOption = DMF_MODULE_OPEN_OPTION_Invalid;
        break;
    }

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_SerialTarget,
                                            SerialTarget,
                                            DMF_CONTEXT_SerialTarget,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            openOption);

    dmfModuleDescriptor_SerialTarget.CallbacksDmf = &dmfCallbacksDmf_SerialTarget;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_SerialTarget,
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

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_SerialTarget_BufferPut(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    )
/*++

Routine Description:

    Add the output buffer back to OutputBufferPool.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The buffer to add to the list.
    NOTE: This must be a properly formed buffer that was created by this Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_SerialTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 SerialTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ContinuousRequestTarget_BufferPut(moduleContext->DmfModuleContinuousRequestTarget,
                                          ClientBuffer);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_SerialTarget_IoTargetGet(
    _In_ DMFMODULE DmfModule,
    _Out_ WDFIOTARGET* IoTarget
    )
/*++

Routine Description:

    Get the IoTarget to Send Requests to.

Arguments:

    DmfModule - This Module's handle.
    IoTarget - IO Target.

Return Value:

    VOID

--*/
{
    DMF_CONTEXT_SerialTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 SerialTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(IoTarget != NULL);
    DmfAssert(moduleContext->IoTarget != NULL);

    *IoTarget = moduleContext->IoTarget;

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_SerialTarget_Send(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _In_opt_ EVT_DMF_ContinuousRequestTarget_SendCompletion* EvtContinuousRequestTargetSingleAsynchronousRequest,
    _In_opt_ VOID* SingleAsynchronousRequestClientContext
    )
/*++

Routine Description:

    Creates and sends an Asynchronous request to the IoTarget given a buffer, IOCTL and other information.

Arguments:

    DmfModule - This Module's handle.
    RequestBuffer - Buffer of data to attach to request to be sent.
    RequestLength - Number of bytes in RequestBuffer to send.
    ResponseBuffer - Buffer of data that is returned by the request.
    ResponseLength - Size of Response Buffer in bytes.
    RequestType - Read or Write or Ioctl
    RequestIoctl - The given IOCTL.
    RequestTimeoutMilliseconds - Timeout value in milliseconds of the transfer or zero for no timeout.
    EvtContinuousRequestTargetSingleAsynchronousRequest - Callback to be called in completion routine.
    SingleAsynchronousRequestClientContext - Client context sent in callback

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SerialTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 SerialTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->IoTarget != NULL);

    ntStatus = DMF_ContinuousRequestTarget_Send(moduleContext->DmfModuleContinuousRequestTarget,
                                                RequestBuffer,
                                                RequestLength,
                                                ResponseBuffer,
                                                ResponseLength,
                                                RequestType,
                                                RequestIoctl,
                                                RequestTimeoutMilliseconds,
                                                EvtContinuousRequestTargetSingleAsynchronousRequest,
                                                SingleAsynchronousRequestClientContext);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_SerialTarget_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
    _In_ size_t ResponseLength,
    _In_ ContinuousRequestTarget_RequestType RequestType,
    _In_ ULONG RequestIoctl,
    _In_ ULONG RequestTimeoutMilliseconds,
    _Out_opt_ size_t* BytesWritten
    )
/*++

Routine Description:

    Creates and sends a synchronous request to the IoTarget given a buffer, IOCTL and other information.

Arguments:

    DmfModule - This Module's handle.
    RequestBuffer - Buffer of data to attach to request to be sent.
    RequestLength - Number of bytes in RequestBuffer to send.
    ResponseBuffer - Buffer of data that is returned by the request.
    ResponseLength - Size of Response Buffer in bytes.
    RequestType - Read or Write or Ioctl
    RequestIoctl - The given IOCTL.
    RequestTimeoutMilliseconds - Timeout value in milliseconds of the transfer or zero for no timeout.
    BytesWritten - Bytes returned by the transaction.

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SerialTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 SerialTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->IoTarget != NULL);

    ntStatus = DMF_ContinuousRequestTarget_SendSynchronously(moduleContext->DmfModuleContinuousRequestTarget,
                                                             RequestBuffer,
                                                             RequestLength,
                                                             ResponseBuffer,
                                                             ResponseLength,
                                                             RequestType,
                                                             RequestIoctl,
                                                             RequestTimeoutMilliseconds,
                                                             BytesWritten);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_SerialTarget_StreamStart(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Starts streaming Asynchronous requests to the IoTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SerialTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 SerialTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->IoTarget != NULL);

    ntStatus = DMF_ContinuousRequestTarget_Start(moduleContext->DmfModuleContinuousRequestTarget);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_SerialTarget_StreamStop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stops streaming Asynchronous requests to the IoTarget and Cancels all the existing requests.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_SerialTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 SerialTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->IoTarget != NULL);

    DMF_ContinuousRequestTarget_StopAndWait(moduleContext->DmfModuleContinuousRequestTarget);

    FuncExitVoid(DMF_TRACE);
}

// eof: Dmf_SerialTarget.c
//
