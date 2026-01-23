add_executable(simplecv_tests
  test_simplecv.cpp
)

target_link_libraries(simplecv_tests PRIVATE SimpleCV::simplecv)
target_compile_features(simplecv_tests PRIVATE cxx_std_17)

add_test(NAME simplecv_tests COMMAND simplecv_tests)

add_executable(test_imread_imwrite
  test_imread_imwrite.cpp
)

target_link_libraries(test_imread_imwrite PRIVATE SimpleCV::simplecv)
target_compile_features(test_imread_imwrite PRIVATE cxx_std_17)

add_executable(test_draw
  test_draw.cpp
)

target_link_libraries(test_draw PRIVATE SimpleCV::simplecv)
target_compile_features(test_draw PRIVATE cxx_std_17)
add_test(NAME test_draw COMMAND test_draw)
