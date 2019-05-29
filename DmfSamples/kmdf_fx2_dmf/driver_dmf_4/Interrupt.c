/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Interrupt.c

Abstract:

    Waits for switch data from OSRFX2 board to arrive. Then, based on that switch data,
    creates PDOs that correspond to the switches that are set.

Environment:

    Kernel mode

--*/

#include <osrusbfx2.h>

#if defined(EVENT_TRACING)
#include "interrupt.tmh"
#endif

_Use_decl_annotations_
ScheduledTask_Result_Type 
OsrFx2QueuedWorkitem(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ VOID* ClientBufferContext
    )
{
    DEVICE_CONTEXT* pDevContext;
    WDFDEVICE device;
    UCHAR* switchState;
    NTSTATUS ntStatus;
    WCHAR* hwIds[] = { L"{3030527A-2C4D-4B80-80ED-05B215E23023}\\OSRFX2DMFPDO"};

    UNREFERENCED_PARAMETER(ClientBufferContext);

    switchState = (UCHAR*)ClientBuffer;

    device = DMF_ParentDeviceGet(DmfModule);
    pDevContext = GetDeviceContext(device);

    for (ULONG bitIndex = 1; bitIndex <= 0x80; bitIndex <<= 1)
    {
        if ((*switchState) & bitIndex)
        {
            // The bit is on. Create the PDO.
            //
            ntStatus = DMF_Pdo_DevicePlug(pDevContext->DmfModulePdo,
                                          hwIds,
                                          1,
                                          NULL,
                                          0,
                                          L"OsrFx2DmfPdo",
                                          bitIndex,
                                          NULL);
        }
        else
        {
            // The bit is off. Destroy the PDO.
            //
            ntStatus = DMF_Pdo_DeviceUnplugUsingSerialNumber(pDevContext->DmfModulePdo,
                                                             bitIndex);
        }
    }
    
    return ScheduledTask_WorkResult_Success;
}

_Use_decl_annotations_
VOID
OsrFx2InterruptPipeCallback(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR SwitchState,
    _In_ NTSTATUS NtStatus
    )
{
    WDFDEVICE device;
    DEVICE_CONTEXT* pDevContext;

    device = DMF_ParentDeviceGet(DmfModule);
    pDevContext = GetDeviceContext(device);

    if (NT_SUCCESS(NtStatus))
    {
        // Create the PDOs in PASSIVE_LEVEL in a synchronized manner.
        //
        DMF_QueuedWorkItem_Enqueue(pDevContext->DmfModuleQueuedWorkitem,
                                   &SwitchState,
                                   sizeof(UCHAR));
    }
}

