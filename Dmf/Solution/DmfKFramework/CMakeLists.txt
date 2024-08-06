# kernel mode lib generator script
set(TargetName "DmfKFramework")
set(${TargetName}_Srcs
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfBranchTrack.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfCall.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfContainer.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfCore.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfDeviceInit.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfFilter.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfGeneric.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfHelpers.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfInterfaceInternal.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfInternal.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfLiveKernelDump.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfModuleCollection.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfPortable.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfUtility.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfValidate.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/Modules.Core/Dmf_BranchTrack.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/Modules.Core/Dmf_Bridge.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/Modules.Core/Dmf_LiveKernelDump.c
	)

wdk_add_kmd_library(${TargetName} STATIC KMDF 1.15 ${${TargetName}_Srcs})

set_property(TARGET ${TargetName} APPEND PROPERTY COMPILE_DEFINITIONS
	"$<$<CONFIG:Debug>:_WIN64;USE_ASSERT_BREAK;_UNICODE;UNICODE;DEBUG;DBG;>"
)

set_property(TARGET ${TargetName} APPEND PROPERTY COMPILE_DEFINITIONS
	"$<$<CONFIG:Release>:_WIN64;NO_USE_ASSERT_BREAK;_UNICODE;UNICODE;>"
)

set_property(TARGET ${TargetName} APPEND PROPERTY COMPILE_FLAGS
	"/TP"
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
	set(WPP_SOURCE_FILES ${${TargetName}_Srcs})
	if(CMAKE_GENERATOR MATCHES "Visual Studio")
		set(wpp_target ${TargetName})
	else()
		add_custom_target(${TargetName}Null)
		add_dependencies(${TargetName} ${TargetName}Null)
		set(wpp_target ${TargetName}Null)
	endif()
	foreach(wpp_file IN LISTS WPP_SOURCE_FILES)
		string(REPLACE "/" "\\" wpp_file_str  ${wpp_file})
		kmd_wpp_preproc(${wpp_target}  ${wpp_file_str} ${WPP_PRPROC_FOLDER}/  ${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/DmfTrace.h)
	endforeach()
endif()

target_include_directories(${TargetName} PRIVATE 
${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/
${CMAKE_CURRENT_SOURCE_DIR}/../../Framework/Modules.Core/
)

add_custom_command(
	TARGET ${TargetName} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${DMF_LIB_FINAL_OUTPUT_PATH}/individual_libs/${TargetName}/
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${TargetName}.lib  ${DMF_LIB_FINAL_OUTPUT_PATH}/individual_libs/${TargetName}/${TargetName}.lib
	VERBATIM
)