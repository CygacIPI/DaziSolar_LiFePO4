cmake_minimum_required(VERSION 3.20)

project(AppLiFePO4
    VERSION 0.1.0
    DESCRIPTION "Monitor de solo lectura para baterias LiFePO4"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(raylib CONFIG REQUIRED)

add_library(lifepo4-core STATIC
    core/src/battery.c
    core/src/battery_calculations.c
    core/src/battery_alarms.c
    core/src/battery_coulomb_counter.c
    core/src/battery_simulator.c
)

target_include_directories(lifepo4-core PUBLIC core/include)

add_executable(lifepo4-desktop
    desktop/main.c
)

target_link_libraries(lifepo4-desktop PRIVATE lifepo4-core raylib)

include(CTest)

if(BUILD_TESTING)
    add_executable(core-tests
        core/tests/test_core.c
    )
    target_link_libraries(core-tests PRIVATE lifepo4-core)
    add_test(NAME core-tests COMMAND core-tests)
endif()
