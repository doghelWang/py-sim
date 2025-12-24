
enable_testing()

add_executable(unit_tests 
    Test_AppModel.cpp
    Test_PhysicsRK4.cpp
)

target_link_libraries(unit_tests PRIVATE 
    amr_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
