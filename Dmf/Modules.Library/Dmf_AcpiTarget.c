/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_AcpiTarget.c

Abstract:

    Supports invoking methods in ASL code using ACPI.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_AcpiTarget.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// This Module has no Context.
//
DMF_MODULE_DECLARE_NO_CONTEXT(AcpiTarget)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(AcpiTarget)

// Memory Pool Tag.
//
#define MemoryTag 'oMTA'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define Add2Ptr(Pointer, Increment) ((VOID*)((UCHAR*)(Pointer) + (Increment)))

#define DSM_METHOD                                          (ULONG) ('MSD_')
#if !defined(ACPI_EVAL_INPUT_BUFFER_COMPLEX_SIGNATURE)
#define ACPI_EVAL_INPUT_BUFFER_COMPLEX_SIGNATURE            'CieA'
#endif // !defined(ACPI_EVAL_INPUT_BUFFER_COMPLEX_SIGNATURE)
#define DSM_QUERY_FUNCTION_INDEX                            0
#define DSM_METHOD_ARGUMENTS_COUNT                          4
#define INITIAL_CONTROL_METHOD_OUTPUT_SIZE                  (0x200)

#define NUMBER_OF_REALLOCATIONS_ALLOWED_IF_BUFFER_OVERFLOW  2

#pragma code_seg("PAGE")

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
AcpiTarget_PrepareInputParametersForDsmMethod(
    _In_ GUID* Guid,
    _In_ ULONG FunctionIndex,
    _In_ ULONG FunctionRevision,
    __in_bcount_opt(FunctionCustomArgumentsBufferSize) VOID* FunctionCustomArgumentsBuffer,
    _In_ ULONG FunctionCustomArgumentsBufferSize,
    __deref_out_bcount(*ReturnBufferSize) PACPI_EVAL_INPUT_BUFFER_COMPLEX *ReturnBuffer,
    _Out_opt_ ULONG* ReturnBufferSize
    )
/*++

Routine Description:

    This routine is invoked for initializing an input parameter blob
    for evaluation of a _DSM control method defined in ASL code.

Arguments:

    Guid - ACPI Guid.
    FunctionIndex - Supplies the value of a function index for an input parameter.
    FunctionRevision - Supplies the version of the function.
    FunctionCustomArgumentsBuffer - Supplies the buffer containing custom arguments to be passed to the function.
    FunctionCustomArgumentsBufferSize - Supplies the size of the custom arguments buffer.
    ReturnBuffer - Supplies a pointer to receive the input parameter blob.
    ReturnBufferSize - Supplies a pointer to receive the size of the data blob returned.

Return Value:

    NTSTATUS

--*/
{
    PACPI_METHOD_ARGUMENT argument;
    PACPI_EVAL_INPUT_BUFFER_COMPLEX parametersBuffer;
    ULONG parametersBufferSize;
    NTSTATUS ntStatus;
    errno_t error;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    parametersBufferSize = sizeof(ACPI_EVAL_INPUT_BUFFER_COMPLEX) +
                           (sizeof(GUID) - sizeof(ULONG)) +
                           (sizeof(ACPI_METHOD_ARGUMENT) *
                           (DSM_METHOD_ARGUMENTS_COUNT - 1));

    // Check if additional memory for Arg3 is needed. 
    // ULONG-size argument buffer is already present in ACPI_METHOD_ARGUMENT.
    //
    if (FunctionCustomArgumentsBufferSize > sizeof(ULONG))
    {
        parametersBufferSize += (FunctionCustomArgumentsBufferSize - sizeof(ULONG));
    }

    parametersBuffer = (ACPI_EVAL_INPUT_BUFFER_COMPLEX*)ExAllocatePoolWithTag(PagedPool,
                                                                              parametersBufferSize,
                                                                              MemoryTag);
    if (NULL == parametersBuffer)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ExAllocatePoolWithTag ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    RtlZeroMemory(parametersBuffer,
                  parametersBufferSize);
    parametersBuffer->Signature = ACPI_EVAL_INPUT_BUFFER_COMPLEX_SIGNATURE;
    parametersBuffer->MethodNameAsUlong = (ULONG)DSM_METHOD;
    parametersBuffer->Size = parametersBufferSize;
    parametersBuffer->ArgumentCount = DSM_METHOD_ARGUMENTS_COUNT;

    //
    // Argument 0: UUID.
    //

    argument = &parametersBuffer->Argument[0];
    ACPI_METHOD_SET_ARGUMENT_BUFFER(argument,
                                    Guid,
                                    sizeof(GUID));

    //
    // Argument 1: Revision.
    //

    argument = ACPI_METHOD_NEXT_ARGUMENT(argument);
    ACPI_METHOD_SET_ARGUMENT_INTEGER(argument, FunctionRevision);

    //
    // Argument 2: Function index.
    //

    argument = ACPI_METHOD_NEXT_ARGUMENT(argument);
    ACPI_METHOD_SET_ARGUMENT_INTEGER(argument, FunctionIndex);

    //
    // Argument 3: Custom package for DSM functions.
    //

    argument = ACPI_METHOD_NEXT_ARGUMENT(argument);
    argument->Type = ACPI_METHOD_ARGUMENT_PACKAGE;
    argument->DataLength = (USHORT)FunctionCustomArgumentsBufferSize;
    argument->Argument = 0;

    if (FunctionCustomArgumentsBufferSize > 0)
    {
        ASSERT(ARGUMENT_PRESENT(FunctionCustomArgumentsBuffer));

        error = memcpy_s(&argument->Data[0],
                         argument->DataLength,
                         FunctionCustomArgumentsBuffer,
                         FunctionCustomArgumentsBufferSize);
        if (error)
        {
            ntStatus = STATUS_UNSUCCESSFUL;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "memcpy_s fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    *ReturnBuffer = parametersBuffer;
    if (ARGUMENT_PRESENT(ReturnBufferSize) != FALSE)
    {
        *ReturnBufferSize = parametersBufferSize;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
AcpiTarget_EvaluateAcpiMethod(
    _In_ WDFDEVICE Device,
    _In_ ULONG MethodName,
    _In_opt_ VOID* InputBuffer,
    __deref_out_bcount_opt(*ReturnBufferSize) VOID* *ReturnBuffer,
    _Out_opt_ ULONG* ReturnBufferSize,
    _In_ ULONG Tag
    )
/*

Routine Description:

    This function sends an IRP to ACPI to evaluate a method.
    ACPI must be in the device stack (either as a bus or filter driver).

Arguments:

    Device - Supplies a handle to the framework device object.
    MethodName - Supplies a packed string identifying the method.
    InputBuffer - Supplies arguments for the method. If specified, the method
                      name must match MethodName.
    ReturnBuffer - Supplies a pointer to receive the return value(s) from
                   the method.
    ReturnBufferSize - Supplies a pointer to receive the size of the data
                       returned.
    Tag - Identifies memory allocation source

Return Value:

    NTSTATUS

--*/
{
    UCHAR attempts;
    PACPI_EVAL_INPUT_BUFFER inputBuffer;
    ULONG inputBufferLength;
    WDF_MEMORY_DESCRIPTOR inputDescriptor;
    WDFIOTARGET ioTarget;
    PACPI_EVAL_OUTPUT_BUFFER outputBuffer;
    ULONG outputBufferLength;
    WDF_MEMORY_DESCRIPTOR outputDescriptor;
    ULONG_PTR sizeReturned;
    ACPI_EVAL_INPUT_BUFFER smallInputBuffer;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    outputBuffer = NULL;

    //
    // Build input buffer if one was not passed in.
    //

    if (NULL == InputBuffer)
    {
        if (0 == MethodName)
        {
            ntStatus = STATUS_INVALID_PARAMETER_1;
            goto Exit;
        }

        smallInputBuffer.Signature = ACPI_EVAL_INPUT_BUFFER_SIGNATURE;
        smallInputBuffer.MethodNameAsUlong = MethodName;

        inputBuffer = &smallInputBuffer;
        inputBufferLength = sizeof(ACPI_EVAL_INPUT_BUFFER);
    }
    else
    {
        inputBuffer = (ACPI_EVAL_INPUT_BUFFER *)InputBuffer;

        //
        // Calculate input buffer size.
        //

        switch (((PACPI_EVAL_INPUT_BUFFER)InputBuffer)->Signature)
        {
            case ACPI_EVAL_INPUT_BUFFER_SIGNATURE:
            {
                inputBufferLength = sizeof(ACPI_EVAL_INPUT_BUFFER);
                break;
            }
            case ACPI_EVAL_INPUT_BUFFER_SIMPLE_INTEGER_SIGNATURE:
            {
                inputBufferLength = sizeof(ACPI_EVAL_INPUT_BUFFER_SIMPLE_INTEGER);
                break;
            }
            case ACPI_EVAL_INPUT_BUFFER_SIMPLE_STRING_SIGNATURE:
            {
                inputBufferLength = sizeof(ACPI_EVAL_INPUT_BUFFER_SIMPLE_STRING) +
                                    ((PACPI_EVAL_INPUT_BUFFER_SIMPLE_STRING)InputBuffer)->StringLength - 1;
                break;
            }
            case ACPI_EVAL_INPUT_BUFFER_COMPLEX_SIGNATURE:
            {
                inputBufferLength = ((PACPI_EVAL_INPUT_BUFFER_COMPLEX)InputBuffer)->Size;
                break;
            }
            default:
            {
                ASSERT(FALSE);
                ntStatus = STATUS_INVALID_PARAMETER_2;
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Signature ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
        }
    }

    //
    // Set the IO target and initial size for the output buffer to be allocated.
    // The IO target is the default underlying device object, which is ACPI
    // in this case.
    //

    attempts = 0;
    ioTarget = WdfDeviceGetIoTarget(Device);
    outputBufferLength = INITIAL_CONTROL_METHOD_OUTPUT_SIZE;

    //
    // Set the input buffer.
    //

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputDescriptor,
                                      (VOID*)inputBuffer,
                                      inputBufferLength);

    do
    {
        outputBuffer = (PACPI_EVAL_OUTPUT_BUFFER)ExAllocatePoolWithTag(PagedPool,
                                                                       outputBufferLength,
                                                                       Tag);
        if (NULL == outputBuffer)
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto Exit;
        }

        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor,
                                          (VOID*)outputBuffer,
                                          outputBufferLength);

        ntStatus = WdfIoTargetSendIoctlSynchronously(ioTarget,
                                                     NULL,
                                                     IOCTL_ACPI_EVAL_METHOD,
                                                     &inputDescriptor,
                                                     &outputDescriptor,
                                                     NULL,
                                                     &sizeReturned);

        //
        // If the output buffer was insufficient, then re-allocate one with
        // appropriate size and retry.
        //

        if (STATUS_BUFFER_OVERFLOW == ntStatus)
        {
            outputBufferLength = outputBuffer->Length;
            ExFreePoolWithTag(outputBuffer,
                              Tag);
            outputBuffer = NULL;
        }

        attempts += 1;
    }
    while ((ntStatus == STATUS_BUFFER_OVERFLOW) &&
           (attempts < NUMBER_OF_REALLOCATIONS_ALLOWED_IF_BUFFER_OVERFLOW));

    // If successful and data returned, return data to caller. If the method
    // returned no data, then set the return values to NULL.
    //

    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (sizeReturned > 0)
    {
        ASSERT(sizeReturned >=
               sizeof(ACPI_EVAL_OUTPUT_BUFFER) - sizeof(ACPI_METHOD_ARGUMENT));

        ASSERT(outputBuffer->Signature == ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE);

        if (ARGUMENT_PRESENT(ReturnBuffer))
        {
            *ReturnBuffer = outputBuffer;
            outputBuffer = NULL;
        }
        if (ARGUMENT_PRESENT(ReturnBufferSize) != FALSE)
        {
            *ReturnBufferSize = (ULONG)sizeReturned;
        }
    }
    else
    {
        *ReturnBuffer = NULL;
        if (ARGUMENT_PRESENT(ReturnBufferSize) != FALSE)
        {
            *ReturnBufferSize = 0;
        }
    }

    //
    // If the method execution fails: then free up the resources.
    //

Exit:

    if (outputBuffer != NULL)
    {
        ExFreePoolWithTag(outputBuffer,
                          Tag);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
AcpiTarget_IsDsmFunctionSupported(
    _In_ GUID* Guid,
    _In_ WDFDEVICE Device,
    _In_ ULONG FunctionIndex,
    _In_ ULONG FunctionRevision,
    __in_bcount_opt(FunctionCustomArgumentBufferSize) VOID* FunctionCustomArgumentBuffer,
    _In_ ULONG FunctionCustomArgumentBufferSize,
    _Out_ PBOOLEAN Supported
    )
/*++

Routine Description:

    This routine is invoked to check if a specific function for a _DSM method
    is supported.

Arguments:

    Device - Supplies a handle to the framework device object.
    FunctionIndex - Supplies the function index to check for.
    FunctionRevision - Indicates the revision of the function of interest.
    FunctionCustomArgumentsBuffer - Supplies the buffer containing custom arguments to be passed to the function.
    FunctionCustomArgumentsBufferSize - Supplies the size of the custom arguments buffer.
    Supported - Supplies a pointer to a boolean that on return will indicate if the given function is supported.

Return Value:

    STATUS_OBJECT_NAME_NOT_FOUND if the _DSM method does not exist. This status
    may be returned for other reasons as well.

    STATUS_INSUFFICIENT_RESOURCES on failure to allocate memory.

    STATUS_SUCCESS if the method evaluates correctly and the output parameter
    if formatted correctly.

    Otherwise a status code from a function call.

--*/
{
    PACPI_EVAL_OUTPUT_BUFFER outputBuffer;
    ULONG outputBufferSize;
    PACPI_EVAL_INPUT_BUFFER_COMPLEX parametersBuffer;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    outputBuffer = NULL;
    parametersBuffer = NULL;

    *Supported = FALSE;

    // When querying, the query is phrased in terms of "give me an array of bits
    // where each bit represents a function for which there is support at this
    // revision level."  I.e. "Revision 1" could return an entirely disjoint
    // bit field than "Revision 2.".
    //
    ntStatus = AcpiTarget_PrepareInputParametersForDsmMethod(Guid,
                                                             DSM_QUERY_FUNCTION_INDEX,
                                                             FunctionRevision,
                                                             FunctionCustomArgumentBuffer,
                                                             FunctionCustomArgumentBufferSize,
                                                             &parametersBuffer,
                                                             NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        parametersBuffer = NULL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "AcpiTarget_PrepareInputParametersForDsmMethod ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Invoke a helper function to send an IOCTL to ACPI to evaluate this
    // control method.
    //
    ntStatus = AcpiTarget_EvaluateAcpiMethod(Device,
                                             parametersBuffer->MethodNameAsUlong,
                                             parametersBuffer,
                                             (VOID* *)&outputBuffer,
                                             &outputBufferSize,
                                             MemoryTag);

    // N.B. If _DSM does not exist, STATUS_OBJECT_NAME_NOT_FOUND is returned
    // here.
    //
    if (! NT_SUCCESS(ntStatus))
    {
        outputBuffer = NULL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "AcpiTarget_EvaluateAcpiMethod ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Exactly one Buffer must be returned.
    //
    if ((NULL == outputBuffer) ||
        (outputBuffer->Count != 1))
    {
        goto Exit;
    }

    // The Buffer must contain at least one bit. 
    // Rounding up implies that it must contain at least one byte.
    //
    ASSERT(outputBufferSize >= sizeof(ACPI_EVAL_OUTPUT_BUFFER));

    if ((outputBuffer->Argument[0].Type != ACPI_METHOD_ARGUMENT_BUFFER ||
        (outputBuffer->Argument[0].DataLength == 0)))
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ACPI_EVAL_OUTPUT_BUFFER ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

    if ((FunctionIndex / 8) < outputBuffer->Argument[0].DataLength)
    {
        if ((outputBuffer->Argument[0].Data[FunctionIndex / 8] &
            (1 << (FunctionIndex % 8))) != 0)
        {
            *Supported = TRUE;
        }
    }

Exit:

    if (parametersBuffer != NULL)
    {
        ExFreePoolWithTag(parametersBuffer,
                          MemoryTag);
        parametersBuffer = NULL;
    }

    if (outputBuffer != NULL)
    {
        ExFreePoolWithTag(outputBuffer,
                          MemoryTag);
        outputBuffer = NULL;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
AcpiTarget_InvokeDsm(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG FunctionIndex,
    __in_bcount_opt(FunctionCustomArgumentsBufferSize) VOID* FunctionCustomArgumentsBuffer,
    _In_ ULONG FunctionCustomArgumentsBufferSize,
    __deref_opt_out_bcount_opt(*ReturnBufferSize) VOID** ReturnBuffer,
    _Out_opt_ ULONG* ReturnBufferSize,
    _In_ ULONG Tag
    )
/*

Routine Description:

    Invoke a DSM (Device Specific Method) and return any data from the method
    to the caller.

Arguments:

    DmfModule - This Module's handle.
    FunctionIndex - DSM Function Index.
    FunctionCustomArgumentsBuffer - DSM Function Custom Arguments buffer.
    FunctionCustomArgumentsBufferSize - The size of the Custom Arguments buffer.
    ReturnBuffer - The data returned by the DSM.
    ReturnBufferSize - Indicates how much data is returned to the Client.
    Tag - Identifies memory allocation source

Return Value:

    NTSTATUS
    On STATUS_SUCCESS, caller needs to free *ReturnBuffer.

--*/
{
    NTSTATUS ntStatus;
    BOOLEAN supported;
    PACPI_EVAL_OUTPUT_BUFFER outputBuffer;
    ULONG outputBufferSize;
    PACPI_EVAL_INPUT_BUFFER_COMPLEX parametersBuffer;
    DMF_CONFIG_AcpiTarget* moduleConfig;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    outputBuffer = NULL;
    parametersBuffer = NULL;
    if (ARGUMENT_PRESENT(ReturnBuffer) != FALSE)
    {
        *ReturnBuffer = NULL;
    }
    if (ARGUMENT_PRESENT(ReturnBufferSize) != FALSE)
    {
        *ReturnBufferSize = 0;
    }

    ntStatus = AcpiTarget_IsDsmFunctionSupported(&moduleConfig->Guid,
                                                 device,
                                                 FunctionIndex,
                                                 moduleConfig->DsmRevision,
                                                 FunctionCustomArgumentsBuffer,
                                                 FunctionCustomArgumentsBufferSize,
                                                 &supported);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to check if _DSM method is supported.");
        goto Exit;
    }

    if (supported == FALSE)
    {
        ntStatus = STATUS_NOT_SUPPORTED;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "_DSM method is not supported for Revision: %d.", moduleConfig->DsmRevision);
        goto Exit;
    }

    // Evaluate this method for real.
    //
    ntStatus = AcpiTarget_PrepareInputParametersForDsmMethod(&moduleConfig->Guid,
                                                             FunctionIndex,
                                                             moduleConfig->DsmRevision,
                                                             FunctionCustomArgumentsBuffer,
                                                             FunctionCustomArgumentsBufferSize,
                                                             &parametersBuffer,
                                                             NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        parametersBuffer = NULL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to prepare input parameters for _DSM call.");
        goto Exit;
    }

    // Invoke a helper function to send an IOCTL to ACPI to evaluate this
    // control method.
    //
    ntStatus = AcpiTarget_EvaluateAcpiMethod(device,
                                             parametersBuffer->MethodNameAsUlong,
                                             parametersBuffer,
                                             (VOID* *)&outputBuffer,
                                             &outputBufferSize,
                                             Tag);
    if (! NT_SUCCESS(ntStatus))
    {
        outputBuffer = NULL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to evaluate _DSM method.");
        goto Exit;
    }

    if (outputBufferSize > 0 && outputBuffer != NULL)
    {
        ASSERT(outputBufferSize >=
               sizeof(ACPI_EVAL_OUTPUT_BUFFER) - sizeof(ACPI_METHOD_ARGUMENT));

        ASSERT(outputBuffer->Signature == ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE);

        if (ARGUMENT_PRESENT(ReturnBuffer) != FALSE)
        {
            *ReturnBuffer = outputBuffer;
            // Caller will free *ReturnBuffer
            //
            outputBuffer = NULL;
        }

        if (ARGUMENT_PRESENT(ReturnBufferSize) != FALSE)
        {
            *ReturnBufferSize = (ULONG)outputBufferSize;
        }
    }

Exit:

    if (parametersBuffer != NULL)
    {
        ExFreePoolWithTag(parametersBuffer, MemoryTag);
        parametersBuffer = NULL;
    }

    if (outputBuffer != NULL)
    {
        ExFreePoolWithTag(outputBuffer, Tag);
        outputBuffer = NULL;
    }

    FuncExitVoid(DMF_TRACE);

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_AcpiTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type AcpiTarget.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_AcpiTarget;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_MODULE_DESCRIPTOR_INIT(dmfModuleDescriptor_AcpiTarget,
                               AcpiTarget,
                               DMF_MODULE_OPTIONS_PASSIVE,
                               DMF_MODULE_OPEN_OPTION_OPEN_Create);

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_AcpiTarget,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}

// Module Methods
//

_Must_inspect_result_
__drv_requiresIRQL(PASSIVE_LEVEL)
// 'ReturnBuffer' could be '0':  this does not adhere to the specification for the function 'AcpiTarget_EvaluateAcpiMethod'.
//
#pragma warning(disable:6387)
NTSTATUS
DMF_AcpiTarget_EvaluateMethod(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG MethodName,
    _In_opt_ VOID* InputBuffer,
    __deref_opt_out_bcount_opt(*ReturnBufferSize) VOID** ReturnBuffer,
    _Out_opt_ ULONG* ReturnBufferSize,
    _In_ ULONG Tag
    )
/*

Routine Description:

    This function sends an IRP to ACPI to evaluate a method.
    ACPI must be in the device stack (either as a bus or filter driver).

Arguments:

    DmfModule - This Module's Module handle.
    MethodName - Supplies a packed string identifying the method.
    InputBuffer - Supplies arguments for the method. If specified, the method
                      name must match MethodName.
    ReturnBuffer - Supplies a pointer to receive the return value(s) from
                   the method.
    ReturnBufferSize - Supplies a pointer to receive the size of the data
                       returned.
    Tag - Identifies memory allocation source

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = AcpiTarget_EvaluateAcpiMethod(DMF_ParentDeviceGet(DmfModule),
                                             MethodName,
                                             InputBuffer,
                                             ReturnBuffer,
                                             ReturnBufferSize,
                                             Tag);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_AcpiTarget_InvokeDsm(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG FunctionIndex,
    _In_ ULONG FunctionCustomArgument,
    _Out_opt_ VOID* ReturnBuffer,
    _Inout_opt_ ULONG* ReturnBufferSize
    )
/*

Routine Description:

    Invoke a DSM (Device Specific Method) and optionally return a buffer to the caller.

Arguments:

    DmfModule - This Module's handle.
    FunctionIndex - DSM Function Index.
    FunctionCustomArgument - DSM Function Custom Argument.
    ReturnBuffer - Pointer to a buffer containing data returned by the DSM.
    ReturnBufferSize - Size of ReturnBuffer on input, size of the data returned by the DSM on output.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    PACPI_EVAL_OUTPUT_BUFFER outputBuffer;
    ULONG outputBufferSize;
    PACPI_METHOD_ARGUMENT argument;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 AcpiTarget);

    outputBuffer = NULL;
    outputBufferSize = 0;
    ntStatus = AcpiTarget_InvokeDsm(DmfModule,
                                    FunctionIndex,
                                    &FunctionCustomArgument,
                                    sizeof(FunctionCustomArgument),
                                    (VOID* *)&outputBuffer,
                                    &outputBufferSize,
                                    MemoryTag);

    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    if (ReturnBufferSize != NULL && ReturnBuffer != NULL)
    {
        // Receive data from the DSM call. 
        //
        if (outputBufferSize > 0 && outputBuffer != NULL)
        {
            ASSERT(Add2Ptr(&outputBuffer->Argument[0],
                           outputBuffer->Count * sizeof(ACPI_METHOD_ARGUMENT)) <=
                   Add2Ptr(outputBuffer,
                           outputBuffer->Length));

            argument = &outputBuffer->Argument[0];

            if ((outputBuffer->Count == 0) ||
                ((argument->Type != ACPI_METHOD_ARGUMENT_INTEGER) && (argument->Type != ACPI_METHOD_ARGUMENT_BUFFER)))
            {
                ntStatus = STATUS_UNSUCCESSFUL;
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "_DSM returned data type invalid!"
                            "Count = %u, Type = 0x%x",
                            outputBuffer->Count,
                            outputBuffer->Argument[0].Type);
            }
            else if (*ReturnBufferSize < argument->DataLength)
            {
                ntStatus = STATUS_BUFFER_TOO_SMALL;
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Output buffer size is too small, "
                            "Size = %u, Required = %u",
                            *ReturnBufferSize,
                            argument->DataLength);
            }
            else
            {
                RtlCopyMemory(ReturnBuffer,
                              argument->Data,
                              argument->DataLength);
                *ReturnBufferSize = argument->DataLength;
            }
        }
        else
        {
            // Either OutputBuffer or OutputBufferSize has invalid value
            //
            ntStatus = STATUS_UNSUCCESSFUL;
            ASSERT(FALSE);
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "_DSM returned data type invalid!"
                        "outputBuffer=0x%p, outputBufferSize=%d",
                        outputBuffer,
                        outputBufferSize);
        }
    }
    else if (ReturnBufferSize != NULL)
    {
        // For SAL. If there is a return buffer size there must be a return
        // buffer so this is invalid code path.
        //
        ASSERT(FALSE);
        *ReturnBufferSize = 0;
    }
    else if (ReturnBuffer != NULL)
    {
        // For SAL. ReturnBufferSize is invalid. Cannot use ReturnBuffer.
        //
        ASSERT(FALSE);
        *(UCHAR *)ReturnBuffer = 0;
    }

Exit:

    if (outputBuffer != NULL)
    {
        ExFreePoolWithTag(outputBuffer,
                          MemoryTag);
        outputBuffer = NULL;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_AcpiTarget_InvokeDsmRaw(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG FunctionIndex,
    _In_ ULONG FunctionCustomArgument,
    _Out_writes_opt_(*ReturnBufferSize) VOID** ReturnBuffer,
    _Out_opt_ ULONG* ReturnBufferSize,
    _In_ ULONG Tag
    )
/*

Routine Description:

    Invoke a DSM (Device Specific Method) and return the raw ACPI output buffer to the caller.

Arguments:

    DmfModule - This Module's handle.
    FunctionIndex - DSM Function Index.
    FunctionCustomArgument - DSM Function Custom Argument.
    ReturnBuffer - Pointer to a buffer containing raw ACPI output buffer.
    ReturnBufferSize - Size of data returned by the DSM.
    Tag - Indicates memory allocation source

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    if (ARGUMENT_PRESENT(ReturnBufferSize))
    {
        *ReturnBufferSize = 0;
    }

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 AcpiTarget);

    ntStatus = AcpiTarget_InvokeDsm(DmfModule,
                                    FunctionIndex,
                                    &FunctionCustomArgument,
                                    sizeof(FunctionCustomArgument),
                                    ReturnBuffer,
                                    ReturnBufferSize,
                                    Tag);

    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_AcpiTarget_InvokeDsmWithCustomBuffer(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG FunctionIndex,
    _In_reads_opt_(FunctionCustomArgumentsBufferSize) VOID* FunctionCustomArgumentsBuffer,
    _In_ ULONG FunctionCustomArgumentsBufferSize
    )
/*

Routine Description:

    Invoke a DSM (Device Specific Method) passing a buffer of arbitrary size as a custom arguments.

Arguments:

    DmfModule - This Module's handle.
    FunctionIndex - DSM Function Index.
    FunctionCustomArgumentsBuffer - DSM Function Custom Arguments buffer.
    FunctionCustomArgumentsBufferSize - The size of the Custom Arguments buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 AcpiTarget);

    ntStatus = AcpiTarget_InvokeDsm(DmfModule,
                                    FunctionIndex,
                                    FunctionCustomArgumentsBuffer,
                                    FunctionCustomArgumentsBufferSize,
                                    NULL,
                                    NULL,
                                    MemoryTag);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// eof: Dmf_AcpiTarget.c
//
