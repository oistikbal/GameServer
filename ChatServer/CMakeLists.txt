find_package(OpenSSL REQUIRED)

add_executable(ChatServer)

target_sources(ChatServer PRIVATE 
    src/main.cpp
    src/chatserver.cpp

    inc/chatserver.h
)

set_target_properties(ChatServer PROPERTIES CXX_STANDARD 20)

target_include_directories(ChatServer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

target_link_libraries(ChatServer OpenSSL::SSL)

if(WIN32)
    target_link_libraries(ChatServer wsock32.lib ws2_32.lib)
    set_property(TARGET ChatServer PROPERTY CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>" )
endif()