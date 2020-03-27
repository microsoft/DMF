/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_AcpiNotification.h

Abstract:

    Companion file to Dmf_AcpiNotification.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

typedef
_Function_class_(EVT_DMF_AcpiNotification_Dispatch)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_AcpiNotification_Dispatch(_In_ DMFMODULE DmfModule,
                                  _In_ ULONG NotifyCode);

typedef
_Function_class_(EVT_DMF_AcpiNotification_Passive)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_AcpiNotification_Passive(_In_ DMFMODULE DmfModule);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Client's DISPATCH_LEVEL callback when Acpi Notification happens.
    //
    EVT_DMF_AcpiNotification_Dispatch* DispatchCallback;
    // Client's PASSIVE_LEVEL callback when Acpi Notification happens.
    //
    EVT_DMF_AcpiNotification_Passive* PassiveCallback;
} DMF_CONFIG_AcpiNotification;

// This macro declares the following functions:
// DMF_AcpiNotification_ATTRIBUTES_INIT()
// DMF_CONFIG_AcpiNotification_AND_ATTRIBUTES_INIT()
// DMF_AcpiNotification_Create()
//
DECLARE_DMF_MODULE(AcpiNotification)

// Module Methods
//

// eof: Dmf_AcpiNotification.h
//

