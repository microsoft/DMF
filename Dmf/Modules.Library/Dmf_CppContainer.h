/*++

    Copyright (c) 2015 Microsoft Corporation. All Rights Reserved.

Module Name:

    Dmf_CppContainer.h

Abstract:

    Companion file to Dmf_CppContainer.cpp.

Environment:

    Kernel-mode Driver Framework

--*/

#pragma once

// Common data structure used by both Dmf Module and the Dmf Module Client.
// It tells the Dmf Module how open the Target.
//
typedef struct
{
    // TEMPLATE: The Continer Driver can set attributes that define the object.
    //
    ULONG Dummy;
} DMF_CONFIG_CppContainer, *PDMF_CONFIG_CppContainer;

// This macro declares the following functions:
// DMF_CppContainer_ATTRIBUTES_INIT()
// DMF_CONFIG_CppContainer_AND_ATTRIBUTES_INIT()
// DMF_CppContainer_Create()
//
DECLARE_DMF_MODULE(CppContainer)

// Module Methods
//

// TEMPLATE: Container drivers don't use Module Methods. However, if this objbect will be
//           used directly by its containing driver, it can use Module Methods declared here.
//

// eof: Dmf_CppContainer.h
//
