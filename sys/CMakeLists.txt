cmake_minimum_required(VERSION 3.16)

project(sys LANGUAGES C)

add_library(sys STATIC
    src/sys.c
)

target_include_directories(sys
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

install(
    TARGETS sys
    EXPORT ComponentsTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(
    DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Build tests if enabled
option(BUILD_SYS_TESTS "Build sys module tests" ON)

if(BUILD_SYS_TESTS)
    enable_testing()
    
    add_executable(test_sys
        test/test_sys.c
    )
    
    target_link_libraries(test_sys
        PRIVATE sys
		m
    )
    
    target_include_directories(test_sys
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    add_test(
        NAME test_sys
        COMMAND test_sys
    )
    
    # Set test properties
    set_tests_properties(test_sys PROPERTIES
        TIMEOUT 30
        LABELS "system;unit"
    )

    add_executable(test_mode_errors
        test/test_mode_errors.c
    )

    target_link_libraries(test_mode_errors
        PRIVATE sys
        m
    )

    target_include_directories(test_mode_errors
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    # The test replaces libc calls to inject failures; no hardware is touched.
    target_link_options(test_mode_errors
        PRIVATE
            "-Wl,--wrap=open"
            "-Wl,--wrap=write"
            "-Wl,--wrap=close"
            "-Wl,--wrap=ioctl"
            "-Wl,--wrap=system"
    )

    set(sys_mode_error_faults
        success optional-missing invalid hooks retry
        governor-open governor-write governor-short
        backlight-missing backlight-no-device backlight-denied
        backlight-write backlight-short
        watchdog-missing watchdog-no-device watchdog-denied
        watchdog-write watchdog-short watchdog-ioctl
        suspend-open suspend-write suspend-short
        network-active network-sensor-off network-suspend
        network-hibernate network-poweroff network-reboot network-spawn
    )

    foreach(fault IN LISTS sys_mode_error_faults)
        add_test(
            NAME test_mode_errors_${fault}
            COMMAND test_mode_errors ${fault}
        )
        set_tests_properties(test_mode_errors_${fault} PROPERTIES
            TIMEOUT 30
            LABELS "system;unit"
        )
    endforeach()
    
    message(STATUS "sys tests enabled")
endif()
