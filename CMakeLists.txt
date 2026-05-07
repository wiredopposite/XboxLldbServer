cmake_minimum_required(VERSION 4.0)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/external/nxdk/cmake/toolchain.cmake")

set(CMAKE_PROJECT_NAME XboxLldbServer)
project(${CMAKE_PROJECT_NAME} C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory($ENV{NXDK_DIR} nxdk)
add_subdirectory(external/JSObjects)

add_executable(${CMAKE_PROJECT_NAME}
    ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp

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

target_include_directories(${CMAKE_PROJECT_NAME} 
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    PRIVATE
        nxdk_core
        nxdk_net
        jsobjects
)

target_compile_definitions(${CMAKE_PROJECT_NAME}
    PRIVATE
        # PLATFORM_NXDK=1
        # OS_WIN32=1
)

add_compile_options("-Wno-macro-redefined")

# target_compile_options(${CMAKE_PROJECT_NAME} 
#     PUBLIC 
#         "-Wno-macro-redefined"
# )

target_link_options(${CMAKE_PROJECT_NAME}
    PRIVATE
        -ENTRY:DmEntryPoint
)