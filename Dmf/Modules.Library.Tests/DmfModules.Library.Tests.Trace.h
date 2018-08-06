/*++

    Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    DmfModule.Library.Tests.Trace.h

Abstract:

    Trace GUID definitions specific to the "Library.Tests" DMF Library.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

#define WPP_CONTROL_GUIDS                                                                              \
    WPP_DEFINE_CONTROL_GUID(                                                                           \
        DMF_TraceGuid_Library_Test, (0,0,0,0,0),                                                       \
        WPP_DEFINE_BIT(DMF_TRACE_Tests_BufferPool)                                                     \
        WPP_DEFINE_BIT(DMF_TRACE_Tests_BufferQueue)                                                    \
        WPP_DEFINE_BIT(DMF_TRACE_Tests_PingPongBuffer)                                                 \
        WPP_DEFINE_BIT(DMF_TRACE_Tests_Registry)                                                       \
        WPP_DEFINE_BIT(DMF_TRACE_Tests_RingBuffer)                                                     \
        WPP_DEFINE_BIT(DMF_TRACE_Tests_ScheduledTask)                                                  \
        WPP_DEFINE_BIT(DMF_TRACE_TestsUtility)                                                         \
        )

// eof: DmfModule.Library.Tests.Trace.h
//
