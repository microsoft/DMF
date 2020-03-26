/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_ThermalCoolingInterface.h

Abstract:

    Companion file to Dmf_ThermalCoolingInterface.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// The PassiveCooling callback routine controls the degree to which the device
// must throttle its performance to meet cooling requirements.
//
typedef
_Function_class_(EVT_DMF_ThermalCoolingInterface_ActiveCooling)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_ThermalCoolingInterface_ActiveCooling(_In_ DMFMODULE DmfModule,
                                              _In_ BOOLEAN Engaged);

// The ActiveCooling callback routine engages or disengages a device's active-cooling function.
//
typedef
_Function_class_(EVT_DMF_ThermalCoolingInterface_PassiveCooling)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_ThermalCoolingInterface_PassiveCooling(_In_ DMFMODULE DmfModule,
                                               _In_ ULONG Percentage);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Reference string for the device interface.
    //
    UNICODE_STRING ReferenceString;
    // ActiveCooling callback routine.
    //
    EVT_DMF_ThermalCoolingInterface_ActiveCooling* CallbackActiveCooling;
    // PassiveCooling callback routine.
    //
    EVT_DMF_ThermalCoolingInterface_PassiveCooling* CallbackPassiveCooling;
} DMF_CONFIG_ThermalCoolingInterface;

// This macro declares the following functions:
// DMF_ThermalCoolingInterface_ATTRIBUTES_INIT()
// DMF_CONFIG_ThermalCoolingInterface_AND_ATTRIBUTES_INIT()
// DMF_ThermalCoolingInterface_Create()
//
DECLARE_DMF_MODULE(ThermalCoolingInterface)

// Module Methods
//

// eof: Dmf_ThermalCoolingInterface.h
//
