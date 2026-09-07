/*
 * BrazilMR — SDK Nativo (ponte JNI)
 * ---------------------------------------------------------------
 * Espelha EXATAMENTE a superfície declarada em
 * `app/src/main/cpp/brazilmr_jni/JniBridge.cpp`. Qualquer mudança
 * de assinatura lá precisa ser refletida aqui (e vice-versa).
 *
 * Thread-safety: todas as funções nativas podem ser chamadas de
 * qualquer thread, EXCETO as marcadas [GL] que devem ser chamadas
 * apenas no thread de renderização com contexto EGL atual.
 */
package com.brazilmr.sdk

import android.graphics.Bitmap
import android.opengl.GLES30
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer

object NativeSdk {
    init {
        // libbrazilmr.so — ver CMakeLists.txt (app/src/main/cpp).
        System.loadLibrary("brazilmr")
    }

    // ------------------------------------------------------------------
    // Constantes espelhadas do nativo (BrazilmrConfig.hpp / VR.hpp /
    // HandTypes.hpp / SpatialWindow.hpp / UiScene.hpp / TrackingTypes.hpp)
    // ------------------------------------------------------------------
    /** SbsMode */
    const val SBS_NORMAL = 0
    const val SBS_INVERTED = 1
    const val SBS_LEFT_ONLY = 2
    const val SBS_RIGHT_ONLY = 3
    const val SBS_CALIBRATION = 4

    /** TrackingBackend */
    const val BACKEND_NONE = 0
    const val BACKEND_IMU_3DOF = 1
    const val BACKEND_CV_SLAM = 2
    const val BACKEND_ARCORE = 3

    /** WindowContent */
    const val CONTENT_PLACEHOLDER = 0
    const val CONTENT_APP_SURFACE = 1
    const val CONTENT_BROWSER = 2
    const val CONTENT_EMBEDDED = 3
    const val CONTENT_LUA_UI = 4

    /** Campos de nativeUpdateWindow(field) */
    const val WINDOW_FIELD_TRANSFORM = 0 // [px,py,pz, qx,qy,qz,qw, scale]
    const val WINDOW_FIELD_SIZE = 1      // [w,h, alpha, curvature]
    const val WINDOW_FIELD_FLAGS = 2     // [visible, focused, zOrder, textureId]

    /** Campos de nativeSetObjectState(field) */
    const val OBJECT_FIELD_TRANSFORM = 0 // [px,py,pz, qx..qw, sx,sy,sz]
    const val OBJECT_FIELD_PHYSICS = 1   // [vx,vy,vz, gravity, radius]
    const val OBJECT_FIELD_COLOR = 2     // [r,g,b,a]

    /** UiControlKind */
    const val UI_BUTTON = 0
    const val UI_SLIDER = 1
    const val UI_LABEL = 2
    const val UI_TOGGLE = 3
    const val UI_ICON = 4

    /** VrEventType (nativo → Java) */
    const val EVENT_UI_BUTTON = 1       // a=panelId, b=controlId
    const val EVENT_UI_SLIDER = 2       // a=panelId, b=controlId, x=value
    const val EVENT_UI_TOGGLE = 3       // a=panelId, b=controlId, x=value
    const val EVENT_UI_HOVER = 4        // a=panelId, b=controlId
    const val EVENT_UI_PANEL_FOCUS = 5  // a=panelId
    const val EVENT_WINDOW_INPUT = 6    // a=windowId, x=u, y=v, z=pointerEvent
    const val EVENT_GESTURE = 7         // a=hand(0=esq,1=dir), b=gesture, x=strength
    const val EVENT_POINTER_CLICK = 8   // x,y,z = ponto 3D (mm)

    /** PointerSource */
    const val POINTER_GAZE = 0
    const val POINTER_HAND = 1
    const val POINTER_CONTROLLER = 2

    /** PointerEvent */
    const val POINTER_HOVER = 0
    const val POINTER_PRESS = 1
    const val POINTER_RELEASE = 2
    const val POINTER_MOVE = 3

    /** HandGesture */
    const val GESTURE_NONE = 0
    const val GESTURE_OPEN_PALM = 1
    const val GESTURE_PINCH = 2
    const val GESTURE_GRAB = 3
    const val GESTURE_FIST = 4
    const val GESTURE_POINT = 5
    const val GESTURE_SWIPE = 6

    const val HAND_LANDMARKS = 21

    // ------------------------------------------------------------------
    // Ciclo de vida
    // ------------------------------------------------------------------
    fun initialize(screenW: Int, screenH: Int): Boolean =
        nativeInitialize(screenW, screenH)

    fun shutdown() = nativeShutdown()

    // ------------------------------------------------------------------
    // GL — todas chamadas no thread de render com contexto atual [GL]
    // ------------------------------------------------------------------
    fun initGl(w: Int, h: Int): Boolean = nativeInitGl(w, h)
    fun resizeGl(w: Int, h: Int) = nativeResizeGl(w, h)
    fun destroyGl() = nativeDestroyGl()

    /** Renderiza um frame estéreo completo. Retorna false se não inicializado. */
    fun renderFrame(dtSec: Float, nowNs: Long): Boolean =
        nativeRenderFrame(dtSec, nowNs)

    // ------------------------------------------------------------------
    // Perfil estéreo / calibração
    // ------------------------------------------------------------------
    fun setStereoProfile(
        fovYDeg: Float, ipdMm: Float, k1: Float, k2: Float,
        chroma: Float, vignette: Float, renderScale: Float,
        swapEyes: Boolean, sbsMode: Int,
        lensCenterX: Float, lensCenterY: Float,
    ) = nativeSetStereoProfile(
        fovYDeg, ipdMm, k1, k2, chroma, vignette, renderScale,
        swapEyes, sbsMode, lensCenterX, lensCenterY,
    )

    fun setScreenSize(widthMm: Float, heightMm: Float) =
        nativeSetScreenSize(widthMm, heightMm)

    fun recenter() = nativeRecenter()

    // ------------------------------------------------------------------
    // Sensores / tracking
    // ------------------------------------------------------------------
    /**
     * @param gyro rad/s no frame da CABEÇA (já remapeado de device→head,
     *             veja ImuTracker).
     * @param accel m/s² incluindo gravidade (repouso: +9.81 em Y do head).
     * @param mag µT ou null se indisponível/implausível.
     */
    fun pushImu(
        gyro: FloatArray, accel: FloatArray, mag: FloatArray?, timestampNs: Long,
    ) = nativePushImu(gyro, accel, mag, timestampNs)

    /** Pose 6DoF de backend externo (ARCore). Convenção Y-up, -Z à frente. */
    fun setPoseFromExternal(
        px: Float, py: Float, pz: Float,
        qx: Float, qy: Float, qz: Float, qw: Float,
        timestampNs: Long, confidence: Float,
    ) = nativeSetPoseFromExternal(px, py, pz, qx, qy, qz, qw, timestampNs, confidence)

    /** Envia apenas o canal luma de um frame YUV_420_888. [GL-independente] */
    fun pushCameraFrame(
        luma: ByteArray, w: Int, h: Int, stride: Int, timestampNs: Long,
    ) = nativePushCameraFrame(luma, w, h, stride, timestampNs)

    fun setTrackingBackend(backend: Int) = nativeSetTrackingBackend(backend)

    /**
     * @param cfg exatamente 10 floats:
     * [minCutoffHz, beta, dCutoffHz, useOneEuro, useKalmanPos,
     *  useOrientationEkf, usePrediction, predictionTimeSec,
     *  processNoiseAccel, measurementNoise]
     */
    fun setFilterConfig(cfg: FloatArray) {
        require(cfg.size == 10) { "setFilterConfig espera 10 floats" }
        nativeSetFilterConfig(cfg)
    }

    /** [px,py,pz, qx,qy,qz,qw, vx,vy,vz, confidence, dofs] (12 floats). */
    fun getHeadPose(): FloatArray? = nativeGetHeadPose()

    // ------------------------------------------------------------------
    // Mãos (MediaPipe) — landmarks em metros, espaço do MUNDO (Y-up).
    // ------------------------------------------------------------------
    fun setHandLandmarks(
        left: Boolean, landmarks: FloatArray, confidence: Float,
        gesture: Int, pinchStrength: Float, timestampNs: Long,
    ) = nativeSetHandLandmarks(
        left, landmarks, confidence, gesture, pinchStrength, timestampNs,
    )

    // ------------------------------------------------------------------
    // Janelas espaciais
    // ------------------------------------------------------------------
    fun createWindow(
        id: Int, px: Float, py: Float, pz: Float,
        qx: Float, qy: Float, qz: Float, qw: Float,
        widthM: Float, heightM: Float, content: Int,
    ): Int = nativeCreateWindow(id, px, py, pz, qx, qy, qz, qw, widthM, heightM, content)

    fun updateWindowTransform(
        windowId: Int, px: Float, py: Float, pz: Float,
        qx: Float, qy: Float, qz: Float, qw: Float, scale: Float,
    ) = nativeUpdateWindow(
        windowId, WINDOW_FIELD_TRANSFORM,
        floatArrayOf(px, py, pz, qx, qy, qz, qw, scale),
    )

    fun updateWindowSize(
        windowId: Int, widthM: Float, heightM: Float, alpha: Float, curvature: Float,
    ) = nativeUpdateWindow(
        windowId, WINDOW_FIELD_SIZE,
        floatArrayOf(widthM, heightM, alpha, curvature),
    )

    fun updateWindowFlags(
        windowId: Int, visible: Boolean, focused: Boolean, zOrder: Int, textureId: Int,
    ) = nativeUpdateWindow(
        windowId, WINDOW_FIELD_FLAGS,
        floatArrayOf(if (visible) 1f else 0f, if (focused) 1f else 0f,
                     zOrder.toFloat(), textureId.toFloat()),
    )

    fun removeWindow(windowId: Int) = nativeRemoveWindow(windowId)
    fun focusWindow(windowId: Int) = nativeFocusWindow(windowId)

    // ------------------------------------------------------------------
    // UI espacial (painéis flutuantes do sistema)
    // ------------------------------------------------------------------
    fun addUiPanel(
        px: Float, py: Float, pz: Float,
        qx: Float, qy: Float, qz: Float, qw: Float,
        widthM: Float, heightM: Float, title: String?, userData: Long = 0L,
    ): Int = nativeAddUiPanel(px, py, pz, qx, qy, qz, qw, widthM, heightM, title, userData)

    fun addUiControl(
        panelId: Int, kind: Int, x: Float, y: Float, w: Float, h: Float,
        label: String?, value: Float = 0f, iconTexture: Int = 0,
    ): Int = nativeAddUiControl(panelId, kind, x, y, w, h, label, value, iconTexture)

    fun setUiControlValue(panelId: Int, controlId: Int, value: Float) =
        nativeSetUiControlValue(panelId, controlId, value)

    fun setUiControlLabel(panelId: Int, controlId: Int, label: String?) =
        nativeSetUiControlLabel(panelId, controlId, label)

    fun setUiControlVisible(panelId: Int, controlId: Int, visible: Boolean) =
        nativeSetUiControlVisible(panelId, controlId, visible)

    fun setUiPanelTransform(
        panelId: Int, px: Float, py: Float, pz: Float,
        qx: Float, qy: Float, qz: Float, qw: Float,
    ) = nativeSetUiPanelTransform(panelId, px, py, pz, qx, qy, qz, qw)

    fun setUiPanelVisible(panelId: Int, visible: Boolean) =
        nativeSetUiPanelVisible(panelId, visible)

    fun removeUiPanel(panelId: Int) = nativeRemoveUiPanel(panelId)

    // ------------------------------------------------------------------
    // Fonte (atlas) — Kotlin gera o atlas e faz o upload GL [GL]
    // ------------------------------------------------------------------
    /**
     * Anuncia o atlas de fonte já carregado em `texId` (upload via GLES30
     * feito pelo Kotlin; o nativo NÃO faz upload).
     *
     * @param metrics layout stride 8:
     * [count, baseHeightPx,
     *  (code, x, y, w, h, xoff, yoff, advance) * count]
     */
    fun setFontAtlas(texId: Int, atlasW: Int, atlasH: Int, metrics: FloatArray) =
        nativeSetFontAtlas(texId, atlasW, atlasH, metrics)

    // ------------------------------------------------------------------
    // Modelos GLB
    // ------------------------------------------------------------------
    fun loadGlb(data: ByteArray): Long = nativeLoadGlb(data, data.size.toLong())

    fun setModelTransform(
        handle: Long, px: Float, py: Float, pz: Float,
        qx: Float, qy: Float, qz: Float, qw: Float,
        sx: Float, sy: Float, sz: Float, visible: Boolean,
    ): Boolean = nativeSetModelTransform(handle, px, py, pz, qx, qy, qz, qw, sx, sy, sz, visible)

    fun setModelAnimation(handle: Long, animIndex: Int, loop: Boolean): Boolean =
        nativeSetModelAnimation(handle, animIndex, loop)

    fun modelAnimationCount(handle: Long): Int = nativeGetModelAnimationCount(handle)

    fun modelAnimationName(handle: Long, index: Int): String? =
        nativeGetModelAnimationName(handle, index)

    // ------------------------------------------------------------------
    // Input espacial
    // ------------------------------------------------------------------
    fun pushPointer(
        source: Int, ox: Float, oy: Float, oz: Float,
        dx: Float, dy: Float, dz: Float, pointerEvent: Int,
    ) = nativePushPointer(source, ox, oy, oz, dx, dy, dz, pointerEvent)

    fun setActiveRay(
        visible: Boolean, ox: Float, oy: Float, oz: Float,
        dx: Float, dy: Float, dz: Float,
        r: Float, g: Float, b: Float,
    ) = nativeSetActiveRay(visible, ox, oy, oz, dx, dy, dz, r, g, b)

    // ------------------------------------------------------------------
    // Objetos 3D (física embutida no nativo)
    // ------------------------------------------------------------------
    fun spawnSphere(x: Float, y: Float, z: Float, radius: Float): Long =
        nativeSpawnSphere(x, y, z, radius)

    fun spawnCube(x: Float, y: Float, z: Float, halfExtent: Float): Long =
        nativeSpawnCube(x, y, z, halfExtent)

    fun setObjectColor(id: Long, r: Float, g: Float, b: Float, a: Float): Boolean =
        nativeSetObjectState(id, OBJECT_FIELD_COLOR, floatArrayOf(r, g, b, a))

    fun removeObject(id: Long): Boolean = nativeRemoveObject(id)

    // ------------------------------------------------------------------
    // Térmico / estatísticas
    // ------------------------------------------------------------------
    fun setThermalBudget(budget01: Float) = nativeSetThermalBudget(budget01)

    /** [fps, frameMs, drawCalls, tris, cpuMs, gpuMs?, thermalBudget] (7). */
    fun getStats(): FloatArray? = nativeGetStats()

    // ------------------------------------------------------------------
    // Eventos (drenar a cada frame)
    // ------------------------------------------------------------------
    /**
     * @param outEvent IntArray(6): [type, a, b, x*1000, y*1000, z*1000].
     * @return true se um evento foi escrito.
     */
    fun pollEvent(outEvent: IntArray): Boolean = nativePollEvent(outEvent)

    // ==================================================================
    // Declarações externas — 1:1 com JniBridge.cpp
    // ==================================================================
    private external fun nativeInitialize(screenW: Int, screenH: Int): Boolean
    private external fun nativeShutdown()
    private external fun nativeInitGl(w: Int, h: Int): Boolean
    private external fun nativeResizeGl(w: Int, h: Int)
    private external fun nativeDestroyGl()
    private external fun nativeSetStereoProfile(
        fovYDeg: Float, ipdMm: Float, k1: Float, k2: Float, chroma: Float,
        vignette: Float, renderScale: Float, swapEyes: Boolean, sbsMode: Int,
        lensCx: Float, lensCy: Float,
    )
    private external fun nativeSetScreenSize(wMm: Float, hMm: Float)
    private external fun nativeRecenter()
    private external fun nativePushImu(
        gyro: FloatArray, accel: FloatArray, mag: FloatArray?, timestampNs: Long,
    )
    private external fun nativeSetPoseFromExternal(
        px: Float, py: Float, pz: Float, qx: Float, qy: Float, qz: Float,
        qw: Float, ts: Long, confidence: Float,
    )
    private external fun nativePushCameraFrame(
        luma: ByteArray, w: Int, h: Int, stride: Int, timestampNs: Long,
    )
    private external fun nativeSetTrackingBackend(backend: Int)
    private external fun nativeSetFilterConfig(cfg: FloatArray)
    private external fun nativeGetHeadPose(): FloatArray?
    private external fun nativeSetHandLandmarks(
        left: Boolean, landmarks: FloatArray, confidence: Float,
        gesture: Int, pinchStrength: Float, timestampNs: Long,
    )
    private external fun nativeCreateWindow(
        id: Int, px: Float, py: Float, pz: Float, qx: Float, qy: Float,
        qz: Float, qw: Float, widthM: Float, heightM: Float, content: Int,
    ): Int
    private external fun nativeUpdateWindow(windowId: Int, field: Int, values: FloatArray)
    private external fun nativeRemoveWindow(windowId: Int)
    private external fun nativeFocusWindow(windowId: Int)
    private external fun nativeAddUiPanel(
        px: Float, py: Float, pz: Float, qx: Float, qy: Float, qz: Float,
        qw: Float, widthM: Float, heightM: Float, title: String?, userData: Long,
    ): Int
    private external fun nativeAddUiControl(
        panelId: Int, kind: Int, x: Float, y: Float, w: Float, h: Float,
        label: String?, value: Float, iconTexture: Int,
    ): Int
    private external fun nativeSetUiControlValue(panelId: Int, controlId: Int, value: Float)
    private external fun nativeSetUiControlLabel(panelId: Int, controlId: Int, label: String?)
    private external fun nativeSetUiControlVisible(panelId: Int, controlId: Int, visible: Boolean)
    private external fun nativeSetUiPanelTransform(
        panelId: Int, px: Float, py: Float, pz: Float,
        qx: Float, qy: Float, qz: Float, qw: Float,
    )
    private external fun nativeSetUiPanelVisible(panelId: Int, visible: Boolean)
    private external fun nativeRemoveUiPanel(panelId: Int)
    private external fun nativeSetFontAtlas(texId: Int, atlasW: Int, atlasH: Int, metrics: FloatArray)
    private external fun nativeLoadGlb(data: ByteArray, size: Long): Long
    private external fun nativeSetModelTransform(
        modelHandle: Long, px: Float, py: Float, pz: Float, qx: Float, qy: Float,
        qz: Float, qw: Float, sx: Float, sy: Float, sz: Float, visible: Boolean,
    ): Boolean
    private external fun nativeSetModelAnimation(modelHandle: Long, animIndex: Int, loop: Boolean): Boolean
    private external fun nativeGetModelAnimationCount(modelHandle: Long): Int
    private external fun nativeGetModelAnimationName(modelHandle: Long, index: Int): String?
    private external fun nativePushPointer(
        source: Int, ox: Float, oy: Float, oz: Float,
        dx: Float, dy: Float, dz: Float, pointerEvent: Int,
    )
    private external fun nativeSetActiveRay(
        visible: Boolean, ox: Float, oy: Float, oz: Float,
        dx: Float, dy: Float, dz: Float, r: Float, g: Float, b: Float,
    )
    private external fun nativeSpawnSphere(x: Float, y: Float, z: Float, radius: Float): Long
    private external fun nativeSpawnCube(x: Float, y: Float, z: Float, halfExtent: Float): Long
    private external fun nativeSetObjectState(id: Long, field: Int, values: FloatArray): Boolean
    private external fun nativeRemoveObject(id: Long): Boolean
    private external fun nativeSetThermalBudget(budget01: Float)
    private external fun nativeGetStats(): FloatArray?
    private external fun nativeRenderFrame(dtSec: Float, nowNs: Long): Boolean
    private external fun nativePollEvent(outEvent: IntArray): Boolean

    // ==================================================================
    // Utilidades (lado Kotlin)
    // ==================================================================

    /** Converte Bitmap → textura GLSLE30 e retorna o id. [GL] */
    fun uploadBitmap(bitmap: Bitmap, linear: Boolean = true): Int {
        val tex = IntArray(1)
        GLES30.glGenTextures(1, tex, 0)
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, tex[0])
        val w = bitmap.width
        val h = bitmap.height
        val buf: ByteBuffer = ByteBuffer.allocateDirect(w * h * 4).order(ByteOrder.nativeOrder())
        bitmap.copyPixelsToBuffer(buf)
        buf.position(0)
        GLES30.glTexImage2D(
            GLES30.GL_TEXTURE_2D, 0, GLES30.GL_RGBA, w, h, 0,
            GLES30.GL_RGBA, GLES30.GL_UNSIGNED_BYTE, buf,
        )
        val filter = if (linear) GLES30.GL_LINEAR else GLES30.GL_NEAREST
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MIN_FILTER, filter)
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MAG_FILTER, filter)
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_WRAP_S, GLES30.GL_CLAMP_TO_EDGE)
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_WRAP_T, GLES30.GL_CLAMP_TO_EDGE)
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, 0)
        return tex[0]
    }

    /** Filtro padrão balanceado do BrazilMR (ver PoseFilterConfig nativo). */
    val FILTER_BALANCED: FloatArray = floatArrayOf(
        1.2f,   // minCutoffHz
        0.05f,  // beta
        1.0f,   // dCutoffHz
        1f,     // useOneEuro
        1f,     // useKalmanPos
        1f,     // useOrientationEkf
        1f,     // usePrediction
        0.012f, // predictionTimeSec
        1.5f,   // processNoiseAccel
        0.02f,  // measurementNoise
    )

    /** Filtro para headset com muito jitter (aparelhos de entrada). */
    val FILTER_STABLE: FloatArray = floatArrayOf(
        0.6f, 0.02f, 0.8f, 1f, 1f, 1f, 1f, 0.020f, 1.0f, 0.05f,
    )

    /** Filtro para modo esporte/ativo (responsividade máxima). */
    val FILTER_RESPONSIVE: FloatArray = floatArrayOf(
        3.0f, 0.15f, 1.5f, 1f, 1f, 1f, 1f, 0.010f, 2.5f, 0.01f,
    )

    /** Garante um FloatBuffer nativo para uploads GL. */
    fun directFloatBuffer(values: FloatArray): FloatBuffer =
        ByteBuffer.allocateDirect(values.size * 4)
            .order(ByteOrder.nativeOrder())
            .asFloatBuffer().apply { put(values); position(0) }
}
