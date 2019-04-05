/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    NonPnp1Exe.c

Abstract:

    Loads a Non-Pnp driver and sends and IOCTL to write/read data to/from
    the driver.

Environment:

    User-mode

--*/

#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <conio.h>
#include <strsafe.h>

// Contains the name of the driver and IOCTL information.
//
#include "..\..\..\DMF\Modules.Template\Dmf_NonPnp_Public.h"

HANDLE
DriverOpenInstallIfNecessary(
    CHAR* SymbolicLinkName,
    CHAR* DriverName,
    CHAR DriverLocation[MAX_PATH],
    ULONG DriverLocationSize
    );

VOID
DriverRemove(
    CHAR* Drivername,
    CHAR* DriverLocation
    );

// This symbolic link is used to access an IOCTL from a Module.
//
#define NONPNP_SAMPLE_MODULE_SYMBOLIC_LINK_NAME     "\\\\.\\NonPnp"
// Driver name for Service Control Manager.
//
#define DRIVER_NAME       "NonPnp1"

VOID
__cdecl
main(
    _In_ ULONG argc,
    _In_reads_(argc) PCHAR argv[]
    )
{
    BOOL status;
    HANDLE device;
    ULONG ulReturnedLength;
    TCHAR driverLocation[MAX_PATH] = { 0 };

    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    device = DriverOpenInstallIfNecessary(NONPNP_SAMPLE_MODULE_SYMBOLIC_LINK_NAME,
                                          DRIVER_NAME,
                                          driverLocation,
                                          sizeof(driverLocation));
    if (INVALID_HANDLE_VALUE == device)
    {
        goto Exit;
    }

    WCHAR messageToModule[100] = { L"This is a message to the NonPnp Module."};
    WCHAR messageFromModule[100];

    printf("Press any key to exit.\n");
    while( !_kbhit() )
    {
        status = DeviceIoControl(device,
                                 IOCTL_NonPnp_MESSAGE_TRANSFER,
                                 messageToModule,
                                 sizeof(messageToModule),
                                 messageFromModule,
                                 sizeof(messageFromModule),
                                 &ulReturnedLength,
                                 NULL);
        if ( !status )
        {
            printf("Ioctl failed with code %d\n", GetLastError() );
            break;
        }
        else 
        {
            printf("NonPnp: returnedLength=%d Message='%S'\n", ulReturnedLength, messageFromModule);
            printf("Waiting for 1 second...\n");
            Sleep(1000);
        }
    }

    // close the handle to the device.
    //
    CloseHandle(device);

    // Unload the driver if loaded.  Ignore any errors.
    //
    DriverRemove(DRIVER_NAME,
                 driverLocation);

Exit:

    return;
}
