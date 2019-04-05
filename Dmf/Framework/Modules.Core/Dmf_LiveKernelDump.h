/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_LiveKernelDump.h

Abstract:

    Companion file to Dmf_LiveKernelDump.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

#include "Dmf_LiveKernelDump_Public.h"

// Maximum allowed Triage Data in a Live Kernel Mini Dump : 180KB.
//
#define DMF_LIVEKERNELDUMP_MAXIMUM_TRIAGE_DATA              (180 * 1024)

// Maximum allowed Secondary Data in a Live Kernel Mini Dump : 
// Selfhost Build: 100MB
// Retail Build: 1MB
//
#if defined(DEBUG)
#define DMF_LIVEKERNELDUMP_MAXIMUM_SECONDARY_DATA           (100 * 1024 * 1024) 
#else
#define DMF_LIVEKERNELDUMP_MAXIMUM_SECONDARY_DATA           (1 * 1024 * 1024) 
#endif // defined(DEBUG)

// Maximum size of Report Type.
//
#define DMF_LIVEKERNELDUMP_MAXIMUM_REPORT_TYPE_SIZE         (12)

// Callback function to allow Client to store the Feature Module.
// This callback is defined if the driver is using this Module as a DMF Feature.
//
typedef VOID EVT_DMF_LiveKernelDump_Initialize(_In_ DMFMODULE DmfModule);

// Common data structure used by both DMF Module and the DMF Module Client.
//
typedef struct
{
    // Callback function to initialize Live Kernel Dump feature.
    //
    EVT_DMF_LiveKernelDump_Initialize* LiveKernelDumpFeatureInitialize;
    // Guid used by applications to find this Module and talk to it.
    //
    GUID GuidDeviceInterface;
    // Null terminated wide character string used to identify the set of minidumps generated from this driver.
    //
    WCHAR ReportType[DMF_LIVEKERNELDUMP_MAXIMUM_REPORT_TYPE_SIZE];
    // Guid used to locate the secondary data associated with the minidumps generated from this driver.
    //
    GUID GuidSecondaryData;
} DMF_CONFIG_LiveKernelDump;

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_LiveKernelDump_CONFIG_INIT(
    _Out_ DMF_CONFIG_LiveKernelDump* ModuleConfig
    );

// This macro declares the following functions:
// DMF_LiveKernelDump_ATTRIBUTES_INIT()
// DMF_CONFIG_LiveKernelDump_AND_ATTRIBUTES_INIT()
// DMF_LiveKernelDump_Create()
//
DECLARE_DMF_MODULE(LiveKernelDump)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_LiveKernelDump_DataBufferSourceAdd(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Buffer,
    _In_ ULONG BufferLength
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_LiveKernelDump_DataBufferSourceRemove(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Buffer,
    _In_ ULONG BufferLength
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_LiveKernelDump_StoreDmfCollectionAsBugcheckParameter(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG_PTR DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_LiveKernelDump_LiveKernelMemoryDumpCreate(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG BugCheckCode,
    _In_ ULONG_PTR BugCheckParameter,
    _In_ BOOLEAN ExcludeDmfData,
    _In_ ULONG NumberOfClientStructures,
    _In_opt_ PLIVEKERNELDUMP_CLIENT_STRUCTURE ArrayOfClientStructures,
    _In_ ULONG SizeOfSecondaryData,
    _In_opt_ VOID* SecondaryDataBuffer
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
ULONG
DMF_LiveKernelDump_DmfDataSizeGet(
    _In_ DMFMODULE DmfModule
    );

#if !defined(DMF_USER_MODE)

// Defines used by the Client Driver.
//
// Define used to generate a Live Kernel Memory Dump from the Client Driver.
//
#define DMF_LIVEKERNELDUMP_CREATE(DmfModule,                                                                        \
                                  BugCheckCode,                                                                     \
                                  BugCheckParameter,                                                                \
                                  ExcludeDmfData,                                                                   \
                                  NumberOfClientStructures,                                                         \
                                  ArrayOfClientStructures,                                                          \
                                  SizeOfSecondaryData,                                                              \
                                  SecondaryDataBuffer)                                                              \
        DMF_LiveKernelDump_LiveKernelMemoryDumpCreate(DmfModule,                                                    \
                                                      BugCheckCode,                                                 \
                                                      (ULONG_PTR)BugCheckParameter,                                 \
                                                      ExcludeDmfData,                                               \
                                                      NumberOfClientStructures,                                     \
                                                      ArrayOfClientStructures,                                      \
                                                      SizeOfSecondaryData,                                          \
                                                      SecondaryDataBuffer)

// Define used to get the length of memory occupied by DMF structures in the LiveKernelDumpModule.
//
#define DMF_LIVEKERNELDUMP_GET_DMF_DATA_SIZE(DmfModule)                                                               \
        DMF_LiveKernelDump_DmfDataSizeGet(DmfModule);

// Defines used by DMF Framework / DMF Modules.
//
// Defines used to store pointers to critical data buffers.
//
#define DMF_MODULE_LIVEKERNELDUMP_POINTER_STORE(DmfModule,                                                                   \
                                                Buffer,                                                                      \
                                                BufferLength)                                                                \
        DMF_LiveKernelDump_DataBufferSourceAdd(DMF_FeatureModuleGetFromModule(DmfModule, DmfFeature_LiveKernelDump),         \
                                               (UCHAR*)Buffer,                                                               \
                                               (ULONG)BufferLength)

// Defines used to remove pointers stored using the above call.
//
#define DMF_MODULE_LIVEKERNELDUMP_POINTER_REMOVE(DmfModule,                                                                   \
                                                 Buffer,                                                                      \
                                                 BufferLength)                                                                \
        DMF_LiveKernelDump_DataBufferSourceRemove(DMF_FeatureModuleGetFromModule(DmfModule, DmfFeature_LiveKernelDump),       \
                                                  (UCHAR*)Buffer,                                                             \
                                                  (ULONG)BufferLength)

// Defines used to generate a Live Kernel Memory Dump.
//
#define DMF_MODULE_LIVEKERNELDUMP_CREATE(DmfModule,                                                                  \
                                         BugCheckCode,                                                               \
                                         BugCheckParameter,                                                          \
                                         ExcludeDmfData,                                                             \
                                         NumberOfModuleStructures,                                                   \
                                         ArrayOfModuleStructures,                                                    \
                                         SizeOfSecondaryData,                                                        \
                                         SecondaryDataBuffer)                                                        \
        DMF_LIVEKERNELDUMP_CREATE(DMF_FeatureModuleGetFromModule(DmfModule, DmfFeature_LiveKernelDump),              \
                                  BugCheckCode,                                                                      \
                                  (ULONG_PTR)BugCheckParameter,                                                      \
                                  ExcludeDmfData,                                                                    \
                                  NumberOfModuleStructures,                                                          \
                                  ArrayOfModuleStructures,                                                           \
                                  SizeOfSecondaryData,                                                               \
                                  SecondaryDataBuffer)

// Defines used to store the first bugcheck parameter.
//
#define DMF_MODULE_LIVEKERNELDUMP_DMFCOLLECTION_AS_BUGCHECK_PARAMETER_STORE(DmfModule,                                                  \
                                                                            DmfCollection)                                              \
        DMF_LiveKernelDump_StoreDmfCollectionAsBugcheckParameter(DMF_FeatureModuleGetFromModule(DmfModule, DmfFeature_LiveKernelDump),  \
                                                                 DmfCollection)                                                         \

#else

// Defines used by the Client Driver.
//
// Defines used to store pointers to critical data buffers.
//
#define DMF_LIVEKERNELDUMP_POINTER_STORE(DmfModule,                                                                   \
                                         Buffer,                                                                      \
                                         BufferLength)

// Defines used to remove pointers stored using the above call.
//
#define DMF_LIVEKERNELDUMP_POINTER_REMOVE(DmfModule,                                                                  \
                                          Buffer,                                                                     \
                                          BufferLength)

// Defines used to generate a Live Kernel Memory Dump.
//
#define DMF_LIVEKERNELDUMP_CREATE(DmfModule,                                                                          \
                                  BugCheckCode,                                                                       \
                                  BugCheckParameter,                                                                  \
                                  ExcludeDmfData,                                                                     \
                                  NumberOfClientStructures,                                                           \
                                  ArrayOfClientStructures,                                                            \
                                  SizeOfSecondaryData,                                                                \
                                  SecondaryDataBuffer)

// Defines used by DMF Framework / DMF Modules.
//
// Defines used to store pointers to critical data buffers.
//
#define DMF_MODULE_LIVEKERNELDUMP_POINTER_STORE(DmfModule,                                                            \
                                                Buffer,                                                               \
                                                BufferLength)

// Defines used to remove pointers stored using the above call.
//
#define DMF_MODULE_LIVEKERNELDUMP_POINTER_REMOVE(DmfModule,                                                           \
                                                 Buffer,                                                              \
                                                 BufferLength)

// Defines used to generate a Live Kernel Memory Dump.
//
#define DMF_MODULE_LIVEKERNELDUMP_CREATE(DmfModule,                                                                  \
                                         BugCheckCode,                                                               \
                                         BugCheckParameter,                                                          \
                                         ExcludeDmfData,                                                             \
                                         NumberOfClientStructures,                                                   \
                                         ArrayOfModuleStructures,                                                    \
                                         SizeOfSecondaryData,                                                        \
                                         SecondaryDataBuffer)

// Defines used to store the first bugcheck parameter.
//
#define DMF_MODULE_LIVEKERNELDUMP_DMFCOLLECTION_AS_BUGCHECK_PARAMETER_STORE(DmfModule,                                                 \
                                                                            DmfCollection)                                             \
       DMF_LiveKernelDump_StoreDmfCollectionAsBugcheckParameter(DMF_FeatureModuleGetFromModule(DmfModule, DmfFeature_LiveKernelDump),  \
                                                  DmfCollection)                                                                       \


#endif // !defined(DMF_USER_MODE)

// eof: Dmf_LiveKernelDump.h
//
