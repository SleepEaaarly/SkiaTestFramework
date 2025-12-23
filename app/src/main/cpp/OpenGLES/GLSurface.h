//
// Created by zeng on 2025/10/22.
//

#ifndef SKIAOPENGLES_GLSURFACE_H
#define SKIAOPENGLES_GLSURFACE_H

#include <memory>
#include <stdint.h>

typedef void* EGLSurface;

class GLSurface {
public:
    GLSurface(int32_t width, int32_t height) : mWidth(width), mHeight(height) {}

    int32_t width() const { return mWidth; }
    int32_t height() const { return mHeight; }

private:
    GLSurface() {}
    friend class EglManager;
    int32_t mWidth;
    int32_t mHeight;
    EGLSurface mSurface;
};

#endif //SKIAOPENGLES_GLSURFACE_H
