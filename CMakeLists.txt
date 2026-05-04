cmake_minimum_required (VERSION 2.6)
project (Quiet-LWIP)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 同级仓库（liquid-dsp / quiet）+ build-win64/src 下的 jansson、portaudio 源码一次性联编。
# quiet/CMakeLists.txt 与本文件下方的 quiet/portaudio find_library 都已用 if(NOT TARGET ...) 守卫，
# 子项目创建的 target 会优先生效。
if(NOT TARGET liquid)
    set(QUIET_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "")
    set(QUIET_SUBDEPS_SRC  "${QUIET_PROJECT_ROOT}/build-win64/src" CACHE PATH "")

    set(BUILD_EXAMPLES        OFF CACHE BOOL "" FORCE)
    set(BUILD_AUTOTESTS       OFF CACHE BOOL "" FORCE)
    set(BUILD_BENCHMARKS      OFF CACHE BOOL "" FORCE)
    set(BUILD_SANDBOX         OFF CACHE BOOL "" FORCE)
    set(BUILD_DOC             OFF CACHE BOOL "" FORCE)
    set(ENABLE_SIMD           OFF CACHE BOOL "" FORCE)
    set(FIND_SIMD             OFF CACHE BOOL "" FORCE)
    set(JANSSON_BUILD_DOCS    OFF CACHE BOOL "" FORCE)
    set(JANSSON_EXAMPLES      OFF CACHE BOOL "" FORCE)
    set(JANSSON_WITHOUT_TESTS ON  CACHE BOOL "" FORCE)
    if(WIN32)
        set(PA_USE_WASAPI ON  CACHE BOOL "" FORCE)
        set(PA_USE_WMME   ON  CACHE BOOL "" FORCE)
        set(PA_USE_DS     OFF CACHE BOOL "" FORCE)
    endif()

    add_subdirectory(${QUIET_PROJECT_ROOT}/liquid-dsp     ${CMAKE_BINARY_DIR}/_deps/liquid-dsp)
    add_subdirectory(${QUIET_SUBDEPS_SRC}/jansson-2.14    ${CMAKE_BINARY_DIR}/_deps/jansson)
    # jansson 2.14 用旧式 include_directories，不会通过 target_link_libraries 传播头文件
    target_include_directories(jansson INTERFACE
        $<BUILD_INTERFACE:${QUIET_SUBDEPS_SRC}/jansson-2.14/src>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/_deps/jansson/include>)
    add_subdirectory(${QUIET_SUBDEPS_SRC}/portaudio       ${CMAKE_BINARY_DIR}/_deps/portaudio)
    add_subdirectory(${QUIET_PROJECT_ROOT}/quiet          ${CMAKE_BINARY_DIR}/_deps/quiet)
    # quiet 同样用旧式 include_directories
    target_include_directories(quiet INTERFACE
        $<BUILD_INTERFACE:${QUIET_PROJECT_ROOT}/quiet/include>)
endif()

include(CheckLibraryExists)
include(CheckIncludeFiles)
include(FindPackageHandleStandardArgs)
include(CheckCCompilerFlag)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DQUIET_DEBUG)
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3 -O0 -fsanitize=address")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3 -O0")
    check_c_compiler_flag(-no-pie HAVE_NO_PIE)
    if(HAVE_NO_PIE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie")
    else()
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-no_pie")
    endif()
else()
    if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
      # set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-Ofast -fsanitize=address -g3")
      # set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} "-Ofast -flto")
    else()
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g")
    endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wpedantic -Wall")
if (NOT APPLE AND NOT WIN32)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_XOPEN_SOURCE=700")
endif()
set(SRCFILES src/driver.c src/util.c)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/lwip)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/lwip/ipv4)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
add_subdirectory(src/lwip)

set(CMAKE_THREAD_PREFER_PTHREAD ON)
find_package(Threads)
set(CORE_DEPENDENCIES quiet ${CMAKE_THREAD_LIBS_INIT})

if(TARGET portaudio)
    set(PORTAUDIO portaudio)
else()
    find_library(PORTAUDIO portaudio)
endif()
if (PORTAUDIO)
  set(SRCFILES ${SRCFILES} src/driver_portaudio.c)
  add_definitions(-DQUIET_PORTAUDIO=1)
  set(CORE_DEPENDENCIES ${CORE_DEPENDENCIES} ${PORTAUDIO})
  add_custom_target(quiet-lwip-portaudio-h ALL COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/include/quiet-lwip-portaudio.h ${CMAKE_BINARY_DIR}/include/quiet-lwip-portaudio.h)

else()
    unset(PORTAUDIO CACHE)
    message(WARNING "

quiet-lwip could not find portaudio. this will prevent quiet-lwip
from building its sound streaming programs. you can get portaudio from
    http://www.portaudio.com/download.html

")
endif()

if(NOT TARGET quiet)
    find_path(quiet_include NAMES quiet.h)
    find_library(quiet_library NAMES quiet)
    find_package_handle_standard_args(Quiet DEFAULT_MSG quiet_library quiet_include)
    mark_as_advanced(quiet_include quiet_library)
    if(QUIET_FOUND)
        add_library(quiet UNKNOWN IMPORTED)
        set_target_properties(quiet PROPERTIES IMPORTED_LOCATION "${quiet_library}" INTERFACE_INCLUDE_DIRECTORIES "${quiet_include}")
    else()
        message(FATAL_ERROR "
quiet-lwip requires libquiet but cannot find it")
    endif()
endif()

add_library(quiet_lwip STATIC ${SRCFILES} $<TARGET_OBJECTS:lwip_sys> $<TARGET_OBJECTS:lwip_core> $<TARGET_OBJECTS:lwip_api> $<TARGET_OBJECTS:lwip_netif> $<TARGET_OBJECTS:lwip_ipv4>)
target_link_libraries(quiet_lwip ${CORE_DEPENDENCIES})
add_library(quiet_lwip_shared SHARED ${SRCFILES} $<TARGET_OBJECTS:lwip_sys> $<TARGET_OBJECTS:lwip_core> $<TARGET_OBJECTS:lwip_api> $<TARGET_OBJECTS:lwip_netif> $<TARGET_OBJECTS:lwip_ipv4>)
target_link_libraries(quiet_lwip_shared ${CORE_DEPENDENCIES})
set_target_properties(quiet_lwip_shared PROPERTIES OUTPUT_NAME "quiet_lwip")
if(NOT TARGET lib)
    add_custom_target(lib DEPENDS quiet_lwip quiet_lwip_shared)
endif()

add_custom_target(lwip-h ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include/lwip/ ${CMAKE_BINARY_DIR}/include/lwip)
add_custom_target(quiet-lwip-h ALL COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/include/quiet-lwip.h ${CMAKE_BINARY_DIR}/include/quiet-lwip.h)
add_custom_target(lwip-socket-h ALL COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/include/quiet-lwip/lwip-socket.h ${CMAKE_BINARY_DIR}/include/lwip-socket.h)
add_custom_target(lwip-netdb-h ALL COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/include/quiet-lwip/lwip-netdb.h ${CMAKE_BINARY_DIR}/include/lwip-netdb.h)

install(DIRECTORY ${CMAKE_BINARY_DIR}/include/ DESTINATION include/quiet-lwip
        FILES_MATCHING PATTERN "*.h")
install(TARGETS quiet_lwip
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(TARGETS quiet_lwip_shared
        RENAME quiet_lwip
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

option(QUIET_LWIP_BUILD_EXAMPLES "Build quiet-lwip examples (need portaudio)" ON)
if(QUIET_LWIP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(TARGETS quiet_lwip ARCHIVE DESTINATION lib)
install(DIRECTORY include/lwip DESTINATION include)
install(FILES include/quiet-lwip.h DESTINATION include)
if(PORTAUDIO)
    install(FILES include/quiet-lwip-portaudio.h DESTINATION include)
endif()
