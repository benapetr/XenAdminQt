file(GLOB_RECURSE XENADMIN_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.ui" "*.qrc")

qt_add_executable(xenadmin-qt ${XENADMIN_SOURCES})

target_include_directories(xenadmin-qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../xenlib
)

target_link_libraries(xenadmin-qt
    PRIVATE
        xenlib
        Qt6::Widgets
        Qt6::Network
        Qt6::Xml
        Qt6::Charts
)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FREERDP QUIET freerdp2)
        if(FREERDP_FOUND)
            message(STATUS "FreeRDP found - enabling RDP support")
            target_compile_definitions(xenadmin-qt PRIVATE HAVE_FREERDP)
            target_link_libraries(xenadmin-qt PRIVATE ${FREERDP_LIBRARIES})
            target_include_directories(xenadmin-qt PRIVATE ${FREERDP_INCLUDE_DIRS})
        else()
            message(STATUS "FreeRDP not found - RDP support disabled (VNC-only)")
        endif()
    endif()

    if(NOT XENADMIN_NO_CRYPTO)
        find_package(OpenSSL REQUIRED)
        target_link_libraries(xenadmin-qt PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
endif()

install(TARGETS xenadmin-qt)
