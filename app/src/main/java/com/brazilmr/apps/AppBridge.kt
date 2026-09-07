/*
 * BrazilMR — App Bridge (coordenador).
 *
 * Une: AppLibrary (ícones) → lançamento do app → VrProjectionService
 * (captura do display) → janela espacial nativa (CONTENT_APP_SURFACE)
 * → injeção de input via VrAccessibilityService.
 *
 * O app Android roda na tela real do aparelho, mas o usuário o vê e
 * opera DENTRO do espaço VR — a tela física está no VR Box.
 */
package com.brazilmr.apps

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.media.Image
import android.media.projection.MediaProjectionManager
import android.os.Build
import com.brazilmr.apps.bridge.VrAccessibilityService
import com.brazilmr.apps.bridge.VrProjectionService
import com.brazilmr.sdk.NativeSdk

class AppBridge(private val activity: Activity) {

    companion object {
        const val WINDOW_ID = 200
        const val PROJECTION_REQUEST = 7002
        private const val REDRAW_HZ = 20
    }

    val library = AppLibrary(activity)

    private var textureId = 0
    private var bitmap: Bitmap? = null
    private var windowActive = false
    private var lastRedrawMs = 0L
    private var lastEventNs = 0L

    /** último estado de input na janela (para swipe) */
    private var lastU = -1f
    private var lastV = -1f
    private var downU = -1f
    private var downV = -1f
    private var pressing = false

    fun start() {
        library.onLaunch = { entry -> launchApp(entry) }
    }

    fun openLibrary() = library.open()

    // ------------------------------------------------------------------
    // Lançar app → pedir projeção (1×, consentimento do sistema)
    // ------------------------------------------------------------------
    fun launchApp(entry: AppLibrary.AppEntry) {
        pendingPackage = entry.packageName
        if (!VrProjectionService.running) {
            requestProjection()
        } else {
            doLaunch(entry.packageName)
        }
    }

    private var pendingPackage: String? = null

    @SuppressLint("WrongConstant")
    private fun requestProjection() {
        activity.runOnUiThread {
            val mpm = activity.getSystemService(
                Context.MEDIA_PROJECTION_SERVICE,
            ) as MediaProjectionManager
            @Suppress("DEPRECATION")
            activity.startActivityForResult(
                mpm.createScreenCaptureIntent(), PROJECTION_REQUEST,
            )
        }
    }

    /** Chamado pela VrActivity após o consentimento do sistema. */
    fun onProjectionResult(resultCode: Int, data: Intent) {
        val svc = Intent(activity, VrProjectionService::class.java).apply {
            putExtra("result_code", resultCode)
            putExtra("result_data", data)
        }
        if (Build.VERSION.SDK_INT >= 29) {
            activity.startForegroundService(svc)
        } else {
            activity.startService(svc)
        }
        pendingPackage?.let { doLaunch(it) }
        pendingPackage = null
        createSpatialWindow()
    }

    fun closeLibrary() = library.close()

    private fun doLaunch(packageName: String) {
        activity.runOnUiThread {
            try {
                val pm = activity.packageManager
                val launch = pm.getLaunchIntentForPackage(packageName) ?: return@runOnUiThread
                launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                activity.startActivity(launch)
            } catch (_: Exception) {
            }
        }
    }

    // ------------------------------------------------------------------
    // Janela espacial + pump de frames (thread GL)
    // ------------------------------------------------------------------
    private fun createSpatialWindow() {
        if (textureId == 0) {
            val tex = IntArray(1)
            android.opengl.GLES30.glGenTextures(1, tex, 0)
            textureId = tex[0]
        }
        val id = NativeSdk.createWindow(
            WINDOW_ID,
            0f, 0.1f, -2.0f,
            0f, 0f, 0f, 1f,
            0.92f, 1.45f, // proporção de celular em paisagem vertical
            NativeSdk.CONTENT_APP_SURFACE,
        )
        if (id >= 0) {
            windowActive = true
            NativeSdk.updateWindowFlags(WINDOW_ID, true, true, 10, textureId)
        }
    }

    fun frame(nowMs: Long) {
        if (!windowActive || !VrProjectionService.running) return
        if (nowMs - lastRedrawMs < 1000 / REDRAW_HZ) return
        lastRedrawMs = nowMs

        val image: Image = VrProjectionService.latestFrame ?: return
        if (image.timestamp == lastEventNs) return
        lastEventNs = image.timestamp

        try {
            val w = image.width
            val h = image.height
            if (bitmap == null || bitmap!!.width != w || bitmap!!.height != h) {
                bitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
            }
            val planes = image.planes
            val buf = planes[0].buffer
            val pixelStride = planes[0].pixelStride
            val rowStride = planes[0].rowStride
            if (pixelStride == 4 && rowStride == w * 4) {
                bitmap!!.copyPixelsFromBuffer(buf)
            } else {
                // linhas com padding — copia linha a linha
                val row = ByteArray(rowStride)
                val pixels = IntArray(w * h)
                var i = 0
                for (y in 0 until h) {
                    buf.get(row, 0, rowStride)
                    for (x in 0 until w) {
                        val o = x * 4
                        pixels[i++] = (row[o + 3].toInt() and 0xFF shl 24) or
                            (row[o].toInt() and 0xFF shl 16) or
                            (row[o + 1].toInt() and 0xFF shl 8) or
                            (row[o + 2].toInt() and 0xFF)
                    }
                }
                bitmap!!.setPixels(pixels, 0, w, 0, 0, w, h)
            }

            android.opengl.GLES30.glBindTexture(android.opengl.GLES30.GL_TEXTURE_2D, textureId)
            android.opengl.GLUtils.texImage2D(
                android.opengl.GLES30.GL_TEXTURE_2D, 0, bitmap, 0,
            )
            android.opengl.GLES30.glTexParameteri(
                android.opengl.GLES30.GL_TEXTURE_2D,
                android.opengl.GLES30.GL_TEXTURE_MIN_FILTER,
                android.opengl.GLES30.GL_LINEAR,
            )
            android.opengl.GLES30.glBindTexture(android.opengl.GLES30.GL_TEXTURE_2D, 0)
        } catch (_: Exception) {
        }
    }

    // ------------------------------------------------------------------
    // Input: janela (u,v) → tela real (acessibilidade)
    // ------------------------------------------------------------------
    fun onWindowInput(u: Float, v: Float, pointerEvent: Int) {
        if (!windowActive || !VrAccessibilityService.available()) return
        val dm = activity.resources.displayMetrics
        val sx = u.coerceIn(0f, 1f) * dm.widthPixels
        val sy = v.coerceIn(0f, 1f) * dm.heightPixels

        when (pointerEvent) {
            NativeSdk.POINTER_PRESS -> {
                downU = sx; downV = sy; lastU = sx; lastV = sy
                pressing = true
                VrAccessibilityService.tap(sx, sy)
            }
            NativeSdk.POINTER_MOVE -> {
                if (pressing && (lastU >= 0)) {
                    // arrasto com injeção contínua seria intrusivo: swipe no release
                }
                lastU = sx; lastV = sy
            }
            NativeSdk.POINTER_RELEASE -> {
                if (pressing && lastU >= 0) {
                    val dx = sx - downU
                    val dy = sy - downV
                    val dist = Math.hypot(dx.toDouble(), dy.toDouble()).toFloat()
                    if (dist > dm.widthPixels * 0.08f) {
                        // drag → swipe
                        VrAccessibilityService.swipe(downU, downV, sx, sy)
                    }
                    // tap já foi injetado no press; swipe cobre o drag
                }
                pressing = false
                lastU = -1f
            }
        }
    }

    fun closeWindow() {
        windowActive = false
        NativeSdk.removeWindow(WINDOW_ID)
    }
}
