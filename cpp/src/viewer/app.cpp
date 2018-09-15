#include <memory>
#include <opencv2/highgui.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include "dp/fx/recv.h"

DEFINE_string(host, "127.0.0.1", "Vision Pipeline host address");
DEFINE_int32(port, 2053, "Vision Pipeline streaming port");
DEFINE_bool(tcp, false, "Use TCP stream instead of UDP");

using namespace std;
using namespace cv;
using namespace dp::fx;

static void image_proc(void* ptr, size_t size) {
    Mat raw(1, (int)size, CV_8UC1, ptr);
    Mat img = imdecode(raw, CV_LOAD_IMAGE_COLOR);
    if (!img.empty()) {
        imshow("view", img);
        waitKey(1);
    }
}

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    unique_ptr<receiver> rcv;
    if (FLAGS_tcp) {
        rcv.reset(new tcp_recv(FLAGS_host, FLAGS_port));
    } else {
        rcv.reset(new udp_recv(FLAGS_host, FLAGS_port));
    }
    rcv->run(image_proc);
    return 0;
}
