/*++

    Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    DmfModule.Core.Trace.h

Abstract:

    Trace GUID definitions specific to the "Core" DMF Library.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

#include "..\Framework\DmfTrace.h"

// WPP Tracing Support
//
#define WPP_CONTROL_GUIDS                                                                                      \
    WPP_DEFINE_CONTROL_GUID(                                                                                   \
        DMF_TraceGuid_Core, (0,0,0,0,0),                                                                       \
        WPP_DEFINE_BIT(DMF_TRACE)                                                                              \
        )                                                                                                      \

// eof: DmfModule.Core.Trace.h
//
