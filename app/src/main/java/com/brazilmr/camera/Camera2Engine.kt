/*
 * BrazilMR — motor de câmera Camera2.
 *
 * Captura YUV_420_888 da câmera TRASEIRA (a que aponta para o mundo no
 * VR Box) com reuso de buffers e latência mínima:
 *   - ImageReader com maxImages=3 (pipeline raso)
 *   - descarte de frames antigos (acquireLatestImage)
 *   - extração direta do plano LUMA (o tracker nativo consome luma)
 *   - presets por classe de aparelho (LOW-END → MAX TRACKING)
 *
 * O mesmo stream alimenta:
 *   - o backend CV/SLAM do nativo (pushCameraFrame)
 *   - o MediaPipe hand tracking (via HandTracking)
 */
package com.brazilmr.camera

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.ImageFormat
import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CaptureRequest
import android.media.Image
import android.media.ImageReader
import android.os.Handler
import android.os.HandlerThread
import android.util.Size
import android.view.Surface
import com.brazilmr.sdk.NativeSdk

class Camera2Engine(private val context: Context) {

    enum class Preset(val width: Int, val height: Int, val fps: Int) {
        LOW_END(320, 240, 30),       // aparelhos de entrada: 3DoF + hands off
        BALANCED(640, 480, 30),      // padrão: VO leve + hands
        PERFORMANCE(896, 672, 30),   // VO completo + hands
        MAX_TRACKING(1280, 720, 30), // SLAM máximo (topo de linha)
    }

    /** Consumidor opcional de frames RGB (MediaPipe). */
    interface FrameListener {
        fun onFrame(bitmap: android.graphics.Bitmap, timestampNs: Long)
    }

    var frameListener: FrameListener? = null

    private var camera: CameraDevice? = null
    private var session: CameraCaptureSession? = null
    private var reader: ImageReader? = null
    private var thread: HandlerThread? = null
    private var handler: Handler? = null
    private var running = false
    private var preset = Preset.BALANCED

    // buffers reutilizados (evita GC no caminho crítico)
    private var lumaBuf: ByteArray = ByteArray(0)
    private var rgbBuf: IntArray = IntArray(0)
    private var rgbBitmap: android.graphics.Bitmap? = null

    @Volatile var lastFrameTimestampNs: Long = 0L
        private set

    fun start(preset: Preset?) {
        if (preset != null) this.preset = preset
        if (running) return
        running = true
        thread = HandlerThread("bmr-camera").also { it.start() }
        handler = Handler(thread!!.looper)
        openCamera()
    }

    fun stop() {
        running = false
        try {
            session?.close()
            camera?.close()
            reader?.close()
        } catch (_: Exception) {
        }
        session = null
        camera = null
        reader = null
        thread?.quitSafely()
        thread = null
        handler = null
    }

    @SuppressLint("MissingPermission") // permissão checada pela VrActivity
    private fun openCamera() {
        val cm = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager
        val cameraId = pickBackCamera(cm) ?: return
        val chars = cm.getCameraCharacteristics(cameraId)
        val size = pickSize(chars, preset.width, preset.height)

        reader = ImageReader.newInstance(size.width, size.height, ImageFormat.YUV_420_888, 3).apply {
            setOnImageAvailableListener({ r -> drain(r), }, handler)
        }
        try {
            cm.openCamera(cameraId, object : CameraDevice.StateCallback() {
                override fun onOpened(device: CameraDevice) {
                    if (!running) {
                        device.close()
                        return
                    }
                    camera = device
                    createSession(device)
                }

                override fun onDisconnected(device: CameraDevice) {
                    device.close()
                    camera = null
                }

                override fun onError(device: CameraDevice, error: Int) {
                    device.close()
                    camera = null
                }
            }, handler)
        } catch (_: SecurityException) {
            // permissão revogada em runtime: mantém 3DoF IMU
            running = false
        }
    }

    private fun createSession(device: CameraDevice) {
        val surface = reader!!.surface
        try {
            device.createCaptureSession(
                listOf(surface),
                object : android.hardware.camera2.CameraCaptureSession.StateCallback() {
                    override fun onConfigured(s: CameraCaptureSession) {
                        session = s
                        val req = device.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW).apply {
                            addTarget(surface)
                            set(CaptureRequest.CONTROL_AF_MODE,
                                CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE)
                            set(CaptureRequest.CONTROL_AE_MODE,
                                CaptureRequest.CONTROL_AE_MODE_ON)
                            set(CaptureRequest.CONTROL_AWB_MODE,
                                CaptureRequest.CONTROL_AWB_MODE_AUTO)
                            // latência: prioriza throughput, não qualidade JPEG
                            set(CaptureRequest.CONTROL_CAPTURE_INTENT,
                                CaptureRequest.CONTROL_CAPTURE_INTENT_PREVIEW)
                        }
                        try {
                            s.setRepeatingRequest(req.build(), null, handler)
                        } catch (_: Exception) {
                        }
                    }

                    override fun onConfigureFailed(s: CameraCaptureSession) {
                        s.close()
                    }
                },
                handler,
            )
        } catch (_: Exception) {
        }
    }

    private fun pickBackCamera(cm: CameraManager): String? {
        for (id in cm.cameraIdList) {
            val c = cm.getCameraCharacteristics(id)
            val facing = c.get(CameraCharacteristics.LENS_FACING)
            if (facing == CameraCharacteristics.LENS_FACING_BACK) return id
        }
        return cm.cameraIdList.firstOrNull()
    }

    /** Stream YUV mais próxima do alvo (menor excesso de resolução). */
    private fun pickSize(chars: CameraCharacteristics, targetW: Int, targetH: Int): Size {
        val map = chars.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)
        val sizes = map?.getOutputSizes(ImageFormat.YUV_420_888)?.toList() ?: emptyList()
        if (sizes.isEmpty()) return Size(targetW, targetH)
        val candidates = sizes.filter { it.width <= targetW * 2 && it.height <= targetH * 2 }
            .ifEmpty { sizes }
        return candidates.minByOrNull {
            Math.abs(it.width - targetW) + Math.abs(it.height - targetH)
        } ?: Size(targetW, targetH)
    }

    // ------------------------------------------------------------------
    // Consumo do frame — caminho crítico (sem alocações por frame além do
    // redimensionamento inicial).
    // ------------------------------------------------------------------
    private fun drain(r: ImageReader) {
        var image: Image? = null
        try {
            image = r.acquireLatestImage() ?: return
            val planes = image.planes
            val yPlane = planes[0]
            val yBuf = yPlane.buffer
            val w = image.width // largura CORTADA; stride vai separado
            val h = image.height
            val rowStride = yPlane.rowStride

            if (lumaBuf.size < rowStride * h) lumaBuf = ByteArray(rowStride * h)
            yBuf.get(lumaBuf, 0, Math.min(lumaBuf.size, yBuf.remaining()))
            val ts = image.timestamp
            lastFrameTimestampNs = ts

            // 1) backend CV do nativo (luma puro)
            NativeSdk.pushCameraFrame(lumaBuf, w, h, rowStride, ts)

            // 2) MediaPipe (RGB) — só se houver consumidor
            if (frameListener != null) {
                yuvToRgbBitmap(image, w, h)
                rgbBitmap?.let { bmp -> frameListener?.onFrame(bmp, ts) }
            }
        } catch (_: Exception) {
            // frame perdido não derruba a plataforma
        } finally {
            image?.close()
        }
    }

    /** Conversão YUV→ARGB em buffer reutilizado (MediaPipe exige RGB). */
    private fun yuvToRgbBitmap(img: Image, w: Int, h: Int) {
        if (rgbBuf.size != w * h) {
            rgbBuf = IntArray(w * h)
            rgbBitmap = android.graphics.Bitmap.createBitmap(w, h, android.graphics.Bitmap.Config.ARGB_8888)
        }
        val yP = img.planes[0]
        val uP = img.planes[1]
        val vP = img.planes[2]
        val yRow = yP.rowStride
        val uRow = uP.rowStride
        val vRow = vP.rowStride
        val yPix = yP.pixelStride
        val uPix = uP.pixelStride
        val vPix = vP.pixelStride
        val yb = yP.buffer
        val ub = uP.buffer
        val vb = vP.buffer
        val halfW = w / 2

        var i = 0
        for (row in 0 until h) {
            val yRowBase = row * yRow
            val uvRowBase = (row shr 1) * uRow
            val vRowBase = (row shr 1) * vRow
            for (col in 0 until w) {
                val y = (yb.get(yRowBase + col * yPix).toInt() and 0xFF)
                val u = (ub.get(uvRowBase + (col shr 1) * uPix).toInt() and 0xFF) - 128
                val v = (vb.get(vRowBase + (col shr 1) * vPix).toInt() and 0xFF) - 128
                var r = y + ((91881 * v) shr 16)
                var g = y - (((22554 * u) + (46802 * v)) shr 16)
                var b = y + ((116130 * u) shr 16)
                if (r < 0) r = 0 else if (r > 255) r = 255
                if (g < 0) g = 0 else if (g > 255) g = 255
                if (b < 0) b = 0 else if (b > 255) b = 255
                rgbBuf[i++] = (0xFF shl 24) or (r shl 16) or (g shl 8) or b
            }
        }
        rgbBitmap?.setPixels(rgbBuf, 0, w, 0, 0, w, h)
    }
}
