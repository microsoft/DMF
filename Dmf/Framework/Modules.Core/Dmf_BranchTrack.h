/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_BranchTrack.h

Abstract:

    Companion file to Dmf_BranchTrack.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Callback function to detect branch check point status.
//
typedef
_Function_class_(EVT_DMF_BranchTrack_StatusQuery)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_BranchTrack_StatusQuery(_In_ DMFMODULE DmfModule,
                                _In_ CHAR* BranchName,
                                _In_ ULONG_PTR BranchContext,
                                _In_ ULONGLONG Count);

// Callback function to allow Client to initialize the records in the BranchTrack table.
// (This is necessary so that lines of code that should have executed but did not execute
// can be properly detected.)
//
// This context is the Module's DMF Module (when BranchTrack is automatically enabled).
// This context is the BranchTrack DMF Module (when BranchTrack is automatically enabled) in the Client Driver.
//
typedef
_Function_class_(EVT_DMF_BranchTrack_BranchesInitialize)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_BranchTrack_BranchesInitialize(_In_ DMFMODULE DmfModule);

// Maximum number of characters in Client Identifier.
//
#define BRANCH_TRACK_CLIENT_NAME_MAXIMUM_LENGTH       64

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Client Driver Name.
    // This name allows BranchTrackReader to identify the what Client is generating
    // the BranchTrack data.
    //
    CHAR ClientName[BRANCH_TRACK_CLIENT_NAME_MAXIMUM_LENGTH + sizeof(CHAR)];

    // Some drivers cannot create DeviceInterfaces so they must use SymbolicLinks.
    // (Filter drivers, for example.)
    //
    PWCHAR SymbolicLinkName;

    // Maximum file name buffer length.
    //
    ULONG MaximumFileNameLength;

    // Maximum branch name buffer length.
    //
    ULONG MaximumBranchNameLength;

    // Maximum number of branch check points.
    //
    ULONG MaximumBranches;

    // Callback function to initialize all the branches.
    //
    EVT_DMF_BranchTrack_BranchesInitialize* BranchesInitialize;
} DMF_CONFIG_BranchTrack;

// This macro declares the following functions:
// DMF_BranchTrack_ATTRIBUTES_INIT()
// DMF_CONFIG_BranchTrack_AND_ATTRIBUTES_INIT()
// DMF_BranchTrack_Create()
//
DECLARE_DMF_MODULE(BranchTrack)

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_BranchTrack_CONFIG_INIT(
    _Out_ DMF_CONFIG_BranchTrack* ModuleConfig,
    _In_ CHAR* ClientName
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_BranchTrack_Helper_BranchStatusQuery_Count(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR* BranchName,
    _In_ ULONG_PTR BranchContext,
    _In_ ULONGLONG Count
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_BranchTrack_Helper_BranchStatusQuery_MoreThan(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR* BranchName,
    _In_ ULONG_PTR BranchContext,
    _In_ ULONGLONG Count
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_BranchTrack_Helper_BranchStatusQuery_LessThan(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR* BranchName,
    _In_ ULONG_PTR BranchContext,
    _In_ ULONGLONG Count
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_BranchTrack_Helper_BranchStatusQuery_AtLeast(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR* BranchName,
    _In_ ULONG_PTR BranchContext,
    _In_ ULONGLONG Count
    );

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BranchTrack_CheckPointExecute(
    _In_opt_ DMFMODULE DmfModule,
    _In_ CHAR* BranchName,
    _In_ CHAR* HintName,
    _In_ CHAR* FileName,
    _In_ ULONG Line,
    _In_ EVT_DMF_BranchTrack_StatusQuery* CallbackStatusQuery,
    _In_ ULONG_PTR Context,
    _In_ BOOLEAN Condition
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BranchTrack_CheckPointCreate(
    _In_opt_ DMFMODULE DmfModule,
    _In_ CHAR* BranchName,
    _In_ CHAR* HintName,
    _In_ CHAR* FileName,
    _In_ ULONG Line,
    _In_ EVT_DMF_BranchTrack_StatusQuery* CallbackStatusQuery,
    _In_ ULONG_PTR Context,
    _In_ BOOLEAN Condition
    );

// BranchTrack macros
//

#if defined(DMF_BRANCH_TRACK_CREATE)
    #define DMF_BRANCHTRACK_GENERIC(DmfObject, Name, Callback, HintName, Context)                                   DMF_BranchTrack_CheckPointCreate(DmfObject, Name, HintName, __FILE__, __LINE__, Callback, Context, TRUE)
    #define DMF_BRANCHTRACK_GENERIC_CONDITIONAL(DmfObject, Name, Callback, HintName, Context, Condition)            DMF_BranchTrack_CheckPointCreate(DmfObject, Name, HintName, __FILE__, __LINE__, Callback, Context, Condition)
#else
    #define DMF_BRANCHTRACK_GENERIC(DmfObject, BranchName, Callback, HintName, Context)                             DMF_BranchTrack_CheckPointExecute(DmfObject, BranchName, HintName, __FILE__, __LINE__, Callback, Context, TRUE)
    #define DMF_BRANCHTRACK_GENERIC_CONDITIONAL(DmfObject, BranchName, Callback, HintName, Context, Condition)      DMF_BranchTrack_CheckPointExecute(DmfObject, BranchName, HintName, __FILE__, __LINE__, Callback, Context, Condition)
#endif // defined(DMF_BRANCH_TRACK_CREATE)
// In some cases, we need to explicitly call DMF_BranchTrack_CheckPointCreate because the table creation is in the 
// same file as the annotations. This is the case when an DMF Module uses BranchTrack to track its own code.
//
#define DMF_BRANCHTRACK_CREATE(DmfObject, BranchName, Callback, HintName, Context)                                  DMF_BranchTrack_CheckPointCreate(DmfObject, BranchName, HintName, __FILE__, __LINE__, Callback, Context, TRUE)
#define DMF_BRANCHTRACK_CREATE_CONDITIONAL(DmfObject, BranchName, Callback, HintName, Context, Condition)           DMF_BranchTrack_CheckPointCreate(DmfObject, BranchName, HintName, __FILE__, __LINE__, Callback, Context, Condition)

#define DMF_BRANCHTRACK_STRING_Exactly              "exactly"
#define DMF_BRANCHTRACK_STRING_MoreThan             "more than"
#define DMF_BRANCHTRACK_STRING_LessThan             "less than"
#define DMF_BRANCHTRACK_STRING_Never                "never"
#define DMF_BRANCHTRACK_STRING_AtLeast              "at least"
#define DMF_BRANCHTRACK_STRING_Optionally           "optionally"
#define DMF_BRANCHTRACK_STRING_FaultInjection       "FaultInjection"
#define DMF_BRANCHTRACK_STRING_NoFaultInjection     "NoFaultInjection"

// Branch that should be executed more than specified number of times to pass.
//
#define DMF_BRANCHTRACK_MORE_THAN(DmfObject, Name, Count)                                           DMF_BRANCHTRACK_GENERIC(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_MoreThan, DMF_BRANCHTRACK_STRING_MoreThan, (ULONG_PTR)Count)
#define DMF_BRANCHTRACK_MORE_THAN_CREATE(DmfObject, Name, Count)                                    DMF_BRANCHTRACK_CREATE(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_MoreThan, DMF_BRANCHTRACK_STRING_MoreThan, (ULONG_PTR)Count)
#define DMF_BRANCHTRACK_MORE_THAN_CONDITIONAL(DmfObject, Name, Count, Condition)                    DMF_BRANCHTRACK_GENERIC_CONDITIONAL(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_MoreThan, DMF_BRANCHTRACK_STRING_MoreThan, (ULONG_PTR)Count, Condition)
#define DMF_BRANCHTRACK_MORE_THAN_CREATE_CONDITIONAL(DmfObject, Name, Count, Condition)             DMF_BRANCHTRACK_CREATE_CONDITIONAL(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_MoreThan, DMF_BRANCHTRACK_STRING_MoreThan, (ULONG_PTR)Count, Condition)

// Branch that should be executed less than specified number of times to pass.
//
#define DMF_BRANCHTRACK_LESS_THAN(DmfObject, Name, Count)                                           DMF_BRANCHTRACK_GENERIC(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_LessThan, DMF_BRANCHTRACK_STRING_LessThan, (ULONG_PTR)Count)
#define DMF_BRANCHTRACK_LESS_THAN_CREATE(DmfObject, Name, Count)                                    DMF_BRANCHTRACK_CREATE(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_LessThan, DMF_BRANCHTRACK_STRING_LessThan, (ULONG_PTR)Count)
#define DMF_BRANCHTRACK_LESS_THAN_CONDITIONAL(DmfObject, Name, Count, Condition)                    DMF_BRANCHTRACK_GENERIC_CONDITIONAL(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_LessThan, DMF_BRANCHTRACK_STRING_LessThan, (ULONG_PTR)Count, Condition)
#define DMF_BRANCHTRACK_LESS_THAN_CREATE_CONDITIONAL(DmfObject, Name, Count, Condition)             DMF_BRANCHTRACK_CREATE_CONDITIONAL(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_LessThan, DMF_BRANCHTRACK_STRING_LessThan, (ULONG_PTR)Count, Condition)

// Branch that should be executed exactly specified number of times to pass.
//
#define DMF_BRANCHTRACK_COUNT(DmfObject, Name, Count)                                               DMF_BRANCHTRACK_GENERIC(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_Count, DMF_BRANCHTRACK_STRING_Exactly, (ULONG_PTR)Count)
#define DMF_BRANCHTRACK_COUNT_CREATE(DmfObject, Name, Count)                                        DMF_BRANCHTRACK_CREATE(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_Count, DMF_BRANCHTRACK_STRING_Exactly, (ULONG_PTR)Count)
#define DMF_BRANCHTRACK_COUNT_CONDITIONAL(DmfObject, Name, Count, Condition)                        DMF_BRANCHTRACK_GENERIC_CONDITIONAL(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_Count, DMF_BRANCHTRACK_STRING_Exactly, (ULONG_PTR)Count, Condition)
#define DMF_BRANCHTRACK_COUNT_CREATE_CONDITIONAL(DmfObject, Name, Count, Condition)                 DMF_BRANCHTRACK_CREATE_CONDITIONAL(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_Count, DMF_BRANCHTRACK_STRING_Exactly, (ULONG_PTR)Count, Condition)

// Branch that should be executed at least a specified number of times to pass.
//
#define DMF_BRANCHTRACK_AT_LEAST(DmfObject, Name, Count)                                            DMF_BRANCHTRACK_GENERIC(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_AtLeast, DMF_BRANCHTRACK_STRING_AtLeast, (ULONG_PTR)Count)
#define DMF_BRANCHTRACK_AT_LEAST_CREATE(DmfObject, Name, Count)                                     DMF_BRANCHTRACK_CREATE(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_AtLeast, DMF_BRANCHTRACK_STRING_AtLeast, (ULONG_PTR)Count)
#define DMF_BRANCHTRACK_AT_LEAST_CONDITIONAL(DmfObject, Name, Count, Condition)                     DMF_BRANCHTRACK_GENERIC_CONDITIONAL(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_AtLeast, DMF_BRANCHTRACK_STRING_AtLeast, (ULONG_PTR)Count, Condition)
#define DMF_BRANCHTRACK_AT_LEAST_CREATE_CONDITIONAL(DmfObject, Name, Count, Condition)              DMF_BRANCHTRACK_CREATE_CONDITIONAL(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_AtLeast, DMF_BRANCHTRACK_STRING_AtLeast, (ULONG_PTR)Count, Condition)

// Branch that should be executed at least once to pass.
//
#define DMF_BRANCHTRACK_RUN(DmfObject, Name)                                                        DMF_BRANCHTRACK_MORE_THAN(DmfObject, Name, 0)
#define DMF_BRANCHTRACK_RUN_CREATE(DmfObject, Name)                                                 DMF_BRANCHTRACK_MORE_THAN_CREATE(DmfObject, Name, 0)
#define DMF_BRANCHTRACK_RUN_CONDITIONAL(DmfObject, Name, Condition)                                 DMF_BRANCHTRACK_MORE_THAN_CONDITIONAL(DmfObject, Name, 0, Condition)
#define DMF_BRANCHTRACK_RUN_CREATE_CONDITIONAL(DmfObject, Name, Condition)                          DMF_BRANCHTRACK_MORE_THAN_CREATE_CONDITIONAL(DmfObject, Name, 0, Condition)

// Branch that should execute exactly one time.
//
#define DMF_BRANCHTRACK_ONCE(DmfObject, Name)                                                       DMF_BRANCHTRACK_COUNT(DmfObject, Name, 1)
#define DMF_BRANCHTRACK_ONCE_CREATE(DmfObject, Name)                                                DMF_BRANCHTRACK_COUNT_CREATE(DmfObject, Name, 1)
#define DMF_BRANCHTRACK_ONCE_CONDITIONAL(DmfObject, Name, Condition)                                DMF_BRANCHTRACK_COUNT_CONDITIONAL(DmfObject, Name, 1, Condition)
#define DMF_BRANCHTRACK_ONCE_CREATE_CONDITIONAL(DmfObject, Name, Condition)                         DMF_BRANCHTRACK_COUNT_CREATE_CONDITIONAL(DmfObject, Name, 1, Condition)

// Branch that should never execute.
//
#define DMF_BRANCHTRACK_NEVER(DmfObject, Name)                                                      DMF_BRANCHTRACK_GENERIC(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_Count, DMF_BRANCHTRACK_STRING_Never, (ULONG_PTR)0)
#define DMF_BRANCHTRACK_NEVER_CREATE(DmfObject, Name)                                               DMF_BRANCHTRACK_CREATE(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_Count, DMF_BRANCHTRACK_STRING_Never, (ULONG_PTR)0)
#define DMF_BRANCHTRACK_NEVER_CONDITIONAL(DmfObject, Name, Condition)                               DMF_BRANCHTRACK_GENERIC_CONDITIONAL(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_Count, DMF_BRANCHTRACK_STRING_Never, (ULONG_PTR)0, Condition)
#define DMF_BRANCHTRACK_NEVER_CREATE_CONDITIONAL(DmfObject, Name, Condition)                        DMF_BRANCHTRACK_CREATE_CONDITIONAL(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_Count, DMF_BRANCHTRACK_STRING_Never, (ULONG_PTR)0, Condition)

// Branch that are optional.
// (This can be used during development until a suitable value is chosen formally.
// It can also be used for information purposes.)
//
#define DMF_BRANCHTRACK_OPTIONAL(DmfObject, Name)                                                   DMF_BRANCHTRACK_GENERIC(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_AtLeast, DMF_BRANCHTRACK_STRING_Optionally, (ULONG_PTR)0)
#define DMF_BRANCHTRACK_OPTIONAL_CREATE(DmfObject, Name)                                            DMF_BRANCHTRACK_CREATE(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_AtLeast, DMF_BRANCHTRACK_STRING_Optionally, (ULONG_PTR)0)
#define DMF_BRANCHTRACK_OPTIONAL_CONDITIONAL(DmfObject, Nam, Conditione)                            DMF_BRANCHTRACK_GENERIC_CONDITIONAL(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_AtLeast, DMF_BRANCHTRACK_STRING_Optionally, (ULONG_PTR)0, Condition)
#define DMF_BRANCHTRACK_OPTIONAL_CREATE_CONDITIONAL(DmfObject, Name, Condition)                     DMF_BRANCHTRACK_CREATE_CONDITIONAL(DmfObject, Name, DMF_BranchTrack_Helper_BranchStatusQuery_AtLeast, DMF_BRANCHTRACK_STRING_Optionally, (ULONG_PTR)0, Condition)

// Branch that are executed with Fault Injection. But Never in general case.
//
#define DMF_BRANCHTRACK_FAULT_INJECTION(DmfObject, Name)                                            {DMF_BRANCHTRACK_NEVER(DmfObject, Name##"["##DMF_BRANCHTRACK_STRING_NoFaultInjection##"]"); DMF_BRANCHTRACK_RUN(DmfObject, Name##"["##DMF_BRANCHTRACK_STRING_FaultInjection##"]");}
#define DMF_BRANCHTRACK_FAULT_INJECTION_CREATE(DmfObject, Name)                                     {DMF_BRANCHTRACK_NEVER_CREATE(DmfObject, Name##"["##DMF_BRANCHTRACK_STRING_NoFaultInjection##"]"); DMF_BRANCHTRACK_RUN_CREATE(DmfObject, Name##"["##DMF_BRANCHTRACK_STRING_FaultInjection##"]");}
#define DMF_BRANCHTRACK_FAULT_INJECTION_CONDITIONAL(DmfObject, Name, Condition)                     {DMF_BRANCHTRACK_NEVER_CONDITIONAL(DmfObject, Name##"["##DMF_BRANCHTRACK_STRING_NoFaultInjection##"]", Condition); DMF_BRANCHTRACK_RUN(DmfObject, Name##"["##DMF_BRANCHTRACK_STRING_FaultInjection##"]", Condition);}
#define DMF_BRANCHTRACK_FAULT_INJECTION_CREATE_CONDITIONAL(DmfObject, Name, Condition)              {DMF_BRANCHTRACK_NEVER_CREATE_CONDITIONALDmfObject, Name##"["##DMF_BRANCHTRACK_STRING_NoFaultInjection##"]", Condition); DMF_BRANCHTRACK_RUN_CREATE(DmfObject, Name##"["##DMF_BRANCHTRACK_STRING_FaultInjection##"]", Condition);}

// These macros are for use by DMF Modules that use BranchTrack.
// Given the Module's DMF Module, these macros retrieve the corresponding BranchTrack handle.
//

// Branch that should be executed more than specified number of times to pass.
//
#define DMF_BRANCHTRACK_MODULE_MORE_THAN(DmfObject, Name, Count)                                    DMF_BRANCHTRACK_MORE_THAN(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count)
#define DMF_BRANCHTRACK_MODULE_MORE_THAN_CREATE(DmfObject, Name, Count)                             DMF_BRANCHTRACK_MORE_THAN_CREATE(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count)
#define DMF_BRANCHTRACK_MODULE_MORE_THAN_CONDITIONAL(DmfObject, Name, Count, Condition)             DMF_BRANCHTRACK_MORE_THAN_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count, Condition)
#define DMF_BRANCHTRACK_MODULE_MORE_THAN_CREATE_CONDITIONAL(DmfObject, Name, Count, Condition)      DMF_BRANCHTRACK_MORE_THAN_CREATE_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count, Condition)

// Branch that should be executed less than specified number of times to pass.
//
#define DMF_BRANCHTRACK_MODULE_LESS_THAN(DmfObject, Name, Count)                                    DMF_BRANCHTRACK_LESS_THAN(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count)
#define DMF_BRANCHTRACK_MODULE_LESS_THAN_CREATE(DmfObject, Name, Count)                             DMF_BRANCHTRACK_LESS_THAN_CREATE(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count)
#define DMF_BRANCHTRACK_MODULE_LESS_THAN_CONDITIONAL(DmfObject, Name, Count, Condition)             DMF_BRANCHTRACK_LESS_THAN_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count, Condition)
#define DMF_BRANCHTRACK_MODULE_LESS_THAN_CREATE_CONDITIONAL(DmfObject, Name, Count, Condition)      DMF_BRANCHTRACK_LESS_THAN_CREATE_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count, Condition)

// Branch that should be executed exactly specified number of times to pass.
//
#define DMF_BRANCHTRACK_MODULE_COUNT(DmfObject, Name, Count)                                        DMF_BRANCHTRACK_COUNT(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count)
#define DMF_BRANCHTRACK_MODULE_COUNT_CREATE(DmfObject, Name, Count)                                 DMF_BRANCHTRACK_COUNT_CREATE(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count)
#define DMF_BRANCHTRACK_MODULE_COUNT_CONDITIONAL(DmfObject, Name, Count, Condition)                 DMF_BRANCHTRACK_COUNT_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count, Condition)
#define DMF_BRANCHTRACK_MODULE_COUNT_CREATE_CONDITIONAL(DmfObject, Name, Count, Condition)          DMF_BRANCHTRACK_COUNT_CREATE_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count, Condition)

// Branch that should be executed at least a specified number of times to pass.
//
#define DMF_BRANCHTRACK_MODULE_AT_LEAST(DmfObject, Name, Count)                                     DMF_BRANCHTRACK_AT_LEAST(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count)
#define DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE(DmfObject, Name, Count)                              DMF_BRANCHTRACK_AT_LEAST_CREATE(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count)
#define DMF_BRANCHTRACK_MODULE_AT_LEAST_CONDITIONAL(DmfObject, Name, Count, Condition)              DMF_BRANCHTRACK_AT_LEAST_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count, Condition)
#define DMF_BRANCHTRACK_MODULE_AT_LEAST_CREATE_CONDITIONAL(DmfObject, Name, Count, Condition)       DMF_BRANCHTRACK_AT_LEAST_CREATE_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Count, Condition)

// Branch that should be executed at least once to pass.
//
#define DMF_BRANCHTRACK_MODULE_RUN(DmfObject, Name)                                                 DMF_BRANCHTRACK_RUN(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name)
#define DMF_BRANCHTRACK_MODULE_RUN_CREATE(DmfObject, Name)                                          DMF_BRANCHTRACK_RUN_CREATE(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name)
#define DMF_BRANCHTRACK_MODULE_RUN_CONDITIONAL(DmfObject, Name, Condition)                          DMF_BRANCHTRACK_RUN_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Condition)
#define DMF_BRANCHTRACK_MODULE_RUN_CREATE_CONDITIONAL(DmfObject, Name, Condition)                   DMF_BRANCHTRACK_RUN_CREATE_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Condition)

// Branch that should execute exactly one time.
//
#define DMF_BRANCHTRACK_MODULE_ONCE(DmfObject, Name)                                                DMF_BRANCHTRACK_ONCE(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name)
#define DMF_BRANCHTRACK_MODULE_ONCE_CREATE(DmfObject, Name)                                         DMF_BRANCHTRACK_ONCE_CREATE(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name)
#define DMF_BRANCHTRACK_MODULE_ONCE_CONDITIONAL(DmfObject, Name, Condition)                         DMF_BRANCHTRACK_ONCE_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Condition)
#define DMF_BRANCHTRACK_MODULE_ONCE_CREATE_CONDITIONAL(DmfObject, Name, Condition)                  DMF_BRANCHTRACK_ONCE_CREATE_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Condition)

// Branch that should never execute.
//
#define DMF_BRANCHTRACK_MODULE_NEVER(DmfObject, Name)                                               DMF_BRANCHTRACK_NEVER(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name)
#define DMF_BRANCHTRACK_MODULE_NEVER_CREATE(DmfObject, Name)                                        DMF_BRANCHTRACK_NEVER_CREATE(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name)
#define DMF_BRANCHTRACK_MODULE_NEVER_CONDITIONAL(DmfObject, Name, Condition)                        DMF_BRANCHTRACK_NEVER_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Condition)
#define DMF_BRANCHTRACK_MODULE_NEVER_CREATE_CONDITIONAL(DmfObject, Name, Condition)                 DMF_BRANCHTRACK_NEVER_CREATE_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Condition)

// Branch that are optional.
// (This can be used during development until a suitable value is chosen formally.
// It can also be used for information purposes.)
//
#define DMF_BRANCHTRACK_MODULE_OPTIONAL(DmfObject, Name)                                            DMF_BRANCHTRACK_OPTIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name)
#define DMF_BRANCHTRACK_MODULE_OPTIONAL_CREATE(DmfObject, Name)                                     DMF_BRANCHTRACK_OPTIONAL_CREATE(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name)
#define DMF_BRANCHTRACK_MODULE_OPTIONAL_CONDITIONAL(DmfObject, Name, Condition)                     DMF_BRANCHTRACK_OPTIONAL_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Condition)
#define DMF_BRANCHTRACK_MODULE_OPTIONAL_CREATE_CONDITIONAL(DmfObject, Name, Condition)              DMF_BRANCHTRACK_OPTIONAL_CREATE_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Condition)

// Branch that are executed with Fault Injection. But Never in general case.
//
#define DMF_BRANCHTRACK_MODULE_FAULT_INJECTION(DmfObject, Name)                                     DMF_BRANCHTRACK_FAULT_INJECTION(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name)
#define DMF_BRANCHTRACK_MODULE_FAULT_INJECTION_CREATE(DmfObject, Name)                              DMF_BRANCHTRACK_FAULT_INJECTION_CREATE(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name)
#define DMF_BRANCHTRACK_MODULE_FAULT_INJECTION_CONDITIONAL(DmfObject, Name, Condition)              DMF_BRANCHTRACK_FAULT_INJECTION_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Condition)
#define DMF_BRANCHTRACK_MODULE_FAULT_INJECTION_CREATE_CONDITIONAL(DmfObject, Name, Condition)       DMF_BRANCHTRACK_FAULT_INJECTION_CREATE_CONDITIONAL(DMF_FeatureModuleGetFromModule(DmfObject, DmfFeature_BranchTrack), Name, Condition)

// eof: Dmf_BranchTrack.h
//