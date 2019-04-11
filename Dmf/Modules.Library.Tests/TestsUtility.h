/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    TestsUtility.h

Abstract:

    Companion file to TestsUtility.c.

Environment:

    Kernel-mode Driver Framework

--*/

#pragma once

// Public Functions
//

#if !defined(DMF_USER_MODE)

_Must_inspect_result_
_IRQL_requires_same_
_IRQL_requires_max_(APC_LEVEL)
ULONG
TestsUtility_GenerateRandomNumber(
    _In_ ULONG Min,
    _In_ ULONG Max
    );

_IRQL_requires_same_
_IRQL_requires_max_(APC_LEVEL)
VOID
TestsUtility_FillWithSequentialData(
    _In_ PUINT8 Buffer,
    _In_ ULONG Size
    );

_IRQL_requires_same_
_IRQL_requires_max_(APC_LEVEL)
VOID
TestsUtility_YieldExecution();

#endif // !defined(DMF_USER_MODE)

_Must_inspect_result_
_IRQL_requires_same_
UINT16
TestsUtility_CrcCompute(
    _In_ UINT8* Message,
    _In_ UINT32 NumberOfBytes
    );

// eof: TestsUtility.h
//
