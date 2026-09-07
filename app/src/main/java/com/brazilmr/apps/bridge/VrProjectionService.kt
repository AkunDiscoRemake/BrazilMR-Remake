/*
 * BrazilMR — Serviço de projeção (captura de apps Android).
 *
 * Mantém um MediaProjection + VirtualDisplay + ImageReader enquanto o
 * usuário usa apps dentro de janelas espaciais. Os frames capturados
 * ficam num holder singleton consumido pelo thread GL do AppBridge.
 *
 * Fluxo (arquitetura APP→ADB/BRIDGE→RUNTIME→SURFACE CAPTURE→SPATIAL
 * WINDOW→STEREO):
 *   APP (lançada normal) → BRIDGE (este serviço) → SURFACE CAPTURE
 *   (VirtualDisplay→ImageReader) → janela espacial nativa → SBS estéreo.
 */
package com.brazilmr.apps.bridge

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.graphics.PixelFormat
import android.hardware.display.DisplayManager
import android.hardware.display.VirtualDisplay
import android.media.Image
import android.media.ImageReader
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.Handler
import android.os.HandlerThread
import android.os.IBinder
import android.util.DisplayMetrics

class VrProjectionService : Service() {

    companion object {
        /** Última captura disponível (RGBA8888). */
        @Volatile var latestFrame: Image? = null
            private set
        @Volatile var frameTimestampNs: Long = 0
            private set
        @Volatile var captureWidth: Int = 0
            private set
        @Volatile var captureHeight: Int = 0
            private set
        @Volatile var running: Boolean = false
            private set

        private const val CHANNEL_ID = "bmr_projection"
        private const val NOTIFICATION_ID = 2001
    }

    private var projection: MediaProjection? = null
    private var virtualDisplay: VirtualDisplay? = null
    private var reader: ImageReader? = null
    private var thread: HandlerThread? = null
    private var handler: Handler? = null

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val resultCode = intent?.getIntExtra("result_code", 0) ?: 0
        @Suppress("DEPRECATION")
        val data: android.content.Intent? =
            if (Build.VERSION.SDK_INT >= 33)
                intent?.getParcelableExtra("result_data", android.content.Intent::class.java)
            else
                @Suppress("UNCHECKED_CAST")
                intent?.getParcelableExtra("result_data") as? android.content.Intent

        if (data == null) {
            stopSelf()
            return START_NOT_STICKY
        }

        startForegroundCompat()
        startCapture(resultCode, data)
        return START_NOT_STICKY
    }

    private fun startForegroundCompat() {
        val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        if (Build.VERSION.SDK_INT >= 26) {
            nm.createNotificationChannel(
                NotificationChannel(
                    CHANNEL_ID, "Captura de apps (VR)",
                    NotificationManager.IMPORTANCE_LOW,
                ),
            )
        }
        val notification: Notification =
            if (Build.VERSION.SDK_INT >= 29)
                Notification.Builder(this, CHANNEL_ID)
                    .setContentTitle("BrazilMR")
                    .setContentText("Projetando apps no espaço VR")
                    .setSmallIcon(android.R.drawable.ic_media_play)
                    .build()
            else
                @Suppress("DEPRECATION")
                Notification.Builder(this)
                    .setContentTitle("BrazilMR")
                    .setContentText("Projetando apps no espaço VR")
                    .setSmallIcon(android.R.drawable.ic_media_play)
                    .build()

        if (Build.VERSION.SDK_INT >= 29) {
            startForeground(
                NOTIFICATION_ID, notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION,
            )
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    private fun startCapture(resultCode: Int, data: android.content.Intent) {
        try {
            val mpm = getSystemService(Context.MEDIA_PROJECTION_SERVICE) as MediaProjectionManager
            @Suppress("DEPRECATION")
            val mp = mpm.getMediaProjection(resultCode, data)
            projection = mp

            val metrics = resources.displayMetrics ?: DisplayMetrics()
            // captura na resolução NATIVA do display (a janela pode reduzir)
            captureWidth = (metrics.widthPixels * 0.7f).toInt().coerceAtLeast(720)
            captureHeight = (metrics.heightPixels * 0.7f).toInt().coerceAtLeast(1280)

            thread = HandlerThread("bmr-capture").also { it.start() }
            handler = Handler(thread!!.looper)

            reader = ImageReader.newInstance(
                captureWidth, captureHeight, PixelFormat.RGBA_8888, 2,
            ).apply {
                setOnImageAvailableListener({ r ->
                    // mantém apenas o MAIS RECENTE (latência mínima)
                    val old = latestFrame
                    latestFrame = r.acquireLatestImage()
                    old?.close()
                    frameTimestampNs = android.os.SystemClock.elapsedRealtimeNanos()
                }, handler)
            }

            // assinatura oficial: (name, w, h, dpi, FLAGS, surface, cb, handler)
            @Suppress("DEPRECATION")
            virtualDisplay = mp.createVirtualDisplay(
                "BrazilMR-Apps",
                captureWidth, captureHeight, metrics.densityDpi,
                0, reader!!.surface, null, null,
            )
            running = true
        } catch (_: Exception) {
            stopSelf()
        }
    }

    override fun onDestroy() {
        running = false
        latestFrame?.close()
        latestFrame = null
        try {
            virtualDisplay?.release()
            projection?.stop()
            reader?.close()
        } catch (_: Exception) {
        }
        thread?.quitSafely()
        super.onDestroy()
    }
}
