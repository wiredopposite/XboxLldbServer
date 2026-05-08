# If linking to project using non nxdk toolchain

include(ExternalProject)

set(_XBOXDM_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(_XBOXDM_NAME XboxDmAdapter)
set(_XBOXDM_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/${_XBOXDM_NAME})

ExternalProject_Add(XboxDmAdapter_Host
    SOURCE_DIR ${_XBOXDM_ROOT}
    BINARY_DIR ${_XBOXDM_BIN_DIR}
    CMAKE_ARGS 
        -DCMAKE_TOOLCHAIN_FILE="${_XBOXDM_ROOT}/external/nxdk/cmake/toolchain.cmake"
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${_XBOXDM_BIN_DIR}/lib
        -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${_XBOXDM_BIN_DIR}/bin
        -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${_XBOXDM_BIN_DIR}/bin
        -DCMAKE_PROJECT_NAME=${_XBOXDM_NAME}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS 
        "${_XBOXDM_BIN_DIR}/bin/${_XBOXDM_NAME}.dll"
        "${_XBOXDM_BIN_DIR}/lib/${_XBOXDM_NAME}.lib"
)

set(_XBOXDM_LIB ${_XBOXDM_BIN_DIR}/lib/${_XBOXDM_NAME}.lib)
set(_XBOXDM_DLL ${_XBOXDM_BIN_DIR}/bin/${_XBOXDM_NAME}.dll)
set(_XBOXDM_DEF ${_XBOXDM_ROOT}/XboxDmAdapterKernel.def)

add_library(LibXboxDmAdapter SHARED IMPORTED GLOBAL)
set_target_properties(LibXboxDmAdapter PROPERTIES
    IMPORTED_IMPLIB ${_XBOXDM_LIB}
    IMPORTED_LOCATION ${_XBOXDM_DLL}
    INTERFACE_INCLUDE_DIRECTORIES ${_XBOXDM_ROOT}/include
)
add_dependencies(LibXboxDmAdapter XboxDmAdapter_Host)