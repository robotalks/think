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

add_library(think
    src/dp/graph.cpp
    src/dp/graph_var.cpp
    src/dp/graph_builder.cpp
    src/dp/dispatch.cpp
    src/dp/ingress.cpp
    src/dp/imageid.cpp
    src/dp/util/pool.cpp
    src/dp/op/imageid.cpp
    src/dp/op/decodeimg.cpp
    src/dp/op/saveimage.cpp
    src/dp/op/sensitivity.cpp
    src/dp/ingress/udp.cpp
    src/dp/ingress/videocap.cpp
    src/mqtt/mqtt.cpp
    src/movidius/ncs.cpp
    src/movidius/fp16.cpp
    src/movidius/operators.cpp
    src/movidius/ssd_mobilenet.cpp
)

add_executable(objdetect src/objdetect/app.cpp)
target_link_libraries(objdetect think ${LIBS})
