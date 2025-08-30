project(mtrancore CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 添加编译器优化选项
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-O3 -march=native -mtune=native)
    add_compile_options(-flto)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  elseif(MSVC)
    add_compile_options(/O2 /GL)
  endif()
endif()

# 启用更多警告
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
  add_compile_options(-Wno-unused-parameter)
elseif(MSVC)
  add_compile_options(/W4)
endif()

add_library(mtrancore SHARED mtrancore.cpp)
target_include_directories(
  mtrancore
  PUBLIC $<TARGET_PROPERTY:bergamot-translator,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(mtrancore bergamot-translator Threads::Threads)

# 添加版本信息
set_target_properties(mtrancore PROPERTIES
    VERSION 1.0.0
    SOVERSION 1)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  # Some versions of GCC will cause uninitialized errors during optimization
  if(CMAKE_COMPILER_IS_GNUCXX)
    add_compile_options(-Wno-error=maybe-uninitialized)
    add_compile_options(-Wno-error=uninitialized)
    add_compile_options(-Wno-maybe-uninitialized)
  endif()

  set(VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/export.map")
  if(EXISTS "${VERSION_SCRIPT}")
    if(NOT APPLE)
      target_link_options(mtrancore PRIVATE -Wl,--version-script=${VERSION_SCRIPT})
    else()
      # On macOS, use -exported_symbols_list with macOS-specific format
      set(MACOS_SYMBOLS "${CMAKE_CURRENT_SOURCE_DIR}/export_macos.symbols")
      target_link_options(mtrancore PRIVATE -Wl,-exported_symbols_list,${MACOS_SYMBOLS})
    endif()
  endif()

  include(CheckIPOSupported)
  check_ipo_supported()
  set_property(TARGET mtrancore PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

  if(NOT APPLE)
    target_link_options(mtrancore PRIVATE -Wl,--strip-all)
  else()
    target_link_options(mtrancore PRIVATE -Wl,-S)
  endif()
endif()

# 示例程序
option(BUILD_MTRANCORE_EXAMPLES "Build example programs" ON)
if(BUILD_MTRANCORE_EXAMPLES)
    add_executable(mtrancore_example example.c)
    target_link_libraries(mtrancore_example mtrancore)
    set_target_properties(mtrancore_example PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples/")
endif()

# 安装规则
install(TARGETS mtrancore
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
install(FILES mtrancore.h DESTINATION include)
if(BUILD_MTRANCORE_EXAMPLES)
    install(TARGETS mtrancore_example DESTINATION bin)
endif()
