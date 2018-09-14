#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string>
#include <stdexcept>
#include <opencv2/highgui.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>

DEFINE_string(host, "127.0.0.1", "Vision Pipeline host address");
DEFINE_int32(port, 2053, "Vision Pipeline streaming port");

using namespace std;
using namespace cv;

static ::std::string error_msg(const string& prefix) {
    char buf[32];
    sprintf(buf, ": %d", errno);
    return prefix + buf;
}

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    hostent* ent = gethostbyname(FLAGS_host.c_str());
    if (ent == nullptr || ent->h_addr_list[0] == nullptr) {
        throw runtime_error("unable to resolve " + FLAGS_host);
    }

    int s = socket(PF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        throw runtime_error(error_msg("socket"));
    }
    sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = PF_INET;
    sa.sin_port = htons((u_short)FLAGS_port);
    sa.sin_addr = *(in_addr*)ent->h_addr_list[0];
    if (connect(s, (sockaddr*)&sa, sizeof(sa)) < 0) {
        throw runtime_error(error_msg("connect"));
    }

    uint16_t size;
    unsigned char buf[0x10000];
    unsigned char *p = (unsigned char*)&size;
    uint16_t recv_cnt = 0;
    bool recv_data = false;

    while (true) {
        if (recv_data) {
            size_t left = size - recv_cnt;
            int r = recv(s, buf+recv_cnt, left, 0);
            if (r < 0) {
                throw runtime_error(error_msg("recv"));
            }
            recv_cnt += (uint16_t)r;
            if (recv_cnt >= size) {
                recv_data = false;
                recv_cnt = 0;
                Mat raw(1, size, CV_8UC1, buf);
                Mat img = imdecode(raw, CV_LOAD_IMAGE_COLOR);
                if (!img.empty()) {
                    imshow("view", img);
                    waitKey(1);
                }
            }
        } else {
            size_t left = 2 - recv_cnt;
            int r = recv(s, p+recv_cnt, left, 0);
            if (r < 0) {
                throw runtime_error(error_msg("recv"));
            }
            recv_cnt += (uint16_t)r;
            if (recv_cnt >= 2) {
                recv_data = true;
                recv_cnt = 0;
            }
        }
    }
}