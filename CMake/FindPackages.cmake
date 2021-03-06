# generated by Buildyard, do not edit.

include(System)
list(APPEND FIND_PACKAGES_DEFINES ${SYSTEM})
find_package(PkgConfig)

set(ENV{PKG_CONFIG_PATH} "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
if(PKG_CONFIG_EXECUTABLE)
  find_package(OFED )
  if((NOT OFED_FOUND) AND (NOT OFED_FOUND))
    pkg_check_modules(OFED OFED)
  endif()
else()
  find_package(OFED  )
endif()

if(PKG_CONFIG_EXECUTABLE)
  find_package(UDT )
  if((NOT UDT_FOUND) AND (NOT UDT_FOUND))
    pkg_check_modules(UDT UDT)
  endif()
else()
  find_package(UDT  )
endif()

if(PKG_CONFIG_EXECUTABLE)
  find_package(Boost 1.41.0 COMPONENTS system regex date_time serialization program_options)
  if((NOT Boost_FOUND) AND (NOT BOOST_FOUND))
    pkg_check_modules(Boost Boost>=1.41.0)
  endif()
  if((NOT Boost_FOUND) AND (NOT BOOST_FOUND))
    message(FATAL_ERROR "Could not find Boost")
  endif()
else()
  find_package(Boost 1.41.0  REQUIRED system regex date_time serialization program_options)
endif()

if(PKG_CONFIG_EXECUTABLE)
  find_package(Lunchbox 1.9.0)
  if((NOT Lunchbox_FOUND) AND (NOT LUNCHBOX_FOUND))
    pkg_check_modules(Lunchbox Lunchbox>=1.9.0)
  endif()
  if((NOT Lunchbox_FOUND) AND (NOT LUNCHBOX_FOUND))
    message(FATAL_ERROR "Could not find Lunchbox")
  endif()
else()
  find_package(Lunchbox 1.9.0  REQUIRED)
endif()


if(EXISTS ${CMAKE_SOURCE_DIR}/CMake/FindPackagesPost.cmake)
  include(${CMAKE_SOURCE_DIR}/CMake/FindPackagesPost.cmake)
endif()

if(OFED_FOUND)
  set(OFED_name OFED)
  set(OFED_FOUND TRUE)
elseif(OFED_FOUND)
  set(OFED_name OFED)
  set(OFED_FOUND TRUE)
endif()
if(OFED_name)
  list(APPEND FIND_PACKAGES_DEFINES COLLAGE_USE_OFED)
  set(FIND_PACKAGES_FOUND "${FIND_PACKAGES_FOUND} OFED")
  link_directories(${${OFED_name}_LIBRARY_DIRS})
  if(NOT "${${OFED_name}_INCLUDE_DIRS}" MATCHES "-NOTFOUND")
    include_directories(${${OFED_name}_INCLUDE_DIRS})
  endif()
endif()

if(UDT_FOUND)
  set(UDT_name UDT)
  set(UDT_FOUND TRUE)
elseif(UDT_FOUND)
  set(UDT_name UDT)
  set(UDT_FOUND TRUE)
endif()
if(UDT_name)
  list(APPEND FIND_PACKAGES_DEFINES COLLAGE_USE_UDT)
  set(FIND_PACKAGES_FOUND "${FIND_PACKAGES_FOUND} UDT")
  link_directories(${${UDT_name}_LIBRARY_DIRS})
  if(NOT "${${UDT_name}_INCLUDE_DIRS}" MATCHES "-NOTFOUND")
    include_directories(${${UDT_name}_INCLUDE_DIRS})
  endif()
endif()

if(BOOST_FOUND)
  set(Boost_name BOOST)
  set(Boost_FOUND TRUE)
elseif(Boost_FOUND)
  set(Boost_name Boost)
  set(BOOST_FOUND TRUE)
endif()
if(Boost_name)
  list(APPEND FIND_PACKAGES_DEFINES COLLAGE_USE_BOOST)
  set(FIND_PACKAGES_FOUND "${FIND_PACKAGES_FOUND} Boost")
  link_directories(${${Boost_name}_LIBRARY_DIRS})
  if(NOT "${${Boost_name}_INCLUDE_DIRS}" MATCHES "-NOTFOUND")
    include_directories(SYSTEM ${${Boost_name}_INCLUDE_DIRS})
  endif()
endif()

if(LUNCHBOX_FOUND)
  set(Lunchbox_name LUNCHBOX)
  set(Lunchbox_FOUND TRUE)
elseif(Lunchbox_FOUND)
  set(Lunchbox_name Lunchbox)
  set(LUNCHBOX_FOUND TRUE)
endif()
if(Lunchbox_name)
  list(APPEND FIND_PACKAGES_DEFINES COLLAGE_USE_LUNCHBOX)
  set(FIND_PACKAGES_FOUND "${FIND_PACKAGES_FOUND} Lunchbox")
  link_directories(${${Lunchbox_name}_LIBRARY_DIRS})
  if(NOT "${${Lunchbox_name}_INCLUDE_DIRS}" MATCHES "-NOTFOUND")
    include_directories(${${Lunchbox_name}_INCLUDE_DIRS})
  endif()
endif()

set(COLLAGE_BUILD_DEBS autoconf;automake;cmake;doxygen;git;git-review;libavahi-compat-libdnssd-dev;libboost-date-time-dev;libboost-filesystem-dev;libboost-program-options-dev;libboost-regex-dev;libboost-serialization-dev;libboost-system-dev;libhwloc-dev;libibverbs-dev;libjpeg-turbo8-dev;librdmacm-dev;libturbojpeg;libudt-dev;pkg-config;subversion)

set(COLLAGE_DEPENDS OFED;UDT;Boost;Lunchbox)

# Write defines.h and options.cmake
if(NOT PROJECT_INCLUDE_NAME)
  message(WARNING "PROJECT_INCLUDE_NAME not set, old or missing Common.cmake?")
  set(PROJECT_INCLUDE_NAME ${CMAKE_PROJECT_NAME})
endif()
if(NOT OPTIONS_CMAKE)
  set(OPTIONS_CMAKE ${CMAKE_BINARY_DIR}/options.cmake)
endif()
set(DEFINES_FILE "${CMAKE_BINARY_DIR}/include/${PROJECT_INCLUDE_NAME}/defines${SYSTEM}.h")
list(APPEND COMMON_INCLUDES ${DEFINES_FILE})
set(DEFINES_FILE_IN ${DEFINES_FILE}.in)
file(WRITE ${DEFINES_FILE_IN}
  "// generated by CMake/FindPackages.cmake, do not edit.\n\n"
  "#ifndef ${CMAKE_PROJECT_NAME}_DEFINES_${SYSTEM}_H\n"
  "#define ${CMAKE_PROJECT_NAME}_DEFINES_${SYSTEM}_H\n\n")
file(WRITE ${OPTIONS_CMAKE} "# Optional modules enabled during build\n")
foreach(DEF ${FIND_PACKAGES_DEFINES})
  add_definitions(-D${DEF}=1)
  file(APPEND ${DEFINES_FILE_IN}
  "#ifndef ${DEF}\n"
  "#  define ${DEF} 1\n"
  "#endif\n")
if(NOT DEF STREQUAL SYSTEM)
  file(APPEND ${OPTIONS_CMAKE} "set(${DEF} ON)\n")
endif()
endforeach()
if(CMAKE_MODULE_INSTALL_PATH)
  install(FILES ${OPTIONS_CMAKE} DESTINATION ${CMAKE_MODULE_INSTALL_PATH}
    COMPONENT dev)
else()
  message(WARNING "CMAKE_MODULE_INSTALL_PATH not set, old or missing Common.cmake?")
endif()
file(APPEND ${DEFINES_FILE_IN}
  "\n#endif\n")

include(UpdateFile)
configure_file(${DEFINES_FILE_IN} ${DEFINES_FILE} COPYONLY)
if(Boost_FOUND) # another WAR for broken boost stuff...
  set(Boost_VERSION ${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION})
endif()
if(CUDA_FOUND)
  string(REPLACE "-std=c++11" "" CUDA_HOST_FLAGS "${CUDA_HOST_FLAGS}")
  string(REPLACE "-std=c++0x" "" CUDA_HOST_FLAGS "${CUDA_HOST_FLAGS}")
endif()
if(QT4_FOUND)
  if(WIN32)
    set(QT_USE_QTMAIN TRUE)
  endif()
  # Configure a copy of the 'UseQt4.cmake' system file.
  if(NOT EXISTS ${QT_USE_FILE})
    message(WARNING "Can't find QT_USE_FILE!")
  else()
    set(_customUseQt4File "${CMAKE_BINARY_DIR}/UseQt4.cmake")
    file(READ ${QT_USE_FILE} content)
    # Change all include_directories() to use the SYSTEM option
    string(REPLACE "include_directories(" "include_directories(SYSTEM " content ${content})
    file(WRITE ${_customUseQt4File} ${content})
    set(QT_USE_FILE ${_customUseQt4File})
    include(${QT_USE_FILE})
  endif()
endif()
if(FIND_PACKAGES_FOUND)
  if(MSVC)
    message(STATUS "Configured with ${FIND_PACKAGES_FOUND}")
  else()
    message(STATUS "Configured with ${CMAKE_BUILD_TYPE}${FIND_PACKAGES_FOUND}")
  endif()
endif()
