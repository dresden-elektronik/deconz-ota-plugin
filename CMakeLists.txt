cmake_minimum_required(VERSION 3.13)

project(std_otau_plugin VERSION 2.22.2 LANGUAGES C;CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Core Widgets REQUIRED)

#--- deCONZ library ---------------------------------------------------

include(FetchContent)

if (NOT DECONZ_FULL_BUILD)

FetchContent_Declare(
  deconzlib
  GIT_REPOSITORY https://github.com/dresden-elektronik/deconz-lib.git
  GIT_TAG        main
)
# FetchContent_MakeAvailable(deconzlib)
# FetchContent_MakeAvailable requires CMake 3.14, but Debian Buster has only 3.13
FetchContent_GetProperties(deconzlib)
if (NOT deconzlib_POPULATED)
    FetchContent_Populate(deconzlib)
    add_subdirectory(${deconzlib_SOURCE_DIR} ${deconzlib_BINARY_DIR})
endif()

# --- actor model -----------------------------------------------------

FetchContent_Declare(
  actormodel
  GIT_REPOSITORY https://git.sr.ht/~cryo/actor_model
  GIT_TAG        master
)

# FetchContent_MakeAvailable(actormodel)
# FetchContent_MakeAvailable requires CMake 3.14, but Debian Buster has only 3.13

FetchContent_GetProperties(actormodel)
if (NOT actormodel_POPULATED)
    FetchContent_Populate(actormodel)
    add_subdirectory(${actormodel_SOURCE_DIR} ${actormodel_BINARY_DIR})
endif()

endif() # NOT DECONZ_FULL_BUILD
#----------------------------------------------------------------------

set(PLUGIN_INCLUDE_FILES
    std_otau_plugin.h
    std_otau_widget.h
    otau_file.h
    otau_file_loader.h
    otau_model.h
    otau_node.h
)

add_library(${PROJECT_NAME} SHARED
    ${PLUGIN_INCLUDE_FILES}

    std_otau_widget.ui

    std_otau_plugin.cpp
    std_otau_widget.cpp
    otau_file.cpp
    otau_file_loader.cpp
    otau_model.cpp
    otau_node.cpp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ACTOR_MODEL)

target_link_libraries(${PROJECT_NAME}
    PRIVATE Qt5::Core
    PRIVATE Qt5::Gui
    PRIVATE Qt5::Widgets
    PRIVATE deCONZLib
    am_plugin_hdr
)

#--------------------------------------------------------------
include(GNUInstallDirs)

if (UNIX)
    if (APPLE)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            INSTALL_RPATH @loader_path)
    else()
        set_target_properties(${PROJECT_NAME} PROPERTIES
            INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}")
    endif()
endif()


if (${CMAKE_HOST_SYSTEM_NAME} MATCHES "Linux")
    install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/deCONZ/plugins"
    )

    # following applies only when build as part of deCONZ package
    # if (DECONZ_FULL_BUILD)
    # endif()
endif()
