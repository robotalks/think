#include <opencv2/highgui/highgui.hpp>
#include <glog/logging.h>

#include "vp/cv/videocap.h"

namespace vp {
    using namespace std;
    using namespace cv;

    CvVideoCapture::CvVideoCapture() {
    }

    CvVideoCapture::CvVideoCapture(const ::std::string& fn)
    : m_cap(fn) {
    }

    CvVideoCapture::CvVideoCapture(int index)
    : m_cap(index) {
    }

    CvVideoCapture::~CvVideoCapture() {
    }

    bool CvVideoCapture::open(const string& name) {
        return m_cap.open(name);
    }

    bool CvVideoCapture::open(int index) {
        return m_cap.open(index);
    }

    void CvVideoCapture::close() {
        m_cap.release();
    }

    void CvVideoCapture::setFrameSize(int w, int h) {
        m_cap.set(CAP_PROP_FRAME_WIDTH, w);
        m_cap.set(CAP_PROP_FRAME_HEIGHT, h);
    }

    bool CvVideoCapture::prepareSession(Session& session, bool nowait) {
        Mat frame;
        m_cap >> frame;
        if (frame.empty()) return false;
        if (!m_show_name.empty()) {
            imshow(m_show_name.c_str(), frame);
            waitKey(1);
        }
        session.initializer = [this, frame] (Graph *g) {
            g->var(inputVar())->set<Mat>(frame);
        };
        return true;
    }
}
