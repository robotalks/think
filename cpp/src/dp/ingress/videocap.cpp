#include <opencv2/highgui/highgui.hpp>
#include <glog/logging.h>

#include "dp/graph_def.h"
#include "dp/ingress/videocap.h"

namespace dp::in {
    using namespace std;
    using namespace cv;

    video_capture::video_capture() {
    }

    video_capture::video_capture(const ::std::string& fn)
    : m_cap(fn) {
    }

    video_capture::video_capture(int index)
    : m_cap(index) {
    }

    video_capture::~video_capture() {
    }

    bool video_capture::open(const string& name) {
        return m_cap.open(name);
    }

    bool video_capture::open(int index) {
        return m_cap.open(index);
    }

    void video_capture::close() {
        m_cap.release();
    }

    void video_capture::set_frame_size(int w, int h) {
        m_cap.set(CAP_PROP_FRAME_WIDTH, w);
        m_cap.set(CAP_PROP_FRAME_HEIGHT, h);
    }

    bool video_capture::prepare_session(session& session, bool nowait) {
        Mat frame;
        m_cap >> frame;
        if (frame.empty()) return false;
        if (!m_show_name.empty()) {
            imshow(m_show_name.c_str(), frame);
            waitKey(1);
        }
        session.initializer = [this, frame] (graph *g) {
            g->var(input_var())->set<Mat>(frame);
        };
        return true;
    }

    struct factory : public dp::graph_def::ingress_factory {
        ingress* create_ingress(
            const string& name,
            const string& type,
            const dp::graph_def::params& arg) {
            int dev_index = 0;
            auto dev_str = arg.at("dev");
            if (!dev_str.empty()) {
                dev_index = atoi(dev_str.c_str());
            }
            return new video_capture(dev_index);
        }
    };

    void video_capture::register_factory() {
        static factory _factory;
        dp::graph_def::ingress_registry::get()->add_factory("dp.videocap", &_factory);
    }
}
