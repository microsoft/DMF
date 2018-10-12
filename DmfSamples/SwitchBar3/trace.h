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
// Client Driver - {0A41377B-C973-4943-89D8-8253D0BE7CED}
//
// NOTE: Every driver must define a unique GUID otherwise tracing from multiple drivers
//       that use the same GUID will appear.
//
// DMF           - {89084538-C6AE-448A-8ADC-55B4B1A0E495}
//

#define WPP_CONTROL_GUIDS                                                               \
   WPP_DEFINE_CONTROL_GUID(                                                             \
        DmfTraceGuid, (89084538,C6AE,448A,8ADC,55B4B1A0E495),                           \
        WPP_DEFINE_BIT(DMF_TRACE)                                                       \
        )                                                                               \
    WPP_DEFINE_CONTROL_GUID(                                                            \
        SwitchBarTraceGuid, (0A41377B,C973,4943,89D8,8253D0BE7CED),                     \
        WPP_DEFINE_BIT(MYDRIVER_ALL_INFO)                                               \
        WPP_DEFINE_BIT(TRACE_DEVICE)                                                    \
        WPP_DEFINE_BIT(TRACE_CALLBACK)                                                  \
        )                                                                               \

#define WPP_LEVEL_FLAGS_LOGGER(lvl,flags) WPP_LEVEL_LOGGER(flags)
#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags) (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level  >= lvl)

// eof: Trace.h
//