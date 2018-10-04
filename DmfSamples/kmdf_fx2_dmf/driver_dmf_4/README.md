Sample KMDF/DMF Function Driver for OSR USB-FX2 (DMF Sample 4)
==============================================================

This sample is an incremental change from the third DMF Sample driver. It shows how to use the Module created in Sample 3 in a different manner.
In this case, the switches on the OSR FX2 board cause the PnP Manager to create PDOs. Also, in this driver the IOCTL interface will be disabled.

This sample uses the OSR FX2 board but the sample it mimics is the "EnumSwitches" sample.

IMPORTANT: For details about how the OSR USB-FX2 device operates, please see the original (non-DMF) sample. This sample is designed to do everything
           the original sample does but also perform the minimum steps necessary to initialize DMF.

Overview
========

Please compare this sample with the previous sample (DMF Sample 3). Also, to really understand this sample, it is necessary to read the documentation
that is included with DMF in the Documentation folder. Also, there will be a PowerPoint slide deck to summarize the important points of the documentation
uploaded to the repository soon.

In this sample, the Client driver instantiates the Dmf_OsrFx2 and Dmf_Pdo Modules. When a switch is set and the button is pressed the board sends 
the switch state data to the Client. The Client then creates and/or destroys PDOs based on the switch settings. These PDOs are visible in Device Manager.

Code tour
=========

Client Driver Code
------------------

The Client Driver has substantial changes:

1. Ioctl.c is removed because the Client driver no longer handles IOCTLs. There is no IOCTL table.
2. There are corresponding changes in device.c to not create the message queue.

Device.c changes:

1. OsrDmfModulesAdd(), the Dmf_OsrFx2 Module is instantiated. Note that its Config supports two new parameters.
2. Also, Dmf_Pdo is instantiated. This is the Module that will do all the work of actually creating PDOs.
3. Dmf_QueuedWorkitem is instantiated. This Module allows us to easily execute code in PASSIVE_LEVEL. PDOs must be created in
PASSIVE_LEVEL. 

Interrupt.c changes:

1. The callback from the Dmf_OsrFx2 Module (OsrFx2InterruptPipeCallback) is located in this file. Instead of trying to complete an
IOCTL, this function will now cause PDOs to be created.

Module Code
-----------

1. Dmf_OsrFx2.c:
* Changes are made to allow Client to control the behavior of the Module using Config.
2. Dmf_OsrFx2.h:
* This file contains the external definitions Clients need to instantiate this Module and use its callbacks/Methods.
3. Dmf_OsrFx2.txt:
* This is the documentation file for Dmf_OsrF2x. This file is changed to support the new Module Config options in Dmf_OsrFx2.

Testing the driver
==================

Please see the original sample for details. All the functionality exposed by the application is supported.

