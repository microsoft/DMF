/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_AcpiPepDeviceFan.c

Abstract:

    Support for creating a Virtual ACPI Fan using MS PEP (Platform Extension Plugin).

Environment:

    Kernel-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_AcpiPepDeviceFan.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_AcpiPepDeviceFan
{
    // Table containing fan ACPI name and parameters.
    //
    PEP_DEVICE_MATCH PepDeviceMatchArray;
    // Table containing all callbacks registered by this PEP device.
    //
    PEP_DEVICE_DEFINITION PepDeviceDefinitionArray;
    // High and low trip points for Fan trigger.
    //
    AcpiPepDeviceFan_TRIP_POINT TripPoint;
    // Handle to Fan's PEP device used for AcpiNotify.
    //
    struct _PEP_INTERNAL_DEVICE_HEADER* FanPepInternalDevice;
    // Keeps track of Fan state, gets set to TRUE once ACPI Initialize
    // takes place.
    //
    BOOLEAN FanInitialized;
} DMF_CONTEXT_AcpiPepDeviceFan;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(AcpiPepDeviceFan)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(AcpiPepDeviceFan)

// MemoryTag.
//
#define MemoryTag 'FDPA'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Defines for FAN _DSM calls.
//
#define FAN_DSM_REVISION                  0
#define FAN_DSM_FUNC_SUPPORT_INDEX        0
#define FAN_DSM_CAPABILITY_INDEX          1
#define FAN_DSM_TRIPPOINT_FUNCTION_INDEX  2
#define FAN_DSM_RANGE_FUNCTION_INDEX      3

// Unique type assigned to device of type Fan.
//
#define PEP_DEVICE_TYPE_FAN0 \
    PEP_MAKE_DEVICE_TYPE(PepMajorDeviceTypeAcpi, \
                         PepAcpiMinorTypeDevice, \
                         0x1)

NTSTATUS
AcpiPepDeviceFan_InitializeCallback(
    _In_ DMFMODULE DmfModule,
    _In_ struct _PEP_INTERNAL_DEVICE_HEADER* PepInternalDevice
    )
/*++

Routine Description:

    This routine handles Device Initialize callback. Fan uses it to
    store the InternalDevice in context for future AcpiNotify requests.

Arguments:

    PepInternalDevice - Internal device initialized by the OS.

Return Value:

    NTSTATUS.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_AcpiPepDeviceFan* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ntStatus = STATUS_SUCCESS;

    // Store PepInternalDevice object for notify operations.
    //
    moduleContext->FanPepInternalDevice = PepInternalDevice;
    moduleContext->FanInitialized = TRUE;

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "Fan Device: Ready for notifications.");

    return ntStatus;
}

PEP_NOTIFICATION_HANDLER_RESULT
AcpiPepDeviceFan_SyncEvaluateControlMethod(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Data,
    _Out_opt_ PEP_WORK_INFORMATION* PoFxWorkInformation
    )
/*++

Routine Description:

    This routine handles PEP_NOTIFY_ACPI_EVALUATE_CONTROL_METHOD
    notification for the bus device. It will try to evaluate the
    _HID method synchronously, while leaving _CID to the asynchronous
    method.

Arguments:

    DmfModule - This Module's handle.
    Data - Supplies a pointer to parameters buffer for this notification.
    PoFxWorkInformation - Unused.

Return Value:

    None.

--*/
{
    PEP_NOTIFICATION_HANDLER_RESULT completeStatus;
    PPEP_ACPI_EVALUATE_CONTROL_METHOD ecmBuffer;
    PACPI_METHOD_ARGUMENT inputArgument;
    PACPI_METHOD_ARGUMENT outputArgument;
    DMF_CONTEXT_AcpiPepDeviceFan* moduleContext;
    DMF_CONFIG_AcpiPepDeviceFan* moduleConfig;
    LPGUID dsmUUID;
    ULONG functionIndex;
    ULONG revisionLevel;
    ULONG uuidIndex;
    NTSTATUS ntStatus;
    UINT16 fanSpeed;

    UNREFERENCED_PARAMETER(PoFxWorkInformation);

    TraceEvents(TRACE_LEVEL_VERBOSE,
                DMF_TRACE,
                "Evaluating Fan Methods.");

    ecmBuffer = (PPEP_ACPI_EVALUATE_CONTROL_METHOD)Data;
    completeStatus = PEP_NOTIFICATION_HANDLER_COMPLETE;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    switch (ecmBuffer->MethodName)
    {
        case ACPI_OBJECT_NAME_FST:

            TraceEvents(TRACE_LEVEL_INFORMATION,
                        DMF_TRACE,
                        "Host invoked FST call on Fan device.");

            fanSpeed = 0;
            ntStatus = moduleConfig->FanSpeedGet(DmfModule,
                                                 moduleConfig->FanInstanceIndex,
                                                 &fanSpeed,
                                                 sizeof(fanSpeed));
            if (NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_INFORMATION,DMF_TRACE, "Fan%d: Fan Speed=%d",
                            moduleConfig->FanInstanceIndex,
                            fanSpeed);
            }
            else
            {
                fanSpeed = 0;
                TraceEvents(TRACE_LEVEL_ERROR,DMF_TRACE, "Fan%d: fails: ntStatus=%!STATUS!", moduleConfig->FanInstanceIndex, ntStatus);
            }

            // _FST response expects a package with three integers - revision, control and fan speed.
            //
            ecmBuffer->OutputArguments->Type = ACPI_METHOD_ARGUMENT_PACKAGE;
            ecmBuffer->OutputArguments->DataLength = 3 * ACPI_METHOD_ARGUMENT_LENGTH(sizeof(ULONG));

            // Package entry 0 - Current revision - ACPI spec 6.4 states 0.
            //
            outputArgument = (PACPI_METHOD_ARGUMENT)ecmBuffer->OutputArguments->Data;
            ACPI_METHOD_SET_ARGUMENT_INTEGER(outputArgument,
                                             0);

            // Package entry 1 - control
            //
            outputArgument = ACPI_METHOD_NEXT_ARGUMENT(outputArgument);
            ACPI_METHOD_SET_ARGUMENT_INTEGER(outputArgument,
                                             fanSpeed);
            // Package entry 2 - fan speed (RPM)
            //
            outputArgument = ACPI_METHOD_NEXT_ARGUMENT(outputArgument);
            ACPI_METHOD_SET_ARGUMENT_INTEGER(outputArgument,
                                             fanSpeed);

            ecmBuffer->OutputArgumentCount = 1;
            ecmBuffer->OutputArgumentSize = ACPI_METHOD_ARGUMENT_LENGTH_FROM_ARGUMENT(ecmBuffer->OutputArguments);
            ecmBuffer->MethodStatus = STATUS_SUCCESS;
            completeStatus = PEP_NOTIFICATION_HANDLER_COMPLETE;
            break;

        case ACPI_OBJECT_NAME_DSM:
            // Test device 3 defines 2 DSMs.
            //
            // _DSM requires 4 input arguments.
            //
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        DMF_TRACE,
                        "Host invoked DSM call on Fan device.");

            ecmBuffer->OutputArgumentCount = 0;
            ecmBuffer->OutputArgumentSize = 0;
            if (ecmBuffer->InputArgumentCount != 4)
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "%s <TST3>: Invalid number of DSM input arguments."
                            "Required = 4, Provided = %d.\n",
                            __FUNCTION__,
                            ecmBuffer->InputArgumentCount);

                ecmBuffer->MethodStatus = STATUS_INVALID_PARAMETER;
                goto Exit;
            }

            // The first argument must be the UUID of the DSM.
            //
            inputArgument = ecmBuffer->InputArguments;
            if (inputArgument->Type != ACPI_METHOD_ARGUMENT_BUFFER)
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "%s <TST3>: Invalid type of the first DSM argument."
                            "Required = ACPI_METHOD_ARGUMENT_BUFFER, "
                            "Provided = %d.\n",
                            __FUNCTION__,
                            inputArgument->Type);

                ecmBuffer->MethodStatus = STATUS_INVALID_PARAMETER_1;
                goto Exit;
            }

            if (inputArgument->DataLength != (USHORT)(sizeof(GUID)))
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "%s <TST3>: Invalid size of the first DSM argument."
                            "Required = %d, Provided = %d.\n",
                            __FUNCTION__,
                            (ULONG)(sizeof(GUID)),
                            (ULONG)(inputArgument->DataLength));

                ecmBuffer->MethodStatus = STATUS_INVALID_PARAMETER_1;
                goto Exit;
            }

            dsmUUID = (LPGUID)(&inputArgument->Data[0]);

            // The second argument must be the revision level.
            //
            inputArgument = ACPI_METHOD_NEXT_ARGUMENT(inputArgument);
            if (inputArgument->Type != ACPI_METHOD_ARGUMENT_INTEGER)
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "%s <TST3>: Invalid type of the second DSM argument."
                            "Required = ACPI_METHOD_ARGUMENT_INTEGER, "
                            "Provided = %d.\n",
                            __FUNCTION__,
                            inputArgument->Type);

                ecmBuffer->MethodStatus = STATUS_INVALID_PARAMETER_2;
                goto Exit;
            }

            revisionLevel = inputArgument->Argument;

            // The third argument must be function index.
            //
            inputArgument = ACPI_METHOD_NEXT_ARGUMENT(inputArgument);
            if (inputArgument->Type != ACPI_METHOD_ARGUMENT_INTEGER)
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "%s <TST3>: Invalid type of the third DSM argument."
                            "Required = ACPI_METHOD_ARGUMENT_INTEGER, "
                            "Provided = %d.\n",
                            __FUNCTION__,
                            inputArgument->Type);

                ecmBuffer->MethodStatus = STATUS_INVALID_PARAMETER_3;
                goto Exit;
            }

            functionIndex = inputArgument->Argument;

            // The fourth argument must be a package.
            //
            inputArgument = ACPI_METHOD_NEXT_ARGUMENT(inputArgument);
            if (inputArgument->Type != ACPI_METHOD_ARGUMENT_PACKAGE &&
                inputArgument->Type != ACPI_METHOD_ARGUMENT_PACKAGE_EX)
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "%s <TST3>: Invalid type of the fourth DSM argument."
                            "Required = ACPI_METHOD_ARGUMENT_PACKAGE(_EX), "
                            "Provided = %d.\n",
                            __FUNCTION__,
                            inputArgument->Type);

                ecmBuffer->MethodStatus = STATUS_INVALID_PARAMETER_4;
                goto Exit;
            }

            // Check the first input argument to decide which UUID is called, and
            // invoke the corresponding methods.
            //
            uuidIndex = 0;
            if (RtlCompareMemory(dsmUUID,
                                 &moduleConfig->FanDsmGuid,
                                 sizeof(GUID)) == 0)
            {
                uuidIndex = 0;
            }
            else
            {
                uuidIndex = 1;
            }

            // Now check for Fan specific parameters.
            //
            if (uuidIndex == 1)
            {
                if (functionIndex == FAN_DSM_TRIPPOINT_FUNCTION_INDEX)
                {
                    ULONG low;
                    ULONG high;
                    PACPI_METHOD_ARGUMENT functionArguments;

                    functionArguments = ((PACPI_METHOD_ARGUMENT)(&inputArgument->Data[0]));

                    if (functionArguments->Type != ACPI_METHOD_ARGUMENT_INTEGER)
                    {
                        TraceEvents(TRACE_LEVEL_ERROR,
                                    DMF_TRACE,
                                    "%s: Unexpected package type for function argument - %d DSM function index 2",
                                    __FUNCTION__,
                                    functionArguments->Type);
                        ecmBuffer->MethodStatus = STATUS_INVALID_PARAMETER;
                        goto Exit;
                    }

                    low = functionArguments->Argument;
                    functionArguments = ACPI_METHOD_NEXT_ARGUMENT(functionArguments);

                    if (functionArguments->Type != ACPI_METHOD_ARGUMENT_INTEGER)
                    {
                        TraceEvents(TRACE_LEVEL_ERROR,
                                    DMF_TRACE,
                                    "%s: Unexpected package type for function argument - %d DSM function index 2",
                                    __FUNCTION__,
                                    functionArguments->Type);
                        ecmBuffer->MethodStatus = STATUS_INVALID_PARAMETER;
                        goto Exit;
                    }
                    high = functionArguments->Argument;

                    moduleContext->TripPoint.Low = (UINT16)low;
                    moduleContext->TripPoint.High =  (UINT16)high;
                    ntStatus = moduleConfig->FanTripPointsSet(DmfModule,
                                                              moduleConfig->FanInstanceIndex,
                                                              moduleContext->TripPoint);

                    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Fan%d: Fantrippoint low: %d, high: %d", moduleConfig->FanInstanceIndex, low, high);

                    if (!NT_SUCCESS(ntStatus))
                    {
                        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "SetFanTrippoints fails with ntstatus=%d", ntStatus);
                    }

                    DMF_AcpiPepDevice_PepAcpiDataReturn((void*)&ntStatus,
                                                        ACPI_METHOD_ARGUMENT_INTEGER,
                                                        sizeof(ULONG),
                                                        FALSE,
                                                        ecmBuffer->OutputArguments,
                                                        &ecmBuffer->OutputArgumentSize,
                                                        &ecmBuffer->OutputArgumentCount,
                                                        &ecmBuffer->MethodStatus,
                                                        "DSM1",
                                                        moduleConfig->FanInstanceName,
                                                        &completeStatus);
                }
                else if (functionIndex == FAN_DSM_CAPABILITY_INDEX)
                {
                    ULONG resolution;
                    resolution = moduleConfig->DsmFanCapabilityResolution;

                    DMF_AcpiPepDevice_PepAcpiDataReturn(&resolution,
                                                        ACPI_METHOD_ARGUMENT_INTEGER,
                                                        sizeof(ULONG),
                                                        FALSE,
                                                        ecmBuffer->OutputArguments,
                                                        &ecmBuffer->OutputArgumentSize,
                                                        &ecmBuffer->OutputArgumentCount,
                                                        &ecmBuffer->MethodStatus,
                                                        "DSM1",
                                                        moduleConfig->FanInstanceName,
                                                        &completeStatus);
                }
                else if (functionIndex == FAN_DSM_FUNC_SUPPORT_INDEX)
                {
                    UCHAR support[1];
                    support[0] = moduleConfig->DsmFunctionSupportIndex;

                    DMF_AcpiPepDevice_PepAcpiDataReturn(&support,
                                                        ACPI_METHOD_ARGUMENT_BUFFER,
                                                        (ULONG)(sizeof(support)),
                                                        FALSE,
                                                        ecmBuffer->OutputArguments,
                                                        &ecmBuffer->OutputArgumentSize,
                                                        &ecmBuffer->OutputArgumentCount,
                                                        &ecmBuffer->MethodStatus,
                                                        "DSM0",
                                                        moduleConfig->FanInstanceName,
                                                        &completeStatus);

                    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Fan%d: Granularity request", moduleConfig->FanInstanceIndex);
                }
                else if (functionIndex == FAN_DSM_RANGE_FUNCTION_INDEX)
                {
                    // _FST response expects a package with three integers - revision, control and fan speed.
                    //
                    ecmBuffer->OutputArguments->Type = ACPI_METHOD_ARGUMENT_PACKAGE;
                    ecmBuffer->OutputArguments->DataLength = AcpiPepDeviceFan_NumberOfFanRanges * ACPI_METHOD_ARGUMENT_LENGTH(sizeof(ULONG));

                    // Package entry 0
                    //
                    outputArgument = (PACPI_METHOD_ARGUMENT)ecmBuffer->OutputArguments->Data;
                    ACPI_METHOD_SET_ARGUMENT_INTEGER(outputArgument,
                                                     moduleConfig->DsmFanRange[AcpiPepDeviceFan_FanRangeIndex0]);

                    // Package entry 1
                    //
                    outputArgument = ACPI_METHOD_NEXT_ARGUMENT(outputArgument);
                    ACPI_METHOD_SET_ARGUMENT_INTEGER(outputArgument,
                                                     moduleConfig->DsmFanRange[AcpiPepDeviceFan_FanRangeIndex1]);

                    // Package entry 2
                    //
                    outputArgument = ACPI_METHOD_NEXT_ARGUMENT(outputArgument);
                    ACPI_METHOD_SET_ARGUMENT_INTEGER(outputArgument,
                                                     moduleConfig->DsmFanRange[AcpiPepDeviceFan_FanRangeIndex2]);

                    // Package entry 3
                    //
                    outputArgument = ACPI_METHOD_NEXT_ARGUMENT(outputArgument);
                    ACPI_METHOD_SET_ARGUMENT_INTEGER(outputArgument,
                                                     moduleConfig->DsmFanRange[AcpiPepDeviceFan_FanRangeIndex3]);

                    ecmBuffer->OutputArgumentCount = 1;
                    ecmBuffer->OutputArgumentSize = ACPI_METHOD_ARGUMENT_LENGTH_FROM_ARGUMENT(ecmBuffer->OutputArguments);
                    ecmBuffer->MethodStatus = STATUS_SUCCESS;
                    completeStatus = PEP_NOTIFICATION_HANDLER_COMPLETE;

                    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Fan%d: Fan Range request", moduleConfig->FanInstanceIndex);
                }
            }
            else
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "Unsupported DSM");
            }

            break;

        default:
        {
            DMF_AcpiPepDevice_ReportNotSupported(DmfModule,
                                                 &ecmBuffer->MethodStatus,
                                                 &ecmBuffer->OutputArgumentCount,
                                                 &completeStatus);
            break;
        }
    }

Exit:

    return completeStatus;
}

// Methods supported by fan device.
//
PEP_OBJECT_INFORMATION
FanNativeMethods[] =
{
    // _FST
    {ACPI_OBJECT_NAME_FST, 0, 1, PepAcpiObjectTypeMethod},

    // _DSM
    {ACPI_OBJECT_NAME_DSM, 4, 1, PepAcpiObjectTypeMethod},
};

// Fan only supports EvaluateMethod and AcpiNotify.
//
PEP_DEVICE_NOTIFICATION_HANDLER
FanNotificationHandler[] =
{
    {PEP_NOTIFY_ACPI_EVALUATE_CONTROL_METHOD,
     AcpiPepDeviceFan_SyncEvaluateControlMethod,
     NULL},

    {PEP_NOTIFY_ACPI_WORK,
    NULL,
    DMF_AcpiPepDevice_AsyncNotifyEvent}
};

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
DMF_AcpiPepDeviceFan_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type AcpiPepDeviceFan.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_AcpiPepDeviceFan* moduleConfig;
    DMF_CONTEXT_AcpiPepDeviceFan* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Device definition array of fan to be passed to Dmf_AcpiPepDevice.
    //

    moduleContext->PepDeviceDefinitionArray = { PEP_DEVICE_TYPE_FAN0,
                                                sizeof(PEP_ACPI_DEVICE),
                                                AcpiPepDeviceFan_InitializeCallback,
                                                ARRAYSIZE(FanNativeMethods),
                                                FanNativeMethods,
                                                ARRAYSIZE(FanNotificationHandler),
                                                FanNotificationHandler,
                                                0,
                                                NULL,
                                                DmfModule};

    // Match table specifying fan name for matching.
    //
    moduleContext->PepDeviceMatchArray = { PEP_DEVICE_TYPE_FAN0,
                                           PEP_NOTIFICATION_CLASS_ACPI,
                                           moduleConfig->FanInstanceNameWchar,
                                           PepDeviceIdMatchFull};

    // Let ACPI Pep Device know that Fan is ready.
    //
    DmfAssert(moduleConfig->ArrivalCallback != NULL);
    moduleConfig->ArrivalCallback(DmfModule);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_MODULEOPEN ntStatus=%!STATUS! DeviceId=%S", ntStatus,moduleContext->PepDeviceMatchArray.DeviceId);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
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
DMF_AcpiPepDeviceFan_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type AcpiPepDeviceFan.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_AcpiPepDeviceFan;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_AcpiPepDeviceFan;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_AcpiPepDeviceFan);
    dmfCallbacksDmf_AcpiPepDeviceFan.DeviceOpen = DMF_AcpiPepDeviceFan_Open;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_AcpiPepDeviceFan,
                                            AcpiPepDeviceFan,
                                            DMF_CONTEXT_AcpiPepDeviceFan,
                                            DMF_MODULE_OPTIONS_DISPATCH,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_AcpiPepDeviceFan.CallbacksDmf = &dmfCallbacksDmf_AcpiPepDeviceFan;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_AcpiPepDeviceFan,
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

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_AcpiPepDeviceFan_AcpiDeviceTableGet(
    _In_ DMFMODULE DmfModule,
    _Out_ PEP_ACPI_REGISTRATION_TABLES* PepAcpiRegistrationTables
    )
/*++

Routine Description:

    This Method is used to fetch Fan Tables to be added to AcpiPepDevice Module before registering
    that Module with PoFx. Upon registration the tables added to the main Module are dequeued and
    merged into one table registered in OS.

Arguments:

    DmfModule - Handle to this Module.
    PepAcpiRegistrationTables - The tables returned by Fan device for registration.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_AcpiPepDeviceFan* moduleContext;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    size_t sizeToAllocate;

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "%!FUNC!");

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 AcpiPepDeviceFan);

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(PepAcpiRegistrationTables != NULL);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    sizeToAllocate = sizeof(PEP_DEVICE_DEFINITION);

    ntStatus = WdfMemoryCreatePreallocated(&objectAttributes,
                                           (VOID*)&moduleContext->PepDeviceDefinitionArray,
                                           sizeToAllocate,
                                           &PepAcpiRegistrationTables->AcpiDefinitionTable);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    sizeToAllocate = sizeof(PEP_DEVICE_MATCH);

    ntStatus = WdfMemoryCreatePreallocated(&objectAttributes,
                                            (VOID*)&moduleContext->PepDeviceMatchArray,
                                            sizeToAllocate,
                                            &PepAcpiRegistrationTables->AcpiMatchTable);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    if (!NT_SUCCESS(ntStatus))
    {
        if (PepAcpiRegistrationTables->AcpiDefinitionTable != NULL)
        {
            WdfObjectDelete(PepAcpiRegistrationTables->AcpiDefinitionTable);
            PepAcpiRegistrationTables->AcpiDefinitionTable = NULL;

        }
        if (PepAcpiRegistrationTables->AcpiMatchTable != NULL)
        {
            WdfObjectDelete(PepAcpiRegistrationTables->AcpiMatchTable);
            PepAcpiRegistrationTables->AcpiMatchTable = NULL;
        }
    }

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_AcpiPepDeviceFan_NotifyRequestSchedule(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG NotifyCode
    )
/*++

Routine Description:

    This Method sends AcpiNotify to the Fan device.

Arguments:

    DmfModule - Handle to this Module.
    NotifyCode - Acpi Notify Code to send Fan device.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    PEP_ACPI_NOTIFY_CONTEXT* notifyContext;
    DMF_CONTEXT_AcpiPepDeviceFan* moduleContext;
    WDFMEMORY notifyContextMemory;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "%!FUNC!");

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 AcpiPepDeviceFan);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->FanInitialized == TRUE)
    {
        // Set up context for ACPI Notify work.
        //
        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        objectAttributes.ParentObject = DmfModule;

        ntStatus = WdfMemoryCreate(&objectAttributes,
                                   NonPagedPoolNx,
                                   MemoryTag,
                                   sizeof(PEP_ACPI_NOTIFY_CONTEXT),
                                   &notifyContextMemory,
                                   (VOID**)&notifyContext);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,  DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        notifyContext->PepInternalDevice = moduleContext->FanPepInternalDevice;
        notifyContext->NotifyCode = NotifyCode;

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Creating work request");

        ntStatus = DMF_AcpiPepDevice_ScheduleNotifyRequest(DmfModule,
                                                           notifyContext);

        WdfObjectDelete(notifyContextMemory);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "%s: Failed. Fan not initialized.\n",
                    __FUNCTION__);
    }

Exit:
    ;
}

// eof: Dmf_AcpiPepDeviceFan.c
//
