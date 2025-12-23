//
// Created by zeng on 2025/10/29.
//
// native-lib.cpp

#include <jni.h>
#include <string>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "My_Old_Renderer.h"

#include <android/log.h>

#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,  "SkiaDemo", __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, "SkiaDemo", __VA_ARGS__)

#define TRACE()    LOGI(">>> %s:%d  %s", __FILE_NAME__, __LINE__, __func__)

// 全局变量，存储渲染器实例
static My_Old_Renderer* gRenderer = nullptr;

extern "C" {

JNIEXPORT void JNICALL
Java_com_example_skiaopengles_SkiaGLSurfaceView_nativeSurfaceCreated(JNIEnv* env, jobject thiz, jobject surface) {
    // 将 Java 的 Surface 对象转换为 ANativeWindow
    ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (!nativeWindow) {
        LOGE("Failed to get ANativeWindow from Surface");
        return;
    }

    int32_t width  = ANativeWindow_getWidth(nativeWindow);
    int32_t height = ANativeWindow_getHeight(nativeWindow);
    if (width <= 0 || height <= 0) {
        width = height = 400;   // 兜底
    }

    if (!gRenderer->init(nativeWindow, width, height)) {
        LOGE("Failed to initialize Renderer");
        delete gRenderer;
        gRenderer = nullptr;
        ANativeWindow_release(nativeWindow);
        return;
    }

    // 释放对 ANativeWindow 的引用
    ANativeWindow_release(nativeWindow);
}

JNIEXPORT void JNICALL
Java_com_example_skiaopengles_SkiaGLSurfaceView_nativeSurfaceChanged(JNIEnv* env, jobject thiz, jint width, jint height) {
    // 如果需要处理尺寸变化，可以在这里重新初始化
    // 为了简化，此处不处理
}

JNIEXPORT void JNICALL
Java_com_example_skiaopengles_SkiaGLSurfaceView_nativeSurfaceDestroyed(JNIEnv* env, jobject thiz) {
    if (gRenderer) {
        delete gRenderer;
        gRenderer = nullptr;
    }
}

JNIEXPORT void JNICALL
Java_com_example_skiaopengles_SkiaGLSurfaceView_nativeDraw(JNIEnv* env, jobject thiz) {
    if (gRenderer) {
        gRenderer->render();
    }
}

} // extern "C"