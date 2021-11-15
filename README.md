# Driver Module Framework (DMF)

DMF is an extension to WDF that enables extra functionality for a WDF driver developer. It helps developers write any type of WDF driver better and faster.  

DMF as a framework allows creation of WDF objects called DMF Modules. The code for these DMF Modules can be shared between different drivers. In addition, DMF bundles a library of DMF Modules that we have developed for our drivers and feel would provide value to other driver developers.  

DMF does not replace WDF. DMF is a second framework that is used with WDF. The developer leveraging DMF still uses WDF and all its primitives to write device drivers.  

The source code for both the framework and Modules written using the framework is released. 

Introduction Video from 2018 WinHEC:<br>
www.WinHEC.com (See the video titled, "Introduction to Driver Module Framework".)

This blog post provides more information: 
https://blogs.windows.com/buildingapps/2018/08/15/introducing-driver-module-framework/

## Questions/Feedback
Please send email to: dmf-feedback@microsoft.com

## **Sample Drivers**<br>
[**DmfSamples**](https://github.com/Microsoft/DMF/tree/master/DmfSamples) has all the sample drivers that show in incremental steps how to use DMF in a driver. 

To build DMF and the sample drivers, follow the steps [here](https://docs.microsoft.com/en-us/windows-hardware/drivers/develop/building-a-driver).

## **Framework Documentation**<br>
The [Documentation](https://github.com/Microsoft/DMF/tree/master/Dmf/Documentation) folder has detailed information about the framework and how to use it. Framework documentation is provided in Markdown (https://github.com/Microsoft/DMF/blob/master/Dmf/Documentation/Driver%20Module%20Framework.md) format.

## **Module Documentation**<br>
Each DMF Module has an associated .md file that explains the Module. 
Here is the list of all Modules available in the library:
*(Note: Unless otherwise indicated, Modules are capable of being loaded dynamically.)*

### Category: *Targets*<br>

[**Dmf_AcpiTarget**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_AcpiTarget.md) **(KMDF only)**<br>
This Module allows a Client to query ACPI in various ways. This Module allows the Client to easily query/invoke/evaluate DSM methods.

[**Dmf_ContinuousRequestTarget**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_ContinuousRequestTarget.md)<br>
This Module allows the Client to easily communicate with an underlying WDFIOTARGET. Two modes are supported: (1) The Client can either call synchronous routines using input/output buffers. (2) The Client can specify the number of asynchronous operations that are automatically created/sent/received to the underlying WDFIOTARGET. In the second case, the Client specifies the sizes of the input/output buffers. The Module creates the buffers as specified. Then, prior to sending an input buffer to the WDFIOTARGET, the Client’s input buffer callback function is called where the Client populates the input buffer that is sent. When an output buffer is received, it is sent to the Client’s output buffer callback function where the Client may retrieve the contents of the output buffer.

[**Dmf_DefaultTarget**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_DefaultTarget.md)<br>
This Module allows the Client to send requests to its default target. Synchronous/asynchronous and continuous requests are supported.

[**Dmf_DeviceInterfaceTarget**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_DeviceInterfaceTarget.md)<br>
The Module allows the Client to specify the GUID of a device interface The Module registers for a PnP Notification for when that device interface appears/disappears. When the device interface appears, the Module automatically opens the corresponding WDFIOTARGET. When the device interface disappears, the Module automatically closes the corresponding WDFIOTARGET. Methods are provided that allow the Client to send requests to that target. Furthermore, it is possible to set up a stream of asynchronous WDFREQUESTS that are automatically created/sent/received to the WDFIOTARGET.

[**Dmf_DeviceInterfaceMultipleTarget**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_DeviceInterfaceMultipleTarget.md) **(KMDF only)**<br>
This Module is the similar to Dmf_DeviceInterfaceTarget but it allows a the Client to easily send requests to
multiple instances of the same device interface. Each instance is identified using a unique handle and the Client is able to easily manage the asynchronous arrival/removal of each instance. Run-down protection is supported on a per-instance basis.  Synchronous/asynchronous and continuous requests are supported. Only KMDF is supported but like Dmf_DeviceInterfaceTarget it is possible to add support for User-mode.

[**Dmf_GpioTarget**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_GpioTarget.md)<br>
This Module allows the Client to communicate with the GPIO pins exposed by the GPIO WDFIOTARGET. This Module automatically looks for the GPIO resources and opens the appropriate targets based on settings set by the Client.

[**Dmf_HidTarget**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_HidTarget.md)<br>
This Module allows the Client to communicate with an underlying HID WDFIOTARGET. Methods are provided that allow the Client to work with input/output/feature reports. This Module automatically opens HID WDFIOTARGETs as they appear. It queries each one to determine if it matches parameters specified by the Client. If so, the Module opens that WDFIOTARGET and prepares it for use by the Client.

[**Dmf_I2cTarget**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_I2cTarget.md)<br>
This Module allows the Client to communicate with an I2C bus that is exposed by SPB. The Client specifies the desired bus index. Methods are provided that allow the Client to read/write I2C data.

[**Dmf_RequestTarget**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_RequestTarget.md)<br>
This Module contains code that builds and send requests (read/write/IOCTL). It can be used with any WDFIOTARGET.

[**Dmf_ResourceHub**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_ResourceHub.md) **(KMDF only)**<br>
This Module allows the Client to communicate with a Resource Hub WDFIOTARGET. The Module contains all the code need to parse the type resource and other information.
**Dynamic: No**

[**Dmf_SelfTarget**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_SelfTarget.md) **(KMDF only)**<br>
This Module allows the Client to send requests to its own stack. Synchronous/asynchronous and continuous requests are supported.

[**Dmf_SpbTarget**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_SpbTarget.md)<br>
This Module allows the Client to communicate with any SPB target. The Client specifies the desired bus index. Methods are provided that allow the Client to read/write buffers via SPB protocol.

[**Dmf_SerialTarget**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_SerialTarget.md) **(KMDF only)**<br>
This Module allows the Client to specify the parameters of the Serial IO Stream target to open. Then, it provides Methods that expose the functionality of DMF_RequestStream to that Serial IO Target.

[**Dmf_SpiTarget**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_SpiTarget.md) **(KMDF only)**<br>
This Module allows the Client to communicate with an SPI bus that is exposed by SPB. The Client specifies the desired bus index. Methods are provided that allow the Client to read/write SPI data.

[**Dmf_SymbolicLinkTarget**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_SpiTarget.md) **(UMDF only)**<br>
This Module allows the Client to send IOCTL buffers to a symbolic link. (This Module currently has minimal support and needs more work. Support using DMF_ContinousRequestTarget needs to be added.)

### Category: *Task Execution*<br>

[**Dmf_QueuedWorkitem**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_QueuedWorkItem.md)<br>
This Module provides functionality similar to a WDFWORKITEM. However, it differs in these ways: 1. The deferred call will execute exactly the number of times that the Client has enqueued it. The Client does not need to track if the deferred call was already enqueued. The Module performs that housekeeping. 2. The order in which the deferred calls happen is exactly the in the order in which the calls are enqueued. 3. Client may wait for the deferred call to execute without explicitly creating and setting an event.

[**Dmf_ScheduledTask**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_ScheduledTask.md)<br>
This Module allows the Client to specify that an operation should happen a single time, either for the lifetime of the machine or for the current boot cycle. There are options for retrying the operation in case of failure or, even success. The Module takes care of all the housekeeping tasks needed to perform the required functionality. For example, the Module will remember that an operation has occurred on a machine by writing a flag to the Registry and automatically reading that same flag every time the Module is instantiated.

[**Dmf_Thread**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_Thread.md)<br>
This Module allows a Client to specify that a callback function is called whenever there is work for that callback function to perform. This Module creates a Thread and two events: 1. A stop event that tells the thread when it should stop. 2. A work event that indicates when work needs to be done. The thread waits on the two events. When the work event is set, the Client’s callback function is called so that the Client can do work. A Method is provided that tells the Module that work is ready to be done. This is a common programming pattern.

### Category: *Buffers*<br>

[**Dmf_BufferPool**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_BufferPool.md)<br>
Provides a list of zero or more pre-allocated buffers. The buffers each have an optional Client Context. The buffers also have sentinels to guard against buffer overrun. These sentinels are checked every time the buffer is used so that when an overrun error happens it can be immediately detected. Furthermore, each buffer can be added to the list in such a way that it is automatically removed from the list after a certain time (on a buffer by buffer basis). This data structure is used by many other Modules. The list is backed up by an optional lookaside list that can allocate buffers when the list of buffers becomes empty and new buffers are needed.

[**Dmf_BufferQueue**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_BufferQueue.md)<br>
This data structure is composed of two DMF_BufferPool instances. One is a list of pre-allocated empty buffers (Producer) and the other is an empty list (Consumer). This Module is often used when a Client needs to create a work queue. When the Client has work to add to the queue, the Client removes a pre-allocated buffer from the Producer, populates it with the work to be done and writes the buffer to the Consumer list. Later, the buffer is removed the from the Consumer list, the indicated work is completed and the buffer is returned to the Producer list. Of course, since this data structure is built using DMF_BufferList, all operations are bounds checked to detect corruption.

[**Dmf_PingPongBuffer**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_PingPongBuffer.md)<br>
Provides a “ping-pong” buffer that allows the Client to easily parse unstructured data into packets where one buffer (ping) holds the last full buffer and the other buffer (pong) holds the current incomplete buffer. The Client can read from the complete buffer while another part of the Client code continues populating the incomplete buffer.

[**Dmf_RingBuffer**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_RingBuffer.md)<br>
Provides a classic ring-buffer data structure. The number of ring-buffer entries as well as the size of each entry is provided by the Client. Additional enhancements include an “infinite” mode that automatically removes the oldest ring-buffer element when a new element is added to a full ring-buffer. Also, each record of the ring-buffer can be populated from a table of addresses and lengths. This is useful when reading/writing structures such as protocol headers where a single copy into the ring-buffer element’s buffer is not sufficient. Also, there is a Method that allows the Client to enumerate all the ring-buffer elements and, optionally, perform an operation on zero, one or more of the elements in the ring-buffer.

[**Dmf_ThreadedBufferQueue**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_ThreadedBufferQueue.md)<br>
This Module uses DMF_Thread and DMF_BufferQueue to allow a Client to insert work into a work queue via multiple threads. When a Client calls the Method to add work to the work queue, a buffer is removed from the Producer list. Then, the buffer is populated with the work to be done and the buffer is added to the Consumer list. Then, the Thread’s work ready event is set. The corresponding thread callback function is called. That function retrieves the work to be done from the Consumer list and calls the Client callback function. That function does the work specified. Finally, the buffer is returned to the Producer list. Options are provided for caller’s threads to wait for the work to be completed or the work can be completed asynchronously. This is an extremely common device driver programming pattern. It ensures that all work is done in order of being requested, one operation at a time.

### Category: *Driver Patterns*<br>

[**Dmf_AcpiNotification**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_AcpiNotification.md) **(KMDF only)**<br>
This Module allows a Client to register for and receive asynchronous notifications via ACPI. The Client specifies the type of notification desired, the callback function the Module should call when the notification happens and the IRQL level at which the callback function is called.

[**Dmf_AlertableSleep**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_AlertableSleep.md)<br>
This Module allows a Client to cause a thread to sleep. However, the Client can cancel that sleep at any time. It is similar to a Sleep() function except that the Sleep() can be interrupted.

[**Dmf_CrashDump**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_CrashDump.md) **(KMDF only)**<br>
This Module allows the Client to easily write data to the Windows Crash Dump file when the system crashes. This Module exposes a ring-buffer so that the Client can simply write data to the ring-buffer at different checkpoints. (Older data is automatically removed when the ring-buffer becomes full.) If the system crashes the entire ring-buffer is reordered so that the earliest entry is listed first. Then, the entire ring-buffer is written to the crash dump file. Optionally, the Client can write other data to the Crash Dump file.
**Dynamic: No**

[**Dmf_CmApi**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_CmApi.md) **(UMDF only)**<br>
This Module allows the Client to work with CM API. Methods in this Module perform common operations that require multiple steps using the CM API.

[**Dmf_IoctlHandler**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_IoctlHandler.md)<br>
This Module allows the Client to create a table of all the supported IOCTLs, including the minimum sizes of the input/output buffers. Each record can also specify a callback function as well as the required User-mode access rights. The Module retrieves the input/output buffers, validates them, validates access rights (optional) and then, if all those validations succeed, the corresponding callback function is called. The callback function is able to directly use the input/output buffers. In the case where the minimum buffer sizes are variable, the callback function is able to validate the length. Finally, the callback function can return STATUS_PENDING to indicate that the WDFREQUEST is held by the Client. Otherwise, the Module automatically completes the WDFREQUEST after the callback function executes.

[**Dmf_InterruptResource**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_InterruptResource.md) **(KMDF only)**<br>
This Module allows a Client to easily connect to an interrupt and receive callbacks using any combination of ISR, DPC or PASSIVE_LEVEL callbacks. 

[**Dmf_NotifyUserWithEvent**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_NotifyUserWithEvent.md) **(KMDF only)**<br>
This Module allows the Client to easily set up an event in Kernel-mode that is also accessible in User-mode. It saves the Client from having to write code that is duplicated often.

[**Dmf_NotifyUserWithRequest**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_NotifyUserWithRequest.md)<br>
This Module allows a Client to specify that is able to enqueue a specific number of WDFREQUESTs that are sent to the Client via an IOCTL. When the Client is ready to complete the WDFREQUESTs (because an event has occurred in the Client), a callback function is called for each pending WDFREQUEST where the Client can populate the output buffer and complete the WDFREQUEST. This programming pattern is useful when a Client wants to notify User-mode programs of events asynchronously.

[**Dmf_Pdo**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_Pdo.md) **(KMDF only)**<br>
This Module allows the Client to easily create and destroy PDOs.

[**Dmf_Registry**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_Registry.md)<br>
This Module has many functions that allow the Client to work easily with the Registry. It has Methods that allow the Client to specify various conditions under which operations that depend on Registry entries will occur. It also allows the Client to easily query and validate Registry entries using a single line of code. Furthermore, it has functions that allow 
the Client to write an entire tree of Registry entries listed in a table.

[**Dmf_Rundown**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_Rundown.md)<br>
This Module allows a Client to easily implement run-down support to protect any resource. DMF provides run-down support internally to protect the Module Context using Open/Close callbacks. In some cases, however, it is necessary for the Client to implement run-down support for other resources. Use this Module to do so. For an example of how to use this Module, see DMF_DeviceInterfaceMultipleTarget.

[**Dmf_SmbiosWmi**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_SmbiosWmi.md)<br>
This Module allows the Client to read the SMBIOS data which the Module retrieves using WMI.

[**Dmf_String**](https://github.com/microsoft/DMF/blob/master/Dmf/Modules.Library/DMF_String.md)<br>
This Module contains various Methods that provide support for working with strings of various types. For example, it contains a Method that allows a Client to search a list of strings. It is anticipated that other similar Methods will be added over time.

[**Dmf_Wmi**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_Wmi.md) **(KMDF only)**<br>
This Module allows the Client work with the WMI API easily.

### Category: *Hid*<br>

[**Dmf_HidPortableDeviceButtons**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_HidPortableDeviceButtons.md) **(KMDF only)**<br>
This Module exposes a HID devices of type "HID Portable Device Buttons".
**Dynamic: No**

[**Dmf_VirtualHidAmbientColorSensor**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_VirtualHidAmbientColorSensor.md) **(KMDF only)**<br>
This Module exposes a Virtual HID Ambient Color Sensor (ALS).

[**Dmf_VirtualHidAmbientLightSensor**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_VirtualHidAmbientLightSensor.md) **(KMDF only)**<br>
This Module exposes a Virtual HID Ambient Light Sensor (ALS).

[**Dmf_VirtualHidDeviceVhf**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_VirtualHidDeviceVhf.md) **(KMDF only)**<br>
This Module is a wrapper around the VHF API. Modules use this Module as a Child Module to create virtual HID devices. Those parent Modules expose Methods that are specific to that virtual HID device. Other Virtual HID Modules use this Module as a Child Module.

[**Dmf_VirtualHidKeyboard**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_VirtualHidKeyboard.md) **(KMDF only)**<br>
This Module is an example of how to create a Virtual HID device using Dmf_VirtualHidDeviceVhf.

[**Dmf_VirtualHidMini**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_VirtualHidMini.md)<br>
Modules use this Module as a Child Module to create virtual HID devices. Those parent Modules expose Methods that are specific to that virtual HID device. Other Virtual HID Modules use this Module as a Child Module. This Module exposes the 
behavior of the VHIDMINI sample in MSDN. It allows a Client to write a Virtual HID device driver that runs in both Kernel and User-mode.

### Category: *Data Structures*<br>
[**Dmf_HashTable**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_HashTable.md)<br>
Provides a classic hash table. The size of the table as well as the size of each record is defined by the Client.

### Category: *Protocols*<br>

[**Dmf_ComponentFimrwareUpdate**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_ComponentFimrwareUpdate.md) **(UMDF only)**<br>
This Module implements the Component Firmware Update (CFU) protocol. This protocol standardizes the way in which firmware is
downloaded to a device. For detailed information and source code for CFU, please look here:<br>
[Information about Component Firmware Update (CFU)](https://blogs.windows.com/windowsdeveloper/2018/10/17/introducing-component-firmware-update/#M84zRDgZAQeRmz9l.97)<br>
[Source code for CFU](https://github.com/Microsoft/CFU/)<br>

[**Dmf_ComponentFimrwareUpdateHidTransport**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_ComponentFimrwareUpdate.md) **(UMDF only)**<br>
This Module implements the Component Firmware Update protocol over a HID transport. 

### Category: *Template*<br>

[**Dmf_NonPnp**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Template/Dmf_NonPnp.md)<br>
This Module is used by the Non-PnP sample.

[**Dmf_OsrFx2**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Template/Dmf_OsrFx2.md)<br>
This Module contains the functionality provided by the OSR USB FX2 sample function driver provided on MSDN samples. This Module is a good example of a "driver as a Module".

[**Dmf_SampleInterfaceProtocol1**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Template/Dmf_SampleInterfaceProtocol1.md)<br>
This Module shows how to write a Protocol Module using DMF's Protocol/Transport Module support. (See the sample named, InterfaceSample1 for more information and example usage.)

[**Dmf_SampleInterfaceTransport1**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Template/Dmf_SampleInterfaceTransport1.md)<br>
This Module shows how to write a Transport Module using DMF's Protocol/Transport Module support. (See the sample named, InterfaceSample1 for more information and example usage.)

[**Dmf_SampleInterfaceTransport2**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Template/Dmf_SampleInterfaceTransport2.md)<br>
This Module shows how to write a Transport Module using DMF's Protocol/Transport Module support. (See the sample named, InterfaceSample1 for more information and example usage.)

[**Dmf_SampleInterfaceLowerProtocol**]<br>
This Module shows how to write an n-layer Protocol Module using DMF's Protocol/Transport Module support. (See the sample 
named InterfaceSample2 for more information and example usage.)

[**Dmf_SampleInterfaceLowerTransport1**]<br>
This Module shows how to write a Transport Module using DMF's Protocol/Transport Module support. (See the sample named InterfaceSample2 for more information and example usage.)

[**Dmf_SampleInterfaceLowerTransport2**]<br>
This Module shows how to write a Transport Module using DMF's Protocol/Transport Module support. (See the sample named InterfaceSample2 for more information and example usage.)

[**Dmf_SampleInterfaceUpperProtocol**]<br>
This Module shows how to write an n-layer Protocol Module using DMF's Protocol/Transport Module support. (See the sample named InterfaceSample2 for more information and example usage.)

[**Dmf_SampleInterfaceUpperTransport1**]<br>
This Module shows how to write a Transport Module using DMF's Protocol/Transport Module support. (See the sample named InterfaceSample2 for more information and example usage.)

[**Dmf_SampleInterfaceUpperTransport2**]<br>
This Module shows how to write a Transport Module using DMF's Protocol/Transport Module support. (See the sample named InterfaceSample2 for more information and example usage.)

[**Dmf_Template**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Template/Dmf_Template.md)<br>
This Module a Module that simply contains all the sections that a Module’s file contains. It also contains all the signatures for all the possible DMF/WDF callback functions. Programmers can copy these signatures when adding a new callback to a Module.

[**Dmf_ToasterBus**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Template/Dmf_ToasterBus.md)<br>
This Module contains the functionality exposed by the WDF ToasterBus sample.

[**Dmf_VirtualHidMiniSample**](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Template/Dmf_VirtualHidMiniSample.md)<br>
This Module shows how to use the Dmf_VirtualHidMini to write a driver for a Virtual HID device that runs in both Kernel and User-mode.
