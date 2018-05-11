cmake_minimum_required(VERSION 3.2)

set(CMAKE_CXX_STANDARD 11)

find_package(OpenCV REQUIRED)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    "inc"
)

set(LIBS
    glog
    gflags
    ${OpenCV_LIBS}
    mosquitto
    ssl
    z
    mvnc
    pthread
)

add_library(vp
    src/vp/graph.cpp
    src/vp/graph_var.cpp
    src/vp/graph_builder.cpp
    src/vp/pool.cpp
    src/vp/dispatch.cpp
    src/vp/ingress.cpp
    src/vp/ingress/udp.cpp
    src/vp/op/image.cpp
    src/vp/op/detection.cpp
    src/vp/op/sensitivity.cpp
    src/vp/cv/videocap.cpp
    src/vp/cv/operators.cpp
    src/vp/mqtt/mqtt.cpp
    src/movidius/ncs.cpp
    src/movidius/fp16.cpp
    src/movidius/ssd_mobilenet.cpp
)

add_executable(vpd src/vpd/app.cpp)
target_link_libraries(vpd vp ${LIBS})
