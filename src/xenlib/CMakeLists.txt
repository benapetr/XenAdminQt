file(GLOB_RECURSE XENLIB_SOURCES CONFIGURE_DEPENDS "*.cpp")

qt_add_library(xenlib STATIC ${XENLIB_SOURCES})

target_include_directories(xenlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(xenlib PRIVATE XENLIB_LIBRARY)

target_link_libraries(xenlib
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Xml
)

option(XENADMIN_NO_CRYPTO "Disable crypto/OpenSSL support" OFF)
if(NOT XENADMIN_NO_CRYPTO)
    if(UNIX AND NOT APPLE)
        find_package(OpenSSL REQUIRED)
        target_link_libraries(xenlib PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    elseif(APPLE)
        # CommonCrypto is part of the macOS system frameworks - no extra linking needed
    elseif(WIN32)
        target_link_libraries(xenlib PUBLIC bcrypt)
    endif()
else()
    target_compile_definitions(xenlib PUBLIC XENADMIN_NO_CRYPTO)
endif()
