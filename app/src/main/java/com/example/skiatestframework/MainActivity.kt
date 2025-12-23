package com.example.skiatestframework

import android.app.Activity
import android.os.Bundle

class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // 原来的 Skia 画面
        val skiaView = SkiaSurfaceView(this)
        setContentView(skiaView)
    }
}