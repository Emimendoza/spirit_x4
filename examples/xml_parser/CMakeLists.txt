# Copyright 2025 Nana Sakisaka
#
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.30)

project(spirit_x4_xml_example LANGUAGES CXX)

# If building standalone (outside the Boost superproject), find the local tree.
if(NOT TARGET Boost::spirit_x4)
    add_subdirectory(../.. spirit_x4_build)
endif()

add_executable(xml_parser xml_parser.cpp)

set_target_properties(xml_parser PROPERTIES CXX_EXTENSIONS OFF)

target_compile_features(xml_parser PRIVATE cxx_std_23)

target_link_libraries(xml_parser PRIVATE Boost::spirit_x4)

# Convenience: run on `cmake --build . --target run_xml_parser`
add_custom_target(run_xml_parser
    COMMAND xml_parser
    DEPENDS xml_parser
    COMMENT "Running xml_parser example"
)
