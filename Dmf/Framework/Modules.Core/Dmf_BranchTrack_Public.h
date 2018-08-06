/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_BranchTrack_Public.h

Abstract:

    Header file containing public interface of Dmf_BranchTrack Module

Environment:

    Kernel-mode
    User-mode

--*/

#pragma once

// Define an Interface Guid so that app can find the device and talk to it.
//
// {1964F671-9F87-4D91-938E-2B15002F249B}
DEFINE_GUID(GUID_DEVINTERFACE_BranchTrack,
            0x1964f671, 0x9f87, 0x4d91, 0x93, 0x8e, 0x2b, 0x15, 0x0, 0x2f, 0x24, 0x9b);

// IOCTL to query collected information.
// (Function code is chosen to maintain compatibility with older client applications.)
//
#define IOCTL_BRANCHTRACK_QUERY_INFORMATION    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x40B, METHOD_BUFFERED, FILE_READ_ACCESS)

// Default settings good for most drivers.
// (Client can override.)
//
#define BRANCHTRACK_DEFAULT_MAXIMUM_BRANCHES                200

enum BRANCHTRACK_REQUEST_TYPE
{
    BRANCHTRACK_REQUEST_TYPE_INVALID = 0,
    BRANCHTRACK_REQUEST_TYPE_STATUS,
    BRANCHTRACK_REQUEST_TYPE_DETAILS
};

typedef struct _BRANCHTRACK_REQUEST_INPUT_DATA
{
    // Request type.
    //
    DWORD Type;
} BRANCHTRACK_REQUEST_INPUT_DATA;

typedef struct _BRANCHTRACK_REQUEST_OUTPUT_DATA_STATUS
{
    // Total number of branch check points in the client Module.
    ULONG BranchesTotal;

    // Number of passed branch check points in the client Module.
    ULONG BranchesPassed;

    // Client Module name, a zero-terminated string.
    //
    UCHAR ClientModuleName[ANYSIZE_ARRAY];
} BRANCHTRACK_REQUEST_OUTPUT_DATA_STATUS;

typedef struct _BRANCHTRACK_REQUEST_OUTPUT_DATA_DETAILS
{
    // Next entry offset from the beginning of the current entry, or 0 if there are no more entries.
    //
    ULONG NextEntryOffset;

    // Checkpoint file name buffer offset from the beginning of the current entry.
    //
    ULONG FileNameOffset;

    // Checkpoint file line number.
    //
    ULONG LineNumber;

    // Branch name buffer offset from the beginning of the current entry.
    //
    ULONG BranchNameOffset;

    // Hint name buffer offset from the beginning of the current entry.
    //
    ULONG HintNameOffset;

    // A flag showing if this branch passed its criteria.
    //
    BOOLEAN IsPassed;

    // The ULONGLONG counter for the User's information.
    //
    ULONGLONG CounterState;

    // The ULONGLONG expected value passed by driver.
    //
    ULONGLONG ExpectedState;

    // Buffer for FileName, BranchName and Hint strings.
    //
    UCHAR StringBuffer[ANYSIZE_ARRAY];
} BRANCHTRACK_REQUEST_OUTPUT_DATA_DETAILS;

typedef struct _BRANCHTRACK_REQUEST_OUTPUT_DATA
{
    // Response Type.
    //
    DWORD    ResponseType;

    // Total length of data in Response field.
    //
    DWORD    ResponseLength;

    union
    {
        BRANCHTRACK_REQUEST_OUTPUT_DATA_STATUS Status;

        // List of BRANCHTRACK_REQUEST_OUTPUT_DATA_DETAILS structures.
        //
        UCHAR Details[ANYSIZE_ARRAY];
    } Response;

} BRANCHTRACK_REQUEST_OUTPUT_DATA;

// eof: Dmf_BranchTrack_Public.h
//
