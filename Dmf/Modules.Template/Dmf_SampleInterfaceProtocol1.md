## DMF_SampleInterfaceProtocol1

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

A summary of the Module is documented here. More details are in the "[Module Remarks]" section below.

Sample Protocol Module to demonstrate Protocol - Transport feature and usage of Interfaces in DMF.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_SampleInterfaceProtocol1
````
typedef struct
{
    // This Module's Id.
    //
    ULONG ModuleId;
    // This Module's Name.
    //
    PSTR ModuleName;
} DMF_CONFIG_SampleInterfaceProtocol1;
````

##### Remarks

This Module implements the Protocol part of the Dmf_Interface_SampleInterface Interface.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

##### Remarks

This section lists all the Enumeration Types specific to this Module that are accessible to Clients.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

Refer Dmf_Interface_SampleInterface for a list of Callbacks implemented.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SampleInterfaceProtocol1_TestMethod

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_SampleInterfaceProtocol1_TestMethod(
    _In_ DMFMODULE DmfModule
    )
````

A sample Method implemented by this Protocol that invokes the TransportMethod1 specified by SampleInterface.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SampleInterfaceProtocol1 Module handle.

##### Remarks

* The primary reason for this Method is to demonstrate how to invoke a function specified by the Interface.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* Detailed explanation about using the Module that Clients need to consider.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* List of any Child Modules instantiated by this Module.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* Examples of where the Module is used.
* ClientDriver
* Module

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

* List possible future work for this Module.

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Template

-----------------------------------------------------------------------------------------------------------------------------------

