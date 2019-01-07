/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Template.h

Abstract:

    Companion file to Dmf_Template.c.

    Search for "TEMPLATE:" and update as needed. Then, remove this line.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// TEMPLATE: Template Transport Messages.
// NOTE: These types of messages are only used if the Module is a "Transport Module".
//
#define Template_TransportMessage_Foo       0
#define Template_TransportMessage_Bar       1

//// TEMPLATE: If the Module has a CONFIG structure, declare it here as well as 
////           the corresponding Module definition macro.
////

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // TEMPLATE: Add context specifically used to open this DMF Module.
    //           (Remove TemplateDummy).
    //
    ULONG Dummy;
} DMF_CONFIG_Template;

// This macro declares the following functions:
// DMF_Template_ATTRIBUTES_INIT()
// DMF_CONFIG_Template_AND_ATTRIBUTES_INIT()
// DMF_Template_Create()
//
DECLARE_DMF_MODULE(Template)

//// TEMPLATE: If the Module has no CONFIG structure, use these lines instead of the 
////           the above structure and definition:
////

//// This macro declares the following functions:
//// DMF_Template_ATTRIBUTES_INIT()
//// DMF_Template_Create()
////
//DECLARE_DMF_MODULE_NO_CONFIG(Template)

// Module Methods
//

// eof: Dmf_Template.h
//
