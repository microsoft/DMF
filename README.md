# Driver Module Framework (DMF)

DMF is an extension to WDF that enables extra functionality for a WDF driver developer. It helps developers write any type of WDF driver better and faster.  

DMF as a framework allows creation of WDF objects called DMF Modules. The code for these DMF Modules can be shared between different drivers. In addition, DMF bundles a library of DMF Modules that we have developed for our drivers and feel would provide value to other driver developers.  

DMF does not replace WDF. DMF is a second framework that is used with WDF. The developer leveraging DMF still uses WDF and all its primitives to write device drivers.  

The source code for both the framework and Modules written using the framework are released. 

This blog post provides more information: 
https://blogs.windows.com/buildingapps/2018/08/15/introducing-driver-module-framework/

The Documentation\ folder has detailed information about the framework and how to use it.

#### DMF Documentation: 
https://github.com/Microsoft/DMF/blob/master/Dmf/Documentation/Driver%20Module%20Framework.md

#### Module Documentation: 
Each DMF Module has an associated .md file that explains the Module. 
Here is the list of all Modules available in the library.

&nbsp;&nbsp;[Dmf_AcpiNotification](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_AcpiNotification.md)<br>
&nbsp;&nbsp;[Dmf_AcpiTarget](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_AcpiTarget.md)<br>
&nbsp;&nbsp;[Dmf_AlertableSleep](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_AlertableSleep.md)<br>
&nbsp;&nbsp;[Dmf_BufferPool](https://github.com/Microsoft/DMF/blob/master/Dmf/Framework/Modules.Core/Dmf_BufferPool.md)<br>
&nbsp;&nbsp;[Dmf_BufferQueue](https://github.com/Microsoft/DMF/blob/master/Dmf/Framework/Modules.Core/Dmf_BufferQueue.md)<br>
&nbsp;&nbsp;[Dmf_ButtonTargetViaMsGpio](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_ButtonTargetViaMsGpio.md)<br>
&nbsp;&nbsp;[Dmf_ContinuousRequestTarget](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_ContinuousRequestTarget.md)<br>
&nbsp;&nbsp;[Dmf_CrashDump](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_CrashDump.md)<br>
&nbsp;&nbsp;[Dmf_DeviceInterfaceTarget](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_DeviceInterfaceTarget.md)<br>
&nbsp;&nbsp;[Dmf_GpioTarget](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_GpioTarget.md)<br>
&nbsp;&nbsp;[Dmf_HashTable](https://github.com/Microsoft/DMF/blob/master/Dmf/Framework/Modules.Core/Dmf_HashTable.md)<br>
&nbsp;&nbsp;[Dmf_HidTarget](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_HidTarget.md)<br>
&nbsp;&nbsp;[Dmf_I2cTarget](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_I2cTarget.md)<br>
&nbsp;&nbsp;[Dmf_IoctlHandler](https://github.com/Microsoft/DMF/blob/master/Dmf/Framework/Modules.Core/Dmf_IoctlHandler.md)<br>
&nbsp;&nbsp;[Dmf_NotifyUserWithEvent](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_NotifyUserWithEvent.md)<br>
&nbsp;&nbsp;[Dmf_NotifyUserWithRequest](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_NotifyUserWithRequest.md)<br>
&nbsp;&nbsp;[Dmf_Pdo](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_Pdo.md)<br>
&nbsp;&nbsp;[Dmf_PingPongBuffer](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_PingPongBuffer.md)<br>
&nbsp;&nbsp;[Dmf_QueuedWorkitem](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_QueuedWorkitem.md)<br>
&nbsp;&nbsp;[Dmf_RequestTarget](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_RequestTarget.md)<br>
&nbsp;&nbsp;[Dmf_Registry](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_Registry.md)<br>
&nbsp;&nbsp;[Dmf_ResourceHub](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_ResourceHub.md)<br>
&nbsp;&nbsp;[Dmf_RingBuffer](https://github.com/Microsoft/DMF/blob/master/Dmf/Framework/Modules.Core/Dmf_RingBuffer.md)<br>
&nbsp;&nbsp;[Dmf_ScheduledTask](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_ScheduledTask.md)<br>
&nbsp;&nbsp;[Dmf_SelfTarget](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_SelfTarget.md)<br>
&nbsp;&nbsp;[Dmf_SerialTarget](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_SerialTarget.md)<br>
&nbsp;&nbsp;[Dmf_SmbiosWmi](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_SmbiosWmi.md)<br>
&nbsp;&nbsp;[Dmf_SpiTarget](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_SpiTarget.md)<br>
&nbsp;&nbsp;[Dmf_Thread](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_Thread.md)<br>
&nbsp;&nbsp;[Dmf_ThreadedBufferQueue](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_ThreadedBufferQueue.md)<br>
&nbsp;&nbsp;[Dmf_VirtualHidDeviceVhf](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_VirtualHidDeviceVhf.md)<br>
&nbsp;&nbsp;[Dmf_VirtualHidKeyboard](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_VirtualHidKeyboard.md)<br>
&nbsp;&nbsp;[Dmf_Wmi](https://github.com/Microsoft/DMF/blob/master/Dmf/Modules.Library/Dmf_Wmi.md)<br>                                       

#### Samples:
[DmfSamples](https://github.com/Microsoft/DMF/tree/master/DmfSamples) has all the sample drivers that show in incremental steps how to use DMF in a driver. 

To build DMF and the sample drivers, follow the steps [here](https://docs.microsoft.com/en-us/windows-hardware/drivers/develop/building-a-driver).

# Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.microsoft.com.

When you submit a pull request, a CLA-bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., label, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
