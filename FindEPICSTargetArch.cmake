# Determine EPICS Arch and OS Class for Target
#
# Use as:
#  find_package(EPICSTargetArch)
#
# Creates variables:
#  EPICS_TARGET_ARCHS    - List of possible target names (best match first)
#  EPICS_TARGET_CLASS    - Primary (first) OSI system class
#  EPICS_TARGET_CLASSES  - List of all OSI classes
#  EPICS_TARGET_COMPILER - Compiler name (Base >=3.15 only)
#  EPICS_TARGET_HOST     - True if target can build/run Host things (not RTEMS or vxWorks)
#

cmake_minimum_required(VERSION 2.8)

include(FindPackageHandleStandardArgs)

# CMake 2.8 promises to provide at least the following variables
# with information about the Target.
#
# For host builds, these are set for the corresponding _HOST variants
# For cross builds, must be set by the tool chain file.
# An attempt is made to normalize these values in CMakeDetermineSystem.cmake
# but some variation will remain.
#
# CMAKE_SYSTEM_NAME
# CMAKE_SYSTEM_PROCESSOR
# CMAKE_SYSTEM_VERSION
#
# Computed about the target
#
# APPLE
# UNIX
# WIN32
# MINGW
# CYGWIN
# CMAKE_C_PLATFORM_ID
# CMAKE_COMPILER_IS_GNUCC
#
# Additional non-standard variables which may be set by
# cross builds
#
# RTEMS
# RTEMS_BSP
#

if(UNIX)
  if(CMAKE_SYSTEM_NAME MATCHES Linux OR CMAKE_C_PLATFORM_ID MATCHES Linux)
    set(EPICS_TARGET_HOST 1)
    set(EPICS_TARGET_CLASS Linux)
    set(EPICS_TARGET_CLASSES Linux posix default)

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^i(.86)$")
      set(EPICS_TARGET_ARCHS "linux-${CMAKE_MATCH_1}" "linux-x86")

    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^x86_64$")
      set(EPICS_TARGET_ARCHS "linux-x86_64")

    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)$")
      set(EPICS_TARGET_ARCHS "linux-ppc")

    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)64$")
      set(EPICS_TARGET_ARCHS "linux-ppc64")

    else()
      message(WARNING "Unknown Linux Variant: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

  elseif(CMAKE_SYSTEM_NAME MATCHES SunOS)
    set(EPICS_CLASS solaris)
    set(EPICS_CLASSES solaris posix default)

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^i.86$")
      if(CMAKE_COMPILER_IS_GNUCC)
        set(EPICS_TARGET_ARCHS "solaris-x86-gnu")
      else()
        set(EPICS_TARGET_COMPILER solStudio)
        set(EPICS_TARGET_ARCHS "solaris-x86")
      endif()

    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^x86_64$")
      if(CMAKE_COMPILER_IS_GNUCC)
        set(EPICS_TARGET_ARCHS "solaris-x86_64-gnu")
      else()
        set(EPICS_TARGET_COMPILER solStudio)
        set(EPICS_TARGET_ARCHS "solaris-x86_64")
      endif()

    else()
      message(WARNING "Unknown SunOS Variant: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

  elseif(RTEMS)
    set(EPICS_TARGET_HOST 0)
    set(EPICS_TARGET_CLASS RTEMS)
    set(EPICS_TARGET_CLASSES RTEMS posix default)
    set(EPICS_TARGET_ARCHS "RTEMS-${RTEMS_BSP}")

  else()
    message(WARNING "Unknown *nix Variant: ${CMAKE_SYSTEM_NAME}")
  endif()

elseif(WIN32)
  set(EPICS_TARGET_HOST 1)
  set(EPICS_TARGET_CLASS WIN32)
  set(EPICS_TARGET_CLASSES WIN32 default)

  if(CMAKE_SYSTEM_PROCESSOR MATCHES "64$")
  
    if(MINGW)
      set(EPICS_TARGET_ARCHS "windows-x64-mingw")
  
    elseif(MSVC)
      set(EPICS_TARGET_ARCHS "windows-x64")

    else(NOT CYGWIN)
      message(WARNING "Unknown Windows 64 variant: ${CMAKE_SYSTEM_NAME}")
      message(WARNING "Assuming default")
      set(EPICS_TARGET_ARCHS "windows-x64" "windows-x64-static")
    endif(MINGW)
  
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "86$")
  
    if(MINGW)
      set(EPICS_TARGET_ARCHS "win32-x86-mingw")
    
    elseif(CYGWIN)
      set(EPICS_TARGET_ARCHS "win32-x86-cygwin")
    
    elseif(MSVC)
      set(EPICS_TARGET_ARCHS "win32-x86")

    else()
      message(WARNING "Unknown Windows 32 variant: ${CMAKE_SYSTEM_NAME}")
      message(WARNING "Assuming default")
      set(EPICS_TARGET_ARCHS "win32-x86")
    endif()
  endif()

elseif(CMAKE_APPLE)
  set(EPICS_TARGET_HOST 1)
  set(EPICS_TARGET_CLASS Darwin)
  set(EPICS_TARGET_CLASSES Darwin default)
  
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "86")
    set(EPICS_TARGET_ARCHS "darwin-x86")

  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)$")
    set(EPICS_TARGET_ARCHS "darwin-ppc")

  else()
    message(WARNING "Unknown Apple variant: ${CMAKE_SYSTEM_NAME}")
  endif()

else(UNIX)
  message(FATAL_ERROR "Unable to determine EPICS OS class")
endif(UNIX)

if(NOT EPICS_TARGET_COMPILER)
  if(CMAKE_COMPILER_IS_GNUCC)
    set(EPICS_TARGET_COMPILER gcc)
  elseif(MSVC)
    set(EPICS_TARGET_COMPILER msvc)
  else()
    message(WARNING "Unable to guess target compiler")
  endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
  # prepend the -debug version of all target names
  list(REVERSE EPICS_TARGET_ARCHS)
  foreach(_arch IN LISTS EPICS_TARGET_ARCHS)
    list(APPEND EPICS_TARGET_ARCHS "${_arch}-debug")
  endforeach()
  unset(_arch)
  list(REVERSE EPICS_TARGET_ARCHS)
endif()

find_package_handle_standard_args(EPICSTargetArch
  REQUIRED_VARS
    EPICS_TARGET_ARCHS
    EPICS_TARGET_CLASS EPICS_TARGET_CLASSES
)
