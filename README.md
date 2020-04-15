# Driver Module Framework (DMF)

DMF is an extension to WDF that enables extra functionality for a WDF driver developer. It helps developers write any type of WDF driver better and faster.  

DMF as a framework allows creation of WDF objects called DMF Modules. The code for these DMF Modules can be shared between different drivers. In addition, DMF bundles a library of DMF Modules that we have developed for our drivers and feel would provide value to other driver developers.  

DMF does not replace WDF. DMF is a second framework that is used with WDF. The developer leveraging DMF still uses WDF and all its primitives to write device drivers.  

The source code for both the framework and Modules written using the framework are released. 

Introduction Video from 2018 WinHEC:<br>
www.WinHEC.com (See the video titled, "Introduction to Driver Module Framework".)

This blog post provides more information: 
https://blogs.windows.com/buildingapps/2018/08/15/introducing-driver-module-framework/

The Documentation\ folder has detailed information about the framework and how to use it.

#### DMF Documentation: 
https://github.com/Microsoft/DMF/blob/master/Dmf/Documentation/Driver%20Module%20Framework.md

#### Module Documentation: 
Detailed documentation about each Module is available on the wiki: https://github.com/Microsoft/DMF/wiki/DMF-Overview

#### Samples:
[DmfSamples](https://github.com/Microsoft/DMF/tree/master/DmfSamples) has all the sample drivers that show in incremental steps how to use DMF in a driver. 

#### Questions/Feedback
Please send email to: dmf-feedback@microsoft.com

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
