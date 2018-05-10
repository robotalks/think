#ifndef __VP_CV_VIDEOCAP_H
#define __VP_CV_VIDEOCAP_H

#include <opencv2/videoio/videoio.hpp>
#include "vp/ingress.h"

namespace vp {
    class CvVideoCapture : public Ingress {
    public:
        CvVideoCapture();
        CvVideoCapture(const ::std::string&);
        CvVideoCapture(int);

        virtual ~CvVideoCapture();

        bool open(const ::std::string& fn);
        bool open(int);
        bool is_open() const { return m_cap.isOpened(); }

        void close();

        void setFrameSize(int w, int h);

        void showTo(const ::std::string& name) { m_show_name = name; }

    protected:
        virtual bool prepareSession(Session&, bool);

    private:
        ::cv::VideoCapture m_cap;
        ::std::string m_show_name;
    };
}

#endif
