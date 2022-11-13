project(lab3)

set(SRC
  main.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)
find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

add_executable(lab3 ${SRC})
target_link_libraries(lab3 PRIVATE Threads::Threads ${CURSES_LIBRARIES})
set_property(TARGET lab3 PROPERTY CXX_STANDARD 17)

add_executable(lab3 ${SRC})