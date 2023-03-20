BusFilter Sample KMDF/DMF Bus Filter Driver
===========================================
This sample shows how to write a bus filter driver using DMF. This sample is an upper filter over DmfKTest.sys.
It intercepts a query interface call to provide its own interface functions.

DMF provides several functions that make it easy to create a Bus Filter driver. This sample shows
how to use those functions.

Code Tour
=========
Using DMF and the default libraries distributed with DMF the above driver is easy to write. The code is small. All of the code is in the file, [DmfInterface.c](DmfInterface.c).
The code for the *entire* driver (minus .rc and .inx file) is listed here:
```
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
```
Annotated Code Tour
===================
In this section, the above code is annotated section by section:

First, DMF is included. Note that most drivers will `#include "DmfModules.Library.h"`. This file includes all of DMF as well as all the Modules distributed with DMF.
Because this driver uses definitions from the unit test DMF Library, its included file is also listed.
```
#include <DmfModules.Library.h>
#include <DmfModules.Library.Tests.h>
```
This line just includes the definitions that allow this driver to talk to the OSR FX2 driver:
```
// This file contains all the OSR FX2 specific definitions.
//
#include "..\Modules.Template\Dmf_OsrFx2_Public.h"
```
These lines are for trace logging:
```

#include "Trace.h"
#include "BusFilter.tmh"
```
Next, the context that stores information for each child device is listed. This context stores the original
interface provided by the bus driver so this driver can call its functions. Not all drivers need to do this; it is
just an example of how to call the original interface.
````
// Child device context
// 
typedef struct _CHILD_DEVICE_CONTEXT
{
    WDFDEVICE Parent;
    Tests_IoctlHandler_INTERFACE_STANDARD OriginalInterface;
} CHILD_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CHILD_DEVICE_CONTEXT, ChildDeviceGetContext)

// Memory allocation tag.
//
#define MemoryTag 'FsuB'
````
These are the forward declarations. DriverEntry and the associated DriverContextCleanup declarations are listed.
Then, DMF Bus Filter callback declarations are listed. Note that driver's DeviceAdd definition is not included because this
driver uses DMF's internal DeviceAdd callback (see next section). However, a callback for when the child device is
added is listed. Finally, the callback for the function this driver intercepts is listed. (This driver
intercepts calls to QueryInterface.) Note that these are DMF callbacks, not WDF callbacks.
```
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_OBJECT_CONTEXT_CLEANUP EvtDriverContextCleanup;
EVT_DMF_BusFilter_DeviceAdd EvtChildDeviceAdded;
EVT_DMF_BusFilter_DeviceQueryInterface EvtChildDeviceQueryInterface;
```
Next, this driver's DriverEntry is defined. DMF Bus Filter drivers must contain specific code to allow
DMF's code to intercept WDF calls. Specifically note the following:
* `DMF_BusFilter_DeviceAdd()` is passed as the Driver's DeviceAdd callback. This callback is defined by DMF.
When that callback happens, DMF is calls into the Client Driver so it can execute and Client Driver specific
code it needs to.
* After the WDFDRIVER object is created, DMF's Bus Filter API must be initialized before DriverEntry exits. Here is where
the Client Driver sets callbacks from DMF's Bus Filter support. In this driver only callback is set, the QueryInterface callback.
* After setting all the callbacks in the Bus Filter API Config, the final step is call `DMF_BusFilter_Initialize()`.
* * Finally, this drivers DriverContextCleanup is listed. It is just a normal callback.
```
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
    attributes.EvtCleanupCallback = EvtDriverContextCleanup;

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
    filterConfig.EvtDeviceAdd = EvtChildDeviceAdded;
    filterConfig.EvtDeviceQueryInterface = EvtChildDeviceQueryInterface;
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
EvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
    )
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE();

    FuncEntry(TRACE_DRIVER);

    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}
#pragma code_seg()
````
Next, the callbacks that intercept calls from the upper driver. In this example, the following calls are intercepted:

* `BusFilter_InterfaceReference()`
* `BusFilter_InterfaceDereference()`
* `BusFilter_ValueGet()`
* `BusFilter_ValueSet()`
```
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
```
Next, a callback is present which allows this driver to allocate a context and attach it to the WDFDEVICE of the added
FDO. This allows each child to have its own specific context. To do this, this driver intercepts `EVT_DMF_BusFilter_DeviceAdd`.
Note that it is possible to intercept before the child device is created and after it is created.
```
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
```
Finally, there is one remaining callback. This callback intercepts QueryInterface, and sets this driver's version
of the QueryInterface buffer returned by the bus driver. Because this driver calls into the bus driver's
QueryInterface itself, this driver stores a version of the buffer returned by the bus driver it is context.
The function driver will now use this driver's version of the QueryInterface buffer which will cause this driver's
functions to be called instead of the underlying bus driver.
```
BOOLEAN
EvtChildDeviceQueryInterface(
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
```

Testing the driver
==================

1. Install the DmfKTest.sys driver. It is both a bus driver and function driver. (It creates a PDO which then causes the function version of the driver to load.)
2. Install this BusFilter sample by right clicking the .inf file and selecting "Install". This driver installs as an upper filter of the bus driver version of DmfKTest.sys.
3. It is possible to see calls intercepted using TraceView or by setting breakpoints using a debugger.

