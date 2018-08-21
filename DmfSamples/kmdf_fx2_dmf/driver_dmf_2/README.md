<!---
    name: Sample KMDF/DMF Function Driver for OSR USB-FX2 (DMF Sample 2)
    platform: KMDF
    language: cpp
    category: USB
    description: Demonstrates how to instantiate a DMF Module in a WDF driver that uses DMF. This driver is based on the first DMF Sample driver.
    samplefwlink: http://go.microsoft.com/fwlink/p/?LinkId=620313
--->

Sample KMDF/DMF Function Driver for OSR USB-FX2 (DMF Sample 2)
==============================================================

This sample is an incremental change from the first DMF Sample driver. It shows how to instantiate a DMF Module. This sample instantiates the 
Dmf_IoctlHandler Module which simplifies IOCTL handling in a driver.

IMPORTANT: For details about how the OSR USB-FX2 device operates, please see the original (non-DMF) sample. This sample is designed to do everything
the original sample does but also perform the minimum steps necessary to initialize DMF.

Please perform a file compare between all the files in this sample and the files in the original sample. That is the best way to see the differences.

Overview
--------

In this sample, as a demonstration of how to use a Module, the Dmf_IoctlHandler Module is selected. The purpose of this Module
is to simplify IOCTL handling. The Client driver supplies a table of the IOCTLs the driver supports as well as the minimum sizes
of the input/output buffers and a callback for each IOCTL. 

As IOCTLs are sent to the Client driver, DMF intercepts them and sends them to the Dmf_IoctlHandler Module that is instantiated.
This Module validates the IOCTLs and if the validation passes, sends them to the Client driver's callback. There, the Client driver
can process the IOCTL without needing to validate the parameters.

Thus, in this sample, the following changes are made:

1. In Device.c a table is created which contains all the information about the IOCTLs that the driver supports. This information
is based on the code in the original sample.

2. In the function DmfModulesAdd, this variable is declared. It needs to be declared a single time regardless of how many Modules
are to be instantiated.
```
    DMF_MODULE_ATTRIBUTES moduleAttributes;
```
3. Next, a variable that contains the Dmf_IoctlHandler specific configuration information is declared:
```
    DMF_CONFIG_IoctlHandler moduleConfigIoctlHandler;
```
4. Next, moduleConfigIoctlHandler is initialized:
```
    DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(&moduleConfigIoctlHandler,
                                                &moduleAttributes);
```
5. Next, Module specific parameters are set in the Module's Config. This is how the Module knows about the Client driver's 
supported IOCTLs.
```
    moduleConfigIoctlHandler.IoctlRecords = OsrFx2_IoctlHandlerTable;
    moduleConfigIoctlHandler.IoctlRecordCount = _countof(OsrFx2_IoctlHandlerTable);
    moduleConfigIoctlHandler.DeviceInterfaceGuid = GUID_DEVINTERFACE_OSRUSBFX2;
    moduleConfigIoctlHandler.AccessModeFilter = IoctlHandler_AccessModeDefault;
```
6. Finally, this Module is added to the list of Modules that DMF will instantiate:
```
    DMF_DmfModuleAdd(DmfModuleInit, 
                     &moduleAttributes, 
                     WDF_NO_OBJECT_ATTRIBUTES, 
                     &pDevContext->DmfModuleIoctlHandler);
```
7. To instantiate more Modules, simply declare the Module's Config structure (if any), initialize it if necessary and
call DMF_DmfModuleAdd() again using the same moduleAttributes and the new Module Config.

Code tour
---------

There are no changes between the original sample and this sample. Please see the original sample for details.

Testing the driver
------------------

Please see the original sample for details. Nothing is changed in the DMF version. You can place a breakpoint in the IOCTL handler
specified by the Client driver to single step and see how it works. You will it contains less code and is simpler. Of course,
this is a trivial example of using a Module, but it is useful for demonstration purposes.

