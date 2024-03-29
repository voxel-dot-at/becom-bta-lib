cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

message (STATUS "CMake version: " ${CMAKE_VERSION} )

project( bta )

SET(CMAKE_COLOR_MAKEFILE ON)
SET(CMAKE_VERBOSE_MAKEFILE OFF)

#Including local extra cmake modules
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

#Function for generation of export macros for libraries
include(GenerateExportHeader) #(toffy_export.h - TOFFY_EXPORT)
# Include module with fuction 'write_basic_package_version_file' for ToffyConfig.cmake
include(CMakePackageConfigHelpers)

################################################################################
#Version stuff
################################################################################

# Make a version file containing the current version from git.
include(GetGitRevisionDescription)
git_describe(VERSION --tag --long)

##parse the version information into pieces.
string(REGEX REPLACE "^v([0-9]+)\\..*" "\\1" PROJECT_VERSION_MAJOR "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.([0-9]+).*" "\\1" PROJECT_VERSION_MINOR "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" PROJECT_VERSION_PATCH "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.[0-9]+\\.[0-9]+(.*)" "\\1" PROJECT_VERSION_TWEAK "${VERSION}")
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
set(PROJECT_DESCRIPTION "Toffy 3D computer vision framework")

set(PROJECT_VERSION_FULL "${PROJECT_VERSION}${PROJECT_VERSION_TWEAK}")

message (STATUS "version: " ${VERSION} )
message (STATUS ${PROJECT_NAME}" version: " ${PROJECT_VERSION_FULL} )



set(bta_VERSION_MAJOR 3)
set(bta_VERSION_MINOR 3)
set(bta_VERSION_PATCH 11)
set(bta_VERSION_STRING ${bta_VERSION_MAJOR}.${bta_VERSION_MINOR}.${bta_VERSION_PATCH})

file(GLOB HEADERS "inc/*.h" )

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_definitions(-O2 -fPIC)

# user selectable options:
if(NOT MSVC)
  option(BTA_WO_USB "disable usb support" OFF)
endif()
option(BTA_WO_JPEG "disable jpeg support" OFF)


option(BTA_WO_UART "disable uart transport" OFF)
option(BTA_WO_USB "disable usb transport" OFF)
option(BTA_WO_ETH "disable eth transport" OFF)

# set install directory to local if not specified otherwise:
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

  set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/dist/" )
    # CACHE PATH "..."
endif()

SET(CMAKE_COLOR_MAKEFILE ON)
SET(CMAKE_VERBOSE_MAKEFILE ON)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(PLAT_WINDOWS "yes")
    add_definitions(-DPLAT_WINDOWS)
endif()
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(PLAT_LINUX "yes")
    add_definitions(-DPLAT_LINUX)
endif()
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(PLAT_APPLE "yes")
    add_definitions(-DPLAT_APPLE)

    # for macports:
    include_directories ("/opt/local/include")
    link_directories("/opt/local/lib")
endif()


macro(use_c99)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_C_FLAGS "-std=gnu99 ${CMAKE_C_FLAGS}")
    endif ()
  else ()
    set (CMAKE_C_STANDARD 99)
  endif ()
endmacro(use_c99)


set( CMAKE_CONFIGURATION_TYPES "Debug;Release" )

# #Build target dir for config, export and package files
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

################################################################################
# Install options
################################################################################
MESSAGE(STATUS "CMAKE_INSTALL_PREFIX: " ${CMAKE_INSTALL_PREFIX})

set(INSTALL_LIB_DIR lib/ CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin/ CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include/${PROJECT_NAME} CACHE PATH "Installation directory for header files")
if(WIN32 AND NOT CYGWIN)
  set(DEF_INSTALL_CMAKE_DIR cmake)
else()
  set(DEF_INSTALL_CMAKE_DIR lib/cmake/${PROJECT_NAME})
endif()
set(INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH "Installation directory for CMake files")
message(INFO "install dir " ${INSTALL_CMAKE_DIR})

################################################################################
# generate cmake config file 
################################################################################
include(CMakePackageConfigHelpers)
set(PROJECT_VERSION ${bta_VERSION_STRING})
set(PROJECT_VERSION_MAJOR ${bta_VERSION_MAJOR})
set(PROJECT_VERSION_MINOR ${bta_VERSION_MINOR})
set(PROJECT_VERSION_PATCH ${bta_VERSION_PATCH})

# define temporary build dir and files 
set(cmake_dir "${CMAKE_CURRENT_BINARY_DIR}/cmake")
set(project_config "${cmake_dir}/${PROJECT_NAME}Config.cmake") #output
set(version_config "${cmake_dir}/${PROJECT_NAME}ConfigVersion.cmake") #output

#Targets name
set(targets_export_name "${PROJECT_NAME}Targets")
set(namespace "bta")

# Set version required to config
write_basic_package_version_file(
   "${version_config}" VERSION ${bta_VERSION_STRING} COMPATIBILITY SameMajorVersion
)

configure_package_config_file("cmake/btaConfig.cmake.in"
    "${project_config}"
    INSTALL_DESTINATION "${INSTALL_CMAKE_DIR}"
)

install(
    FILES "${project_config}" "${version_config}"
    DESTINATION "${INSTALL_CMAKE_DIR}"
)

#######################################
# configure config header file
#######################################
set(config_h_in "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}_config.h.in")
set(config_h "${generated_dir}/${PROJECT_NAME}_config.h")
configure_file("${config_h_in}" "${config_h}")
list(APPEND headers ${config_h})
install(
   FILES ${config_h}
   DESTINATION include
)

#######################################
# Generating .pc file for pkg-config
#######################################
SET(PKG_CONFIG_REQUIRES "")
SET(PKG_CONFIG_LIBDIR
    "\${prefix}/lib/"
)
SET(PKG_CONFIG_INCLUDEDIR
    "\${prefix}/include/${PROJECT_NAME}"
)
SET(PKG_CONFIG_LIBS
    "-L\${libdir} -l${PROJECT_NAME}"
)
SET(PKG_CONFIG_CFLAGS
    "-I\${includedir}"
)

CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pkg-config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc"
)

install(
    FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc"
    DESTINATION lib/pkgconfig
)


################################################################################
# Build config windows - unix - arm
################################################################################
message (STATUS "MSVC: " ${MSVC})
if(MSVC)
    #Generated ProductVersionFiles --> https://github.com/halex2005/CMakeHelpers
    #VersionInfo.h & VersionResource.h
    include(generate_product_version)
    generate_product_version(
       ProductVersionFiles
       NAME ${PROJECT_NAME}
       ICON ${PATH_TO_APPLICATION_ICON}
       VERSION_MAJOR ${PROJECT_VERSION_MAJOR}
       VERSION_MINOR ${PROJECT_VERSION_MINOR}
       VERSION_PATCH ${PROJECT_VERSION_PATCH}
       VERSION_REVISION ${BUILD_REVISION}
       COMPANY_NAME VoXel Interaction Design
       FILE_DESCRIPTION ${toffy_DESCRIPTION}
    )

    #Set MSVC to preprocessor
    list(APPEND DEFINITIONS -DMSVC)

    #Set windows preprocesor definitions
    list(APPEND DEFINITIONS -D_WIN32_WINNT=0x0601) #Win10 0501-> Win7

    #Build static version of the library for windows
    if(BUILD_STATIC)
        #We link statically to reduce dependancies
        foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            if(${flag_var} MATCHES "/MD")
                string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
            endif(${flag_var} MATCHES "/MD")
            if(${flag_var} MATCHES "/MDd")
                string(REGEX REPLACE "/MDd" "/MTd" ${flag_var} "${${flag_var}}")
            endif(${flag_var} MATCHES "/MDd")
        endforeach(flag_var)


    endif()

else(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    #Unix build options
  message("unix")

    # TODO check compile options for build type, Debug, Release, ....
    list(APPEND DEFINITIONS -DEIGEN_DONT_VECTORIZE -DEIGEN_DISABLE_UNALIGNED_ARRAY_ASSERT -DEIGEN_SUPPORT_STAGE=99)
    add_compile_options(-pthread -g -ggdb -Wall -Wno-long-long)
    #ARM complains if pthread not added explicitly
    list(APPEND LIBS pthread)
endif()


################################################################################
# Find dependencies
################################################################################

if(NOT MSVC)
    find_package(PkgConfig)

    pkg_check_modules( LIBUSB libusb-1.0 )
    if( NOT LIBUSB_FOUND)
      set(BTA_WO_USB ON)
      message("libusb not found.")
    else()
      message("libusb inc dir " ${LIBUSB_INCLUDE_DIR})
      message("libusb lib     " ${LIBUSB_LIBRARIES})
    endif()
    if( BTA_WO_USB )
      message("will not build USB support. " ${BTA_WO_USB} )
      add_definitions( -DBTA_WO_USB )
    endif()
    
    if(LIBJPEG_FOUND)
      include_directories( ${LIBUSB_INCLUDE_DIR})
      link_libraries( ${LIBUSB_LIBRARIES})
    else()
      add_definitions( -DBTA_WO_JPEG )
    endif()

    pkg_check_modules( LIBJPEG libjpeg )
    if(LIBJPEG_FOUND)
        message("libjpeg inc dir " ${LIBJPEG_INCLUDE_DIR})
        message("libjpeg lib     " ${LIBJPEG_LIBRARIES})

        include_directories( ${LIBJPEG_INCLUDE_DIR})
        link_libraries( ${LIBJPEG_LIBRARIES})
    else()
        message("libjpeg not found ")
        add_definitions( -DBTA_WO_LIBJPEG)
    endif()
endif()

if (APPLE)
  message("APPLE platform detected! Disabling P100 and USB")
  add_definitions( -DBTA_WO_USB -DBTA_WO_P100 )
endif()

#
#
#
include_directories(
        "${CMAKE_CURRENT_SOURCE_DIR}/inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/common"
        "${CMAKE_CURRENT_SOURCE_DIR}/common/fastBf"
        "${CMAKE_CURRENT_SOURCE_DIR}/common/lzma"
        "${CMAKE_CURRENT_SOURCE_DIR}/sdk"
        "${generated_dir}"
	)

#
# source helper files first
#
add_subdirectory (common) 

add_subdirectory (sdk) 
