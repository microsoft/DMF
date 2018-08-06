/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_SpiTarget.h

Abstract:

    Companion file to Dmf_SpiTarget.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

typedef enum
{
    SpiTarget_LatencyCalculation_Message_Invalid,
    SpiTarget_LatencyCalculation_Message_Start,
    SpiTarget_LatencyCalculation_Message_End
} SpiTarget_LatencyCalculation_Message;

typedef
_Function_class_(EVT_DMF_SpiTarget_LatencyCalculation)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_SpiTarget_LatencyCalculation(_In_ DMFMODULE DmfModule,
                                     _In_ SpiTarget_LatencyCalculation_Message LatencyCalculationMessage,
                                     _In_ VOID* Buffer,
                                     _In_ ULONG BufferSize);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Indicates the resource index of the SPI line this instance connects to.
    //
    ULONG ResourceIndex;
    // This is for debugging and validation purposes only.
    // Do not enable in a production driver.
    //
    EVT_DMF_SpiTarget_LatencyCalculation* LatencyCalculationCallback;
} DMF_CONFIG_SpiTarget;

// This macro declares the following functions:
// DMF_SpiTarget_ATTRIBUTES_INIT()
// DMF_CONFIG_SpiTarget_AND_ATTRIBUTES_INIT()
// DMF_SpiTarget_Create()
//
DECLARE_DMF_MODULE(SpiTarget)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SpiTarget_Write(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_(BufferLength) UCHAR* Buffer,
    _In_ ULONG BufferLength,
    _In_ ULONG TimeoutMilliseconds
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SpiTarget_WriteRead(
    _In_ DMFMODULE DmfModule,
    _In_reads_(OutDataLength) UCHAR* OutData,
    _In_ ULONG OutDataLength,
    _Inout_updates_(InDataLength) UCHAR* InData,
    _In_ ULONG InDataLength,
    _In_ ULONG TimeoutMilliseconds
    );

// eof: Dmf_SpiTarget.h
//
