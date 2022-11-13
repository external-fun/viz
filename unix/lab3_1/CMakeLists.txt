project(lab3_1)

set(SRC
  main.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME} ${SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)