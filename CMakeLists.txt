cmake_minimum_required(VERSION 3.20)

project(AppLiFePO4
    VERSION 0.1.0
    DESCRIPTION "Monitor de solo lectura para baterias LiFePO4"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(BUILD_DESKTOP "Build the raylib desktop application" ON)
option(APP_ENABLE_STRICT_WARNINGS "Treat strict compiler warnings as errors" ON)

add_library(lifepo4-core STATIC
    core/src/battery.c
    core/src/battery_calculations.c
    core/src/battery_alarms.c
    core/src/battery_coulomb_counter.c
    core/src/battery_csv.c
    core/src/battery_simulator.c
    core/src/battery_transport.c
)

target_include_directories(lifepo4-core PUBLIC core/include)

function(apply_project_warnings target)
    if(APP_ENABLE_STRICT_WARNINGS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4)
        elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target} PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
                -Wstrict-prototypes
                -Wformat=2
                -Werror
            )
        endif()
    endif()
endfunction()

apply_project_warnings(lifepo4-core)

if(BUILD_DESKTOP)
    find_package(raylib CONFIG REQUIRED)

    add_executable(lifepo4-desktop
        desktop/main.c
    )

    target_link_libraries(lifepo4-desktop PRIVATE lifepo4-core raylib)
    apply_project_warnings(lifepo4-desktop)

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/assets")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/dazi-solar-logo.png"
        "${CMAKE_CURRENT_BINARY_DIR}/assets/dazi-solar-logo.png"
        COPYONLY
    )
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(core-tests
        core/tests/test_core.c
    )
    target_link_libraries(core-tests PRIVATE lifepo4-core)
    apply_project_warnings(core-tests)
    add_test(NAME core-tests COMMAND core-tests)
endif()
