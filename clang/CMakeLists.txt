cmake_minimum_required(VERSION 3.10)
project(clang-extra)

add_executable(llvme-ast-print LLVM_AST_Parser.cpp)
target_include_directories(llvme-ast-print PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(llvme-ast-print PRIVATE ${Boost_LIBRARIES})