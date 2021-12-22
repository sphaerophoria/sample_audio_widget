project(AudioChartTest CXX)
cmake_minimum_required(VERSION 3.22)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Threads REQUIRED)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(main main.cpp)
set_target_properties(main PROPERTIES AUTOMOC true)
target_compile_features(main PUBLIC cxx_std_17)
target_include_directories(main PRIVATE ${OPENAL_INCLUDE_DIR})
target_link_libraries(main Qt5::Widgets ${OPENAL_LIBRARY} Threads::Threads)
