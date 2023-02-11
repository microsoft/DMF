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

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_SerialTarget.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef enum
{
    StreamingState_Invalid,
    StreamingState_Stopped,
    StreamingState_Started,
    StreamingState_StoppedDuringQueryRemove,
} StreamingStateType;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_SerialTarget
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
    // Synchronizes QueryRemove/RemoveCancel/RemoveComplete with Start/Stop.
    //
    DMFMODULE DmfModuleRundown;
    // Tracks the state of the stream so that it can be started if necessary during
    // RemoveCancel.
    //
    StreamingStateType StreamingState;
    // Tracks if QueryRemove succeeded. This is needed for surprise remove where RemoveComplete can
    // happened without a QueryRemove.
    //
    BOOLEAN QueryRemoveSucceeded;
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

#define OS_BUILD_WITH_SERIAL_CONTROLLER_HIGH_RESOLUTION_TIMER_SUPPORT 22000

#define RESHUB_USE_HELPER_ROUTINES
#include "reshub.h"

_Function_class_(EVT_DMF_ContinuousRequestTarget_BufferInput)
VOID
SerialTarget_StreamAsynchronousBufferInput(
    _In_ DMFMODULE DmfModuleContinuousRequestTarget,
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

    dmfModule = DMF_ParentModuleGet(DmfModuleContinuousRequestTarget);
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
    _In_ DMFMODULE DmfModuleContinuousRequestTarget,
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

    dmfModule = DMF_ParentModuleGet(DmfModuleContinuousRequestTarget);
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

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
SerialTarget_StreamStart(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Starts streaming Asynchronous requests to the IoTarget. This function is meant to be called by the Method
    with a reference acquired, or internally by the Module without acquiring a reference.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS if the stream was successfully started.
    Otherwise NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SerialTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->IoTarget != NULL);

    ntStatus = DMF_ContinuousRequestTarget_Start(moduleContext->DmfModuleContinuousRequestTarget);
    if (NT_SUCCESS(ntStatus))
    {
        moduleContext->StreamingState = StreamingState_Started;
    }
    else
    {
        moduleContext->StreamingState = StreamingState_Stopped;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
SerialTarget_StreamStop(
    _In_ DMFMODULE DmfModule,
    _In_ StreamingStateType TargetState
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
    StreamingStateType currentState;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->IoTarget != NULL);
    DmfAssert(TargetState == StreamingState_Stopped || 
              TargetState == StreamingState_StoppedDuringQueryRemove);

    DMF_ModuleLock(DmfModule);
    currentState = moduleContext->StreamingState;

    if (currentState == StreamingState_Started)
    {
        // Only change state if streaming was started.
        //
        moduleContext->StreamingState = TargetState;
    }
    DMF_ModuleUnlock(DmfModule);

    if (currentState == StreamingState_Started)
    {
        // Only stop streaming if streaming was started.
        //
        DMF_ContinuousRequestTarget_StopAndWait(moduleContext->DmfModuleContinuousRequestTarget);
    }
 
    FuncExitVoid(DMF_TRACE);
}

EVT_WDF_IO_TARGET_QUERY_REMOVE SerialTarget_EvtIoTargetQueryRemove;

_Use_decl_annotations_
NTSTATUS
SerialTarget_EvtIoTargetQueryRemove(
    _In_ WDFIOTARGET IoTarget
    )
/*++

Routine Description:

    Indicates whether the framework can safely remove a specified remote I/O target's device.

Arguments:

    IoTarget - A handle to an I/O target object.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE* dmfModuleAddress;
    DMF_CONTEXT_SerialTarget* moduleContext;
    DMF_CONFIG_SerialTarget* moduleConfig;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    dmfModuleAddress = WdfObjectGet_DMFMODULE(IoTarget);
    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);
    DmfAssert(moduleContext != NULL);

    // Call client's QueryRemove callback so client can take action before IoTarget is closed.
    //
    if (moduleConfig->EvtSerialTargetQueryRemove != NULL)
    {
        ntStatus = moduleConfig->EvtSerialTargetQueryRemove(*dmfModuleAddress);

        // Only stop streaming and close the target if Client has not vetoed QueryRemove.
        //
        if (!NT_SUCCESS(ntStatus))
        {
            moduleContext->QueryRemoveSucceeded = FALSE;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtSerialTargetQueryRemove fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    moduleContext->QueryRemoveSucceeded = TRUE;

    // Let any Start/Stop that has started executing finish.
    //
    DMF_Rundown_EndAndWait(moduleContext->DmfModuleRundown);

    // After this point Start/Stop will fail with STATUS_INVALID_DEVICE_STATE if a thread
    // calls those Methods while QueryRemove/QueryCancel/RemoveComplete path is executing
    // so this state can be checked.
    //
    SerialTarget_StreamStop(*dmfModuleAddress,
                            StreamingState_StoppedDuringQueryRemove);

    WdfIoTargetCloseForQueryRemove(IoTarget);

Exit:

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
    DMF_CONFIG_SerialTarget* moduleConfig;

    WDF_IO_TARGET_OPEN_PARAMS openParams;

    FuncEntry(DMF_TRACE);

    dmfModuleAddress = WdfObjectGet_DMFMODULE(IoTarget);
    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);
    DmfAssert(moduleContext != NULL);

    if (moduleContext->QueryRemoveSucceeded == FALSE)
    {
        DmfAssert(IoTarget == moduleContext->IoTarget);
        // No need to re-open IoTarget if client vetoed QueryRemove.
        //
        goto Exit;
    }

    moduleContext->QueryRemoveSucceeded = FALSE;

    WDF_IO_TARGET_OPEN_PARAMS_INIT_REOPEN(&openParams);
    ntStatus = WdfIoTargetOpen(IoTarget,
                               &openParams);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to re-open serial target - %!STATUS!", ntStatus);

        // No need to clear or delete IoTarget here.
        // Any Module calls to IoTarget will fail gracefully and ModuleClose will handle cleanup.
        // IoTarget is Created/Destroyed in ModuleClose/ModuleOpen. A new target will not created before Module is closed.
        //

        // Back to original state after Module Open.
        //
        moduleContext->StreamingState = StreamingState_Stopped;

        goto Exit;
    }

    // Start/Stop will fail with STATUS_INVALID_DEVICE_STATE if a thread calls those Methods while
    // QueryRemove/QueryCancel/RemoveComplete path is executing so this state can be checked.
    //
    if (moduleContext->StreamingState == StreamingState_StoppedDuringQueryRemove)
    {
        // Start the stream again. Reference is not acquired because Rundown_EndAndWait has executed.
        //
        ntStatus = SerialTarget_StreamStart(*dmfModuleAddress);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "SerialTarget_StreamStart fails: ntStatus=%!STATUS!", ntStatus);
        }
    }

    // Allow Start/Stop to execute.
    //
    DMF_Rundown_Start(moduleContext->DmfModuleRundown);

    // Call client's RemoveCanceled callback so client can take action after IoTarget is opened.
    //
    if (moduleConfig->EvtSerialTargetRemoveCanceled != NULL)
    {
        moduleConfig->EvtSerialTargetRemoveCanceled(*dmfModuleAddress);
    }

Exit:

    FuncExitVoid(DMF_TRACE);
}

EVT_WDF_IO_TARGET_REMOVE_CANCELED SerialTarget_EvtIoTargetRemoveComplete;

VOID
SerialTarget_EvtIoTargetRemoveComplete(
    _In_ WDFIOTARGET IoTarget
    )
/*++

Routine Description:

    Called when the Target device is removed (either the target
    received IRP_MN_REMOVE_DEVICE or IRP_MN_SURPRISE_REMOVAL)

Arguments:

    IoTarget - A handle to an I/O target object.

Return Value:

    None

--*/
{
    DMFMODULE* dmfModuleAddress;
    DMF_CONTEXT_SerialTarget* moduleContext;
    DMF_CONFIG_SerialTarget* moduleConfig;

    FuncEntry(DMF_TRACE);

    dmfModuleAddress = WdfObjectGet_DMFMODULE(IoTarget);
    moduleContext = DMF_CONTEXT_GET(*dmfModuleAddress);
    moduleConfig = DMF_CONFIG_GET(*dmfModuleAddress);
    DmfAssert(moduleContext != NULL);

    // Start/Stop will fail with STATUS_INVALID_DEVICE_STATE if a thread calls those Methods while
    // QueryRemove/QueryCancel/RemoveComplete path is executing so this state can be checked.
    //
    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RemoveComplete");

    // Call client's RemoveCanceled callback so client can take action on RemoveComplete.
    //
    if (moduleConfig->EvtSerialTargetRemoveComplete != NULL)
    {
        moduleConfig->EvtSerialTargetRemoveComplete(*dmfModuleAddress);
    }

    if (moduleContext->QueryRemoveSucceeded == FALSE)
    {
        // QueryRemove did not happen e.g. Surprise Removal. Do necessary clean up.
        // 

        // Let any Start/Stop that has started executing finish.
        //
        DMF_Rundown_EndAndWait(moduleContext->DmfModuleRundown);

        // After this point Start/Stop will fail with STATUS_INVALID_DEVICE_STATE if a thread
        // calls those Methods while QueryRemove/QueryCancel/RemoveComplete path is executing
        // so this state can be checked.
        //
        SerialTarget_StreamStop(*dmfModuleAddress,
                                StreamingState_Stopped);
    }

    moduleContext->QueryRemoveSucceeded = FALSE;

    // No need to delete target here. ModuleClose will handle clean up.
    // IoTarget is Created/Destroyed in ModuleOpen/ModuleClose. A new target will not be created before Module is closed.
    //
    WdfIoTargetClose(IoTarget);

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
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
    NTSTATUS ntStatus;
    DMF_CONTEXT_SerialTarget* moduleContext;
    DMF_CONFIG_SerialTarget* moduleConfig;
    WDF_IO_TARGET_OPEN_PARAMS  targetOpenParams;
    SerialTarget_Configuration serialIoConfigurationParameters;
    WDF_OBJECT_ATTRIBUTES targetAttributes;
    WDF_MEMORY_DESCRIPTOR inputMemoryDescriptor;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    FuncEntry(DMF_TRACE);

    // Create the device path using the connection ID.
    //
    DECLARE_UNICODE_STRING_SIZE(DevicePath, RESOURCE_HUB_PATH_SIZE);

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
    targetOpenParams.EvtIoTargetRemoveComplete = SerialTarget_EvtIoTargetRemoveComplete;

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
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemoryDescriptor,
                                          &serialIoConfigurationParameters.BaudRate,
                                          sizeof(SERIAL_BAUD_RATE));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_SET_BAUD_RATE,
                                                     &inputMemoryDescriptor,
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
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialHandflow,
                                          sizeof(SERIAL_HANDFLOW));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_SET_HANDFLOW,
                                                     &inputMemoryDescriptor,
                                                     NULL,
                                                     NULL,
                                                     NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to SET_HANDFLOW - %!STATUS!", ntStatus);
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "successfully SET_HANDFLOW");

        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialHandflow,
                                          sizeof(SERIAL_HANDFLOW));

        RtlZeroMemory(&serialIoConfigurationParameters.SerialHandflow, sizeof(SERIAL_HANDFLOW));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_GET_HANDFLOW,
                                                     NULL,
                                                     &inputMemoryDescriptor,
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
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialWaitMask,
                                          sizeof(ULONG));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_SET_WAIT_MASK,
                                                     &inputMemoryDescriptor,
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

        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialWaitMask,
                                          sizeof(ULONG));

        serialIoConfigurationParameters.SerialWaitMask = 0;

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_GET_WAIT_MASK,
                                                     NULL,
                                                     &inputMemoryDescriptor,
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

        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialLineControl,
                                          sizeof(SERIAL_LINE_CONTROL));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_SET_LINE_CONTROL,
                                                     &inputMemoryDescriptor,
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
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialChars,
                                          sizeof(SERIAL_CHARS));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_SET_CHARS,
                                                     &inputMemoryDescriptor,
                                                     NULL,
                                                     NULL,
                                                     NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to SET_CHARS - %!STATUS!", ntStatus);
            goto Exit;
        }

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "successfully SET_CHARS");

        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialChars,
                                          sizeof(SERIAL_CHARS));

        RtlZeroMemory(&serialIoConfigurationParameters.SerialChars, sizeof(SERIAL_CHARS));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_GET_CHARS,
                                                     NULL,
                                                     &inputMemoryDescriptor,
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
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.SerialTimeouts,
                                          sizeof(SERIAL_TIMEOUTS));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_SET_TIMEOUTS,
                                                     &inputMemoryDescriptor,
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
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemoryDescriptor,
                                          (VOID*)&serialIoConfigurationParameters.QueueSize,
                                          sizeof(SERIAL_QUEUE_SIZE));

        ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                     NULL,
                                                     IOCTL_SERIAL_SET_QUEUE_SIZE,
                                                     &inputMemoryDescriptor,
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

#if IS_WIN10_21H2_OR_LATER
    // IOCTL_SERIAL_SET_INTERVAL_TIMER_RESOLUTION was not defined until 21H2.
    // It will not be present in earlier EWDK and code below will not compile when built with them.
    //

    if (serialIoConfigurationParameters.Flags & SerialHighResolutionTimerFlag)
    {
        // Serial Controller Driver did not implement IOCTL_SERIAL_SET_INTERVAL_TIMER_RESOLUTION until
        // Windows 11 i.e. build 22000. In earlier OS, this IOCTL will fall back to the IHV serial driver and
        // and the behavior will be dependent on the serial driver implementation. To avoid issues variabilty
        // in how the serial driver handles this IOCTL, do not send it for versions of Windows earlier than
        // Windows11.
        //

        RTL_OSVERSIONINFOEXW osVersion;
        ULONGLONG conditonMask;

        conditonMask = 0;
        RtlZeroMemory(&osVersion,
                      sizeof(osVersion));

        osVersion.dwBuildNumber = OS_BUILD_WITH_SERIAL_CONTROLLER_HIGH_RESOLUTION_TIMER_SUPPORT;
        VER_SET_CONDITION(conditonMask,
                          VER_BUILDNUMBER,
                          VER_GREATER_EQUAL);

        ntStatus = RtlVerifyVersionInfo(&osVersion,
                                        VER_BUILDNUMBER,
                                        conditonMask);
        if (NT_SUCCESS(ntStatus))
        {
            WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemoryDescriptor,
                                              (VOID*)&serialIoConfigurationParameters.EnableHighResolutionTimer,
                                              sizeof(BOOLEAN));

            ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->IoTarget,
                                                         NULL,
                                                         IOCTL_SERIAL_SET_INTERVAL_TIMER_RESOLUTION,
                                                         &inputMemoryDescriptor,
                                                         NULL,
                                                         NULL,
                                                         NULL);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to SET_INTERVAL_TIMER_RESOLUTION ");
                goto Exit;
            }

            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "successfully SET_INTERVAL_TIMER_RESOLUTION");
        }
        else
        {
            // Reset NTSTATUS since failing RtlVerifyVersionInfo is not an actual failure.
            //
            ntStatus = STATUS_SUCCESS;
        }
    }
#endif

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

_Must_inspect_result_
NTSTATUS
SerialTarget_Reference(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Reference both the Module and the underlying WDFIOTAGET.
    It is necessary to reference both because either or both can happen:
    1. D0Exit happens while Client thread is calling Methods.
    2. Underlying WDFIOTARGET can be removed while Client thread is calling Methods.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS - Both references acquired.
    Other - One of the references could not be acquired.

--*/
{
    DMF_CONTEXT_SerialTarget* moduleContext;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        // Module is closing or closed.
        //
        goto Exit;
    }

    ntStatus = DMF_Rundown_Reference(moduleContext->DmfModuleRundown);
    if (!NT_SUCCESS(ntStatus))
    {
        // QueryRemove has started.
        //
        DMF_ModuleDereference(DmfModule);
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

VOID
SerialTarget_Dereference(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Dereference both the Module and the underlying WDFIOTAGET.
    Must be called after successful call to SerialTarget_Reference().

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_SerialTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_Rundown_Dereference(moduleContext->DmfModuleRundown);
    DMF_ModuleDereference(DmfModule);

    FuncExitVoid(DMF_TRACE);
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

    moduleContext->QueryRemoveSucceeded = FALSE;
    moduleContext->StreamingState = StreamingState_Stopped;
    
    // Allow Start/Stop to execute.
    // Call before SerialTarget_InitializeSerialPort since QueryRemove can happen once the target is created.
    //
    DMF_Rundown_Start(moduleContext->DmfModuleRundown);

    ntStatus = SerialTarget_InitializeSerialPort(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "SerialTarget_InitializeSerialPort fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (moduleContext->ContinuousRequestTargetMode == ContinuousRequestTarget_Mode_Automatic)
    {
        // By calling this Method here, callbacks at the Client will happen only after the Module is open.
        //
        DmfAssert(moduleContext->DmfModuleContinuousRequestTarget != NULL);
        ntStatus = DMF_SerialTarget_StreamStart(DmfModule);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_SerialTarget_StreamStart fails: ntStatus=%!STATUS!", ntStatus);
        }
    }

Exit:

    if (!NT_SUCCESS(ntStatus))
    {
        // ModuleClose won't be called.
        //
        DMF_Rundown_EndAndWait(moduleContext->DmfModuleRundown);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

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

    DMF_Rundown_EndAndWait(moduleContext->DmfModuleRundown);

    if (moduleContext->IoTarget != NULL)
    {
        if (moduleContext->ContinuousRequestTargetMode == ContinuousRequestTarget_Mode_Automatic)
        {
            // If QueryRemove path starts before this call, this call does nothing.
            //
            DmfAssert(moduleContext->DmfModuleContinuousRequestTarget != NULL);
            SerialTarget_StreamStop(DmfModule,
                                    StreamingState_Stopped);
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
_Must_inspect_result_
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

    // Store ContinuousRequestTarget callbacks from Config into SerialTarget context for redirection.
    //
    moduleContext->EvtContinuousRequestTargetBufferInput = moduleConfig->ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferInput;
    moduleContext->EvtContinuousRequestTargetBufferOutput = moduleConfig->ContinuousRequestTargetModuleConfig.EvtContinuousRequestTargetBufferOutput;

    // Replace ContinuousRequestTarget callbacks in Config with SerialTarget callbacks.
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

    // Rundown
    // -------
    //
    DMF_Rundown_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleRundown);

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
    // Choose the open option based on Module Config. 
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
_Must_inspect_result_
NTSTATUS
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
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 SerialTarget);

    // 1. Prevent callers from calling Methods when the Module is Closed.
    // 2. Prevent external callers from accessing WDFIOTARGET while it might have been
    //    removed or be in process of being removed.
    //
    ntStatus = SerialTarget_Reference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        // Module is closing, closed or QueryRemove has started.
        //
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(IoTarget != NULL);
    DmfAssert(moduleContext->IoTarget != NULL);

    *IoTarget = moduleContext->IoTarget;

    SerialTarget_Dereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SerialTarget_Send(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
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

    // 1. Prevent callers from calling Methods when the Module is Closed.
    // 2. Prevent external callers from accessing WDFIOTARGET while it might have been
    //    removed or be in process of being removed.
    //
    ntStatus = SerialTarget_Reference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        // Module is closing, closed or QueryRemove has started.
        //
        goto Exit;
    }

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

    SerialTarget_Dereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SerialTarget_SendSynchronously(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_opt_(RequestLength) VOID* RequestBuffer,
    _In_ size_t RequestLength,
    _Out_writes_bytes_opt_(ResponseLength) VOID* ResponseBuffer,
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

    // 1. Prevent callers from calling Methods when the Module is Closed.
    // 2. Prevent external callers from accessing WDFIOTARGET while it might have been
    //    removed or be in process of being removed.
    //
    ntStatus = SerialTarget_Reference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        // Module is closing, closed or QueryRemove has started.
        //
        goto Exit;
    }

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

    SerialTarget_Dereference(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
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

    STATUS_SUCCESS if the stream was successfully started.
    Otherwise NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SerialTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 SerialTarget);

    // 1. Prevent callers from calling Methods when the Module is Closed.
    // 2. Prevent external callers from accessing WDFIOTARGET while it might have been
    //    removed or be in process of being removed.
    //
    ntStatus = SerialTarget_Reference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        // Module is closing, closed or QueryRemove has started.
        //
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->IoTarget != NULL);

    ntStatus = SerialTarget_StreamStart(DmfModule);

    SerialTarget_Dereference(DmfModule);

Exit:

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
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 SerialTarget);

    // 1. Prevent callers from calling Methods when the Module is Closed.
    // 2. Prevent external callers from accessing WDFIOTARGET while it might have been
    //    removed or be in process of being removed.
    //
    ntStatus = SerialTarget_Reference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        // Module is closing, closed or QueryRemove has started.
        //
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->IoTarget != NULL);

    SerialTarget_StreamStop(DmfModule,
                            StreamingState_Stopped);

    SerialTarget_Dereference(DmfModule);

Exit:

    FuncExitVoid(DMF_TRACE);
}

// eof: Dmf_SerialTarget.c
//
