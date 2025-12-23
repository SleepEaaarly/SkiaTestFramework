//
// Created by zeng on 2025/10/22.
//

#ifndef SKIAOPENGLES_EGLMANAGER_H
#define SKIAOPENGLES_EGLMANAGER_H


#include <EGL/egl.h>
#include <memory>
#include <stdint.h>

#include "GLSurface.h"
#include "include/core/SkColorSpace.h"
#include "../ColorMode.h"

class EglManager {
public:
    explicit EglManager();

    ~EglManager();

    static const char* eglErrorString();

    void initialize();

    EGLSurface createSurface(EGLNativeWindowType window, ColorMode colorMode, sk_sp<SkColorSpace> colorSpace);

    void destroySurface(EGLSurface surface);

    void destroy();

    bool swapBuffers(const GLSurface& frame);


    GLSurface beginFrame(EGLSurface surface);
    bool makeCurrent(EGLSurface surface, EGLint* errOut = nullptr, bool force = false);
    bool isCurrent(EGLSurface surface) { return mCurrentSurface == surface; }
    bool hasEglContext();
private:
    void initExtensions();
    void loadConfig();
    void createContext();
    void createPBufferSurface();
    EGLConfig load8BitsConfig(EGLDisplay display);
    EGLConfig loadFP16Config(EGLDisplay  display);
    EGLConfig  load1010102Config(EGLDisplay  display);
    EGLConfig loadA8Config(EGLDisplay display);
private:
    EGLDisplay mEglDisplay;
    EGLConfig  mEglConfig;
    EGLConfig  mEglConfigF16;
    EGLConfig  mEglConfig1010102;
    EGLConfig  mEglConfigA8;
    EGLContext mEglContext;
    EGLSurface mPBufferSurface;
    EGLSurface mCurrentSurface;
};


#endif //SKIAOPENGLES_EGLMANAGER_H
