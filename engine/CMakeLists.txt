file(GLOB SRC_FILES src/*.cpp include/*.h)
list(APPEND SRC_FILES src/query_rewriter.cpp)
add_executable(sqlopt ${SRC_FILES})
target_include_directories(sqlopt PRIVATE include)

# Find MySQL Connector/C++
find_path(MYSQL_INCLUDE_DIR mysql/mysql.h PATHS /opt/homebrew/include /opt/homebrew/Cellar/mysql/9.4.0_3/include /usr/local/include)
find_library(MYSQL_LIBRARY mysqlclient PATHS /opt/homebrew/lib /opt/homebrew/Cellar/mysql/9.4.0_3/lib /usr/local/lib)
if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
    target_include_directories(sqlopt PRIVATE ${MYSQL_INCLUDE_DIR})
    target_link_libraries(sqlopt ${MYSQL_LIBRARY})
    add_definitions(-DHAVE_MYSQL)
endif()

if (APPLE)
  target_compile_options(sqlopt PRIVATE -Wall -Wextra -Wpedantic -Werror)
else()
  target_compile_options(sqlopt PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
