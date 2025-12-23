//
// Created by zeng on 2025/10/22.
//

#ifndef SKIAOPENGLES_SKIAPIPELINE_H
#define SKIAOPENGLES_SKIAPIPELINE_H


#include "include/codec/SkCodec.h"
#include "include/gpu/ganesh/GrDirectContext.h"

class SkCanvas;
struct ANativeWindow;
class Frame;
class SkiaPipeline {
public:
    virtual ~SkiaPipeline() = default;

    virtual bool setSurface(ANativeWindow* surface) = 0;

    virtual void draw() = 0;

    // virtual Frame getFrame() = 0;

    // void setAssetManager(AAssetManager* assetMgr);

protected:
    void setGrContext(sk_sp<GrDirectContext> grContext) { fGrContext = std::move(grContext); }
    virtual void renderFrame(SkCanvas* canvas) = 0;
private:
    void preloadTexture();
    void initInputTexture(SkCanvas* canvas);

protected:
    // AAssetManager* fAssetMgr;
    sk_sp<SkData> fImgRawData;
    std::unique_ptr<SkCodec> fImgCodec;
//    sk_sp<SkImage> fImgData[12];
//    sk_sp<SkImage> fTestFlingerImageData;
//
//    bool fHasInitInputTexture = false;
//    sk_sp<SkImage> inputImage;

    sk_sp<GrRecordingContext> fGrContext;

    //Framework LM Blur
    // MiLMBlur fMiLMBlur;
};



#endif //SKIAOPENGLES_SKIAPIPELINE_H
