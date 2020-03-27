/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Bridge.h

Abstract:

    Companion file to Dmf_Bridge.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    PFN_WDF_OBJECT_CONTEXT_CLEANUP EvtDeviceContextCleanup;
    PFN_WDF_DEVICE_PREPARE_HARDWARE EvtDevicePrepareHardware;
    PFN_WDF_DEVICE_RELEASE_HARDWARE EvtDeviceReleaseHardware;
    PFN_WDF_DEVICE_D0_ENTRY EvtDeviceD0Entry;
    PFN_WDF_DEVICE_D0_ENTRY_POST_INTERRUPTS_ENABLED EvtDeviceD0EntryPostInterruptsEnabled;
    PFN_WDF_DEVICE_D0_EXIT_PRE_INTERRUPTS_DISABLED EvtDeviceD0ExitPreInterruptsDisabled;
    PFN_WDF_DEVICE_D0_EXIT EvtDeviceD0Exit;
    PFN_WDF_IO_QUEUE_IO_READ EvtQueueIoRead;
    PFN_WDF_IO_QUEUE_IO_WRITE EvtQueueIoWrite;
    PFN_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtDeviceIoControl;
#if !defined(DMF_USER_MODE)
    PFN_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtInternalDeviceIoControl;
#endif // !defined(DMF_USER_MODE)
    PFN_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP EvtDeviceSelfManagedIoCleanup;
    PFN_WDF_DEVICE_SELF_MANAGED_IO_FLUSH EvtDeviceSelfManagedIoFlush;
    PFN_WDF_DEVICE_SELF_MANAGED_IO_INIT EvtDeviceSelfManagedIoInit;
    PFN_WDF_DEVICE_SELF_MANAGED_IO_SUSPEND EvtDeviceSelfManagedIoSuspend;
    PFN_WDF_DEVICE_SELF_MANAGED_IO_RESTART EvtDeviceSelfManagedIoRestart;
    PFN_WDF_DEVICE_SURPRISE_REMOVAL EvtDeviceSurpriseRemoval;
    PFN_WDF_DEVICE_QUERY_REMOVE EvtDeviceQueryRemove;
    PFN_WDF_DEVICE_QUERY_STOP EvtDeviceQueryStop;
    PFN_WDF_DEVICE_RELATIONS_QUERY EvtDeviceRelationsQuery;
    PFN_WDF_DEVICE_USAGE_NOTIFICATION_EX EvtDeviceUsageNotificationEx;
    PFN_WDF_DEVICE_ARM_WAKE_FROM_S0 EvtDeviceArmWakeFromS0;
    PFN_WDF_DEVICE_DISARM_WAKE_FROM_S0 EvtDeviceDisarmWakeFromS0;
    PFN_WDF_DEVICE_WAKE_FROM_S0_TRIGGERED EvtDeviceWakeFromS0Triggered;
    PFN_WDF_DEVICE_ARM_WAKE_FROM_SX_WITH_REASON EvtDeviceArmWakeFromSxWithReason;
    PFN_WDF_DEVICE_DISARM_WAKE_FROM_SX EvtDeviceDisarmWakeFromSx;
    PFN_WDF_DEVICE_WAKE_FROM_SX_TRIGGERED EvtDeviceWakeFromSxTriggered;
    PFN_WDF_DEVICE_FILE_CREATE EvtFileCreate;
    PFN_WDF_FILE_CLEANUP EvtFileCleanup;
    PFN_WDF_FILE_CLOSE EvtFileClose;
} DMF_CONFIG_Bridge;

// This macro declares the following functions:
// DMF_Bridge_ATTRIBUTES_INIT()
// DMF_CONFIG_Bridge_AND_ATTRIBUTES_INIT()
// DMF_Bridge_Create()
//
DECLARE_DMF_MODULE(Bridge)

// Module Methods
//

// eof: Dmf_Bridge.h
//
