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
    
    message(STATUS "sys tests enabled")
endif()
