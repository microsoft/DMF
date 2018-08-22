/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    private.h

Abstract:

    Contains structure definitions and function prototypes private to
    the driver.

Environment:

    Kernel mode

--*/

#include <initguid.h>
// DMF: DmfModules.Template.h inlcudes DmfModules.Library.h because every library
//      includes dependent libraries.
//
#include <DmfModules.Template.h>

// DMF: Note that public.h is no longer necessary as those definitions are in the Template library.
//
#include "driverspecs.h"

#include "trace.h"

//
// Include auto-generated ETW event functions (created by MC.EXE from 
// osrusbfx2.man)
//
#include "fx2Events.h"

#ifndef _PRIVATE_H
#define _PRIVATE_H

#define POOL_TAG (ULONG) 'FRSO'
#define _DRIVER_NAME_ "OSRUSBFX2"

//
// A structure representing the instance information associated with
// this particular device.
//

typedef struct _DEVICE_CONTEXT {
    //
    // The following fields are used during event logging to 
    // report the events relative to this specific instance 
    // of the device.
    //
    PCWSTR                          DeviceName;
    PCWSTR                          Location;

    DMFMODULE                       DmfModuleOsrFx2;
    DMFMODULE                       DmfModulePdo;
    DMFMODULE                       DmfModuleQueuedWorkitem;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

extern ULONG DebugLevel;

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_OBJECT_CONTEXT_CLEANUP OsrFxEvtDriverContextCleanup;

EVT_WDF_DRIVER_DEVICE_ADD OsrFxEvtDeviceAdd;

EVT_DMF_OsrFx2_InterruptPipeCallback OsrFx2InterruptPipeCallback;

EVT_DMF_QueuedWorkItem_Callback OsrFx2QueuedWorkitem;

EVT_DMF_OsrFx2_EventWriteCallback OsrFx2_EventWriteCallback;

//
// DMF: This is the callback function called by DMF that allows this driver (the Client Driver)
//      to set the CONFIG for each DMF Module the driver will use.
//
EVT_DMF_DEVICE_MODULES_ADD OsrDmfModulesAdd;

_IRQL_requires_(PASSIVE_LEVEL)
PCHAR
DbgDevicePowerString(
    _In_ WDF_POWER_DEVICE_STATE Type
    );

#endif


