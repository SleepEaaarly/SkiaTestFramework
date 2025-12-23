package com.example.skiatestframework;

import android.content.Context;
import android.graphics.PixelFormat;
import android.util.AttributeSet;
import android.view.Choreographer;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;

public class SkiaSurfaceView extends SurfaceView implements SurfaceHolder.Callback {

    private final Choreographer mChoreographer = Choreographer.getInstance();
    private final Choreographer.FrameCallback mFrameCallback = new Choreographer.FrameCallback() {
        @Override
        public void doFrame(long frameTimeNanos) {
            nativeDraw();          // 只管调 native
            postFrame();           // 预约下一帧
        }
    };

    private native void nativeSurfaceCreated(android.view.Surface surface);
    private native void nativeSurfaceDestroyed();
    private native void nativeDraw();
    static { System.loadLibrary("native-lib"); }

    public SkiaSurfaceView(Context context) {
        super(context);
        init();
    }
    public SkiaSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }
    private void init() {
        getHolder().setFormat(PixelFormat.TRANSPARENT);
        setZOrderOnTop(true);
        getHolder().addCallback(this);
    }

    /* -------------------- 生命周期 -------------------- */
    @Override
    public void surfaceCreated(@NonNull SurfaceHolder holder) {
        nativeSurfaceCreated(holder.getSurface());

        postFrame();                 // 开始循环
    }
    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
        stopFrame();                 // 停止循环
        nativeSurfaceDestroyed();
    }
    @Override
    public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {}

    /* -------------------- VSYNC 调度 -------------------- */
    private void postFrame() {
        mChoreographer.postFrameCallback(mFrameCallback);
    }
    private void stopFrame() {
        mChoreographer.removeFrameCallback(mFrameCallback);
    }
}