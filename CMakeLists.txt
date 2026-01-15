cmake_minimum_required(VERSION 3.16)

project(SimpleCV
  VERSION 0.1.0
  DESCRIPTION "A tiny OpenCV-like wrapper powered by stb"
  LANGUAGES CXX)

option(SIMPLECV_BUILD_TESTS "Build SimpleCV tests" ON)

add_library(simplecv STATIC
  src/SimpleCV.cpp
)

add_library(SimpleCV::simplecv ALIAS simplecv)

target_include_directories(simplecv
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(simplecv PUBLIC cxx_std_17)

if(MSVC)
  target_compile_options(simplecv PRIVATE /W4)
else()
  target_compile_options(simplecv PRIVATE -Wall -Wextra -Wpedantic -Wno-missing-field-initializers)
endif()

include(GNUInstallDirs)

install(TARGETS simplecv
  EXPORT SimpleCVTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT SimpleCVTargets
  NAMESPACE SimpleCV::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SimpleCV)

if(SIMPLECV_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
