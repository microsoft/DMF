
set(TargetName "DmfUModules.Library")
set(${TargetName}_Srcs
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/DMF_String.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/DMF_UefiLogs.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/DMF_UefiOperation.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_AcpiTarget.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_ActivitySensor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_AlertableSleep.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_BufferPool.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_BufferQueue.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_ComponentFirmwareUpdateHidTransport.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_ContinuousRequestTarget.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_DefaultTarget.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_DeviceInterfaceMultipleTarget.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_DeviceInterfaceTarget.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_Doorbell.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_EyeGazeIoctl.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_File.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_GpioTarget.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_HashTable.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_HingeAngle.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_I2cTarget.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_Interface_BusTarget.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_Interface_ComponentFirmwareUpdate.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_Interface_SystemManagementFramework.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_InterruptResource.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_IoctlHandler.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_MobileBroadband.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_PingPongBuffer.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_QueuedWorkItem.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_Registry.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_RequestTarget.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_RingBuffer.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_Rundown.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_ScheduledTask.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_SimpleOrientation.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_SmbiosWmi.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_SpbTarget.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_SpiTarget.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_Stack.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_Thread.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_ThreadedBufferQueue.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_Time.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_CmApi.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_VirtualHidMini.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_ComponentFirmwareUpdate.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Library/Dmf_SymbolicLinkTarget.c
)

wdk_add_umd_library(${TargetName} STATIC UMDF 2.23 ${${TargetName}_Srcs})

set_property(TARGET ${TargetName} APPEND PROPERTY COMPILE_DEFINITIONS
	"$<$<CONFIG:Debug>:_WIN64;USE_ASSERT_BREAK;_UNICODE;UNICODE;DEBUG;DBG;WIN32_LEAN_AND_MEAN=1>"
)

set_property(TARGET ${TargetName} APPEND PROPERTY COMPILE_DEFINITIONS
	"$<$<CONFIG:Release>:_WIN64;NO_USE_ASSERT_BREAK;_UNICODE;UNICODE;WIN32_LEAN_AND_MEAN=1>"
)

set_property(TARGET ${TargetName} APPEND PROPERTY COMPILE_FLAGS
	"/TP /std:c++17"
)

if(WPP_ENABLED EQUAL 1)
	set(WPP_PRPROC_FOLDER ${CMAKE_CURRENT_BINARY_DIR}/WppTmp)
	file(MAKE_DIRECTORY ${WPP_PRPROC_FOLDER})
	set_property(TARGET ${TargetName} APPEND PROPERTY COMPILE_DEFINITIONS
	"ENABLE_WPP_RECORDER=1;WPP_EMIT_FUNC_NAME"
	)
	target_include_directories(${TargetName} PRIVATE 
		${WPP_PRPROC_FOLDER}
	)
	if(CMAKE_GENERATOR MATCHES "Visual Studio")
		set(wpp_target ${TargetName})
	else()
		add_custom_target(${TargetName}Null)
		add_dependencies(${TargetName} ${TargetName}Null)
		set(wpp_target ${TargetName}Null)
	endif()
	set(WPP_SOURCE_FILES ${${TargetName}_Srcs})
	foreach(wpp_file IN LISTS WPP_SOURCE_FILES)
		string(REPLACE "/" "\\" wpp_file_str  ${wpp_file})
		umd_wpp_preproc(${wpp_target}  ${wpp_file_str} ${WPP_PRPROC_FOLDER}/  ${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfTrace.h)
	endforeach()
endif()

target_include_directories(${TargetName} BEFORE PRIVATE 
	${CMAKE_CURRENT_SOURCE_DIR}/
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/Modules.Core/
)

add_custom_command(
	TARGET ${TargetName} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${DMF_LIB_FINAL_OUTPUT_PATH}/individual_libs/${TargetName}/
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${TargetName}.lib  ${DMF_LIB_FINAL_OUTPUT_PATH}/individual_libs/${TargetName}/${TargetName}.lib
	VERBATIM
)