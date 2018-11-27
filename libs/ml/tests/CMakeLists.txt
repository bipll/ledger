################################################################################
# F E T C H   M L   T E S T S
################################################################################
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(fetch-ml)

# CMake configuration
include(${FETCH_ROOT_CMAKE_DIR}/BuildTools.cmake)

# Compiler Configuration
setup_compiler()
add_fetch_gtest(ops_gtest fetch-ml ml/ops)
add_fetch_gtest(session_gtest fetch-ml ml/session)
add_fetch_gtest(variable_gtest fetch-ml ml/variable)
add_fetch_gtest(layers_gtest fetch-ml ml/layers)