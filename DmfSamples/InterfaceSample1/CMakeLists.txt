
set(TargetName "InterfaceSample1")

set(${TargetName}_Srcs
	${CMAKE_CURRENT_SOURCE_DIR}/DmfInterface.c
	)

wdk_add_kmd_driver(${TargetName} KMDF 1.15 ${${TargetName}_Srcs})
target_sources(${TargetName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${TargetName}.rc)

set_property(TARGET ${TargetName} APPEND PROPERTY COMPILE_DEFINITIONS
	"$<$<CONFIG:Debug>:_WIN64;USE_ASSERT_BREAK;_UNICODE;UNICODE;DEBUG;DBG;>"
)

set_property(TARGET ${TargetName} APPEND PROPERTY COMPILE_DEFINITIONS
	"$<$<CONFIG:Release>:_WIN64;NO_USE_ASSERT_BREAK;_UNICODE;UNICODE;>"
)

target_link_libraries(${TargetName} ${DMF_KLIBS})
target_link_libraries(${TargetName}
	WDK::NTSTRSAFE
	WDK::LKMDTEL
	WDK::VHFKM
	WDK::WERKERNELAPI
)


if(WPP_ENABLED EQUAL 1)

	set(WPP_PRPROC_FOLDER ${CMAKE_CURRENT_BINARY_DIR}/WppTmp)
	file(MAKE_DIRECTORY ${WPP_PRPROC_FOLDER})
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
		kmd_wpp_preproc(${wpp_target}  ${wpp_file_str} ${WPP_PRPROC_FOLDER}/  ${CMAKE_CURRENT_SOURCE_DIR}/../../Dmf/Framework/DmfTrace.h)
	endforeach()

endif()

target_include_directories(${TargetName} BEFORE PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}/../../Dmf/Framework/
	${CMAKE_CURRENT_SOURCE_DIR}/../../Dmf/Modules.Library.Tests/
	${CMAKE_CURRENT_SOURCE_DIR}/../../Dmf/Modules.Template/
	${CMAKE_CURRENT_SOURCE_DIR}/../../Dmf/Modules.Library/
	${CMAKE_CURRENT_SOURCE_DIR}/
	)
