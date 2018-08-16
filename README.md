# Driver Module Framework (DMF)

DMF is an extension to WDF that enables extra functionality for a WDF driver developer. It helps developers write any type of WDF driver better and faster.  

DMF as a framework allows creation of WDF objects called DMF Modules. The code for these DMF Modules can be shared between different drivers. In addition, DMF bundles a library of DMF Modules that we have developed for our drivers and feel would provide value to other driver developers.  

DMF does not replace WDF. DMF is a second framework that is used with WDF. The developer leveraging DMF still uses WDF and all its primitives to write device drivers.  

The source code for both the framework and Modules written using the framework are released. 

This blog post provides more information: 
https://blogs.windows.com/buildingapps/2018/08/15/introducing-driver-module-framework/

The Documentation\ folder has detailed information about the framework and how to use it.
Furthermore, each DMF Module has an associated .txt file accessible directly from the Visual Studio project that explains the Module.
Also, there are three sample drivers that show in incremental steps how to use DMF in a driver. We are working to add more samples as well as a Powerpoint presentation that will sumarize how to use and create DMF Modules.

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
