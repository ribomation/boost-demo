cmake_minimum_required(VERSION 3.28)
project(boost_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED yes)
set(CMAKE_CXX_EXTENSIONS no)
add_compile_options(-Wall -Wextra -Wpedantic)

# Point CMake at the Boost install with -DBoost_ROOT=$HOME/Libs/Boost/latest
find_package(Boost 1.92 CONFIG REQUIRED COMPONENTS program_options iostreams)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Everything except main(), shared by the application and the unit tests
add_library(boost-demo-core STATIC src/options.cxx src/config.cxx src/csv.cxx src/stats.cxx src/report.cxx src/http.cxx)
target_include_directories(boost-demo-core PUBLIC src)
target_link_libraries(boost-demo-core PUBLIC Boost::program_options Boost::headers OpenSSL::SSL OpenSSL::Crypto Threads::Threads)

add_executable(boost-demo src/app.cxx)
target_link_libraries(boost-demo PRIVATE boost-demo-core Boost::iostreams)

# Unit tests, with Catch2 from the ~/Libs tree
enable_testing()
list(APPEND CMAKE_MODULE_PATH "$ENV{HOME}/Libs/CMake-modules")
find_package(Catch2 REQUIRED)

add_executable(unit-tests
        test/options.test.cxx test/config.test.cxx test/tar.test.cxx test/csv.test.cxx test/stats.test.cxx test/report.test.cxx test/http.test.cxx)
target_link_libraries(unit-tests PRIVATE boost-demo-core Boost::iostreams Catch2::Catch2WithMain)
add_test(NAME unit-tests COMMAND unit-tests)
