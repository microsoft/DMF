
set(TargetName "DmfKModules.Template")

set(${TargetName}_Srcs
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_Interface_SampleInterface.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_Interface_SampleInterfaceLower.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_Interface_SampleInterfaceUpper.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_LegacyProtocol.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_LegacyTransportA.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_LegacyTransportB.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_NonPnp.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_OsrFx2.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_SampleInterfaceLowerProtocol.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_SampleInterfaceLowerTransport1.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_SampleInterfaceLowerTransport2.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_SampleInterfaceProtocol1.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_SampleInterfaceTransport1.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_SampleInterfaceTransport2.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_SampleInterfaceUpperProtocol.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_SampleInterfaceUpperTransport1.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_SampleInterfaceUpperTransport2.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_Template.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_ToasterBus.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/Dmf_VirtualHidMiniSample.c
	)

if(CMAKE_GENERATOR MATCHES "Visual Studio")
	# set(wpp_target ${TargetName})
	set_source_files_properties(${${TargetName}_Srcs} PROPERTIES LANGUAGE CXX)
endif()

wdk_add_kmd_library(${TargetName} STATIC KMDF 1.15 ${${TargetName}_Srcs})

set_property(TARGET ${TargetName} APPEND PROPERTY COMPILE_DEFINITIONS
	"$<$<CONFIG:Debug>:_WIN64;USE_ASSERT_BREAK;_UNICODE;UNICODE;DEBUG;DBG;>"
)

set_property(TARGET ${TargetName} APPEND PROPERTY COMPILE_DEFINITIONS
	"$<$<CONFIG:Release>:_WIN64;NO_USE_ASSERT_BREAK;_UNICODE;UNICODE;>"
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
	target_include_directories(${TargetName} BEFORE PUBLIC 
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
		kmd_wpp_preproc(${wpp_target}  ${wpp_file_str} ${WPP_PRPROC_FOLDER}/  ${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfTrace.h)
	endforeach()

endif()

target_include_directories(${TargetName} BEFORE PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework
	${CMAKE_CURRENT_SOURCE_DIR}/../../Modules.Template/
	${CMAKE_CURRENT_SOURCE_DIR}/
)

add_custom_command(
	TARGET ${TargetName} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${DMF_LIB_FINAL_OUTPUT_PATH}/individual_libs/${TargetName}/
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${TargetName}.lib  ${DMF_LIB_FINAL_OUTPUT_PATH}/individual_libs/${TargetName}/${TargetName}.lib
	VERBATIM
)