/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    BusFilter.c

Abstract:

    BusFilter Sample: Loads as a filter driver over DmfKTest.sys. This driver shows how the DMF Bus Filter
                      functions work.

Environment:

    Kernel mode only

--*/


#include <DmfModules.Library.h>
#include <DmfModules.Library.Tests.h>

#include "Trace.h"

#include "BusFilter.tmh"

// Child device context
// 
typedef struct _CHILD_DEVICE_CONTEXT
{
    WDFDEVICE Parent;
    Tests_IoctlHandler_INTERFACE_STANDARD OriginalInterface;
} CHILD_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CHILD_DEVICE_CONTEXT, ChildDeviceGetContext)

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_OBJECT_CONTEXT_CLEANUP BusFilter_EvtDriverContextCleanup;
EVT_DMF_BusFilter_DeviceAdd BusFilter_EvtChildDeviceAdded;
EVT_DMF_BusFilter_DeviceQueryInterface BusFilter_EvtChildDeviceQueryInterface;

// Memory allocation tag.
//
#define MemoryTag 'FsuB'

#pragma code_seg("INIT")
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS ntStatus;
    WDFDRIVER driver;

    WPP_INIT_TRACING(DriverObject,
                     RegistryPath);

    FuncEntry(TRACE_DRIVER);

    driver = NULL;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = BusFilter_EvtDriverContextCleanup;

    // NOTE: Use the DeviceAdd provided by DMF. This driver receives callbacks from there.
    //
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config,
                           DMF_BusFilter_DeviceAdd);

    ntStatus = WdfDriverCreate(DriverObject,
                               RegistryPath,
                               &attributes,
                               &config,
                               &driver);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceError(TRACE_DRIVER, "WdfDriverCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // NOTE: To use the DMF Bus Filter support it is necessary to initialize that support from
    //       the Client Driver's DriverEntry.
    //
    DMF_BusFilter_CONFIG filterConfig;
    DMF_BusFilter_CONFIG_INIT(&filterConfig,
                              DriverObject);
    filterConfig.DeviceType = FILE_DEVICE_BUS_EXTENDER;
    filterConfig.EvtDeviceAdd = BusFilter_EvtChildDeviceAdded;
    filterConfig.EvtDeviceQueryInterface = BusFilter_EvtChildDeviceQueryInterface;
    ntStatus = DMF_BusFilter_Initialize(&filterConfig);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "DMF_BusFilter_Initialize fails: ntSTatus%!STATUS!", ntStatus);
        goto Exit;
    }

    FuncExit(TRACE_DRIVER, "status=%!STATUS!", ntStatus);

    return ntStatus;

Exit:

    WPP_CLEANUP(DriverObject);
    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGED")
VOID
BusFilter_EvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
    )
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE();

    FuncEntry(TRACE_DRIVER);

    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}
#pragma code_seg()

static
VOID
BusFilter_InterfaceReference(
    _In_ PVOID Context
    )
{
    const CHILD_DEVICE_CONTEXT* childDeviceContext = ChildDeviceGetContext(Context);

    childDeviceContext->OriginalInterface.InterfaceHeader.InterfaceReference(childDeviceContext->OriginalInterface.InterfaceHeader.Context);
}

static
VOID
BusFilter_InterfaceDereference(
    _In_ PVOID Context
    )
{
    const CHILD_DEVICE_CONTEXT* childDeviceContext = ChildDeviceGetContext(Context);

    childDeviceContext->OriginalInterface.InterfaceHeader.InterfaceDereference(childDeviceContext->OriginalInterface.InterfaceHeader.Context);
}

static
BOOLEAN
BusFilter_ValueGet(
    _In_ VOID* Context,
    _Out_ UCHAR* Value
    )
{
    UCHAR localValue;
    const CHILD_DEVICE_CONTEXT* childDeviceContext = ChildDeviceGetContext(Context);
    DEVICE_OBJECT* deviceObject;

    // For debug purposes only.
    //
    deviceObject = DMF_BusFilter_WdmPhysicalDeviceGet(Context);

    // Get the original context and call the original ValueGet().
    //
    childDeviceContext->OriginalInterface.InterfaceValueGet(childDeviceContext->OriginalInterface.InterfaceHeader.Context,
                                                            &localValue);
    
    // Update value received from original function before sending it to upper driver.
    //
    *Value = localValue + 1;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Get: Original Value to set=%d. Updated value=%d", localValue, *Value);

    return TRUE;
}

BOOLEAN
BusFilter_ValueSet(
    _In_ VOID* Context,
    _In_ UCHAR Value
    )
{
    UCHAR localValue;
    const CHILD_DEVICE_CONTEXT* childDeviceContext = ChildDeviceGetContext(Context);
    DEVICE_OBJECT* deviceObject;

    // For debug purposes only.
    //
    deviceObject = DMF_BusFilter_WdmPhysicalDeviceGet(Context);

    // Subtract one from value passed by upper driver and pass down.
    //
    localValue = Value - 1;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Set: Original Value to set=%d. Updated value=%d", Value, localValue);

    // Get the original context and call the original ValueSet().
    //
    childDeviceContext->OriginalInterface.InterfaceValueSet(childDeviceContext->OriginalInterface.InterfaceHeader.Context,
                                                            localValue);

    return TRUE;
}

NTSTATUS
BusFilter_EvtChildDeviceAdded(
    _In_ WDFDEVICE Device,
    _In_ DMFBUSCHILDDEVICE ChildDevice
    )
{
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES attributes;
    CHILD_DEVICE_CONTEXT* childContext;

    FuncEntry(TRACE_DEVICE);

    childContext = NULL;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                            CHILD_DEVICE_CONTEXT);
    ntStatus = WdfObjectAllocateContext(ChildDevice,
                                        &attributes,
                                        (void**)&childContext);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceError(TRACE_DEVICE, "WdfObjectAllocateContext fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    childContext->Parent = Device;

Exit:

    FuncExit(TRACE_DEVICE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

BOOLEAN
BusFilter_EvtChildDeviceQueryInterface(
    _In_ DMFBUSCHILDDEVICE ChildDevice,
    _In_ PIRP Irp
    )
{
    BOOLEAN returnValue;
    Tests_IoctlHandler_INTERFACE_STANDARD* originalInterface;
    Tests_IoctlHandler_INTERFACE_STANDARD* upperInterface;
    KEVENT event;
    IO_STATUS_BLOCK ioStatusBlock;

    FuncEntry(TRACE_DEVICE);

    CHILD_DEVICE_CONTEXT* childDeviceContext = ChildDeviceGetContext(ChildDevice);
    const PIO_STACK_LOCATION currentStack = IoGetCurrentIrpStackLocation(Irp);

    returnValue = FALSE;

    if (KeGetCurrentIrql() > PASSIVE_LEVEL)
    {
        goto Exit;
    }

    if (NULL == childDeviceContext)
    {
        TraceError(TRACE_DEVICE, "Child device context not yet allocated");
        goto Exit;
    }

    // Only intercept the proper interface known by this filter driver.
    //
    if (!IsEqualGUID(currentStack->Parameters.QueryInterface.InterfaceType,
                     &GUID_Tests_IoctlHandler_INTERFACE_STANDARD) ||
          currentStack->Parameters.QueryInterface.Version != 1)
    {
        goto Exit;
    }

    upperInterface = (Tests_IoctlHandler_INTERFACE_STANDARD*)currentStack->Parameters.QueryInterface. Interface;

    TraceInformation(TRACE_DEVICE, "QueryInterface.InterfaceType=%!GUID!", currentStack->Parameters.QueryInterface.InterfaceType);
    TraceInformation(TRACE_DEVICE, "QueryInterface.Version=%d",  currentStack->Parameters.QueryInterface.Version );

    RtlZeroMemory(&ioStatusBlock,
                  sizeof(IO_STATUS_BLOCK));

    // Create buffer for the original interface.
    //
    originalInterface = ExAllocatePool2(PagedPool,
                                        sizeof(Tests_IoctlHandler_INTERFACE_STANDARD),
                                        MemoryTag);
    if (NULL == originalInterface)
    {
        goto Exit;
    }
    RtlZeroMemory(originalInterface,
                  sizeof(Tests_IoctlHandler_INTERFACE_STANDARD));

    // Get the original interface.
    //
    const PDEVICE_OBJECT deviceObject = DMF_BusFilter_WdmAttachedDeviceGet(ChildDevice);

    KeInitializeEvent(&event,
                      NotificationEvent, FALSE);

    const PIRP irp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP,
                                                  deviceObject,
                                                  NULL,
                                                  0,
                                                  NULL,
                                                  &event,
                                                  &ioStatusBlock);
    irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    irp->IoStatus.Information = 0;

    const PIO_STACK_LOCATION stack = IoGetNextIrpStackLocation(irp);

    stack->MajorFunction = IRP_MJ_PNP;
    stack->MinorFunction = IRP_MN_QUERY_INTERFACE;

    stack->Parameters.QueryInterface.InterfaceType = currentStack->Parameters.QueryInterface.InterfaceType;
    stack->Parameters.QueryInterface.Version = currentStack->Parameters.QueryInterface.Version;
    stack->Parameters.QueryInterface.Size = currentStack->Parameters.QueryInterface.Size;
    stack->Parameters.QueryInterface.Interface = (INTERFACE*)originalInterface;
    stack->Parameters.QueryInterface.InterfaceSpecificData = NULL;

    const NTSTATUS ntStatus = IoCallDriver(deviceObject,
                                           irp);
    if (ntStatus == STATUS_PENDING)
    {
        KeWaitForSingleObject(&event,
                              Executive,
                              KernelMode,
                              FALSE,
                              NULL);
    }

    if (NT_SUCCESS(irp->IoStatus.Status))
    {
        // Save original interface header/context.
        //
        childDeviceContext->OriginalInterface.InterfaceHeader = originalInterface->InterfaceHeader;

        // Save original interface functions.
        //
        childDeviceContext->OriginalInterface.InterfaceValueSet = originalInterface->InterfaceValueSet;
        childDeviceContext->OriginalInterface.InterfaceValueGet = originalInterface->InterfaceValueGet;

        // Set override functions.
        //
        upperInterface->InterfaceHeader.InterfaceReference = BusFilter_InterfaceReference;
        upperInterface->InterfaceHeader.InterfaceDereference = BusFilter_InterfaceDereference;
        upperInterface->InterfaceValueGet = BusFilter_ValueGet;
        upperInterface->InterfaceValueSet = BusFilter_ValueSet;

        // Set child device so it is sent to hooked functions.
        //
        upperInterface->InterfaceHeader.Context = ChildDevice;

        Irp->IoStatus.Status = STATUS_SUCCESS;
        
        // Tell DMF to complete the request.
        //
        returnValue = TRUE;
    }

    ExFreePoolWithTag(originalInterface,
                      MemoryTag);

Exit:

    FuncExitNoReturn(TRACE_DEVICE);

    return returnValue;
}

// eof: BusFilter.c
//
