/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    Trace.h

Abstract:

    Header file for the debug tracing related function definitions and macros.

Environment:

    Kernel mode

--*/

#pragma once

// Define the tracing flags.
// Tracing GUIDs:
//
// Client Driver - {F473BC4D-9F08-4C2E-92CE-16F36CF3752B}
//
// NOTE: Every driver must define a unique GUID otherwise tracing from multiple drivers
//       that use the same GUID will appear.
//
// DMF           - {D4ED5BA0-1F5F-498C-81E9-F80FDE8E1DE4}
//

#define WPP_CONTROL_GUIDS                                                               \
   WPP_DEFINE_CONTROL_GUID(                                                             \
        DmfTraceGuid, (D4ED5BA0,1F5F,498C,81E9,F80FDE8E1DE4),                           \
        WPP_DEFINE_BIT(DMF_TRACE)                                                       \
        )                                                                               \
    WPP_DEFINE_CONTROL_GUID(                                                            \
        InterfaceClientServerTraceGuid, (F473BC4D,9F08,4C2E,92CE,16F36CF3752B),         \
        WPP_DEFINE_BIT(MYDRIVER_ALL_INFO)                                               \
        WPP_DEFINE_BIT(TRACE_DEVICE)                                                    \
        WPP_DEFINE_BIT(TRACE_CALLBACK)                                                  \
        )                                                                               \

#define WPP_LEVEL_FLAGS_LOGGER(lvl,flags) WPP_LEVEL_LOGGER(flags)
#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags) (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level  >= lvl)

// eof: Trace.h
//