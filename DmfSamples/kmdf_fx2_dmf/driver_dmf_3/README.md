<!---
    name: Sample KMDF/DMF Function Driver for OSR USB-FX2 (DMF Sample 3)
    platform: KMDF/DMF
    language: cpp
    category: USB
    description: Demonstrates how to create a DMF Module.
    samplefwlink: http://go.microsoft.com/fwlink/p/?LinkId=620313
--->

Sample KMDF/DMF Function Driver for OSR USB-FX2 (DMF Sample 3)
==============================================================

This sample is an incremental change from the third DMF Sample driver. It shows how to create a DMF Module. Specifically, this sample shows the
following:

	1. The OSR-FX2 sample converted into a Module called Dmf_OsrFx2.
	2. The Module performs most, but not all, the functions that the original driver performs. In this case, some of the functionality
           is left in the Client driver (which hosts the new Dmf_OsrFx2 Module).
	3. The Dmf_OsrFx2 Module has a callback into the Client. In this case, when data from the interrupt Read-pipe arrives, the Dmf_OsrFx2 Module 
           processes that data. Then, if a Client callback has been set, that data is sent to the Client callback.
	4. All the IOCTLs are handled directly by the Module except for one. For demonstration purposes (to show that both Modules and Client
           drivers can handle IOCTLs) one IOCTL is handled by the Client.
	5. IMPORTANT: This sample shows how to add a new Module to a Module Library. In this case, it is added to the Template Library. In most cases,
                      teams will use the Template Library to build a new private Library.

IMPORTANT: For details about how the OSR USB-FX2 device operates, please see the original (non-DMF) sample. This sample is designed to do everything
           the original sample does but also perform the minimum steps necessary to initialize DMF.

Overview
========

Please compare this sample with the previous sample (DMF Sample 2). Also, to really understand this sample, it is necessary to read the documentation
that is included with DMF in the Documentation folder. Also, there will be a PowerPoint slide deck to summarize the important points of the documentation
uploaded to the repository soon.

This sample works almost identically to the original OSR FX2 sample. But, the OSR FX2 code resides mostly in the Template Library in the Dmf_OsrFx2
Module. The Client driver initializes DMF in the same way as the previous sample. However, since the Client driver does not directly handle
the PnP callbacks (because all that functionality is now in the Dmf_OsrFx2 Module), the Client driver does not need to register for PnP callbacks. 
The Client driver only needs to hook DMF.

NOTE: Two features are removed in this sample:

	1. To simplify the sample, event logging is removed from the sample. Most likely, it will be added later in an incremental update to the sample.
	2. It is not possible to get the DMFMODULE directly from a WDFUSBPIPE that the failed read API uses. We are looking for a way to resolve this. 
           For now, the callback is received by the Module and TRUE is returned as in the original sample, but the callback to the Client is not performed.

Code tour
=========

DmfKModules.Template has four new files:

	1. Dmf_OsrFx2.c - This is the new Module (Dmf_OsrFx2) that supports the OSR FX2 board. The code is based on the previous samples.
	2. Dmf_OsrFx2.h - This is the Module Include file for Dmf_OsrFx2. It contains all the definitions for the Module that are externally available.
	3. Dmf_OsrFx2_Public.h This is the Module's Public Include file. It contains all the definitions needed by applications or drivers that send
           commands to the OSR FX2 driver.
	4. Dmf_OsrFx2.txt - This the Dmf_OsrFx2 documentation file. It follows a specific format that is identical to all other Module documentation 
           files. It contains information that programmers that use the Module will find useful.

Client Driver Code
------------------

The Client Driver has substantial changes:

	1. Bulkwr.c is removed because all that logic is now in Dmf_OsrFx2 Module.
	2. Most of the code in ioctl.c and interrupt.c is removed. The missing code is in Dmf_OsrFx2.c.
	3. There are corresponding changes in device.c.

Device.c changes:

	1. These callbacks are completely removed as their code has been moved to Dmf_OsrFx2.c:

		OsrFxEvtDevicePrepareHardware()
		OsrFxEvtDeviceD0Exit
		SelectInterfaces()
		OsrFxSetPowerPolicy()
		OsrFxReadFdoRegistryKeyValue()

	2. Because of #1 just above, the AddDevice function has become smaller:

		a. It is not necessary to register for PnP Power callbacks. Instead, DMF is hooked into those callbacks using this line:
			DMF_DmfDeviceInitHookPnpPowerEventCallbacks(dmfDeviceInit, NULL);
		b. The Read and Write queues are no longer created by the driver because they are created by the Dmf_OsrFx2 Module.
		c. The wait lock is no longer needed because the Module has its own lock.

	3. The IOCTL table now only has a single entry for the single IOCTL that the Client driver supports. The rest of the IOCTLs are supported
	   directly by the Dmf_OsrFx2 Module. This IOCTL is left here for to show that both Modules and Client drivers can support IOCTLs.

	4. Finally, in OsrDmfModulesAdd(), in addition to the Dmf_IoctlHandler being instantiated the Dmf_OsrFx2 Module is instantiated. Its Config
	   has a single element which is a callback that the Module calls when the interrupt endpoint reads data.

Ioctl.c changes:

	1. Code to support all IOCTLs (except one) is removed. That code has been moved to the Dmf_OsrFx2 Module as well.

Interrupt.c changes:

	1. The callback from the Dmf_OsrFx2 Module (OsrFx2InterruptPipeCallback) is located in this file.

Module Code
-----------

1. Dmf_OsrFx2.c:

	* DMF_OsrFx2_Create() - Called by DMF to create the Module and its Child Modules. This function executes in AddDevice(). Any code that should execute 
in AddDevice() should be called here for now. 
	* DMF_OsrFx2_ModuleD0Entry() - Called by DMF when WDF calls into the driver's EvtDeviceD0Entry() callback. This callback performs the same functions 
as the original sample, but it uses its private Module Context instead. 
	* DMF_OsrFx2_ModuleD0Exit() - Called by DMF when WDF calls into the driver's EvtDeviceD0Exit() callback. This callback performs the same functions 
as the original sample, but it uses its private Module Context instead. 
	* DMF_OsrFx2_Open() - Called by DMF when WDF executes EvtDevicePrepareHardware(). Here, as in the original sample, the WDFUSBDEVICE is created and 
its pipes initialized.
	* DMF_OsrFx2_SelfManagedIoFlush() - Called by DMF when WDF calls into the driver's EvtDeviceSelfmanagedIoFlush(). Again, the Module does performs the 
same functions as the original sample.
	* DMF_OsrFx2_SwitchStateGet() - This is a Module Method that is available externally. The Client calls this Method to get the state of the switches
that are stored by the Module. Note how the switch data is store in the Module's private context.
	* OsrFx2_EvtIoRead() - Called directly by WDF when a read request is submitted. IMPORTANT: Note how DMFMODULE is sent to the Module.
	* OsrFx2_EvtIoWrite() - Called directly by WDF when a write request is submitted. IMPORTANT: Note how DMFMODULE is sent to the Module.
	* OsrFx2_EvtRequestReadCompletionRoutine() - Called directly by WDF when the hardware completes a read request.
	* OsrFx2_EvtRequestWriteCompletionRoutine() - Called directly by WDF when the hardware completes a write request.
	* OsrFx2_EvtUsbInterruptPipeReadComplete() - Called directly by WDF when the hardware detects a switch has been changed.
	* OsrFx2_EvtUsbInterruptReadersFailed() - Called directly by WDF when there is an error reading. NOTE: There is a glitch that prevents this 
function from doing exactly what the previous sample did. We are trying to resolve this.
	* OsrFx2_EvtIoStop() - Called directly by WDF for each pending request when the driver will transition to a lower power state.

        These functions support the IOCTLs as in the previous sample:
	
	* OsrFx2_IoctlHandler()
	* OsrFx2_GetBarGraphState()
	* OsrFx2_GetSevenSegmentState()
	* OsrFx2_GetSwitchState()
	* OsrFx2_ReenumerateDevice()
	* OsrFx2_ResetDevice()
	* OsrFx2_SetBarGraphState()
	* OsrFx2_SetSevenSegmentState()

	These functions are the same as the original sample. They initialize the USB infrastructure needed to talk to the OSR FX2 device:

	* OsrFx2_ConfigureContinuousReaderForInterruptEndpoint()
	* OsrFx2_SelectInterfaces()
	* OsrFx2_SetPowerPolicy()
	* OsrFx2_StartAllPipes()
	* OsrFx2_StopAllPipes()

2. Dmf_OsrFx2.h:

	* This file contains the external definitions Clients need to instantiate this Module and use its callbacks/Methods.

3. Dmf_OsrFx2_Public.h:

	* This file contains the external definitions applications and drivers need to communicate with this Module.

4. Dmf_OsrFx2.txt:

	* This is the documentation file for Dmf_OsrF2x.

Testing the driver
==================

Please see the original sample for details. All the functionality exposed by the application is supported.

