#ifndef __DP_INGRESS_VIDEOCAP_H
#define __DP_INGRESS_VIDEOCAP_H

#include <opencv2/videoio/videoio.hpp>
#include "dp/ingress.h"

namespace dp::in {
    class video_capture : public ingress {
    public:
        video_capture();
        video_capture(const ::std::string&);
        video_capture(int);

        virtual ~video_capture();

        bool open(const ::std::string& fn);
        bool open(int);
        bool is_open() const { return m_cap.isOpened(); }

        void close();

        void set_frame_size(int w, int h);

        void show_to(const ::std::string& name) { m_show_name = name; }

    protected:
        virtual bool prepare_session(session&, bool);

    private:
        ::cv::VideoCapture m_cap;
        ::std::string m_show_name;
    };
}

#endif
