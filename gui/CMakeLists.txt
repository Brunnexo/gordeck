
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui Concurrent Multimedia OpenGL Svg LinguistTools)
if(APPLE)
	find_package(Qt5 REQUIRED COMPONENTS MacExtras)
endif()
if(CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER)
	find_package(SDL2 MODULE REQUIRED)
endif()

if(WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()

set(RESOURCE_FILES "")

# Translations, compiled from i18n/*.ts and embedded as resources under :/i18n
set(CHIAKI_GUI_TS_FILES
		i18n/chiaki_pt_BR.ts)
qt5_add_translation(CHIAKI_GUI_QM_FILES ${CHIAKI_GUI_TS_FILES})
set(CHIAKI_GUI_TRANSLATIONS_QRC "${CMAKE_CURRENT_BINARY_DIR}/translations.qrc")
set(CHIAKI_GUI_TRANSLATIONS_QRC_CONTENT "<RCC>\n  <qresource prefix=\"/i18n\">\n")
foreach(qm_file ${CHIAKI_GUI_QM_FILES})
	get_filename_component(qm_name "${qm_file}" NAME)
	string(APPEND CHIAKI_GUI_TRANSLATIONS_QRC_CONTENT "    <file alias=\"${qm_name}\">${qm_file}</file>\n")
endforeach()
string(APPEND CHIAKI_GUI_TRANSLATIONS_QRC_CONTENT "  </qresource>\n</RCC>\n")
file(WRITE "${CHIAKI_GUI_TRANSLATIONS_QRC}" "${CHIAKI_GUI_TRANSLATIONS_QRC_CONTENT}")
qt5_add_resources(CHIAKI_GUI_TRANSLATIONS_RCC "${CHIAKI_GUI_TRANSLATIONS_QRC}")

if(APPLE)
	list(APPEND RESOURCE_FILES "chiaki.icns")
endif()

add_executable(gordeck WIN32
		${RESOURCE_FILES}
		include/exception.h
		src/main.cpp
		include/streamwindow.h
		src/streamwindow.cpp
		include/mainwindow.h
		src/mainwindow.cpp
		include/consolecarousel.h
		src/consolecarousel.cpp
		include/hintbar.h
		src/hintbar.cpp
		include/moderndialog.h
		src/moderndialog.cpp
		include/modernwidgets.h
		src/modernwidgets.cpp
		include/gamepadnavigator.h
		src/gamepadnavigator.cpp
		include/i18n.h
		src/i18n.cpp
		include/theme.h
		src/theme.cpp
		include/discoverymanager.h
		src/discoverymanager.cpp
		include/streamsession.h
		src/streamsession.cpp
		include/sessionlog.h
		src/sessionlog.cpp
		include/avopenglwidget.h
		src/avopenglwidget.cpp
		include/avopenglframeuploader.h
		src/avopenglframeuploader.cpp
		include/settings.h
		src/settings.cpp
		include/registdialog.h
		src/registdialog.cpp
		include/host.h
		src/host.cpp
		include/settingskeycapturedialog.h
		src/settingskeycapturedialog.cpp
		include/settingsdialog.h
		src/settingsdialog.cpp
		include/manualhostdialog.h
		src/manualhostdialog.cpp
		res/resources.qrc
		include/controllermanager.h
		src/controllermanager.cpp
		include/loginpindialog.h
		src/loginpindialog.cpp
		${CHIAKI_GUI_QM_FILES}
		${CHIAKI_GUI_TRANSLATIONS_RCC})
target_include_directories(gordeck PRIVATE include)

target_link_libraries(gordeck chiaki-lib)
if(CHIAKI_ENABLE_CLI)
	add_definitions(-DCHIAKI_ENABLE_CLI)
	target_link_libraries(gordeck chiaki-cli-lib)
endif()

target_link_libraries(gordeck Qt5::Core Qt5::Widgets Qt5::Gui Qt5::Concurrent Qt5::Multimedia Qt5::OpenGL Qt5::Svg)
if(APPLE)
	target_link_libraries(gordeck Qt5::MacExtras)
	target_compile_definitions(gordeck PRIVATE CHIAKI_GUI_ENABLE_QT_MACEXTRAS)
endif()
if(CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER)
	target_link_libraries(gordeck SDL2::SDL2)
	target_compile_definitions(gordeck PRIVATE CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER)
endif()
if(CHIAKI_ENABLE_SETSU)
	target_link_libraries(gordeck setsu)
	target_compile_definitions(gordeck PRIVATE CHIAKI_GUI_ENABLE_SETSU)
endif()
if(NOT CHIAKI_ENABLE_FFMPEG_DECODER)
	message(FATAL_ERROR "Chiaki GUI requires CHIAKI_ENABLE_FFMPEG_DECODER=ON")
endif()

set_target_properties(gordeck PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/MacOSXBundleInfo.plist.in"
		MACOSX_BUNDLE_BUNDLE_NAME Gordeck
		MACOSX_BUNDLE_BUNDLE_VERSION ${CHIAKI_VERSION}
		MACOSX_BUNDLE_COPYRIGHT "thestr4ng3r (AGPLv3)"
		MACOSX_BUNDLE_GUI_IDENTIFIER "com.bnx.gordeck"
		MACOSX_BUNDLE_ICON_FILE chiaki.icns
		RESOURCE "${RESOURCE_FILES}")

install(TARGETS gordeck
		RUNTIME DESTINATION bin
		BUNDLE DESTINATION bin)
install(FILES gordeck.desktop DESTINATION share/applications)
install(FILES gordeck.png DESTINATION share/icons/hicolor/512x512/apps)
install(FILES com.bnx.gordeck.metainfo.xml DESTINATION share/metainfo)
