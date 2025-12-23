// skia_gl_renderer.h

#ifndef SKIA_GL_RENDERER_H
#define SKIA_GL_RENDERER_H

#include <jni.h>
#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"


class My_Old_Renderer {
public:
    My_Old_Renderer();
    ~My_Old_Renderer();

    // 初始化 OpenGL ES 上下文和 Skia 环境
    bool init(ANativeWindow* window, int width, int height);

    // 执行绘制操作
    void render();

    // 销毁资源
    void destroy();

    void initializeEventTracingForTools(const char* traceFlag);

private:
    ANativeWindow* mNativeWindow;
    EGLDisplay mEglDisplay;
    EGLSurface mEglSurface;
    EGLContext mEglContext;
    int mWidth;
    int mHeight;

    sk_sp<GrDirectContext> mGrContext;
    sk_sp<SkSurface> mSkSurface;
};

#endif // SKIA_GL_RENDERER_H