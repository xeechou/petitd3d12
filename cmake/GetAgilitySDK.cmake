function(GetAgilitySDK)
  if(NOT MSVC)
    message(FATAL_ERROR "AgilitySDK is only supported for Visual Studio Tool Chains")
    if (MSVC_VERSION LESS 1910)
      message(FATAL_ERROR "Requires At Least Visual Studio 2017")
    endif()
  endif()

  set(options)
  set(oneValueArgs VERSION)
  set(multiValueArgs)

  cmake_parse_arguments(AG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if (NOT DEFINED AG_VERSION)
    set(AG_VERSION "1.608.3")
  endif()

  ## Setting directories
  set(NuGetPackagesRoot "${CMAKE_BINARY_DIR}/packages")
  set(DXAgilitySDK_VERSION_STRING ${AG_VERSION})
  set(DXAgilitySDK_INSTALL_DIR "${NuGetPackagesRoot}/DXAgilitySDK.${DXAgilitySDK_VERSION_STRING}")
  set(AgilitySDK_DIR "${AgilitySDK_INSTALL_DIR}" CACHE STRING
    "DX12 Agility SDK Directory")
  set(DXAgilitySDK_DIR "${DXAgilitySDK_INSTALL_DIR}/build/native" )
  set(DXAgilitySDK_INCLUDE_DIR "${DXAgilitySDK_DIR}/build/native/include")

  ## Downloading the SDK

  if (NOT EXISTS "${DXAgilitySDK_INCLUDE_DIR}")
    set( DXAgilitySDK_DOWNLOAD_URL "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/${DXAgilitySDK_VERSION_STRING}" )
    message( STATUS "Downloading DXAgilitySDK from ${DXAgilitySDK_DOWNLOAD_URL} ..." )
    file( MAKE_DIRECTORY "${DXAgilitySDK_INSTALL_DIR}" )
    file( DOWNLOAD "${DXAgilitySDK_DOWNLOAD_URL}" "${DXAgilitySDK_INSTALL_DIR}.zip" )
    message( STATUS "Extracting ${DXAgilitySDK_INSTALL_DIR}.zip to ${DXAgilitySDK_INSTALL_DIR} ..." )
    execute_process(
      COMMAND ${CMAKE_COMMAND} "-E" "tar" "xvz" "${DXAgilitySDK_INSTALL_DIR}.zip"
      WORKING_DIRECTORY "${DXAgilitySDK_INSTALL_DIR}"
      OUTPUT_QUIET
      ERROR_VARIABLE DXAgilitySDK_ERROR
      )

    message( STATUS "Cleaning ${DXAgilitySDK_INSTALL_DIR}.zip ..." )
    # cleanup temp folder
    file( REMOVE_RECURSE "${DXAgilitySDK_INSTALL_DIR}.zip" )

    if ( "${DXAgilitySDK_ERROR}" STREQUAL "" )
      message( STATUS "Successfully installed DXAgilitySDK to ${DXAgilitySDK_INSTALL_DIR}" )
      message( STATUS "DXAgilitySDK_INCLUDE_DIR = ${DXAgilitySDK_INCLUDE_DIR}")
    else()
      message( WARNING "Unable to install DXAgilitySDK install failed with: ${DX12AgilitySDK_ERROR}" )
    endif()
  endif()

endfunction()
