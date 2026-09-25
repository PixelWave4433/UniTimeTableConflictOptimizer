cmake_minimum_required(VERSION 4.3)

project(UniTimeTableConflictOptimizer C)

set(CMAKE_C_STANDARD 11)

add_executable(UniTimeTableConflictOptimizer scheduler.c)

# Tell C/CLion where mysql.h is
target_include_directories(UniTimeTableConflictOptimizer PRIVATE /opt/homebrew/opt/mysql/include)

# Link the MySQL client library
target_link_libraries(UniTimeTableConflictOptimizer PRIVATE /opt/homebrew/opt/mysql/lib/libmysqlclient.dylib)