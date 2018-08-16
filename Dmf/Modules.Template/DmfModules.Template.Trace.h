/*++

    Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    DmfModule.Template.Trace.h

Abstract:

    Trace GUID definitions specific to the "Template" DMF Library.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

#define WPP_CONTROL_GUIDS                                                                              \
    WPP_DEFINE_CONTROL_GUID(                                                                           \
        DMF_TraceGuid_Template, (0,0,0,0,0),                                                           \
        WPP_DEFINE_BIT(DMF_TRACE_Template)                                                             \
        WPP_DEFINE_BIT(DMF_TRACE_ToasterBus)                                                           \
        WPP_DEFINE_BIT(DMF_TRACE_OsrFx2)                                                               \
        )

// eof: DmfModule.Template.Trace.h
//
