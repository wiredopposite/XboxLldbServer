set(LLDB_SERVER "lldbServer")
set(GDB_REMOTE "lldbGdbRemote")
set(GDB_REMOTE_DIR ${CMAKE_CURRENT_LIST_DIR}/lldb/source/Plugins/Process/gdb-remote)
set(LLDB_SERVER_DIR )

add_library(${GDB_REMOTE} STATIC
    ${GDB_REMOTE_DIR}/GDBRemoteClientBase.cpp
    ${GDB_REMOTE_DIR}/GDBRemoteCommunication.cpp
    ${GDB_REMOTE_DIR}/GDBRemoteCommunicationClient.cpp
    ${GDB_REMOTE_DIR}/GDBRemoteCommunicationHistory.cpp
    ${GDB_REMOTE_DIR}/GDBRemoteCommunicationReplayServer.cpp
    ${GDB_REMOTE_DIR}/GDBRemoteCommunicationServer.cpp
    ${GDB_REMOTE_DIR}/GDBRemoteCommunicationServerCommon.cpp
    ${GDB_REMOTE_DIR}/GDBRemoteCommunicationServerLLGS.cpp
    ${GDB_REMOTE_DIR}/GDBRemoteCommunicationServerPlatform.cpp
    ${GDB_REMOTE_DIR}/GDBRemoteRegisterContext.cpp
    ${GDB_REMOTE_DIR}/ProcessGDBRemote.cpp
    ${GDB_REMOTE_DIR}/ProcessGDBRemoteLog.cpp
    ${GDB_REMOTE_DIR}/ThreadGDBRemote.cpp
)

target_include_directories(${GDB_REMOTE} PUBLIC
    ${GDB_REMOTE_DIR}
)

target_link_libraries(${GDB_REMOTE} PUBLIC
    nxdk_core nxdk_net nxdk_zlib
)

add_library(${LLDB_SERVER} STATIC

)