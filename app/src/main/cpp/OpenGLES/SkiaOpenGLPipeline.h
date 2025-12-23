//
// Created by zeng on 2025/10/22.
//

#ifndef SKIAOPENGLES_SKIAOPENGLPIPELINE_H
#define SKIAOPENGLES_SKIAOPENGLPIPELINE_H

#include <EGL/egl.h>
#include <memory>

#include "EglManager.h"
#include "../SkiaPipeline.h"

class SkiaOpenGLPipeline : public SkiaPipeline  {
public:
    SkiaOpenGLPipeline();
    ~SkiaOpenGLPipeline() = default;
    SkColorType getSurfaceColorType() const { return mSurfaceColorType; }
    sk_sp<SkColorSpace> getSurfaceColorSpace()  { return mSurfaceColorSpace; }


    void draw() override;

    bool setSurface(ANativeWindow* surface) override;

    // Frame getFrame() override;

protected:
    void renderFrame(SkCanvas *canvas) override;

private:
    void requireGlContext();

    EGLSurface mEglSurface = EGL_NO_SURFACE;

    ColorMode mColorMode = ColorMode::Default;
    SkColorType mSurfaceColorType;
    sk_sp<SkColorSpace> mSurfaceColorSpace;
    std::unique_ptr<EglManager> mEglManager;

};


#endif //SKIAOPENGLES_SKIAOPENGLPIPELINE_H
