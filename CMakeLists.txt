cmake_minimum_required(VERSION 4.0)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/external/nxdk/cmake/toolchain.cmake")
endif()

set(XDM_NAME XboxDmAdapter)

if(NOT DEFINED CMAKE_PROJECT_NAME)
    set(CMAKE_PROJECT_NAME ${XDM_NAME})
    project(${XDM_NAME} C CXX)
endif()

set(CMAKE_SHARED_LIBRARY_SUFFIX ".dll")
set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options("-Wno-macro-redefined")

add_subdirectory($ENV{NXDK_DIR} nxdk)
add_subdirectory(external/JSObjects)
add_subdirectory(external/minIni)

add_library(${XDM_NAME} SHARED
    ${CMAKE_CURRENT_LIST_DIR}/src/XboxDmAdapter/XboxDmAdapter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/XboxDmAdapter/Memory.cpp

    ${CMAKE_CURRENT_LIST_DIR}/src/GDBRemote/Structures.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/GDBRemote/SlaveSessionImpl.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/GDBRemote/SessionBase.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/GDBRemote/Session.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/GDBRemote/ProtocolInterpreter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/GDBRemote/PlatformSessionImpl.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/GDBRemote/PacketProcessor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/GDBRemote/DummySessionDelegateImpl.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/GDBRemote/DebugSessionImpl.cpp

    ${CMAKE_CURRENT_LIST_DIR}/src/Utils/CRTCompat.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Utils/Log.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Utils/OptParse.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Utils/Stringify.cpp

    ${CMAKE_CURRENT_LIST_DIR}/src/Target/ProcessBase.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Target/ProcessBaseX86.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Target/ThreadBase.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Target/Process.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Target/Thread.cpp

    ${CMAKE_CURRENT_LIST_DIR}/src/Architecture/RegisterLayout.cpp

    ${CMAKE_CURRENT_LIST_DIR}/src/Host/Socket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Host/QueueChannel.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Host/ProcessSpawner.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Host/Platform.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Host/File.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Host/Channel.cpp

    ${CMAKE_CURRENT_LIST_DIR}/src/Core/SoftwareBreakpointManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Core/SessionThread.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Core/MessageQueue.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Core/HardwareBreakpointManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Core/ErrorCodes.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Core/CPUTypes.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/Core/BreakpointManager.cpp
)

target_include_directories(${XDM_NAME} 
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(${XDM_NAME}
    PRIVATE
        nxdk_core
        nxdk_net
        jsobjects
        minIni
)

target_compile_definitions(${XDM_NAME}
    PRIVATE
        _HAS_EXCEPTIONS=0
        DM_SHARED_LIBRARY=1
)

target_link_options(${XDM_NAME}
    PRIVATE
        "-ENTRY:DmEntryPoint"
        "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/XboxDmAdapterKernel.def"
)

include(ExternalProject)
set(REGSGEN2_DIR ${CMAKE_CURRENT_LIST_DIR}/external/RegsGen2)
set(REGSGEN2_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/RegsGen2)
set(REGSGEN2_EXECUTABLE ${REGSGEN2_BUILD_DIR}/RegsGen2${CMAKE_HOST_EXECUTABLE_SUFFIX})
ExternalProject_Add(RegsGen2_Host
    SOURCE_DIR ${REGSGEN2_DIR}
    BINARY_DIR ${REGSGEN2_BUILD_DIR}
    CMAKE_ARGS 
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${REGSGEN2_BUILD_DIR}
    INSTALL_COMMAND ""
)

set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(GEN_HEADER_OUTPUT ${GEN_DIR}/Architecture/X86/RegistersDescriptors.h)
set(GEN_CPP_OUTPUT ${GEN_DIR}/Architecture/X86/RegistersDescriptors.cpp)
set(GEN_JSON_INPUT ${CMAKE_CURRENT_LIST_DIR}/json/X86.json)
set(GEN_HEADER_INPUT ${CMAKE_CURRENT_LIST_DIR}/src/Architecture/RegisterLayout.h)
add_custom_command(
    OUTPUT ${GEN_HEADER_OUTPUT}
    DEPENDS RegsGen2_Host ${GEN_JSON_INPUT} ${GEN_HEADER_INPUT}
    COMMAND
        ${REGSGEN2_EXECUTABLE} 
        -a windows
        -I ${GEN_HEADER_INPUT}
        -h -o ${GEN_HEADER_OUTPUT}
        -f ${GEN_JSON_INPUT}
)
add_custom_command(
    OUTPUT ${GEN_CPP_OUTPUT}
    DEPENDS ${GEN_HEADER_OUTPUT}
    COMMAND
        ${REGSGEN2_EXECUTABLE} 
        -a windows
        -I ${GEN_HEADER_OUTPUT} 
        -c -o ${GEN_CPP_OUTPUT}
        -f ${GEN_JSON_INPUT}
)
add_custom_target(GenerateX86RegisterDescriptors ALL DEPENDS ${GEN_CPP_OUTPUT})
add_dependencies(${XDM_NAME} GenerateX86RegisterDescriptors)
target_include_directories(${XDM_NAME} PRIVATE ${GEN_DIR})
target_sources(${XDM_NAME} PRIVATE ${GEN_CPP_OUTPUT})
