find_package(Threads REQUIRED)

add_executable(basic_park_test basic_park_test.cpp)
target_link_libraries(basic_park_test threadpark)
target_link_libraries(basic_park_test PRIVATE Threads::Threads)

add_test(NAME basic_park_test COMMAND basic_park_test)