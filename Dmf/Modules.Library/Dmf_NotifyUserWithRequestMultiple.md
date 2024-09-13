## DMF_NotifyUserWithRequestMultiple

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

This Module implements a design pattern where the client gets Data packets from one source (such as hardware notifications) in which multiple users (applications, services, drivers) are interested.

Each user opens a File handle to the client and pends WDFREQUESTs to retrieve the data notifications.

This Module broadcasts each data packet to each of the user.

Internally for each user, this Module creates a separate instance of NotifyUserWithRequest Module.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_NotifyUserWithRequestMultiple
````
typedef struct
{
    // Maximum number of pending events allowed. This is to prevent a caller from
    // submitting millions of events.
    //
    LONG MaximumNumberOfPendingRequests;
    // Maximum number of data entries stored.
    //
    LONG MaximumNumberOfPendingDataBuffers;
    // Size of each data entry.
    //
    LONG SizeOfDataBuffer;
    // Client callback function invoked by passing a request and data buffer.
    // All NotifyUserWithRequest child Modules will share this callback.
    //
    EVT_DMF_NotifyUserWithRequeset_Complete* CompletionCallback;
    // Callback registered by Client for Data/Request processing
    // upon new Client arrival. If Client returns a non-success status,
    // the User will not receieve the nofications from this Module.
    //
    EVT_DMF_NotifyUserWithRequestMultiple_ArrivalCallback* EvtClientArrivalCallback;
    // Callback registered by Client for Data/Request processing
    // upon Client departure.
    //
    EVT_DMF_NotifyUserWithRequestMultiple_DepartureCallback* EvtClientDepartureCallback;
    // Client can specify special functionality provided by this Module.
    //
    NOTIFY_USER_MULTIPLE_MODE_TYPE ModeType;
} DMF_CONFIG_NotifyUserWithRequestMultiple;
````
Member | Description
----|----
MaximumNumberOfPendingRequests | The maximum number of events the Client wants to enqueue. If a caller sends more Requests (via IOCTLS) simultaneously, the Module will fail DMF_NotifyUserWithRequestMultiple_RequestProcess with STATUS_INVALID_DEVICE_STATE.
MaximumNumberOfPendingDataBuffers | The maximum number of entries that contain data that will be returned via the Requests. If more data is available, the oldest entry in the queue is discarded.
SizeOfDataBuffer | Size of data buffer that is passed to the Client's callback.
EvtClientArrivalCallback | Callback registered by Client for Data/Request processing upon new Client arrival. If Client returns a non-success status, the User will not be added to collection.
EvtClientDepartureCallback | Callback registered by Client for Data/Request processing upon new Client departure.
ModeType | Used by Client to specify special functionality provided by this Module.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

##### NOTIFY_USER_MULTIPLE_MODE_TYPE

````
// These definitions specify additional functionality
// that can be enabled.
//
typedef union _NOTIFY_USER_MULTIPLE_MODE_TYPE
{
    UINT32 NotifyUserMultipleMode;
    struct {                                      // Bits:
        UINT32 ReplayLastMessageToNewClients:1;   //    0: Cache last buffer and send to new user.
        UINT32 Reserved:31;                       // 31-1: Reserved
    } Modes;
} NOTIFY_USER_MULTIPLE_MODE_TYPE;
````
-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

##### EVT_DMF_NotifyUserWithRequestMultiple_ArrivalCallback
````
typedef
_Function_class_(EVT_DMF_NotifyUserWithRequestMultiple_ArrivalCallback)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_NotifyUserWithRequestMultiple_ArrivalCallback(_In_ DMFMODULE DmfModule,
                                                      _In_ WDFFILEOBJECT FileObject);
````

This is an optional callback that can be used in situations where the client needs to evaluate the WDFFILEOBJECT to 
decide whether or the user (driver, application, service) corresponding to the WDFFILEOBJECT gets the notifications from this Module.

#### Return Value
If the status is a success status, the this user will recieve the notifications from this Module.
If the status is a failure status, this user will be ignored by the Module

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NotifyUserWithRequestMultiple Module handle.
FileObject | WDF file object that describes a file that is being opened for the specified request.

##### EVT_DMF_NotifyUserWithRequestMultiple_DepartureCallback
````
typedef
_Function_class_(EVT_DMF_NotifyUserWithRequestMultiple_DepartureCallback)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_NotifyUserWithRequestMultiple_DepartureCallback(_In_ DMFMODULE DmfModule,
                                                        _In_ WDFFILEOBJECT FileObject);
````

Optional callback registered by Client for user departure. Note this is not called if the client returned a failure status in 
EVT_DMF_NotifyUserWithRequestMultiple_ArrivalCallback.


##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NotifyUserWithRequestMultiple Module handle.
FileObject | WDF file object that describes a file that is being opened for the specified request.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

##### DMF_NotifyUserWithRequestMultiple_DataBroadcast

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_NotifyUserWithRequestMultiple_DataBroadcast(
    _In_ DMFMODULE DmfModule,
    _In_reads_bytes_(DataBufferSize) VOID* DataBuffer,
    _In_ size_t DataBufferSize,
    _In_ NTSTATUS NtStatus
    );
````

Broadcasts data to all NotifyUserWithRequest Modules corresponding to the number of Client connections.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NotifyUserWithRequestMultiple Module handle.
DataBuffer | A Client Specific context that is passed to the given callback.
DataBufferSize | Size of EventCallbackContext buffer (for static analysis). This must be equal to the SizeOfDataBuffer parameter in the config. If any other size is passed this API will fail.
NtStatus | The NTSTATUS to set in the Request that is to be returned.

##### Remarks
* This Module caches the data in the DataBuffer internally
* This API can also fail in cases of low memory. In such a situation the users will essentially miss a message. At present there is no workaround for this case.

##### DMF_NotifyUserWithRequestMultiple_RequestProcess

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_NotifyUserWithRequestMultiple_RequestProcess(
  _In_ DMFMODULE DmfModule,
  _In_ WDFREQUEST Request
  );
````

This Method routes the Request to NotifyUserWithRequest_RequestProcess in the Client's dynamically created NotifyUserWithRequest
Module.

##### Returns

NTSTATUS. Fails if the Request cannot be added to the queue. Caller is responsible for WdfRequestComplete if this fails.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NotifyUserWithRequestMultiple Module handle.
Request | The given Request.

##### Remarks

* The caller must not touch the WDFREQUEST as soon as the call to this API is made as the ownership of the request is
transferred to this Module. The caller's callback routine (CompletionCallback) would be called when it is time to complete the request.
* If this API fails, the WDFREQUEST is still owned by the caller, and the caller must take the appropriate action
to complete the request.
-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module adds multiple Client support to the NotifyUserWithRequest Module by dynamically creating/destroying
NotifyUserWithRequest Module with a FileCreate/Close.

* Only last MaximumNumberOfPendingDataBuffers buffers are preserved. Any new Data that comes in will overwrite older data.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

* Uses WDF Callbacks for FileCreate and FileClose to manage connection specific dynamic Modules.
-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* Dmf_PostureRequestHandler

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Driver Patterns.

-----------------------------------------------------------------------------------------------------------------------------------

