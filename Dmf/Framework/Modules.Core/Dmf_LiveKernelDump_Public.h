/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:

    Dmf_LiveKernelDump_Public.h

Abstract:

    This module contains the common declarations shared by driver and user applications.

Environment:

    User and Kernel Modes

--*/

#pragma once

// Use the device interface exposed by the Driver that contains this Module to send IOCTLs to this Module.
//

//-[Live Kernel Dump Create]-----------------------------------------------------------------
//


// Data structure used to pass pointers to structures that needs to be included in Live Kernel Minidump
//
#pragma pack(push, 1)
typedef struct
{
    // Indicates address of the structure.
    //
    PVOID Address;
    // Indicates size of the structure.
    //
    ULONG Size;
} LIVEKERNELDUMP_CLIENT_STRUCTURE, *PLIVEKERNELDUMP_CLIENT_STRUCTURE;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    // A bucheck code used to identify this Live Dump.
    //
    ULONG BugCheckCode;
    // Bugcheck parameter.
    //
    ULONG_PTR BugCheckParameter;
    // Flag to indicate if Dmf data should be included in the mini dump.
    //
    BOOLEAN ExcludeDmfData;
    // Number of client data structures.
    //
    ULONG NumberOfClientStructures;
    // Array of client data structures.
    //
    PLIVEKERNELDUMP_CLIENT_STRUCTURE ArrayOfClientStructures;
    // Size of Secondary data.
    //
    ULONG SizeOfSecondaryData;
    // Pointer to Secondary data.
    //
    PVOID SecondaryDataBuffer;
} LIVEKERNELDUMP_INPUT_BUFFER, *PLIVEKERNELDUMP_INPUT_BUFFER;
#pragma pack(pop)

#define IOCTL_LIVEKERNELDUMP_CREATE    CTL_CODE(FILE_DEVICE_UNKNOWN, 4800, METHOD_BUFFERED, FILE_WRITE_ACCESS)

//------------------------------------------------------------------------------------------
//

// eof: Dmf_LiveKernelDump_Public.h
//
