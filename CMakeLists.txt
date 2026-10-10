cmake_minimum_required(VERSION 3.24)
project(gomp VERSION 0.1.0 DESCRIPTION "A native flight deck for the omp coding agent" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GOMP_WERROR "Treat compiler warnings as errors" ON)
option(GOMP_BUILD_TESTS "Build unit and smoke tests" ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 ShaderTools Svg DBus Concurrent Test)
qt_standard_project_setup(REQUIRES 6.6)
qt_policy(SET QTP0004 NEW)

# Project-wide warning policy. Qt headers come in as SYSTEM includes through the imported targets,
# so -Werror applies only to our code.
add_library(gomp_warnings INTERFACE)
target_compile_options(gomp_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wcast-align -Woverloaded-virtual
    -Wimplicit-fallthrough -Wno-gnu-zero-variadic-macro-arguments
    $<$<BOOL:${GOMP_WERROR}>:-Werror>)
target_compile_definitions(gomp_warnings INTERFACE
    QT_NO_CAST_FROM_ASCII QT_NO_URL_CAST_FROM_STRING QT_NO_NARROWING_CONVERSIONS_IN_CONNECT)

add_subdirectory(src)
add_subdirectory(qml)

if(GOMP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(TARGETS gomp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/dev.gomp.Gomp.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/dev.gomp.Gomp.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES resources/icons/gomp-mark.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME dev.gomp.Gomp.svg)
