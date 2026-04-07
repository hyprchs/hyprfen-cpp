cmake_minimum_required(VERSION 3.20)

project(hyprfen VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(hyprfen STATIC
    src/hyprfen.cpp
)
add_library(hyprfen::hyprfen ALIAS hyprfen)

target_compile_features(hyprfen PUBLIC cxx_std_20)
target_include_directories(hyprfen
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(hyprfen PROPERTIES
    EXPORT_NAME hyprfen
    POSITION_INDEPENDENT_CODE ON
)

if(BUILD_TESTING)
    add_executable(hyprfen_tests
        tests/test_hyprfen.cpp
    )
    target_link_libraries(hyprfen_tests PRIVATE hyprfen::hyprfen)
    add_test(NAME hyprfen_tests COMMAND hyprfen_tests)
endif()

install(
    TARGETS hyprfen
    EXPORT hyprfenTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/hyprfenConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/hyprfenConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/hyprfenConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/hyprfen"
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/hyprfenConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/hyprfenConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/hyprfen"
)
install(
    EXPORT hyprfenTargets
    FILE hyprfenTargets.cmake
    NAMESPACE hyprfen::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/hyprfen"
)
