/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_AcpiPepDevice.c

Abstract:

    Support for creating a Virtual ACPI Device using MS PEP (Platform Extension Plugin).
    This Module is created using sample code from:
    https://github.com/microsoft/Windows-driver-samples/tree/master/pofx/PEP

Environment:

    Kernel-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_AcpiPepDevice.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_AcpiPepDevice
{
    // Definition table for the root device.
    //
    PEP_DEVICE_DEFINITION PepRootDefinition;
    // Kernel information structure used in PoFx registration.
    //
    PEP_KERNEL_INFORMATION PepKernelInformation;
    // List for tracking work items.
    //
    LIST_ENTRY PepCompletedWorkList;
    // List for tracking devices.
    //
    LIST_ENTRY PepDeviceList;
    // List for tracking pending work items.
    //
    LIST_ENTRY PepPendingWorkList;
    // The full table containing all supported functions for all devices.
    //
    PEP_DEVICE_DEFINITION* PepDeviceDefinitionArray;
    // Size of definition table.
    //
    ULONG PepDeviceDefinitionArraySize;
    // Collection containing all the devices tables.
    //
    WDFCOLLECTION PepDefinitionTableCollection;
    // WDFMEMORY handle for table buffer.
    //
    WDFMEMORY DeviceDefinitionMemory;
    // The full table containing all supported devices.
    //
    PEP_DEVICE_MATCH* PepDeviceMatchArray;
    // Size of match table.
    //
    ULONG PepDeviceMatchArraySize;
    // Collection containing all the devices tables.
    //
    WDFCOLLECTION PepMatchTableCollection;
    // WDFMEMORY handle for table buffer.
    //
    WDFMEMORY DeviceMatchMemory;
    // Array of child PEP Devices.
    //
    DMFMODULE* ChildPepDeviceModules;
    // Track the child PEP devices that have been registered.
    //
    ULONG ChildrenRegistered;
    // Tracks whether ChildModulesAdd happened successfully.
    //
    BOOLEAN ChildrenEnumerated;
} DMF_CONTEXT_AcpiPepDevice;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(AcpiPepDevice)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(AcpiPepDevice)

// MemoryTag.
//
#define MemoryTag 'MDPA'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// This global variable is necessary because there is no way to get a context passed into the callbacks.
// NOTE: Only a single instance of this Module may be instantiated per driver instance.
//
static DMFMODULE g_DmfModuleAcpiPepDevice;

// Root device does not intercept any methods.
//
PEP_OBJECT_INFORMATION
RootNativeMethods[1] =
{};

PEP_NOTIFICATION_HANDLER_RESULT
AcpiPepDevice_RootSyncEvaluateControlMethod(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Data,
    _Out_opt_ PEP_WORK_INFORMATION* PoFxWorkInformation
    );

PEP_DEVICE_NOTIFICATION_HANDLER
RootNotificationHandler[] =
{
    {
        PEP_NOTIFY_ACPI_EVALUATE_CONTROL_METHOD,
        AcpiPepDevice_RootSyncEvaluateControlMethod,
        NULL
    }
};

#define ACPI_ROOT_ANSI "\\_SB"
#define ACPI_ROOT_WCHAR L"\\_SB"

// Memory Allocation Tag for PEP.
//
#define PEP_TAG 'TpeP'

PEP_DEVICE_MATCH
PepRootMatch =
{
     PEP_DEVICE_TYPE_ROOT,
     PEP_NOTIFICATION_CLASS_ACPI,
     ACPI_ROOT_WCHAR,
     PepDeviceIdMatchFull
};

#define PEP_INVALID_DEVICE_TYPE \
    PEP_MAKE_DEVICE_TYPE(PepMajorDeviceTypeMaximum, \
                         PepAcpiMinorTypeMaximum, \
                         0xFFFF)

#define PEP_CHECK_DEVICE_TYPE_ACCEPTED(_Type, _Mask) \
    (((_Type) & (_Mask)) == (_Mask))

typedef enum _PEP_HANDLER_TYPE
{
    PepHandlerTypeSyncCritical,
    PepHandlerTypeWorkerCallback
} PEP_HANDLER_TYPE;

typedef struct _PEP_WORK_ITEM_CONTEXT
{
    DMFMODULE DmfModule;
    WDFWORKITEM WorkItem;
    PEP_NOTIFICATION_CLASS WorkType;
} PEP_WORK_ITEM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PEP_WORK_ITEM_CONTEXT, PepWorkItemContextGet)

// Define the default ACPI notification handlers.
//
PEP_GENERAL_NOTIFICATION_HANDLER_ROUTINE AcpiPepDevice_DevicePrepare;
PEP_GENERAL_NOTIFICATION_HANDLER_ROUTINE AcpiPepDevice_DeviceAbandon;
PEP_GENERAL_NOTIFICATION_HANDLER_ROUTINE AcpiPepDevice_DeviceRegister;
PEP_GENERAL_NOTIFICATION_HANDLER_ROUTINE AcpiPepDevice_DeviceUnregister;
PEP_GENERAL_NOTIFICATION_HANDLER_ROUTINE AcpiPepDevice_DeviceNamespaceEnumerate;
PEP_GENERAL_NOTIFICATION_HANDLER_ROUTINE AcpiPepDevice_ObjectInformationQuery;
PEP_GENERAL_NOTIFICATION_HANDLER_ROUTINE AcpiPepDevice_ControlMethodEvaluate;
PEP_GENERAL_NOTIFICATION_HANDLER_ROUTINE AcpiPepDevice_DeviceControlResourcesQuery;
PEP_GENERAL_NOTIFICATION_HANDLER_ROUTINE AcpiPepDevice_TranslatedDeviceControlResources;
PEP_GENERAL_NOTIFICATION_HANDLER_ROUTINE AcpiPepDevice_WorkNotification;

PEP_GENERAL_NOTIFICATION_HANDLER
PepAcpiNotificationHandlers[] =
{
    {0, NULL, "UNKNOWN"},

    {PEP_NOTIFY_ACPI_PREPARE_DEVICE,
     AcpiPepDevice_DevicePrepare,
     "PEP_ACPI_PREPARE_DEVICE"},

    {PEP_NOTIFY_ACPI_ABANDON_DEVICE,
     AcpiPepDevice_DeviceAbandon,
     "PEP_ACPI_ABANDON_DEVICE"},

    {PEP_NOTIFY_ACPI_REGISTER_DEVICE,
     AcpiPepDevice_DeviceRegister,
     "PEP_ACPI_REGISTER_DEVICE"},

    {PEP_NOTIFY_ACPI_UNREGISTER_DEVICE,
     AcpiPepDevice_DeviceUnregister,
     "PEP_ACPI_UNREGISTER_DEVICE"},

    {PEP_NOTIFY_ACPI_ENUMERATE_DEVICE_NAMESPACE,
     AcpiPepDevice_DeviceNamespaceEnumerate,
     "PEP_ACPI_ENUMERATE_DEVICE_NAMESPACE"},

    {PEP_NOTIFY_ACPI_QUERY_OBJECT_INFORMATION,
     AcpiPepDevice_ObjectInformationQuery,
     "PEP_ACPI_QUERY_OBJECT_INFORMATION"},

    {PEP_NOTIFY_ACPI_EVALUATE_CONTROL_METHOD,
     AcpiPepDevice_ControlMethodEvaluate,
     "PEP_ACPI_EVALUATE_CONTROL_METHOD"},

    {PEP_NOTIFY_ACPI_QUERY_DEVICE_CONTROL_RESOURCES,
     AcpiPepDevice_DeviceControlResourcesQuery,
     "PEP_ACPI_QUERY_DEVICE_CONTROL_RESOURCES"},

    {PEP_NOTIFY_ACPI_TRANSLATED_DEVICE_CONTROL_RESOURCES,
     AcpiPepDevice_TranslatedDeviceControlResources,
     "PEP_ACPI_TRANSLATED_DEVICE_CONTROL_RESOURCES"},

    {PEP_NOTIFY_ACPI_WORK,
     AcpiPepDevice_WorkNotification,
     "PEP_ACPI_WORK"}
};

#define OffsetToPtr(base, offset) ((VOID*)((ULONG_PTR)(base) + (offset)))
#define NAME_NATIVE_METHOD(_Name) (((_Name) == NULL) ? "Unknown" : (_Name))
#define NAME_DEBUG_INFO(_Info) (((_Info) == NULL) ? "" : (_Info))

typedef struct _PEP_WORK_CONTEXT
{
    // Entry of this request on its current (pending or completed) queue.
    //
    LIST_ENTRY ListEntry;

    // Request signature (for validation purposes).
    //
    ULONG Signature;

    // The type of the request.
    //
    PEP_NOTIFICATION_CLASS WorkType;
    ULONG NotificationId;
    BOOLEAN WorkCompleted;

    // The device for which the request is associated with. This value may be
    // NULL if request is tagged as a parent.
    //
    PEP_INTERNAL_DEVICE_HEADER* PepInternalDevice;
    PEP_DEVICE_DEFINITION* DeviceDefinitionEntry;

    // PoFx-supplied PEP_WORK for work requests.
    //
    PEP_WORK_INFORMATION LocalPoFxWorkInfo;

    // Work item context
    //
    WDFMEMORY WorkContextMemory;
    SIZE_T WorkContextSize;
    NTSTATUS* WorkRequestStatus;
    WDFMEMORY WorkRequestMemory;
} PEP_WORK_CONTEXT;

PEP_NOTIFICATION_HANDLER_RESULT
AcpiPepDevice_RootSyncEvaluateControlMethod(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Data,
    _Out_opt_ PEP_WORK_INFORMATION* PoFxWorkInformation
    )
/*++

Routine Description:

    This routine handles PEP_NOTIFY_ACPI_EVALUATE_CONTROL_METHOD
    notification for the bus device.

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

    UNREFERENCED_PARAMETER(PoFxWorkInformation);

    ecmBuffer = (PPEP_ACPI_EVALUATE_CONTROL_METHOD)Data;
    completeStatus = PEP_NOTIFICATION_HANDLER_COMPLETE;

    DMF_AcpiPepDevice_ReportNotSupported(DmfModule,
                                         &ecmBuffer->MethodStatus,
                                         &ecmBuffer->OutputArgumentCount,
                                         &completeStatus);

    // Return complete status.
    //
    return completeStatus;
}

BOOLEAN
AcpiPepDevice_IsDeviceIdMatched(
    _In_ PWSTR String,
    _In_ ULONG StringLength,
    _In_ PWSTR SearchString,
    _In_ ULONG SearchStringLength,
    _In_ PEP_DEVICE_ID_MATCH DeviceIdCompareMethod
    )
/*++

Routine Description:

    This routine check whether two given strings matches in the way specified
    by the DeviceIdCompare flag.

Arguments:

    String - Supplies a pointer to a unicode string to search within.
    StringLength - Supplies the length of the string to search within.
    SearchString - Supplies a pointer to a unicode string to search for.
    SearchStringLength - Supplies the length of the string to search for.
    DeviceIdCompareMethod - Supplies the flag indicating the way two
        strings should match.
        - PepDeviceIdMatchPartial: substring match;
        - PepDeviceIdMatchFull: whole string match.

Return Value:

    TRUE if the search string is found;
    FALSE otherwise.

--*/
{
    BOOLEAN found;
    ULONG index;
    ULONG result;
    UNICODE_STRING searchStringUnicode;
    UNICODE_STRING sourceStringUnicode;

    found = FALSE;
    switch (DeviceIdCompareMethod)
    {
        case PepDeviceIdMatchFull:
            RtlInitUnicodeString(&sourceStringUnicode,
                                 String);
            RtlInitUnicodeString(&searchStringUnicode,
                                 SearchString);
            result = RtlCompareUnicodeString(&sourceStringUnicode,
                                             &searchStringUnicode,
                                             FALSE);
            if (result == 0)
            {
                found = TRUE;
            }
            break;

        case PepDeviceIdMatchPartial:
            if (StringLength < SearchStringLength)
            {
                goto Exit;
            }

            for (index = 0; index <= (StringLength - SearchStringLength); index += 1)
            {
                if (!_wcsnicmp(String + index,
                               SearchString,
                               SearchStringLength))
                {
                    found = TRUE;
                    goto Exit;
                }
            }
            break;

        default:
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "%s: Unknown DeviceIdCompare = %d.",
                        __FUNCTION__,
                        (ULONG)DeviceIdCompareMethod);
            break;
    }

Exit:

    return found;
}

BOOLEAN
AcpiPepDevice_IsDeviceAccepted(
    _In_ DMFMODULE DmfModule,
    _In_ PEP_NOTIFICATION_CLASS OwnedType,
    _In_ PCUNICODE_STRING DeviceId,
    _Out_ PEP_DEVICE_DEFINITION* *DeviceDefinition,
    _Out_ DMFMODULE* DmfModulePepClient
    )
/*++

Routine Description:

    This routine is called to see if a device should be accepted by this PEP.

Arguments:

    DmfModule - This Module's handle.
    OwnedType - Supplies the expected type of this PEP, which can be a
                combination of ACPI, DPM, PPM or NONE.
    DeviceId - Supplies a pointer to a unicode string that contains the
               device ID or instance path for the device.
    DeviceDefinition - Supplies the pointer for storing the accepted device's
                       device-specific definitions.

Return Value:

    TRUE if this PEP owns this device;
    FALSE otherwise.

--*/
{
    PEP_DEVICE_ID_MATCH compare;
    ULONG index;
    BOOLEAN match;
    PEP_DEVICE_TYPE type;
    DMF_CONTEXT_AcpiPepDevice* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    match = FALSE;
    type = PEP_INVALID_DEVICE_TYPE;
    for (index = 0; index < moduleContext->PepDeviceMatchArraySize; index += 1)
    {
        if (PEP_CHECK_DEVICE_TYPE_ACCEPTED((moduleContext->PepDeviceMatchArray[index].OwnedType),
                                           OwnedType) == FALSE)
        {
            continue;
        }

        // If the type is owned by this PEP, check the device id.
        //
        compare = moduleContext->PepDeviceMatchArray[index].CompareMethod;
        match = AcpiPepDevice_IsDeviceIdMatched(DeviceId->Buffer,
                                                DeviceId->Length / sizeof(WCHAR),
                                                moduleContext->PepDeviceMatchArray[index].DeviceId,
                                                (ULONG)wcslen(moduleContext->PepDeviceMatchArray[index].DeviceId),
                                                compare);
        if (match != FALSE)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                        DMF_TRACE,
                        "%s: Found device whose type matches. "
                        "DeviceId: %S",
                        __FUNCTION__,
                        DeviceId->Buffer);

            type = moduleContext->PepDeviceMatchArray[index].Type;
            break;
        }
    }

    if (match == FALSE)
    {
        goto Exit;
    }

    match = FALSE;
    for (index = 0; index < moduleContext->PepDeviceDefinitionArraySize; index += 1)
    {
        if (moduleContext->PepDeviceDefinitionArray[index].Type == type)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                        DMF_TRACE,
                        "%s: Found device definition of the given type.",
                        __FUNCTION__);

            match = TRUE;
            *DeviceDefinition = &moduleContext->PepDeviceDefinitionArray[index];
            *DmfModulePepClient = moduleContext->PepDeviceDefinitionArray[index].DmfModule;
            break;
        }
    }

Exit:

    return match;
}

VOID
AcpiPepDevice_DevicePrepare(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_bytes_(sizeof(PEP_ACPI_PREPARE_DEVICE)) VOID* Data
    )
/*++

Routine Description:

    This routine is called by default to prepare a device to be created.

Arguments:

    DmfModule - This Module's handle.
    Data - Supplies a pointer to a PEP_ACPI_PREPARE_DEVICE structure.

Return Value:

    None.

--*/
{

    PPEP_ACPI_PREPARE_DEVICE acpiPrepareDevice;
    PEP_DEVICE_DEFINITION* deviceDefinition;
    DMFMODULE dmfModulePepClient;

    deviceDefinition = NULL;
    acpiPrepareDevice = (PPEP_ACPI_PREPARE_DEVICE)Data;
    acpiPrepareDevice->OutputFlags = PEP_ACPI_PREPARE_DEVICE_OUTPUT_FLAG_NONE;
    acpiPrepareDevice->DeviceAccepted =  AcpiPepDevice_IsDeviceAccepted(DmfModule,
                                                                        PEP_NOTIFICATION_CLASS_ACPI,
                                                                        acpiPrepareDevice->AcpiDeviceName,
                                                                        &deviceDefinition,
                                                                        &dmfModulePepClient);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "%s: %s: Device = %S, Accepted = %d.",
                __FUNCTION__,
                PepAcpiNotificationHandlers[PEP_NOTIFY_ACPI_PREPARE_DEVICE].Name,
                acpiPrepareDevice->AcpiDeviceName->Buffer,
                (ULONG)acpiPrepareDevice->DeviceAccepted);
}

VOID
AcpiPepDevice_DeviceAbandon(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_bytes_(sizeof(PEP_ACPI_ABANDON_DEVICE)) VOID* Data
    )
/*++

Routine Description:

    This routine is called to abandon a device once it is being removed.

Arguments:

    DmfModule - This Module's handle.
    Data - Supplies a pointer to a PEP_ACPI_ABANDON_DEVICE structure.

Return Value:

    None.

--*/
{
    PEP_ACPI_ABANDON_DEVICE* acpiAbandonDevice;
    PEP_DEVICE_DEFINITION* deviceDefinition;
    DMFMODULE dmfModulePepClient;

    acpiAbandonDevice = (PEP_ACPI_ABANDON_DEVICE*)Data;
    acpiAbandonDevice->DeviceAccepted = AcpiPepDevice_IsDeviceAccepted(DmfModule,
                                                                       PEP_NOTIFICATION_CLASS_ACPI,
                                                                       acpiAbandonDevice->AcpiDeviceName,
                                                                       &deviceDefinition,
                                                                       &dmfModulePepClient);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "%s: %s: Device = %S, Accepted = %d.",
                __FUNCTION__,
                PepAcpiNotificationHandlers[
                    PEP_NOTIFY_ACPI_ABANDON_DEVICE].Name,
                acpiAbandonDevice->AcpiDeviceName->Buffer,
                acpiAbandonDevice->DeviceAccepted);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
AcpiPepDevice_WorkRequestCreate(
    _In_ DMFMODULE DmfModule,
    _In_ PEP_NOTIFICATION_CLASS WorkType,
    _In_ ULONG NotificationId,
    _In_ PEP_INTERNAL_DEVICE_HEADER* PepInternalDevice,
    _In_ PEP_DEVICE_DEFINITION* DeviceDefinitionEntry,
    _In_opt_ VOID* WorkContext,
    _In_ SIZE_T WorkContextSize,
    _In_opt_ NTSTATUS* WorkRequestStatus,
    _Out_ WDFMEMORY* WorkRequestMemory
    )
/*++

Routine Description:

    This routine creates a new work request. Note the caller is responsible
    for adding this request to the pending queue after filling in
    request-specific data.

Arguments:

    DmfModule - This Module's handle.
    WorkType - Supplies the type of the work (ACPI/DPM/PPM).
    NotificationId - Supplies the PEP notification type.
    PepInternalDevice - Supplies the internal PEP device.
    DeviceDefinitionEntry - Supplies a pointer to the device definition for
        the device.
    WorkContext - Supplies optional pointer to the context of the work request.
    WorkContextSize - Supplies the size of the work request context.
    WorkRequestStatus -  Supplies optional pointer to report the status of the work request.
    OutputWorkRequest - Supplies a pointer that receives the created request.

Return Value:

    NTSTATUS.

--*/
{
    VOID* localWorkContext;
    NTSTATUS ntStatus;
    PEP_WORK_CONTEXT* workRequest;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDFMEMORY localWorkContextMemory;

    UNREFERENCED_PARAMETER(DmfModule);

    DmfAssert(WorkType != PEP_NOTIFICATION_CLASS_NONE);

    localWorkContextMemory = NULL;

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = g_DmfModuleAcpiPepDevice;

    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               sizeof(PEP_WORK_CONTEXT),
                               WorkRequestMemory,
                               (VOID**)&workRequest);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,  DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    localWorkContext = NULL;
    if (WorkContext != NULL)
    {
        DmfAssert(WorkContextSize != 0);

        ntStatus = WdfMemoryCreate(&objectAttributes,
                                   NonPagedPoolNx,
                                   MemoryTag,
                                   WorkContextSize,
                                   &localWorkContextMemory,
                                   (VOID**)&localWorkContext);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,  DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        RtlCopyMemory(localWorkContext,
                      WorkContext,
                      WorkContextSize);

    }
    else
    {
        WorkContextSize = 0;
    }

    RtlZeroMemory(workRequest,
                  sizeof(PEP_WORK_CONTEXT));
    InitializeListHead(&workRequest->ListEntry);
    workRequest->WorkRequestMemory = *WorkRequestMemory;
    workRequest->WorkType = WorkType;
    workRequest->NotificationId = NotificationId;
    workRequest->PepInternalDevice = PepInternalDevice;
    workRequest->DeviceDefinitionEntry = DeviceDefinitionEntry;
    workRequest->WorkContextSize = WorkContextSize;
    workRequest->WorkContextMemory = localWorkContextMemory;
    workRequest->WorkRequestStatus = WorkRequestStatus;
    workRequest->WorkCompleted = FALSE;
    ntStatus = STATUS_SUCCESS;

Exit:
    return ntStatus;
}

VOID
AcpiPepDevice_NotificationHandlerInvoke(
    _In_ DMFMODULE DmfModule,
    _In_ PEP_NOTIFICATION_CLASS WorkType,
    _In_opt_ PEP_WORK_CONTEXT* WorkRequest,
    _In_ PEP_HANDLER_TYPE HandlerType,
    _In_ ULONG NotificationId,
    _In_opt_ PEP_INTERNAL_DEVICE_HEADER* PepInternalDevice,
    _In_ VOID* Data,
    _In_ SIZE_T DataSize,
    _In_opt_ NTSTATUS* WorkRequestStatus
    );

VOID
AcpiPepDevice_PendingWorkRequestsProcess(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This function processes all pending work. It calls the handler
    routine for each pending work.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None.

--*/
{
    LIST_ENTRY* nextEntry;
    PEP_WORK_CONTEXT* workRequest;
    VOID* workContext;

    DMF_CONTEXT_AcpiPepDevice* moduleContext = DMF_CONTEXT_GET(g_DmfModuleAcpiPepDevice);

    // Go through pending work list and handle them.
    //
    DMF_ModuleAuxiliaryLock(g_DmfModuleAcpiPepDevice,
                            0);

    while (IsListEmpty(&moduleContext->PepPendingWorkList) == FALSE)
    {
        nextEntry = RemoveHeadList(&moduleContext->PepPendingWorkList);
        InitializeListHead(nextEntry);
        workRequest = CONTAINING_RECORD(nextEntry,
                                        PEP_WORK_CONTEXT,
                                        ListEntry);

        // Drop the request list lock prior to processing work.
        //
        DMF_ModuleAuxiliaryUnlock(g_DmfModuleAcpiPepDevice,
                                  0);

        // Invoke the request processing async handler.
        //
        DmfAssert(workRequest->WorkCompleted == FALSE);

        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "%s: Asynchronously processing request. "
                    "Device=%p, WorkType=%d, NotificationId=%d.",
                    __FUNCTION__,
                    (VOID*)workRequest->PepInternalDevice,
                    (ULONG)workRequest->WorkType,
                    (ULONG)workRequest->NotificationId);

        workContext = WdfMemoryGetBuffer(workRequest->WorkContextMemory,
                                         NULL);
        AcpiPepDevice_NotificationHandlerInvoke(DmfModule,
                                                workRequest->WorkType,
                                                workRequest,
                                                PepHandlerTypeWorkerCallback,
                                                workRequest->NotificationId,
                                                workRequest->PepInternalDevice,
                                                workContext,
                                                workRequest->WorkContextSize,
                                                workRequest->WorkRequestStatus);

        // Reacquire the request list lock prior to dequeuing next request.
        //
        DMF_ModuleAuxiliaryLock(g_DmfModuleAcpiPepDevice,
                                0);
    }

    DMF_ModuleAuxiliaryUnlock(g_DmfModuleAcpiPepDevice,
                              0);
}

VOID
AcpiPepDevice_WorkerWrapper(
    _In_ WDFWORKITEM WorkItem
    )
/*++

Routine Description:

    This routine is wrapper for the actual worker routine that processes
    pending work.

Arguments:

    WorkItem -  Supplies a handle to the workitem supplying the context.

Return Value:

    None.
--*/
{
    PEP_WORK_ITEM_CONTEXT* context;

    context = PepWorkItemContextGet(WorkItem);
    AcpiPepDevice_PendingWorkRequestsProcess(context->DmfModule);

    // Delete the work item as it is no longer required.
    //
    WdfObjectDelete(WorkItem);
}

NTSTATUS
AcpiPepDevice_ScheduleWorker(
    _In_ PEP_WORK_CONTEXT* WorkContext
    )
/*++

Routine Description:

    This function schedules a worker thread to process pending work requests.

Arguments:

    WorkContext - Supplies the context of the work.

Return Value:

    NTSTATUS.

--*/
{
    WDF_OBJECT_ATTRIBUTES attributes;
    NTSTATUS ntStatus;
    BOOLEAN synchronous;
    WDFWORKITEM workItem;
    WDF_WORKITEM_CONFIG workItemConfiguration;

    workItem = NULL;
    synchronous = FALSE;

    // Create a workitem to process events.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&attributes,
                                           PEP_WORK_ITEM_CONTEXT);

    attributes.ParentObject = g_DmfModuleAcpiPepDevice;

    // Initialize the handler routine and create a new workitem.
    //
    WDF_WORKITEM_CONFIG_INIT(&workItemConfiguration,
                             AcpiPepDevice_WorkerWrapper);

    // Disable automatic serialization by the framework for the worker thread.
    // The parent device object is being serialized at device level (i.e.,
    // WdfSynchronizationScopeDevice), and the framework requires it to be
    // passive level (i.e., WdfExecutionLevelPassive) if automatic
    // synchronization is desired.
    //
    workItemConfiguration.AutomaticSerialization = FALSE;

    // Create the work item and queue it. If the workitem cannot be created
    // for some reason, just call the worker routine synchronously.
    //
    ntStatus = WdfWorkItemCreate(&workItemConfiguration,
                                 &attributes,
                                 &workItem);
    if (!NT_SUCCESS(ntStatus))
    {
        synchronous = TRUE;
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "%s: Failed to allocate work item to process pending"
                    "work! ntStatus = %!STATUS!. Will synchronously process.",
                    __FUNCTION__,
                    ntStatus);
    }

    // If the operation is to be performed synchronously, then directly
    // invoke the worker routine. Otherwise, queue a workitem to run the
    // worker routine.
    //
    if (synchronous != FALSE)
    {
        AcpiPepDevice_PendingWorkRequestsProcess(g_DmfModuleAcpiPepDevice);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "%s: Work request scheduled to run asynchronously. "
                    "Device=%p, WorkType=%d, NotificationId=%d.",
                    __FUNCTION__,
                    (VOID*)WorkContext->PepInternalDevice,
                    (ULONG)WorkContext->WorkType,
                    (ULONG)WorkContext->NotificationId);

        WdfWorkItemEnqueue(workItem);
    }

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
AcpiPepDevice_WorkRequestPend(
    _In_ WDFMEMORY WorkRequestMemory
    )
/*++

Routine Description:

    This routine adds the given work request to the pending queue.

Arguments:

    WorkRequest - Supplies a pointer to the work request.

Return Value:

    None.

--*/
{
    PEP_WORK_CONTEXT* workRequest;
    DMF_CONTEXT_AcpiPepDevice* moduleContext = DMF_CONTEXT_GET(g_DmfModuleAcpiPepDevice);

    workRequest = (PEP_WORK_CONTEXT*)WdfMemoryGetBuffer(WorkRequestMemory,
                                                        NULL);

    // Ensure that the request is not already on some other queue.
    //
    DmfAssert(IsListEmpty(&workRequest->ListEntry) != FALSE);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "%s: Insert pending work request. "
                "Device=%p, WorkType=%d, NotificationId=%d.",
                __FUNCTION__,
                (VOID*)workRequest->PepInternalDevice,
                (ULONG)workRequest->WorkType,
                (ULONG)workRequest->NotificationId);

    // Add the new request to the end of tail of the pending work queue.
    //
    DMF_ModuleAuxiliaryLock(g_DmfModuleAcpiPepDevice,
                            0);
    InsertTailList(&moduleContext->PepPendingWorkList,
                   &workRequest->ListEntry);
    DMF_ModuleAuxiliaryUnlock(g_DmfModuleAcpiPepDevice,
                              0);

    // Schedule a worker to pick up the new work.
    //
    AcpiPepDevice_ScheduleWorker(workRequest);
}

VOID
AcpiPepDevice_NotificationHandlerSchedule(
    _In_ DMFMODULE DmfModule,
    _In_ PEP_NOTIFICATION_CLASS WorkType,
    _In_ ULONG NotificationId,
    _In_ PEP_INTERNAL_DEVICE_HEADER* PepInternalDevice,
    _In_opt_ VOID* WorkContext,
    _In_ SIZE_T WorkContextSize,
    _In_opt_ NTSTATUS* WorkRequestStatus
    )
/*++

Routine Description:

    This routine schedules the device-specific handler.

Arguments:

    DmfModule - This Module's handle.
    WorkType - Supplies the type of the work (ACPI/DPM/PPM).
    NotificationId - Supplies the PEP notification type.
    PepInternalDevice - Supplies the internal PEP device.
    WorkContext - Supplies optional pointer to the context of the work request.
    WorkContextSize - Supplies the size of the work request context.
    WorkRequestStatus -  Supplies optional pointer to report the status of the work request.

Return Value:

    None.

--*/
{
    PEP_DEVICE_DEFINITION* deviceDefinition;
    NTSTATUS ntStatus;
    WDFMEMORY workRequestMemory;

    deviceDefinition = PepInternalDevice->DeviceDefinition;
    ntStatus = AcpiPepDevice_WorkRequestCreate(DmfModule,
                                               WorkType,
                                               NotificationId,
                                               PepInternalDevice,
                                               deviceDefinition,
                                               WorkContext,
                                               WorkContextSize,
                                               WorkRequestStatus,
                                               &workRequestMemory);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "%s: PepCreateWorkRequest() failed!. "
                    "ntStatus = %!STATUS!.\n ",
                    __FUNCTION__,
                    ntStatus);

        goto Exit;
    }

    AcpiPepDevice_WorkRequestPend(workRequestMemory);

    // Mark the work request status as pending.
    //
    if (WorkRequestStatus != NULL)
    {
        *WorkRequestStatus = STATUS_PENDING;
    }

Exit:
    ;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
AcpiPepDevice_MarkWorkRequestComplete(
    _In_ PEP_WORK_CONTEXT* WorkRequest
    )
/*++

Routine Description:

    This routine marks the given work request as completed. This will cause
    it to be moved to the completion queue.

Arguments:

    WorkRequest - Supplies a pointer to the work request.

Return Value:

    None.

--*/
{
    // Ensure the request wasn't already completed in a different context
    // (and thus potentially already on the completed queue).
    //
    DmfAssert(WorkRequest->WorkCompleted == FALSE);

    WorkRequest->WorkCompleted = TRUE;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
AcpiPepDevice_WorkRequestComplete(
    _In_ PEP_WORK_CONTEXT* WorkRequest
    )
/*++

Routine Description:

    This routine adds the given work request to the completed queue.

Arguments:

    WorkRequest - Supplies a pointer to the work request.

Return Value:

    None.

--*/
{
    // Mark the request as completed.
    //
    DMF_CONTEXT_AcpiPepDevice* moduleContext = DMF_CONTEXT_GET(g_DmfModuleAcpiPepDevice);

    AcpiPepDevice_MarkWorkRequestComplete(WorkRequest);

    // Ensure that the request is not already on some other queue.
    //
    DmfAssert(IsListEmpty(&WorkRequest->ListEntry) != FALSE);
    DmfAssert(WorkRequest->WorkCompleted != FALSE);

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "%s: Insert complete work request. "
                "Device=%p, WorkType=%d, NotificationId=%d.",
                __FUNCTION__,
                (VOID*)WorkRequest->PepInternalDevice,
                (ULONG)WorkRequest->WorkType,
                (ULONG)WorkRequest->NotificationId);

    // Move the request into the completed queue.
    //
    DMF_ModuleAuxiliaryLock(g_DmfModuleAcpiPepDevice,
                            0);
    InsertTailList(&moduleContext->PepCompletedWorkList,
                   &(WorkRequest->ListEntry));
    DMF_ModuleAuxiliaryUnlock(g_DmfModuleAcpiPepDevice,
                              0);

    // Request Windows Runtime Power framework to query PEP for more work. It
    // will be notified of completed work in the context of that query.
    //
    moduleContext->PepKernelInformation.RequestWorker(moduleContext->PepKernelInformation.Plugin);
    return;
}

VOID
AcpiPepDevice_NotificationHandlerInvoke(
    _In_ DMFMODULE DmfModule,
    _In_ PEP_NOTIFICATION_CLASS WorkType,
    _In_opt_ PEP_WORK_CONTEXT* WorkRequest,
    _In_ PEP_HANDLER_TYPE HandlerType,
    _In_ ULONG NotificationId,
    _In_opt_ PEP_INTERNAL_DEVICE_HEADER* PepInternalDevice,
    _In_ VOID* Data,
    _In_ SIZE_T DataSize,
    _In_opt_ NTSTATUS* WorkRequestStatus
    )
/*++

Routine Description:

    This routine invokes the handler of the specified type if one is
    registered.

Arguments:

    DmfModule - This Module's handle.
    WorkType - Supplies the type of the work (ACPI/DPM/PPM).
    WorkRequest -Supplies optional pointer to the work context for async work.
    HandlerType - Supplies the type of handler to be invoked.
    NotificationId - Supplies the PEP notification type.
    PepInternalDevice - Supplies the internal PEP device.
    Data - Supplies a pointer to data to be passed to the handler.
    DataSize - Supplies the size of the data.
    WorkRequestStatus -  Supplies optional pointer to report the status of the work request.

Return Value:

    None.

--*/
{
    ULONG dpmNotificationHandlerCount;
    PEP_DEVICE_DEFINITION* deviceDefinition;
    PEP_ACPI_EVALUATE_CONTROL_METHOD* ecmBuffer;
    PEP_NOTIFICATION_HANDLER_ROUTINE* handler;
    PEP_NOTIFICATION_HANDLER_RESULT handlerResult;
    ULONG tableIndex;
    BOOLEAN noSyncHandler;
    PEP_WORK_INFORMATION* poFxWorkInformation;
    PEP_DEVICE_NOTIFICATION_HANDLER* table;

    UNREFERENCED_PARAMETER(DmfModule);

    deviceDefinition = PepInternalDevice->DeviceDefinition;
    if (WorkRequest != NULL)
    {
        poFxWorkInformation = &(WorkRequest->LocalPoFxWorkInfo);
    }
    else
    {
        poFxWorkInformation = NULL;
    }

    switch (WorkType)
    {
        case PEP_NOTIFICATION_CLASS_ACPI:
            dpmNotificationHandlerCount = deviceDefinition->AcpiNotificationHandlerCount;
            table = deviceDefinition->AcpiNotificationHandlers;
            break;

        case PEP_NOTIFICATION_CLASS_DPM:
            dpmNotificationHandlerCount = deviceDefinition->DpmNotificationHandlerCount;
            table = deviceDefinition->DpmNotificationHandlers;
            break;

        default:
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "%s: Unknown WorkType = %d.",
                        __FUNCTION__,
                        (ULONG)WorkType);
            goto Exit;
    }

    for (tableIndex = 0; tableIndex < dpmNotificationHandlerCount; tableIndex += 1)
    {
        if (table[tableIndex].Notification != NotificationId)
        {
            continue;
        }

        handler = NULL;
        noSyncHandler = FALSE;
        switch (HandlerType)
        {
            case PepHandlerTypeSyncCritical:
                handler = table[tableIndex].Handler;
                if (handler == NULL)
                {
                    noSyncHandler = TRUE;
                    handler = table[tableIndex].WorkerCallbackHandler;
                }

            break;

        case PepHandlerTypeWorkerCallback:
            handler = table[tableIndex].WorkerCallbackHandler;
            break;

        default:
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "%s: Unknown HandlerType = %d.",
                        __FUNCTION__,
                        (ULONG)HandlerType);

            goto Exit;
        }

        if ((WorkType == PEP_NOTIFICATION_CLASS_ACPI) &&
            (WorkRequest != NULL))
        {

            switch (NotificationId)
            {
                case PEP_NOTIFY_ACPI_EVALUATE_CONTROL_METHOD:
                    ecmBuffer = (PEP_ACPI_EVALUATE_CONTROL_METHOD*)Data;
                    poFxWorkInformation->ControlMethodComplete.OutputArguments = ecmBuffer->OutputArguments;
                    poFxWorkInformation->ControlMethodComplete.OutputArgumentSize = ecmBuffer->OutputArgumentSize;
                    break;

                default:
                    break;
            }
        }

        handlerResult = PEP_NOTIFICATION_HANDLER_MAX;
        if (handler != NULL)
        {
            if (noSyncHandler == FALSE)
            {
                handlerResult = handler(PepInternalDevice->DmfModule,
                                        Data,
                                        poFxWorkInformation);

                // Result is expected to be either complete or need more work.
                //
                DmfAssert(handlerResult < PEP_NOTIFICATION_HANDLER_MAX);
            }

            // If the handler completes the request and the work context is
            // not NULL, report to PoFx.
            //
            if (noSyncHandler == FALSE &&
                handlerResult == PEP_NOTIFICATION_HANDLER_COMPLETE)
            {
                if (WorkRequest != NULL)
                {
                    AcpiPepDevice_WorkRequestComplete(WorkRequest);
                }

            }
            else
            {
                // Make sure the request has been dequeued.
                //
                DmfAssert((WorkRequest == NULL) ||
                          (IsListEmpty(&WorkRequest->ListEntry) != FALSE));

                DmfAssert((noSyncHandler != FALSE) ||
                          (handlerResult == PEP_NOTIFICATION_HANDLER_MORE_WORK));

                // If the handler needs to do async work, schedule a worker.
                //
                AcpiPepDevice_NotificationHandlerSchedule(PepInternalDevice->DmfModule,
                                                          WorkType,
                                                          NotificationId,
                                                          PepInternalDevice,
                                                          Data,
                                                          DataSize,
                                                          WorkRequestStatus);
            }
        }

        break;
    }

Exit:
    ;
}

VOID
AcpiPepDevice_DeviceRegister(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_bytes_(sizeof(PEP_ACPI_REGISTER_DEVICE)) VOID* Data
    )
/*++

Routine Description:

    This routine is called to claim responsibility for a device. As part of
    registration, it will invoke the device-specific registered routine if
    one is supplied.

Arguments:

    DmfModule - This Module's handle.
    Data - Supplies a pointer to a PEP_ACPI_REGISTER_DEVICE structure.

Return Value:

    None.

--*/
{

    BOOLEAN deviceAccepted;
    PEP_DEVICE_DEFINITION* deviceDefinition;
    ULONG instancePathOffset;
    PEP_INTERNAL_DEVICE_HEADER* pepInternalDevice;
    WDFMEMORY pepInternelDeviceMemory;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    PEP_ACPI_REGISTER_DEVICE* registerDevice;
    ULONG sizeNeeded;
    NTSTATUS ntStatus;
    DMFMODULE dmfModulePepClient;

    DMF_CONTEXT_AcpiPepDevice* moduleContext = DMF_CONTEXT_GET(g_DmfModuleAcpiPepDevice);

    pepInternalDevice = NULL;
    registerDevice = (PEP_ACPI_REGISTER_DEVICE*)Data;
    deviceAccepted = AcpiPepDevice_IsDeviceAccepted(DmfModule,
                                                    PEP_NOTIFICATION_CLASS_ACPI,
                                                    registerDevice->AcpiDeviceName,
                                                    &deviceDefinition,
                                                    &dmfModulePepClient);
    if (deviceAccepted == FALSE)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "%s: %s: Device registration routine "
                    "failed. Device = %S.",
                    __FUNCTION__,
                    PepAcpiNotificationHandlers[PEP_NOTIFY_ACPI_REGISTER_DEVICE].Name,
                    registerDevice->AcpiDeviceName->Buffer);

        registerDevice->DeviceHandle = NULL;
        goto Exit;
    }

    instancePathOffset = ALIGN_UP_BY(deviceDefinition->ContextSize,
                                     sizeof(WCHAR));

    sizeNeeded = instancePathOffset +
                 registerDevice->AcpiDeviceName->Length +
                 sizeof(WCHAR);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;

    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               sizeNeeded,
                               &pepInternelDeviceMemory,
                               (VOID**)&pepInternalDevice);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,  DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        registerDevice->DeviceHandle = NULL;
        goto Exit;
    }

    RtlZeroMemory(pepInternalDevice,
                  sizeNeeded);
    pepInternalDevice->PepInternalDeviceMemory = pepInternelDeviceMemory;
    pepInternalDevice->DmfModule = dmfModulePepClient;
    pepInternalDevice->KernelHandle = registerDevice->KernelHandle;
    pepInternalDevice->DeviceType = deviceDefinition->Type;
    pepInternalDevice->DeviceDefinition = deviceDefinition;
    pepInternalDevice->InstancePath = (PWSTR)OffsetToPtr(pepInternalDevice,
                                                         instancePathOffset);

    RtlCopyMemory(pepInternalDevice->InstancePath,
                  registerDevice->AcpiDeviceName->Buffer,
                  registerDevice->AcpiDeviceName->Length);

    // Invoke the device initialization routine if one is supplied.
    //
    if (deviceDefinition->Initialize != NULL)
    {
        ntStatus = deviceDefinition->Initialize(dmfModulePepClient,
                                                pepInternalDevice);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "%s: %s: Device initialization "
                        "routine failed. Status = %!STATUS!.",
                        __FUNCTION__,
                        PepAcpiNotificationHandlers[
                            PEP_NOTIFY_ACPI_REGISTER_DEVICE].Name,
                        ntStatus);

            registerDevice->DeviceHandle = NULL;
            goto Exit;
        }
    }

    // Invoke the device-specific registered routine if one is supplied.
    //
    AcpiPepDevice_NotificationHandlerInvoke(dmfModulePepClient,
                                            PEP_NOTIFICATION_CLASS_ACPI,
                                            NULL,
                                            PepHandlerTypeSyncCritical,
                                            PEP_NOTIFY_ACPI_REGISTER_DEVICE,
                                            pepInternalDevice,
                                            Data,
                                            sizeof(PEP_ACPI_REGISTER_DEVICE),
                                            NULL);

    // Store the device inside the internal list.
    //
    DMF_ModuleLock(DmfModule);
    InsertTailList(&moduleContext->PepDeviceList,
                   &pepInternalDevice->ListEntry);
    DMF_ModuleUnlock(DmfModule);

    // Return the ACPI handle back to the PoFx.
    //
    registerDevice->DeviceHandle = (PEPHANDLE)pepInternalDevice;
    registerDevice->OutputFlags = PEP_ACPI_REGISTER_DEVICE_OUTPUT_FLAG_NONE;
    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE,
                "%s: %s: SUCCESS! Device = %S, "
                "PEPHANDLE = %p\n.",
                __FUNCTION__,
                PepAcpiNotificationHandlers[
                    PEP_NOTIFY_ACPI_REGISTER_DEVICE].Name,
                registerDevice->AcpiDeviceName->Buffer,
                pepInternalDevice);

    pepInternalDevice = NULL;

Exit:

    if (pepInternalDevice != NULL)
    {
        if (pepInternalDevice->PepInternalDeviceMemory != NULL)
        {
            WdfObjectDelete(pepInternalDevice->PepInternalDeviceMemory);
        }
    }
}

PWSTR
AcpiPepDevice_DeviceNameGet(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG DeviceType
    )
/*++

Routine Description:

    This routine retrives the device Id by device type.

Arguments:

    DmfModule - This Module's handle.
    DeviceType - Supplies a unique identifier of the device.

Return Value:

    DeviceId if the device is accepted by PEP;
    NULL otherwise.

--*/
{
    ULONG objectIndex;
    PWSTR deviceId;
    DMF_CONTEXT_AcpiPepDevice* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    deviceId = NULL;
    for (objectIndex = 0; objectIndex < moduleContext->PepDeviceMatchArraySize; objectIndex += 1)
    {
        if (moduleContext->PepDeviceMatchArray[objectIndex].Type == DeviceType)
        {
            deviceId = moduleContext->PepDeviceMatchArray[objectIndex].DeviceId;
            break;
        }
    }

    return deviceId;
}

VOID
AcpiPepDevice_DeviceUnregister(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_bytes_(sizeof(PEP_ACPI_UNREGISTER_DEVICE)) VOID* Data
    )
/*++

Routine Description:

    This routine is called to release responsibility for a device. As part of
    unregistration, it will invoke the device-specific registered routine if
    one is supplied.

Arguments:

    DmfModule - This Module's handle.
    Data - Supplies a pointer to a PEP_ACPI_UNREGISTER_DEVICE structure.

Return Value:

    None.

--*/
{
    PWSTR deviceName;
    PEP_INTERNAL_DEVICE_HEADER* pepInternalDevice;
    PEP_ACPI_UNREGISTER_DEVICE* unregisterDevice;

    unregisterDevice = (PEP_ACPI_UNREGISTER_DEVICE*)Data;
    pepInternalDevice = (PEP_INTERNAL_DEVICE_HEADER*)unregisterDevice->DeviceHandle;

    DMF_ModuleLock(DmfModule);
    RemoveEntryList(&pepInternalDevice->ListEntry);
    DMF_ModuleUnlock(DmfModule);

    deviceName = AcpiPepDevice_DeviceNameGet(DmfModule,
                                             pepInternalDevice->DeviceType);
    if (deviceName != NULL)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "%s: %s: Device = %S.",
                    __FUNCTION__,
                    PepAcpiNotificationHandlers[PEP_NOTIFY_ACPI_UNREGISTER_DEVICE].Name,
                    deviceName);
    }

    WdfObjectDelete(pepInternalDevice->PepInternalDeviceMemory);
}

VOID
AcpiPepDevice_DeviceNamespaceEnumerate(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_bytes_(sizeof(PEP_ACPI_ENUMERATE_DEVICE_NAMESPACE)) VOID* Data
    )
/*++

Routine Description:

    This routine handles PEP_NOTIFY_ACPI_ENUMERATE_DEVICE_NAMESPACE
    notification. As part of enumeration, it will invoke the device-specific
    registered routine if one is supplied.

Arguments:

    DmfModule - This Module's handle.
    Data - Supplies a pointer to a PEP_ACPI_ENUMERATE_DEVICE_NAMESPACE
        structure.

Return Value:

    None.

--*/
{
    ULONG count;
    PEP_DEVICE_DEFINITION* deviceDefinition;
    PEP_ACPI_ENUMERATE_DEVICE_NAMESPACE* ednBuffer;
    ULONG objectIndex;
    SIZE_T objectBufferSize;
    PEP_INTERNAL_DEVICE_HEADER* pepInternalDevice;
    SIZE_T requiredSize;

    UNREFERENCED_PARAMETER(DmfModule);

    ednBuffer = (PEP_ACPI_ENUMERATE_DEVICE_NAMESPACE*)Data;
    pepInternalDevice = (PEP_INTERNAL_DEVICE_HEADER*)ednBuffer->DeviceHandle;
    deviceDefinition = pepInternalDevice->DeviceDefinition;

    // Always return method count regardless of success or failure.
    //
    ednBuffer->ObjectCount = deviceDefinition->ObjectCount;

    // Check if the output buffer size is sufficient or not.
    //
    objectBufferSize = ednBuffer->ObjectBufferSize;
    count = deviceDefinition->ObjectCount;
    requiredSize = count * sizeof(PEP_ACPI_OBJECT_NAME_WITH_TYPE);
    if (objectBufferSize < requiredSize)
    {
        ednBuffer->Status = STATUS_BUFFER_TOO_SMALL;
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "%s: %s: "
                    "Insufficient buffer size. "
                    "Required = %d, Provided = %d.",
                    __FUNCTION__,
                    PepAcpiNotificationHandlers[
                        PEP_NOTIFY_ACPI_ENUMERATE_DEVICE_NAMESPACE].Name,
                    (ULONG)requiredSize,
                    (ULONG)objectBufferSize);

        goto Exit;
    }

    for (objectIndex = 0; objectIndex < count; objectIndex += 1)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "%s: %s: Enumerate method %d.",
                    __FUNCTION__,
                    PepAcpiNotificationHandlers[
                        PEP_NOTIFY_ACPI_ENUMERATE_DEVICE_NAMESPACE].Name,
                    (ULONG)deviceDefinition->Objects[objectIndex].ObjectName);

        ednBuffer->Objects[objectIndex].Name.NameAsUlong = deviceDefinition->Objects[objectIndex].ObjectName;

        ednBuffer->Objects[objectIndex].Type = deviceDefinition->Objects[objectIndex].ObjectType;
    }

    ednBuffer->Status = STATUS_SUCCESS;

    // Invoke the device-specific registered routine if one is supplied.
    //
    AcpiPepDevice_NotificationHandlerInvoke(pepInternalDevice->DmfModule,
                                            PEP_NOTIFICATION_CLASS_ACPI,
                                            NULL,
                                            PepHandlerTypeSyncCritical,
                                            PEP_NOTIFY_ACPI_ENUMERATE_DEVICE_NAMESPACE,
                                            pepInternalDevice,
                                            Data,
                                            sizeof(PEP_ACPI_ENUMERATE_DEVICE_NAMESPACE),
                                            NULL);

Exit:
    ;
}

VOID
AcpiPepDevice_ObjectInformationQuery(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_bytes_(sizeof(PEP_ACPI_QUERY_OBJECT_INFORMATION)) VOID* Data
    )
/*++

Routine Description:

    This routine handles PEP_NOTIFY_ACPI_QUERY_OBJECT_INFORMATION
    notification. As part of query, it will invoke the device-specific
    registered routine if one is supplied.

Arguments:

    DmfModule - This Module's handle.
    Data - Supplies a pointer to a PEP_NOTIFY_ACPI_QUERY_OBJECT_INFORMATION
        structure.

Return Value:

    None.

--*/
{
    ULONG count;
    PEP_DEVICE_DEFINITION* deviceDefinition;
    ULONG objectsIndex;
    PEP_ACPI_QUERY_OBJECT_INFORMATION* qoiBuffer;
    PEP_INTERNAL_DEVICE_HEADER* pepInternalDevice;

    UNREFERENCED_PARAMETER(DmfModule);

    qoiBuffer = (PEP_ACPI_QUERY_OBJECT_INFORMATION*)Data;
    pepInternalDevice = (PEP_INTERNAL_DEVICE_HEADER*)qoiBuffer->DeviceHandle;
    deviceDefinition = pepInternalDevice->DeviceDefinition;
    count = deviceDefinition->ObjectCount;
    for (objectsIndex = 0; objectsIndex < count; objectsIndex += 1)
    {
        if (qoiBuffer->Name.NameAsUlong == deviceDefinition->Objects[objectsIndex].ObjectName)
        {
            qoiBuffer->MethodObject.InputArgumentCount = deviceDefinition->Objects[objectsIndex].InputArgumentCount;

            qoiBuffer->MethodObject.OutputArgumentCount = deviceDefinition->Objects[objectsIndex].OutputArgumentCount;
        }
    }

    // Invoke the device-specific registered routine if one is supplied.
    //
    AcpiPepDevice_NotificationHandlerInvoke(pepInternalDevice->DmfModule,
                                            PEP_NOTIFICATION_CLASS_ACPI,
                                            NULL,
                                            PepHandlerTypeSyncCritical,
                                            PEP_NOTIFY_ACPI_QUERY_OBJECT_INFORMATION,
                                            pepInternalDevice,
                                            Data,
                                            sizeof(PEP_ACPI_QUERY_OBJECT_INFORMATION),
                                            NULL);
}

VOID
AcpiPepDevice_ControlMethodEvaluate(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_bytes_(sizeof(PEP_ACPI_EVALUATE_CONTROL_METHOD)) VOID* Data
    )
/*++

Routine Description:

    This routine handles PEP_NOTIFY_ACPI_EVALUATE_CONTROL_METHOD
    notification. By default, this routine will fail the evaluation. If
    device-specific registered routine is applied, it will be called instead.

Arguments:

    DmfModule - This Module's handle.
    Data - Supplies a pointer to a PEP_ACPI_EVALUATE_CONTROL_METHOD structure.

Return Value:

    None.

--*/
{
    PEP_ACPI_EVALUATE_CONTROL_METHOD* ecmBuffer;
    PEP_INTERNAL_DEVICE_HEADER* pepInternalDevice;

    UNREFERENCED_PARAMETER(DmfModule);

    ecmBuffer = (PEP_ACPI_EVALUATE_CONTROL_METHOD*)Data;
    pepInternalDevice = (PEP_INTERNAL_DEVICE_HEADER*)ecmBuffer->DeviceHandle;

    // By default, assume the method evaluation will fail.
    //
    ecmBuffer->MethodStatus = STATUS_NOT_IMPLEMENTED;

    // Invoke the device-specific registered routine if one is supplied.
    //
    AcpiPepDevice_NotificationHandlerInvoke(pepInternalDevice->DmfModule,
                                            PEP_NOTIFICATION_CLASS_ACPI,
                                            NULL,
                                            PepHandlerTypeSyncCritical,
                                            PEP_NOTIFY_ACPI_EVALUATE_CONTROL_METHOD,
                                            pepInternalDevice,
                                            Data,
                                            sizeof(PEP_ACPI_EVALUATE_CONTROL_METHOD),
                                            &ecmBuffer->MethodStatus);
}

VOID
AcpiPepDevice_DeviceControlResourcesQuery (
    _In_ DMFMODULE DmfModule,
    _Inout_updates_bytes_(sizeof(PEP_ACPI_QUERY_DEVICE_CONTROL_RESOURCES)) VOID* Data
    )
/*++

Routine Description:

    This routine handles PEP_NOTIFY_ACPI_QUERY_DEVICE_CONTROL_RESOURCES
    notification. By default, this routine assumes no resource needed. If
    device-specific registered routine is applied, it will be called instead.
    If no handler is implemented, mark the request as success to indicate no
    resource is needed.

Arguments:

    DmfModule - This Module's handle.
    Data - Supplies a pointer to a
        PEP_NOTIFY_ACPI_QUERY_DEVICE_CONTROL_RESOURCES structure.

Return Value:

    None.

--*/
{
    PEP_INTERNAL_DEVICE_HEADER* pepInternalDevice;
    PEP_ACPI_QUERY_DEVICE_CONTROL_RESOURCES* resourceBuffer;

    UNREFERENCED_PARAMETER(DmfModule);

    resourceBuffer = (PEP_ACPI_QUERY_DEVICE_CONTROL_RESOURCES*)Data;
    pepInternalDevice = (PEP_INTERNAL_DEVICE_HEADER*)resourceBuffer->DeviceHandle;

    // By default, assume the device doesn't need any BIOS control resources.
    // If they are required, a device-specific handler will be installed to
    // fill in the appropriate values.
    //
    resourceBuffer->Status = STATUS_NOT_IMPLEMENTED;

    // Invoke the device-specific registered routine if one is supplied.
    //
    AcpiPepDevice_NotificationHandlerInvoke(pepInternalDevice->DmfModule,
                                            PEP_NOTIFICATION_CLASS_ACPI,
                                            NULL,
                                            PepHandlerTypeSyncCritical,
                                            PEP_NOTIFY_ACPI_QUERY_DEVICE_CONTROL_RESOURCES,
                                            pepInternalDevice,
                                            Data,
                                            sizeof(PEP_ACPI_QUERY_DEVICE_CONTROL_RESOURCES),
                                            &resourceBuffer->Status);

    // If no handler was implemented, then succeed the request to indicate
    // no resources are needed.
    //
    if (resourceBuffer->Status == STATUS_NOT_IMPLEMENTED)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    DMF_TRACE,
                    "%s: %s: No resource required.",
                    __FUNCTION__,
                    PepAcpiNotificationHandlers[PEP_NOTIFY_ACPI_QUERY_DEVICE_CONTROL_RESOURCES].Name);

        resourceBuffer->BiosResourcesSize = 0;
        resourceBuffer->Status = STATUS_SUCCESS;
    }
}

VOID
AcpiPepDevice_TranslatedDeviceControlResources (
    _In_ DMFMODULE DmfModule,
    _Inout_updates_bytes_(sizeof(PEP_ACPI_TRANSLATED_DEVICE_CONTROL_RESOURCES)) VOID* Data
    )
/*++

Routine Description:

    This routine handles PEP_NOTIFY_ACPI_TRANSLATED_DEVICE_CONTROL_RESOURCES
    notification. By default, this routine does nothing. If device-specific
    registered routine is applied, it will be called instead.

Arguments:

    DmfModule - This Module's handle.
    Data - Supplies a pointer a
           PEP_NOTIFY_ACPI_TRANSLATED_DEVICE_CONTROL_RESOURCES structure.

Return Value:

    None.

--*/
{
    PEP_INTERNAL_DEVICE_HEADER* pepInternalDevice;
    PEP_ACPI_TRANSLATED_DEVICE_CONTROL_RESOURCES* resourceBuffer;

    UNREFERENCED_PARAMETER(DmfModule);

    resourceBuffer = (PEP_ACPI_TRANSLATED_DEVICE_CONTROL_RESOURCES*)Data;
    pepInternalDevice = (PEP_INTERNAL_DEVICE_HEADER*)resourceBuffer->DeviceHandle;

    // Invoke the device-specific registered routine if one is supplied.
    //
    AcpiPepDevice_NotificationHandlerInvoke(pepInternalDevice->DmfModule,
                                            PEP_NOTIFICATION_CLASS_ACPI,
                                            NULL,
                                            PepHandlerTypeSyncCritical,
                                            PEP_NOTIFY_ACPI_TRANSLATED_DEVICE_CONTROL_RESOURCES,
                                            pepInternalDevice,
                                            Data,
                                            sizeof(PEP_ACPI_TRANSLATED_DEVICE_CONTROL_RESOURCES),
                                            NULL);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
AcpiPepDevice_WorkRequestDestroy(
    _In_ WDFMEMORY WorkRequestMemory
    )
/*++

Routine Description:

    This routine destroys the given work request.

Arguments:

    WorkRequest - Supplies a pointer to the work request.

Return Value:

    None.

--*/
{
    PEP_WORK_CONTEXT* workRequest;

    workRequest = (PEP_WORK_CONTEXT*)WdfMemoryGetBuffer(WorkRequestMemory,
                                                        NULL);

    if (workRequest->WorkContextMemory != NULL)
    {
        WdfObjectDelete(workRequest->WorkContextMemory);
    }

    WdfObjectDelete(WorkRequestMemory);
}

VOID
AcpiPepDevice_WorkRequestsProcess(
    _In_ VOID* Data
    )
/*++

Routine Description:

    This function completes work by calling into the specific
    completion handler, which is responsible for filling in the PEP_WORK
    structure.

    Differences with pending worker routine:
        - Keeps invoking PepKernelInformation.RequestWorker until the
          completed work queue is not drained completely.

Arguments:

    Data - Supplies a pointer to the PEP_WORK structure.

Return Value:

    None.

--*/
{

    LIST_ENTRY* nextEntry;
    BOOLEAN moreWork;
    PEP_WORK_CONTEXT* workRequest;
    PEP_WORK* poFxWork;

    moreWork = FALSE;
    nextEntry = NULL;
    poFxWork = (PEP_WORK*)Data;

    DMF_CONTEXT_AcpiPepDevice* moduleContext = DMF_CONTEXT_GET(g_DmfModuleAcpiPepDevice);

    // Grab the next item from the completed work queue.
    //
    DMF_ModuleAuxiliaryLock(g_DmfModuleAcpiPepDevice,
                            0);

    if (IsListEmpty(&moduleContext->PepCompletedWorkList) == FALSE)
    {
        nextEntry = RemoveHeadList(&moduleContext->PepCompletedWorkList);

        // Check if there is more work after this request.
        //
        if (IsListEmpty(&moduleContext->PepCompletedWorkList) == FALSE)
        {
            moreWork = TRUE;
        }
    }

    // Drop the request list lock prior to processing work.
    //
    DMF_ModuleAuxiliaryUnlock(g_DmfModuleAcpiPepDevice,
                              0);

    // If a completed request was found, report back to PoFx and
    // reclaim its resources.
    //
    if (nextEntry != NULL)
    {
        InitializeListHead(nextEntry);
        workRequest = CONTAINING_RECORD(nextEntry,
                                        PEP_WORK_CONTEXT,
                                        ListEntry);

        // Invoke the request processing routine.
        //
        switch (workRequest->WorkType)
        {
            case PEP_NOTIFICATION_CLASS_ACPI:
                poFxWork->NeedWork = TRUE;
                RtlCopyMemory(poFxWork->WorkInformation,
                              &workRequest->LocalPoFxWorkInfo,
                              sizeof(PEP_WORK_INFORMATION));

            case PEP_NOTIFICATION_CLASS_DPM:
                break;

            default:
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "%s: Unknown WorkType = %d.",
                            __FUNCTION__,
                            (ULONG)workRequest->WorkType);
                break;
            }
       }

       // Destroy the request.
       //
       AcpiPepDevice_WorkRequestDestroy(workRequest->WorkRequestMemory);
    }

    // If there is more work left, then request another PEP_WORK.
    //
    if (moreWork != FALSE)
    {
        moduleContext->PepKernelInformation.RequestWorker(moduleContext->PepKernelInformation.Plugin);
    }

    return;
}

VOID
AcpiPepDevice_WorkNotification(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_bytes_(sizeof(PEP_WORK)) VOID* Data
    )
/*++

Routine Description:

    This routine is called to handle PEP_NOTIFY_ACPI_WORK notification. It calls
    the worker routine to drain the completed work queue.

Arguments:

    DmfModule - This Module's handle.
    Data - Supplies a pointer to a PEP_WORK structure.

Return Value:

    None.

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    AcpiPepDevice_WorkRequestsProcess(Data);
}

BOOLEAN
AcpiPepDevice_PepAcpiNotify(
    _In_ ULONG Notification,
    _In_ VOID* Data
    )
/*++

Routine Description:

    Handles all incoming ACPI notifications from the OS.

Arguments:

    Notification - Supplies the notification type.
    Data - Supplies a pointer to a data structure specific to the
        notification type.

Return Value:

    TRUE if the notification type was recognized;
    FALSE otherwise.

--*/
{
    BOOLEAN recognized;

    recognized = FALSE;
    if (Notification >= ARRAYSIZE(PepAcpiNotificationHandlers))
    {
        goto Exit;
    }

    if ((PepAcpiNotificationHandlers[Notification].Notification == 0) ||
        (PepAcpiNotificationHandlers[Notification].Handler == NULL))
    {

        goto Exit;
    }

    if (g_DmfModuleAcpiPepDevice == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                DMF_TRACE,
                "%s:  Failed! g_DmfModuleAcpiPepDevice is NULL",
                __FUNCTION__);
        goto Exit;
    }

    PepAcpiNotificationHandlers[Notification].Handler(g_DmfModuleAcpiPepDevice,
                                                      Data);
    recognized = TRUE;

Exit:

    return recognized;
}

NTSTATUS
AcpiPepDevice_PepRegisterWithPoFx()
/*++

Routine Description:

    This routine registers as a power engine plugin with the OS.

Arguments:

    None

Return Value:

    NTSTATUS.

--*/
{
    PEP_INFORMATION pepInformation;
    NTSTATUS ntStatus;

    ntStatus = STATUS_UNSUCCESSFUL;

    if (g_DmfModuleAcpiPepDevice == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                DMF_TRACE,
                "%s:  Failed! g_DmfModuleAcpiPepDevice is NULL",
                __FUNCTION__);
        goto Exit;
    }

    DMF_CONTEXT_AcpiPepDevice* moduleContext = DMF_CONTEXT_GET(g_DmfModuleAcpiPepDevice);

    RtlZeroMemory(&pepInformation,
                  sizeof(pepInformation));
    pepInformation.Version = PEP_INFORMATION_VERSION;
    pepInformation.Size = sizeof(pepInformation);
    pepInformation.AcceptAcpiNotification = AcpiPepDevice_PepAcpiNotify;

    RtlZeroMemory(&moduleContext->PepKernelInformation,
                  sizeof(moduleContext->PepKernelInformation));
    moduleContext->PepKernelInformation.Version = PEP_KERNEL_INFORMATION_V3;
    moduleContext->PepKernelInformation.Size = sizeof(moduleContext->PepKernelInformation);

    ntStatus = PoFxRegisterPlugin(&pepInformation,
                                  &moduleContext->PepKernelInformation);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "%s: PoFxRegisterPlugin() Failed! ntStatus = %!STATUS!.",
                    __FUNCTION__,
                    ntStatus);

        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "PEP Registration successful.");

    DmfAssert(moduleContext->PepKernelInformation.Plugin != NULL);
    DmfAssert(moduleContext->PepKernelInformation.RequestWorker != NULL);

Exit:

    return ntStatus;
}

NTSTATUS
AcpiPepDevice_PepRegister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This Method registers the accumulated device tables with Platform Extensions Plugin.

Arguments:

    DmfModule - Handle to this Module.

Return Value:

    NTSTATUS.

--*/
{
    size_t sizeToAllocate;
    ULONG numberOfEntries;
    DMF_CONTEXT_AcpiPepDevice* moduleContext;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    NTSTATUS ntStatus;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    numberOfEntries = WdfCollectionGetCount(moduleContext->PepDefinitionTableCollection);
    DmfAssert(numberOfEntries == WdfCollectionGetCount(moduleContext->PepMatchTableCollection));

    // Device definition array has all elements of collection and root.
    //
    sizeToAllocate = sizeof(PEP_DEVICE_DEFINITION) * (numberOfEntries + 1);

    TraceEvents(TRACE_LEVEL_VERBOSE,DMF_TRACE,"SizeToAllocate=%llu NumEntries=%llu",sizeToAllocate,numberOfEntries + 1);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               sizeToAllocate,
                               &moduleContext->DeviceDefinitionMemory,
                               (VOID**)&moduleContext->PepDeviceDefinitionArray);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Index 0 belongs to the ACPI root.
    //
    RtlCopyMemory(&moduleContext->PepDeviceDefinitionArray[0],
                  &moduleContext->PepRootDefinition,
                  sizeof(PEP_DEVICE_DEFINITION));

    TraceEvents(TRACE_LEVEL_VERBOSE,DMF_TRACE,"Printing PepDeviceDefinitionArray[0] ");
    TraceEvents(TRACE_LEVEL_VERBOSE,DMF_TRACE,"Type=%lx "
                                              "ContextSize=%lx "
                                              "Initialize=%p "
                                              "ObjectCount=%lx "
                                              "Objects=%p "
                                              "AcpiNotificationHandlerCount=%lx "
                                              "AcpiNotificationHandlers=%p "
                                              "DpmNotificationHandlerCount=%lx "
                                              "DpmNotificationHandlers=%p ",
                                              moduleContext->PepDeviceDefinitionArray[0].Type,
                                              moduleContext->PepDeviceDefinitionArray[0].ContextSize,
                                              moduleContext->PepDeviceDefinitionArray[0].Initialize,
                                              moduleContext->PepDeviceDefinitionArray[0].ObjectCount,
                                              moduleContext->PepDeviceDefinitionArray[0].Objects,
                                              moduleContext->PepDeviceDefinitionArray[0].AcpiNotificationHandlerCount,
                                              moduleContext->PepDeviceDefinitionArray[0].AcpiNotificationHandlers,
                                              moduleContext->PepDeviceDefinitionArray[0].DpmNotificationHandlerCount,
                                              moduleContext->PepDeviceDefinitionArray[0].DpmNotificationHandlers);

    for (ULONG objectIndex = 0; objectIndex < moduleContext->PepDeviceDefinitionArray[0].ObjectCount; objectIndex++)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE,DMF_TRACE,"Object %d"
                                                  "ObjectName=%lx "
                                                  "InputArgumentCount=%lx "
                                                  "OutputArgumentCount=%lx "
                                                  "ObjectType=%d ",
                                                  objectIndex,
                                                  moduleContext->PepDeviceDefinitionArray[0].Objects[objectIndex].ObjectName,
                                                  moduleContext->PepDeviceDefinitionArray[0].Objects[objectIndex].InputArgumentCount,
                                                  moduleContext->PepDeviceDefinitionArray[0].Objects[objectIndex].OutputArgumentCount,
                                                  moduleContext->PepDeviceDefinitionArray[0].Objects[objectIndex].ObjectType);

    }

    // Add device specific entries to the Definition table.
    //
    for (ULONG collectionIndex = 0; collectionIndex < numberOfEntries; collectionIndex++)
    {
        PEP_DEVICE_DEFINITION* pepDeviceDefinitionEntry;
        WDFMEMORY pepDeviceDefinitionMemory;

        pepDeviceDefinitionMemory = (WDFMEMORY)WdfCollectionGetItem(moduleContext->PepDefinitionTableCollection,
                                                                    collectionIndex);

        pepDeviceDefinitionEntry = (PEP_DEVICE_DEFINITION*)WdfMemoryGetBuffer(pepDeviceDefinitionMemory,
                                                                              NULL);
        ULONG targetIndex = collectionIndex + 1;

        RtlCopyMemory(&moduleContext->PepDeviceDefinitionArray[targetIndex],
                      pepDeviceDefinitionEntry,
                      sizeof(PEP_DEVICE_DEFINITION));

        TraceEvents(TRACE_LEVEL_VERBOSE,DMF_TRACE,"Printing PepDeviceDefinitionArray[%d] ", targetIndex);
        TraceEvents(TRACE_LEVEL_VERBOSE,DMF_TRACE,"Type=%lx "
                                                  "ContextSize=%lx "
                                                  "Initialize=%p "
                                                  "ObjectCount=%lx "
                                                  "Objects=%p "
                                                  "AcpiNotificationHandlerCount=%lx "
                                                  "AcpiNotificationHandlers=%p "
                                                  "DpmNotificationHandlerCount=%lx "
                                                  "DpmNotificationHandlers=%p ",
                                                  moduleContext->PepDeviceDefinitionArray[targetIndex].Type,
                                                  moduleContext->PepDeviceDefinitionArray[targetIndex].ContextSize,
                                                  moduleContext->PepDeviceDefinitionArray[targetIndex].Initialize,
                                                  moduleContext->PepDeviceDefinitionArray[targetIndex].ObjectCount,
                                                  moduleContext->PepDeviceDefinitionArray[targetIndex].Objects,
                                                  moduleContext->PepDeviceDefinitionArray[targetIndex].AcpiNotificationHandlerCount,
                                                  moduleContext->PepDeviceDefinitionArray[targetIndex].AcpiNotificationHandlers,
                                                  moduleContext->PepDeviceDefinitionArray[targetIndex].DpmNotificationHandlerCount,
                                                  moduleContext->PepDeviceDefinitionArray[targetIndex].DpmNotificationHandlers);

        for (ULONG objectIndex = 0; objectIndex < moduleContext->PepDeviceDefinitionArray[targetIndex].ObjectCount; objectIndex++)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,DMF_TRACE,"Object %d "
                                                      "ObjectName=%lx "
                                                      "InputArgumentCount=%lx "
                                                      "OutputArgumentCount=%lx "
                                                      "ObjectType=%d ",
                                                      objectIndex,
                                                      moduleContext->PepDeviceDefinitionArray[targetIndex].Objects[objectIndex].ObjectName,
                                                      moduleContext->PepDeviceDefinitionArray[targetIndex].Objects[objectIndex].InputArgumentCount,
                                                      moduleContext->PepDeviceDefinitionArray[targetIndex].Objects[objectIndex].OutputArgumentCount,
                                                      moduleContext->PepDeviceDefinitionArray[targetIndex].Objects[objectIndex].ObjectType);

        }
    }

    moduleContext->PepDeviceDefinitionArraySize = numberOfEntries + 1;

    // Device Match array has all elements of collection and root.
    //
    sizeToAllocate = sizeof(PEP_DEVICE_MATCH) * (numberOfEntries + 1);

    TraceEvents(TRACE_LEVEL_INFORMATION,DMF_TRACE,"SizeToAllocate=%llu NumEntries=%llu",sizeToAllocate,numberOfEntries);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               sizeToAllocate,
                               &moduleContext->DeviceMatchMemory,
                               (VOID**)&moduleContext->PepDeviceMatchArray);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Index 0 belongs to the ACPI root.
    //
    RtlCopyMemory(&moduleContext->PepDeviceMatchArray[0],
                  &PepRootMatch,
                  sizeof(PEP_DEVICE_DEFINITION));

    TraceEvents(TRACE_LEVEL_VERBOSE,DMF_TRACE,"Printing PepDeviceMatchArray[0] ");
    TraceEvents(TRACE_LEVEL_VERBOSE,DMF_TRACE,"Type=%lx "
                                              "OwnedType=%d "
                                              "DeviceId= %S "
                                              "CompareMethod=%d ",
                                              moduleContext->PepDeviceMatchArray[0].Type,
                                              moduleContext->PepDeviceMatchArray[0].OwnedType,
                                              moduleContext->PepDeviceMatchArray[0].DeviceId,
                                              moduleContext->PepDeviceMatchArray[0].CompareMethod);

    // Add device specific entries to the Definition table.
    //
    for (ULONG collectionIndex = 0; collectionIndex < numberOfEntries; collectionIndex++)
    {
        PEP_DEVICE_MATCH* pepDeviceMatchEntry;
        WDFMEMORY pepDeviceMatchMemory;
        pepDeviceMatchMemory = (WDFMEMORY)WdfCollectionGetItem(moduleContext->PepMatchTableCollection,
                                                               collectionIndex);

        pepDeviceMatchEntry = (PEP_DEVICE_MATCH*)WdfMemoryGetBuffer(pepDeviceMatchMemory,
                                                                    NULL);

        ULONG targetIndex = collectionIndex + 1;

        RtlCopyMemory(&moduleContext->PepDeviceMatchArray[targetIndex],
                      pepDeviceMatchEntry,
                      sizeof(PEP_DEVICE_MATCH));

        TraceEvents(TRACE_LEVEL_VERBOSE,DMF_TRACE,"Printing PepDeviceMatchArray[%d] ",targetIndex);
        TraceEvents(TRACE_LEVEL_VERBOSE,DMF_TRACE,"Type=%lx "
                                                  "OwnedType=%d "
                                                  "DeviceId= %S "
                                                  "CompareMethod=%d ",
                                                  moduleContext->PepDeviceMatchArray[targetIndex].Type,
                                                  moduleContext->PepDeviceMatchArray[targetIndex].OwnedType,
                                                  moduleContext->PepDeviceMatchArray[targetIndex].DeviceId,
                                                  moduleContext->PepDeviceMatchArray[targetIndex].CompareMethod);
    }

    moduleContext->PepDeviceMatchArraySize = numberOfEntries + 1;

    // Register the tables with PoFxPep framework.
    //
    ntStatus = AcpiPepDevice_PepRegisterWithPoFx();
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "%s: PepRegister() Failed! ntStatus = %!STATUS!.",
                    __FUNCTION__,
                    ntStatus);
    }

Exit:

    return ntStatus;
}

NTSTATUS
AcpiPepDevice_AcpiDeviceAdd(
    _In_ DMFMODULE DmfModule,
    _In_ PEP_ACPI_REGISTRATION_TABLES PepAcpiRegistrationTables
    )
/*++

Routine Description:

    This Method is used to add AcpiPepDevice Tables to this Module before registering
    with PoFx. Upon registration the tables added to the collection are dequeued and
    merged into one table registered in OS.

Arguments:

    DmfModule - Handle to this Module
    PepAcpiRegistrationTables - Tables from AcpiPepDevice instance.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_AcpiPepDevice* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = WdfCollectionAdd(moduleContext->PepDefinitionTableCollection,
                                PepAcpiRegistrationTables.AcpiDefinitionTable);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfCollectionAdd fails: ntStatus=%!STATUS!",ntStatus);
        goto Exit;
    }

    ntStatus = WdfCollectionAdd(moduleContext->PepMatchTableCollection,
                                PepAcpiRegistrationTables.AcpiMatchTable);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfCollectionAdd fails: ntStatus=%!STATUS!",ntStatus);
        goto Exit;
    }

Exit:
    return ntStatus;
}

VOID
AcpiPepDevice_ChildArrivalCallback(
    _In_ DMFMODULE DmfModuleChildDevice
    )
/*++

Routine Description:

    Child PEP device calls into this when it is ready.

Arguments:

    DmfModule - Handle to Child Module.

Return Value:

    None.

--*/
{
    PEP_ACPI_REGISTRATION_TABLES pepAcpiRegistrationTables;
    NTSTATUS ntStatus;
    DMFMODULE dmfModulePepDevice;
    DMF_CONTEXT_AcpiPepDevice* moduleContext;
    DMF_CONFIG_AcpiPepDevice* moduleConfig;

    dmfModulePepDevice = DMF_ParentModuleGet(DmfModuleChildDevice);
    moduleContext = DMF_CONTEXT_GET(dmfModulePepDevice);
    moduleConfig = DMF_CONFIG_GET(dmfModulePepDevice);

    // Get the PEP tables from child device;
    //
    ntStatus = DMF_AcpiPepDeviceFan_AcpiDeviceTableGet(DmfModuleChildDevice,
                                                       &pepAcpiRegistrationTables);

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Could not get child tables for PEP registration.");
        goto Exit;
    }

    // Add these tables to main PEP tables.
    //
    ntStatus = AcpiPepDevice_AcpiDeviceAdd(dmfModulePepDevice,
                                           pepAcpiRegistrationTables);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Could not integrate child PEP tables.");
        goto Exit;
    }

    moduleContext->ChildrenRegistered++;
    // Check if all the expected arrivals have come in.
    //
    if (moduleContext->ChildrenRegistered == moduleConfig->ChildDeviceArraySize)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Ready to register with PEP.");
        ntStatus = AcpiPepDevice_PepRegister(dmfModulePepDevice);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Could not register with PEP.");
            goto Exit;
        }
    }

Exit:
    ;
}


#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
AcpiPepDevice_ContextInitialize(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type AcpiPepDevice.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_AcpiPepDevice* moduleContext;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    TraceEvents(TRACE_LEVEL_VERBOSE,
                DMF_TRACE,
                "AcpiPepDevice Open called.");

    // Initialize root definition.
    //
    moduleContext->PepRootDefinition = {PEP_DEVICE_TYPE_ROOT,
                                        sizeof(PEP_ACPI_DEVICE),
                                        NULL,
                                        ARRAYSIZE(RootNativeMethods),
                                        RootNativeMethods,
                                        ARRAYSIZE(RootNotificationHandler),
                                        RootNotificationHandler,
                                        0,
                                        NULL,
                                        g_DmfModuleAcpiPepDevice};

    // This list is protected by a DMF Auxiliary lock.
    //
    InitializeListHead(&moduleContext->PepDeviceList);
    InitializeListHead(&moduleContext->PepPendingWorkList);
    InitializeListHead(&moduleContext->PepCompletedWorkList);

    // Create a collection to hold all the PEP device definition tables.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfCollectionCreate(&objectAttributes,
                                   &moduleContext->PepDefinitionTableCollection);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfCollectionCreate fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    // Create a collection to hold all the PEP device match tables.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfCollectionCreate(&objectAttributes,
                                   &moduleContext->PepMatchTableCollection);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfCollectionCreate fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

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


#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_AcpiPepDevice_ChildModulesAdd(
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
    DMF_CONTEXT_AcpiPepDevice* moduleContext;
    DMF_CONFIG_AcpiPepDevice* moduleConfig;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDFMEMORY childPepDeviceMemory;
    NTSTATUS ntStatus;
    ULONG totalSizeOfChildModules;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Child Module Add called.");

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    DmfAssert(moduleConfig->ChildDeviceConfigurationArray != NULL);
    DmfAssert(moduleConfig->ChildDeviceArraySize > 0);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;

    totalSizeOfChildModules = sizeof(DMFMODULE) * moduleConfig->ChildDeviceArraySize;

    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               PEP_TAG,
                               totalSizeOfChildModules,
                               &childPepDeviceMemory,
                               (VOID**)&moduleContext->ChildPepDeviceModules);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfMemoryCreate fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Child device memory created.");
    // Initialize child Modules based on the passed configuration array.
    //
    for (ULONG childIndex = 0; childIndex < moduleConfig->ChildDeviceArraySize; ++childIndex)
    {
        switch (moduleConfig->ChildDeviceConfigurationArray[childIndex].PepDeviceType)
        {
            case AcpiPepDeviceType_Fan:
            {
                DMF_CONFIG_AcpiPepDeviceFan acpiPepDeviceFanConfig;
                DMF_CONFIG_AcpiPepDeviceFan_AND_ATTRIBUTES_INIT(&acpiPepDeviceFanConfig,
                                                                &moduleAttributes);
                acpiPepDeviceFanConfig = *((DMF_CONFIG_AcpiPepDeviceFan*)moduleConfig->ChildDeviceConfigurationArray[childIndex].PepDeviceConfiguration);
                acpiPepDeviceFanConfig.ArrivalCallback = AcpiPepDevice_ChildArrivalCallback;
                DMF_DmfModuleAdd(DmfModuleInit,
                                 &moduleAttributes,
                                 WDF_NO_OBJECT_ATTRIBUTES,
                                 &moduleContext->ChildPepDeviceModules[childIndex]);

                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "PEP Fan child created.");
                break;
            }
        }
    }
    // Set the children enumerated flag.
    //
    moduleContext->ChildrenEnumerated = TRUE;

Exit:
    ;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ModuleInstanceDestroy)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_AcpiPepDevice_Destroy(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Destroy an instance of this Module.
    This callback is used to clear the global pointer.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    // Clear global instance handle.
    //
    g_DmfModuleAcpiPepDevice = NULL;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_AcpiPepDevice_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type AcpiPepDevice.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_AcpiPepDevice* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ntStatus = STATUS_SUCCESS;

    if (moduleContext->ChildrenEnumerated != TRUE)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "AcpiPepDevice could not initialize children: ntStatus=%!STATUS!", ntStatus);
    }

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
DMF_AcpiPepDevice_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type AcpiPepDevice.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_AcpiPepDevice;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_AcpiPepDevice;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_UNSUCCESSFUL;

    if (g_DmfModuleAcpiPepDevice != NULL)
    {
        // Only one instance of this Module can exist at a time. This handle is used to pass context into the crash
        // dump callbacks called by the OS.
        //
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Only one instance of this Module can exist at time");
        goto Exit;
    }

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_AcpiPepDevice);
    dmfCallbacksDmf_AcpiPepDevice.ModuleInstanceDestroy = DMF_AcpiPepDevice_Destroy;
    dmfCallbacksDmf_AcpiPepDevice.ChildModulesAdd = DMF_AcpiPepDevice_ChildModulesAdd;
    dmfCallbacksDmf_AcpiPepDevice.DeviceOpen = DMF_AcpiPepDevice_Open;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_AcpiPepDevice,
                                            AcpiPepDevice,
                                            DMF_CONTEXT_AcpiPepDevice,
                                            DMF_MODULE_OPTIONS_DISPATCH,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_AcpiPepDevice.CallbacksDmf = &dmfCallbacksDmf_AcpiPepDevice;
    dmfModuleDescriptor_AcpiPepDevice.NumberOfAuxiliaryLocks = 1;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_AcpiPepDevice,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Save global context. The PEP callbacks do not have a context passed into them.
    //
    g_DmfModuleAcpiPepDevice = *DmfModule;

    ntStatus = AcpiPepDevice_ContextInitialize(*DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
PEP_NOTIFICATION_HANDLER_RESULT
DMF_AcpiPepDevice_AsyncNotifyEvent(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Data,
    _Out_opt_ PEP_WORK_INFORMATION* PoFxWorkInformation
    )
/*++

Routine Description:

    This Method serves AcpiPepDevices as a generic callback for Acpi Notification requests.
    It is scheduled to run asychronously.

Arguments:

    Data - Supplies a pointer to parameters buffer for this notification.
    PoFxWorkInformation - Supplies a pointer to the PEP_WORK structure used to
                          report result to PoFx.

Return Value:

    Whether the operation is complete or needs more work.

--*/
{
    PEP_NOTIFICATION_HANDLER_RESULT completeStatus;
    PEP_ACPI_NOTIFY_CONTEXT* notifyContext;
    PEP_INTERNAL_DEVICE_HEADER* pepInternalDevice;

    // NOTE: Handle validation not done here because DmfModule
    // is unused.
    //
    UNREFERENCED_PARAMETER(DmfModule);

    TraceEvents(TRACE_LEVEL_VERBOSE,
                DMF_TRACE,
                "PEP DEVICE: Notify method scheduled to run.");

    notifyContext = (PEP_ACPI_NOTIFY_CONTEXT*)Data;
    pepInternalDevice = notifyContext->PepInternalDevice;
    completeStatus = PEP_NOTIFICATION_HANDLER_COMPLETE;
    PoFxWorkInformation->WorkType = PepWorkAcpiNotify;
    PoFxWorkInformation->AcpiNotify.DeviceHandle = pepInternalDevice->KernelHandle;
    PoFxWorkInformation->AcpiNotify.NotifyCode = notifyContext->NotifyCode;

    return completeStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
DMFMODULE*
DMF_AcpiPepDevice_ChildHandlesReturn(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    This Method provides Client with a handle to all initializes Child Modules.

Arguments:

    DmfModule - Handle to this Module.

Return Value:

    Array of Child DMF Module Handles.

--*/
{
    DMF_CONTEXT_AcpiPepDevice* moduleContext;

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 AcpiPepDeviceFan);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->ChildrenEnumerated)
    {
        return moduleContext->ChildPepDeviceModules;
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Child Module handles are not ready");
        return NULL;
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_AcpiPepDevice_PepAcpiDataReturn(
    _In_ VOID* Value,
    _In_ USHORT ValueType,
    _In_ ULONG ValueLength,
    _In_ BOOLEAN ReturnAsPackage,
    _Out_ PACPI_METHOD_ARGUMENT Arguments,
    _Inout_ SIZE_T* OutputArgumentSize,
    _Out_opt_ ULONG* OutputArgumentCount,
    _Out_ NTSTATUS* ntStatus,
    _In_opt_ CHAR* MethodName,
    _In_opt_ CHAR* DebugInfo,
    _Out_ PEP_NOTIFICATION_HANDLER_RESULT* CompleteResult
    )
/*++

Routine Description:

    This routine returns data of specific type back to PoFx.

Arguments:

    Value - Supplies the pointer to the data returned.
    ValueType - Supplies the type of the data.
    ValueLength - Supplies the length (raw, without ACPI method argument)
                  of the data.
    ReturnAsPackage - Supplies a flag indicating whether to return the data
                      in a package.
    Arguments - Supplies a pointer to receive the returned package.
    OutputArgumentSize - Supplies a pointer to receive the returned
                         argument size.
    OutputArgumentCount - Supplies an optional pointer to receive the returned
                          argument number.
    ntStatus - Supplies a pointer to receive the evaluation result.
    MethodName - Supplies an optional string that names the native method
                 used for logging.
    DebugInfo - Supplies an optional string that contains the debugging
                information to be included in the log.
    CompleteResult - Supplies a pointer to receive the complete result.

Return Value:

    None.

--*/
{
    PACPI_METHOD_ARGUMENT argumentLocal;
    ULONG requiredSize;
    ULONG* valueAsInteger;
    UCHAR* valueAsString;

    TraceEvents(TRACE_LEVEL_VERBOSE,
                DMF_TRACE,
                "%s <%s> [%s]: Start processing.",
                __FUNCTION__,
                NAME_DEBUG_INFO(DebugInfo),
                NAME_NATIVE_METHOD(MethodName));

    requiredSize = ACPI_METHOD_ARGUMENT_LENGTH(ValueLength);
    if (ReturnAsPackage != FALSE)
    {
        argumentLocal = (PACPI_METHOD_ARGUMENT)&Arguments->Data[0];
    }
    else
    {
        argumentLocal = Arguments;
    }

    // Set the returned value base on the type.
    //
    switch (ValueType)
    {
        case ACPI_METHOD_ARGUMENT_INTEGER:
            valueAsInteger = (PULONG)Value;
            ACPI_METHOD_SET_ARGUMENT_INTEGER(argumentLocal, (*valueAsInteger));
            TraceEvents(TRACE_LEVEL_VERBOSE,
                        DMF_TRACE,
                        "%s <%s> [%s]: Returntype = Integer, Result = %#x.",
                        __FUNCTION__,
                        NAME_DEBUG_INFO(DebugInfo),
                        NAME_NATIVE_METHOD(MethodName),
                        (ULONG)argumentLocal->Argument);

            break;

        case ACPI_METHOD_ARGUMENT_STRING:
            valueAsString = (PUCHAR)Value;

            // N.B. ACPI_METHOD_SET_ARGUMENT_STRING will copy the string as
            //      well.
            //      ACPI_METHOD_SET_ARGUMENT_STRING currently has a bug:
            //      error C4267: '=' : conversion from 'size_t' to 'USHORT',
            //      possible loss of data.
            //
            //      error C4057: char * is different from PUCHAR.
            //
            { argumentLocal->Type = ACPI_METHOD_ARGUMENT_STRING;
                argumentLocal->DataLength = (USHORT)(strlen((const char*)valueAsString)) + (USHORT)sizeof(UCHAR);
                memcpy_s(&argumentLocal->Data[0],
                        argumentLocal->DataLength,
                        (PUCHAR)valueAsString,
                        argumentLocal->DataLength); }
            TraceEvents(TRACE_LEVEL_VERBOSE,
                        DMF_TRACE,
                        "%s <%s> [%s]: ReturnType = String, Result = %s.",
                        __FUNCTION__,
                        NAME_DEBUG_INFO(DebugInfo),
                        NAME_NATIVE_METHOD(MethodName),
                        (PSTR)&argumentLocal->Data[0]);

            break;

        case ACPI_METHOD_ARGUMENT_BUFFER:
            valueAsString = (PUCHAR)Value;
            ACPI_METHOD_SET_ARGUMENT_BUFFER(argumentLocal,
                                            valueAsString,
                                            (USHORT)ValueLength);

            TraceEvents(TRACE_LEVEL_VERBOSE,
                        DMF_TRACE,
                        "%s <%s> [%s]: ReturnType = Buffer.",
                        __FUNCTION__,
                        NAME_DEBUG_INFO(DebugInfo),
                        NAME_NATIVE_METHOD(MethodName));

            break;

        default:
            NT_ASSERT(FALSE);
            return;
    }

    if (ReturnAsPackage != FALSE)
    {
        Arguments->Type = ACPI_METHOD_ARGUMENT_PACKAGE_EX;
        Arguments->DataLength = ACPI_METHOD_ARGUMENT_LENGTH_FROM_ARGUMENT(argumentLocal);
    }

    // Return the output argument count, size and status.
    //
    if (OutputArgumentCount != NULL)
    {
        *OutputArgumentCount = 1;
    }

    *OutputArgumentSize = ACPI_METHOD_ARGUMENT_LENGTH_FROM_ARGUMENT(Arguments);

    *ntStatus = STATUS_SUCCESS;

    *CompleteResult = PEP_NOTIFICATION_HANDLER_COMPLETE;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_AcpiPepDevice_ReportNotSupported(
    _In_ DMFMODULE DmfModule,
    _Out_ NTSTATUS* Status,
    _Out_ ULONG* Count,
    _Out_ PEP_NOTIFICATION_HANDLER_RESULT* CompleteResult
    )
/*++

Routine Description:

    This Method reports to PoFx that the notification is not supported.

Arguments:

    DmfModule - Handle to this Module.
    Status - Supplies a pointer to receive the evaluation status.
    Count - Supplies a pointer to receive the output argument count/size.
    CompleteResult - Supplies a pointer to receive the complete result.

Return Value:

    None.

--*/
{
    // NOTE: Handle validation not done here because DMF handle is unused.
    //
    UNREFERENCED_PARAMETER(DmfModule);

    *Count = 0;
    *Status = STATUS_NOT_SUPPORTED;

    TraceEvents(TRACE_LEVEL_ERROR,
                DMF_TRACE,
                "%s [UNKNOWN] Native method not supported.",
                PepAcpiNotificationHandlers[PEP_NOTIFY_ACPI_EVALUATE_CONTROL_METHOD].Name);

    *CompleteResult = PEP_NOTIFICATION_HANDLER_COMPLETE;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_AcpiPepDevice_ScheduleNotifyRequest(
    _In_ DMFMODULE DmfModule,
    _In_ PEP_ACPI_NOTIFY_CONTEXT* NotifyContext
    )
/*++

Routine Description:

    This Method sends AcpiNotify to the PoFx device passed in Context.

Arguments:

    DmfModule - Handle to this Module.
    NotifyContext - Contains target device and notify code.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFMEMORY workRequestMemory;

    // NOTE: Handle validation not done here because DMF handle is unused.
    //
    UNREFERENCED_PARAMETER(DmfModule);

    ntStatus = AcpiPepDevice_WorkRequestCreate(DmfModule,
                                               PEP_NOTIFICATION_CLASS_ACPI,
                                               PEP_NOTIFY_ACPI_WORK,
                                               NotifyContext->PepInternalDevice,
                                               NotifyContext->PepInternalDevice->DeviceDefinition,
                                               NotifyContext,
                                               sizeof(PEP_ACPI_NOTIFY_CONTEXT),
                                               NULL,
                                               &workRequestMemory);
    if (NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Scheduling work request");
        AcpiPepDevice_WorkRequestPend(workRequestMemory);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Could not create work request.");
    }

    return ntStatus;
}

// eof: Dmf_AcpiPepDevice.c
//
