/*++

    Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    DmfTrace.h

Abstract:

    Header file for the debug tracing related function definitions and macros.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Define the tracing flags.
//
// DMF Library Root Tracing GUID - {25AB6EA0-(Driver Id)-(DMF Module Id)-A85C-2C29C1C3FA97}
//

// Each DMF Module needs a unique Id (defined here).
//

// TODO: DMF Client Driver Writer:
//
// Copy lines 35 through 83 into your Trace.h file.
//

// From DmfTrace.h.
// ----------------
//

#define WPP_FLAG_LEVEL_LOGGER(flag, level)                              \
    WPP_LEVEL_LOGGER(flag)

#define WPP_FLAG_LEVEL_ENABLED(flag, level)                             \
    (WPP_LEVEL_ENABLED(flag) &&                                         \
     WPP_CONTROL(WPP_BIT_ ## flag).Level >= level)

#define WPP_LEVEL_FLAGS_LOGGER(lvl,flags) \
           WPP_LEVEL_LOGGER(flags)

#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags) \
           (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level >= lvl)

//
// This comment block is scanned by the trace preprocessor to define our
// Trace function.
//
// USEPREFIX and USESUFFIX strip all trailing whitespace, so we need to surround
// FuncExit messages with brackets 
//
// begin_wpp config
// FUNC Trace{FLAG=MYDRIVER_ALL_INFO}(LEVEL, MSG, ...);
// FUNC TraceEvents(LEVEL, FLAGS, MSG, ...);
// FUNC LogEvents(IFRLOG, LEVEL, FLAGS, MSG, ...);
// FUNC FuncEntry{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS);
// FUNC FuncEntryArguments{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS, MSG, ...);
// FUNC FuncExit{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS, MSG, ...);
// FUNC FuncExitVoid{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS);
// FUNC TraceError{LEVEL=TRACE_LEVEL_ERROR}(FLAGS, MSG, ...);
// FUNC TraceInformation{LEVEL=TRACE_LEVEL_INFORMATION}(FLAGS, MSG, ...);
// FUNC TraceVerbose{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS, MSG, ...);
// FUNC FuncExitNoReturn{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS);
// USEPREFIX(FuncEntry, "%!STDPREFIX! [%!FUNC!] --> Entry");
// USEPREFIX(FuncEntryArguments, "%!STDPREFIX! [%!FUNC!] --> Entry <");
// USEPREFIX(FuncExit, "%!STDPREFIX! [%!FUNC!] <-- Exit <");
// USESUFFIX(FuncExit, ">");
// USEPREFIX(FuncExitVoid, "%!STDPREFIX! [%!FUNC!] <-- Exit");
// USEPREFIX(TraceError, "%!STDPREFIX! [%!FUNC!] ERROR:");
// USEPREFIX(TraceEvents, "%!STDPREFIX! [%!FUNC!] ");
// USEPREFIX(TraceInformation, "%!STDPREFIX! [%!FUNC!] ");
// USEPREFIX(TraceVerbose, "%!STDPREFIX! [%!FUNC!] ");
// USEPREFIX(FuncExitNoReturn, "%!STDPREFIX! [%!FUNC!] <--");
// CUSTOM_TYPE(SMFX_MACHINE_EXCEPTION, ItemEnum(SmFx::MachineException));
// CUSTOM_TYPE(SMFX_TRANSITION_TYPE, ItemEnum(SmFx::TransitionType));
// CUSTOM_TYPE(UCMUCSI_PPM_IOCTL, ItemEnum(_UCMUCSI_PPM_IOCTL));
// end_wpp

// eof: DmfTrace.h
//
