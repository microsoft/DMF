/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_ToasterBus.h

Abstract:

    Companion file to Dmf_ToasterBus.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Number of Toasters.
    //
    ULONG DefaultNumberOfToasters;
    // Bus Device Class Guid.
    //
    GUID ToasterBusDevClassGuid;
    // Bus Device Interface Guid.
    //
    GUID ToasterBusDevInterfaceGuid;
    // Hardware Id.
    //
    PWCHAR ToasterBusHardwareId;
    // Hardware Id Length.
    //
    ULONG ToasterBusHardwareIdLength;
    // Hardware Compatible Id.
    //
    PWCHAR ToasterBusHardwareCompatibleId;
    // Hardware compatible Id length.
    //
    ULONG ToasterBusHardwareCompatibleIdLength;
    // Description format for Bus Device.
    //
    PWCHAR ToasterBusDeviceDescriptionFormat;
    // Toaster Bus Number.
    //
    ULONG ToasterBusNumber;
    // Configuration for Wmi.
    //
    DMF_CONFIG_Wmi WmiConfig;
} DMF_CONFIG_ToasterBus;

// This macro declares the following functions:
// DMF_ToasterBus_ATTRIBUTES_INIT()
// DMF_CONFIG_ToasterBus_AND_ATTRIBUTES_INIT()
// DMF_ToasterBus_Create()
//
DECLARE_DMF_MODULE(ToasterBus)

// Module Methods
//

// eof: Dmf_ToasterBus.h
//
