# The gomp executable and its Gomp QML module (the UI). Defined here because qt_add_qml_module must
# run in the same directory as the target it extends. QML files are globbed so adding a component
# is a one-file change; singletons are flagged below.
qt_add_executable(gomp
    ${PROJECT_SOURCE_DIR}/src/app/main.cpp
    ${PROJECT_SOURCE_DIR}/src/app/LaunchOptions.h ${PROJECT_SOURCE_DIR}/src/app/LaunchOptions.cpp)
set_target_properties(gomp PROPERTIES OUTPUT_NAME gomp RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
target_link_libraries(gomp PRIVATE gomp_core gomp_coreplugin Qt6::QuickControls2 Qt6::Svg gomp_warnings)
target_compile_definitions(gomp PRIVATE GOMP_VERSION="${PROJECT_VERSION}")
target_include_directories(gomp PRIVATE ${PROJECT_SOURCE_DIR}/src/app)

file(GLOB_RECURSE GOMP_QML_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.qml)
file(GLOB GOMP_SINGLETONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} Theme/*.qml)
set_source_files_properties(${GOMP_SINGLETONS} PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(gomp
    URI Gomp
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${GOMP_QML_FILES}
    IMPORTS Gomp.Core
)

file(GLOB GOMP_FONTS ${PROJECT_SOURCE_DIR}/resources/fonts/*.ttf)
file(GLOB GOMP_ICONS ${PROJECT_SOURCE_DIR}/resources/icons/*.svg)
file(GLOB GOMP_BRAND ${PROJECT_SOURCE_DIR}/resources/brand/*)
# Provider logos for the sign-in grid; dropping a file named <provider-id>.svg in is enough.
file(GLOB GOMP_PROVIDER_LOGOS CONFIGURE_DEPENDS
     ${PROJECT_SOURCE_DIR}/resources/providers/*.svg ${PROJECT_SOURCE_DIR}/resources/providers/*.png)
qt_add_resources(gomp "gomp_assets"
    PREFIX /qt/qml/Gomp
    BASE ${PROJECT_SOURCE_DIR}/resources
    FILES ${GOMP_FONTS} ${GOMP_ICONS} ${GOMP_BRAND} ${GOMP_PROVIDER_LOGOS}
)

file(GLOB GOMP_SHADERS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/resources/shaders/*.frag ${PROJECT_SOURCE_DIR}/resources/shaders/*.vert)
if(GOMP_SHADERS)
    qt_add_shaders(gomp "gomp_shaders"
        PREFIX /qt/qml/Gomp
        BASE ${PROJECT_SOURCE_DIR}/resources
        FILES ${GOMP_SHADERS}
    )
endif()
