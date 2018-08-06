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

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // TEMPLATE: Add context specifically used to open this DMF Module.
    //           (Remove TemplateDummy).
    //
    ULONG Dummy;
} DMF_CONFIG_Template;

// TEMPLATE: Template Transport Messages
// 
#define Template_TransportMessage_Foo       0
#define Template_TransportMessage_Bar       1

// This macro declares the following functions:
// DMF_Template_ATTRIBUTES_INIT()
// DMF_CONFIG_Template_AND_ATTRIBUTES_INIT()
// DMF_Template_Create()
//
DECLARE_DMF_MODULE(Template)

// Module Methods
//

// TEMPLATE: Add prototypes for any Module Methods.
//

// eof: Dmf_Template.h
//
