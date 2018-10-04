Sample KMDF/DMF Function Driver for OSR USB-FX2 (DMF Sample 1)
==============================================================

This sample shows the minimum steps needed to use DMF in an existing driver.

This sample uses the OSR FX2 sample as the driver that is updated to use DMF.

To keep this sample very simple, no DMF Modules are instantiated. The second sample uses this sample
as a base and shows how to add and use a single Module.

IMPORTANT: For details about how the OSR USB-FX2 device operates, please see the original (non-DMF) sample. This sample is designed to do everything
the original sample does but also perform the minimum steps necessary to initialize DMF.

Please perform a file compare between all the files in this sample and the files in the original sample. That is the best way to see the differences.

Overview
--------

In this sample, the following changes are made:

OsrFxEvtDeviceAdd:

1. Declare two variables that are used to initialize DMF:
```
    PDMFDEVICE_INIT                     dmfDeviceInit;
    DMF_EVENT_CALLBACKS                 dmfEventCallbacks;
```
2. Allocate a PDMFDEVICE_INIT structure. This is an opaque structure DMF uses to keep track of what the Client
Driver (the driver that uses DMF is called the Client driver) is doing during initialization.
```
    //
    // DMF: Create the PDMFDEVICE_INIT structure.
    //
    dmfDeviceInit = DMF_DmfDeviceInitAllocate(DeviceInit);
```
3. Hook the Client driver's WDF callbacks so DMF's callbacks will be called. NOTE: These three calls are ALWAYS
necessary even if the Client driver does not register for any of those callbacks. In this case, the Client driver 
only registers for PnP Power callbacks.
```
    //
    // DMF: Hook Pnp Power Callbacks. This allows DMF to receive callbacks first so it can dispatch them
    //      to the tree of instantiated Modules. If the driver does not use Pnp Power Callbacks, you must
    //      call this function with NULL as the second parameter. This is to prevent developers from 
    //      forgetting this step if the driver adds support for Pnp Power Callbacks later.
    //
    DMF_DmfDeviceInitHookPnpPowerEventCallbacks(dmfDeviceInit, &pnpPowerCallbacks);

    //
    // DMF: Hook Power Policy Callbacks. This allows DMF to receive callbacks first so it can dispatch them
    //      to the tree of instantiated Modules. If the driver does not use Power Policy Callbacks, you must
    //      call this function with NULL as the second parameter. This is to prevent developers from 
    //      forgetting this step if the driver adds support for Power Policy Callbacks later.
    //
    DMF_DmfDeviceInitHookPowerPolicyEventCallbacks(dmfDeviceInit, NULL);

    //
    // DMF: Hook File Object Callbacks. This allows DMF to receive callbacks first so it can dispatch them
    //      to the tree of instantiated Modules. If the driver does not use File Object Callbacks, you must
    //      call this function with NULL as the second parameter. This is to prevent developers from 
    //      forgetting this step if the driver adds support for File Object Callbacks later.
    //
    DMF_DmfDeviceInitHookFileObjectConfig(dmfDeviceInit, NULL);
```
4. Hook the default queue callbacks. This step is optional if the Client driver does not use a default queue. In 
Sample 2, this queue is not necessary because the Client driver will the default queue that DMF creates.
```
    //
    // DMF: Hook Default Queue Config Callbacks. This allows DMF to receive callbacks first 
    //      so it can dispatch them to the tree of instantiated Modules. If the driver does 
    //      not use a default queue, it is NOT NECESSARY to call this function (unlike the
    //      above three functions) because DMF will set up a default queue for itself.
    //
    DMF_DmfDeviceInitHookQueueConfig(dmfDeviceInit, &ioQueueConfig);
```
5. Initialize the DMF Module Add callback function. This function is called by DMF to ask the Client driver
to initialize the DMF Modules the Client driver will use. In this sample, no Modules are used by the
Client but the callback is set for illustration purposes.
```
    //
    // DMF: Initialize the DMF_EVENT_CALLBACKS to set the callback DMF will call
    //      to get the list of Modules to instantiate.
    //
    DMF_EVENT_CALLBACKS_INIT(&dmfEventCallbacks);
    dmfEventCallbacks.EvtDmfDeviceModulesAdd = OsrDmfModulesAdd;

    DMF_DmfDeviceInitSetEventCallbacks(dmfDeviceInit,
                                       &dmfEventCallbacks);
```
6. Initialize DMF. This allows DMF's core to ask the Client driver what Modules it wants to use and then create
the tree of instantiated Modules.
```
    //
    // DMF: Tell DMF to create its data structures and create the tree of Modules the 
    //      Client driver has specified (using the above callback). After this call
    //      succeeds DMF has all the information it needs to dispatch WDF entry points
    //      to the tree of Modules.
    //
    status = DMF_ModulesCreate(device,
                               &dmfDeviceInit);
```
7. Define the function that tells DMF what Modules the Client driver will use. This sample uses no Modules so this
function does nothing. See Sample 2 to see how to use this function.
```
    //
    // DMF: This is the callback function called by DMF that allows this driver (the Client driver)
    //      to set the CONFIG for each DMF Module the driver will use.
    //
    _IRQL_requires_max_(PASSIVE_LEVEL)
    VOID
    OsrDmfModulesAdd(
        _In_ WDFDEVICE Device,
        _In_ PDMFMODULE_INIT DmfModuleInit
        )
    /*++
    Routine Description:

        EvtDmfDeviceModulesAdd is called by DMF during the Client driver's 
        DeviceAdd call from the PnP manager. Here the Client driver declares a
        CONFIG structure for every instance of every Module the Client driver 
        uses. Each CONFIG structure is properly initialized for its specific
        use. Then, each CONFIG structure is added to the list of Modules that
        DMF will instantiate.

    Arguments:

        Device - The Client driver's WDFDEVICE.
        DmfModuleInit - An opaque PDMFMODULE_INIT used by DMF.

    Return Value:

        None

    --*/
    {
        UNREFERENCED_PARAMETER(Device);
        UNREFERENCED_PARAMETER(DmfModuleInit);

        // In this sample, no Modules are instantiated.
        //
    }
```
8. Finally, the project settings are modified to set include paths and link library names. Also, it is necessary to
set EnableWpp = TRUE because the DMF library requires that setting.

Code tour
---------

There are no changes between the original sample and this sample. Please see the original sample for details.

Testing the driver
------------------

Please see the original sample for details. Nothing is changed in the DMF version.

