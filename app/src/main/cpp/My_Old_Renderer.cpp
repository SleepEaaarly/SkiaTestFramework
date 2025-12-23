// skia_gl_My_Old_Renderer.cpp

#include "include/core/SkColorSpace.h"
#include "include/core/SkRefCnt.h"

#include "My_Old_Renderer.h"

// ✅ Updated GPU backend headers for modern Skia
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/core/SkRRect.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkBitmap.h"
#include "include/utils/SkEventTracer.h"
#include "src/core/SkATrace.h"
#include "tools/trace/SkDebugfTracer.h"
#include "tools/trace/ChromeTracingTracer.h"
#include "tools/trace/SkPerfettoTrace.h"

#include <android/log.h>
#include <android/trace.h>

#define LOG_TAG "My_Old_Renderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static bool hasInitTracer = false;

My_Old_Renderer::My_Old_Renderer() = default;

My_Old_Renderer::~My_Old_Renderer() {
    destroy();
}

void My_Old_Renderer::initializeEventTracingForTools(const char* traceFlag) {
    hasInitTracer = true;
    SkEventTracer* eventTracer = nullptr;
    if (0 == strcmp(traceFlag, "atrace")) {
        eventTracer = new SkATrace();
    } else if (0 == strcmp(traceFlag, "debugf")) {
        eventTracer = new SkDebugfTracer();
    } else if (0 == strcmp(traceFlag, "perfetto")) {
#if defined(SK_USE_PERFETTO)
        eventTracer = new SkPerfettoTrace();
#else
        SkDebugf("Perfetto is not enabled (SK_USE_PERFETTO is false). Perfetto tracing will not "
                 "be performed.\nTracing tools with Perfetto is only enabled for Linux, Android, "
                 "and Mac.\n");
        return;
#endif
    }
    else {
        eventTracer = new ChromeTracingTracer(traceFlag);
    }

    SkAssertResult(SkEventTracer::SetInstance(eventTracer));
}

bool My_Old_Renderer::init(ANativeWindow* window, int width, int height) {
    if (!window) {
        LOGE("Native window is null");
        return false;
    }

    mNativeWindow = window;
    mWidth = width;
    mHeight = height;

    // 1. Get EGL display
    mEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (mEglDisplay == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed");
        return false;
    }

    // 2. Initialize EGL
    EGLint major, minor;
    if (!eglInitialize(mEglDisplay, &major, &minor)) {
        LOGE("eglInitialize failed");
        return false;
    }
    LOGI("EGL initialized, version %d.%d", major, minor);

    // 3. Choose EGL config
    const EGLint configAttribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 0,
            EGL_STENCIL_SIZE, 0,
            EGL_NONE
    };

    EGLConfig eglConfig;
    EGLint numConfigs;
    if (!eglChooseConfig(mEglDisplay, configAttribs, &eglConfig, 1, &numConfigs) || numConfigs == 0) {
        LOGE("eglChooseConfig failed");
        return false;
    }

    // 4. Create EGL context
    const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE
    };
    mEglContext = eglCreateContext(mEglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (mEglContext == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed");
        return false;
    }

    // 5. Create EGL surface
    mEglSurface = eglCreateWindowSurface(mEglDisplay, eglConfig, mNativeWindow, nullptr);
    if (mEglSurface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed");
        return false;
    }

    // 6. Make context current
    if (!eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext)) {
        LOGE("eglMakeCurrent failed");
        return false;
    }


    // 7. ✅ CRITICAL: Create GrDirectContext with native OpenGL interface
    sk_sp<const GrGLInterface> interface = nullptr;

    mGrContext = GrDirectContexts::MakeGL(interface);
    if (!mGrContext) {
        LOGE("GrDirectContexts::MakeGL failed");
        return false;
    }

    // 8. Set up framebuffer info
    GrGLFramebufferInfo framebufferInfo;
    framebufferInfo.fFBOID = 0; // default framebuffer
    framebufferInfo.fFormat = GL_RGBA8;

    // 9. Create backend render target
    auto backendRenderTarget = GrBackendRenderTargets::MakeGL(
            mWidth, mHeight,
            /* sample count */ 1,
            /* stencil bits */ 8,
            framebufferInfo
    );
    auto colorSpace = SkColorSpace::MakeSRGB();
    // 10. ✅ Create SkSurface with explicit color space (recommended)
    mSkSurface = SkSurfaces::WrapBackendRenderTarget(
            mGrContext.get(),
            backendRenderTarget,
            kBottomLeft_GrSurfaceOrigin,
            kRGBA_8888_SkColorType,
            colorSpace, // ← explicit color space
            nullptr
    );

    if (!mSkSurface) {
        LOGE("SkSurface::MakeFromBackendRenderTarget failed");
        return false;
    }

    // 11. Clear screen
    glViewport(0, 0, mWidth, mHeight);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 12. initialTracingTools
//    if (!hasInitTracer)
//        initializeEventTracingForTools("perfetto");

    LOGI("My_Old_Renderer initialized successfully");
    return true;
}

void drawMany(SkCanvas* canvas) {
    // canvas->drawColor(SK_ColorWHITE);

    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(4);
    paint.setColor(SK_ColorRED);

    SkRect rect = SkRect::MakeXYWH(50, 50, 40, 60);
    canvas->drawRect(rect, paint);

    SkRRect oval;
    oval.setOval(rect);
    oval.offset(40, 60);
    paint.setColor(SK_ColorBLUE);
    canvas->drawRRect(oval, paint);

    paint.setColor(SK_ColorCYAN);
    canvas->drawCircle(180, 50, 25, paint);

    rect.offset(80, 0);
    paint.setColor(SK_ColorYELLOW);
    canvas->drawRoundRect(rect, 10, 10, paint);

    SkPath path;
    path.cubicTo(768, 0, -512, 256, 256, 256);
    paint.setColor(SK_ColorGREEN);
    canvas->drawPath(path, paint);
    canvas->drawPath(path, paint);

//    auto image = createTestImage(256, 256);
//
//    canvas->drawImage(image, 128, 128, SkSamplingOptions(), nullptr);
//
//    SkRect rect2 = SkRect::MakeXYWH(0, 0, 40, 60);
//    canvas->drawImageRect(image, rect2, SkSamplingOptions(), nullptr);
//
//    SkPaint paint2;
//    auto text = SkTextBlob::MakeFromString("Hello, Skia!", SkFont(nullptr, 18));
//    canvas->drawTextBlob(text.get(), 50, 25, paint2);
}

void My_Old_Renderer::render() {
    ATrace_beginSection("My_Old_Renderer::draw");

    LOGI("GrDirectContext %p, backend %d",
         mGrContext.get(),
         (int)mGrContext->backend());

    if (!mSkSurface) {
        LOGE("SkSurface not initialized");
        return;
    }


    SkCanvas* canvas = mSkSurface->getCanvas();
    // 获取画布尺寸
    SkISize canvasSize = canvas->getBaseLayerSize();
    int screenWidth = canvasSize.width();
    int screenHeight = canvasSize.height();

    // Optional: clear each frame
    canvas->clear(SK_ColorWHITE);

    SkPaint paint;
    paint.setColor(SK_ColorRED);
    paint.setStyle(SkPaint::kFill_Style);

    // 定义三角形的尺寸
    float triangleWidth = 200.0f;  // 三角形底边宽度
    float triangleHeight = 173.0f; // 等边三角形高度 (≈ 200 * √3/2)

    // 计算屏幕中心点
    float centerX = screenWidth / 2.0f;
    float centerY = screenHeight / 2.0f;

    // 计算三角形三个顶点（以中心点为基准）
    // 顶点1：顶部
    float topX = centerX;
    float topY = centerY - triangleHeight / 2.0f;

    // 顶点2：左下角
    float bottomLeftX = centerX - triangleWidth / 2.0f;
    float bottomLeftY = centerY + triangleHeight / 2.0f;

    // 顶点3：右下角
    float bottomRightX = centerX + triangleWidth / 2.0f;
    float bottomRightY = centerY + triangleHeight / 2.0f;

    SkPath path;
    path.moveTo(topX, topY);
    path.lineTo(bottomLeftX, bottomLeftY);
    path.lineTo(bottomRightX, bottomRightY);
    path.close();

    ATrace_beginSection("DrawTriangle");
    canvas->drawPath(path, paint);
    ATrace_endSection();

    ATrace_beginSection("DrawMany");
    drawMany(canvas);
    ATrace_endSection();

    // Submit to GPU and present
    mGrContext->flushAndSubmit();
    eglSwapBuffers(mEglDisplay, mEglSurface);

    ATrace_endSection();
}

void My_Old_Renderer::destroy() {
    if (mSkSurface) {
        mSkSurface.reset();
    }
    if (mGrContext) {
        mGrContext->flushAndSubmit();
        mGrContext.reset();
    }

    if (mEglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(mEglDisplay, mEglSurface);
        mEglSurface = EGL_NO_SURFACE;
    }
    if (mEglContext != EGL_NO_CONTEXT) {
        eglDestroyContext(mEglDisplay, mEglContext);
        mEglContext = EGL_NO_CONTEXT;
    }
    if (mEglDisplay != EGL_NO_DISPLAY) {
        eglTerminate(mEglDisplay);
        mEglDisplay = EGL_NO_DISPLAY;
    }
    mNativeWindow = nullptr;
}