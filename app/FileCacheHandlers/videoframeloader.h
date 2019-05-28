#ifndef VIDEOFRAMELOADER_H
#define VIDEOFRAMELOADER_H
#include "updatable.h"
#include "skia/skiaincludes.h"
extern "C" {
    #include <libavutil/opt.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswresample/swresample.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}

class VideoCacheHandler;
struct VideoStreamsData;
class VideoFrameLoader : public HDDTask {
    friend class StdSelfRef;
protected:
    VideoFrameLoader(VideoCacheHandler * const cacheHandler,
                     const stdsptr<VideoStreamsData>& openedVideo,
                     const int& frameId) :
        mCacheHandler(cacheHandler), mOpenedVideo(openedVideo),
        mFrameId(frameId) {}
    VideoFrameLoader(VideoCacheHandler * const cacheHandler,
                     const stdsptr<VideoStreamsData>& openedVideo,
                     const int& frameId, AVFrame* const frame);

    void afterProcessing();
    void afterCanceled();

    void scheduleTaskNow();

    void setFrameToConvert(AVFrame * const frame,
                           AVCodecContext * const codecContext) {
        mFrameToConvert = frame;
        mSwsContext = sws_getContext(codecContext->width,
                                     codecContext->height,
                                     codecContext->pix_fmt,
                                     codecContext->width,
                                     codecContext->height,
                                     AV_PIX_FMT_BGRA, SWS_BICUBIC,
                                     nullptr, nullptr, nullptr);
    }
public:
    ~VideoFrameLoader() {
        for(auto& excess : mExcessFrames) {
            av_frame_unref(excess.second);
            av_frame_free(&excess.second);
        }
        cleanUp();
    }
    void processTask();
private:
    void cleanUp() {
        if(mFrameToConvert) {
            av_frame_unref(mFrameToConvert);
            av_frame_free(&mFrameToConvert);
            mFrameToConvert = nullptr;
        }
        if(mSwsContext) {
            sws_freeContext(mSwsContext);
            mSwsContext = nullptr;
        }
    }
    void setupSwsContext(AVCodecContext * const codecContext);
    void readFrame();
    void convertFrame();

    VideoCacheHandler * const mCacheHandler;
    const stdsptr<VideoStreamsData> mOpenedVideo;
    const int mFrameId;
    sk_sp<SkImage> mLoadedFrame;

    QList<std::pair<int, AVFrame*>> mExcessFrames;

    AVFrame * mFrameToConvert = nullptr;
    struct SwsContext * mSwsContext = nullptr;
};

#endif // VIDEOFRAMELOADER_H
