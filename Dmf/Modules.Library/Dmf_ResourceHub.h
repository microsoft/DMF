/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_ResourceHub.h

Abstract:

    Companion file to Dmf_ResourceHub.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

#if !defined(DMF_USER_MODE)

// Client Driver callback to get TransferList from Spb.
//
typedef
_Function_class_(EVT_DMF_ResourceHub_DispatchTransferList)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_ResourceHub_DispatchTransferList(_In_ DMFMODULE DmfModule,
                                         _In_ SPB_TRANSFER_LIST* SpbTransferListBuffer,
                                         _In_ size_t SpbTransferListBufferSize,
                                         _In_ USHORT I2CSecondaryDeviceAddress,
                                         _Out_ size_t *TotalTransferLength);

typedef enum
{
    ResourceHub_Reserved = 0,
    ResourceHub_I2C,
    ResourceHub_SPI,
    ResourceHub_UART
} ResourceHub_DIRECTFW_SERIAL_BUS_TYPE;

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // TODO: Currently only I2C is supported.
    //
    ResourceHub_DIRECTFW_SERIAL_BUS_TYPE TargetBusType;
    // Callback to get TransferList from Spb.
    //
    EVT_DMF_ResourceHub_DispatchTransferList* EvtResourceHubDispatchTransferList;
} DMF_CONFIG_ResourceHub;

// This macro declares the following functions:
// DMF_ResourceHub_ATTRIBUTES_INIT()
// DMF_CONFIG_ResourceHub_AND_ATTRIBUTES_INIT()
// DMF_ResourceHub_Create()
//
DECLARE_DMF_MODULE(ResourceHub)

// Module Methods
//

#endif // !defined(DMF_USER_MODE)

// eof: Dmf_ResourceHub.h
//
