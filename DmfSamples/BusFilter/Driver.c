/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    DmfInterface.c

Abstract:

    BusFilter Sample: Loads as a filter driver over DmfKTest.sys. This driver shows how the DMF Bus Filter
                      functions work.

Environment:

    Kernel mode only

--*/

#include <DmfModules.Library.h>

#include "Device.h"

#include "Trace.h"

//#include "Driver.tmh"

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_OBJECT_CONTEXT_CLEANUP EvtDriverContextCleanup;

#pragma code_seg("INIT")
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS ntStatus;
    WDFDRIVER driver;

    //WPP_INIT_TRACING(DriverObject,
//                     RegistryPath);

    //FuncEntry(TRACE_DRIVER);

    driver = NULL;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = EvtDriverContextCleanup;
DbgBreakPoint();
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
        //TraceError(TRACE_DRIVER, "WdfDriverCreate fails: ntStatus=%!STATUS!", ntStatus);
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
        //TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "DMF_BusFilter_Initialize fails: ntSTatus%!STATUS!", ntStatus);
        goto Exit;
    }

    //FuncExit(TRACE_DRIVER, "status=%!STATUS!", ntStatus);

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
    PAGED_CODE();
DbgBreakPoint();
    //FuncEntry(TRACE_DRIVER);

    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}
#pragma code_seg()

// eof: Driver.c
//
