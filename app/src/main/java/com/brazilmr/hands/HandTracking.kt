/*
 * BrazilMR — hand tracking (MediaPipe HandLandmarker).
 *
 * 21 landmarks por mão em espaço NORMALIZADO da câmera → projetados para
 * METROS no espaço do mundo (Y-up) usando a pose da cabeça + modelo de
 * pinhole (mão à distância de trabalho ~45 cm da câmera). Gestos são
 * derivados dos próprios landmarks (pinça, punho, palma aberta, apontar,
 * grab, swipe) — sem gesture classifier extra.
 *
 * Degradação: sem modelo em assets/hand_landmarker.task (baixe com
 * tools/fetch_models.sh), sem câmera ou sem lib nativa do MediaPipe no
 * aparelho → hands OFF e o apontador de cabeça (gaze) cobre a interação.
 */
package com.brazilmr.hands

import android.content.Context
import android.graphics.Bitmap
import com.google.mediapipe.framework.image.MPImage
import com.google.mediapipe.tasks.core.BaseOptions
import com.google.mediapipe.tasks.vision.core.RunningMode
import com.google.mediapipe.tasks.vision.handlandmarker.HandLandmarker
import com.google.mediapipe.tasks.vision.handlandmarker.HandLandmarkerResult
import com.brazilmr.camera.Camera2Engine
import com.brazilmr.sdk.NativeSdk
import kotlin.math.abs
import kotlin.math.hypot

class HandTracking(
    private val context: Context,
    private val camera: Camera2Engine,
) : Camera2Engine.FrameListener {

    // índices MediaPipe (21 landmarks)
    private val WRIST = 0
    private val THUMB_TIP = 4
    private val INDEX_MCP = 5
    private val INDEX_TIP = 8
    private val MIDDLE_TIP = 12
    private val RING_TIP = 16
    private val PINKY_TIP = 17

    /** Distância de trabalho da mão (m) para a projeção pinhole. */
    private val handDistance = 0.45f
    private val fovXDeg = 63f // FOV horizontal típico de câmera traseira

    private var landmarker: HandLandmarker? = null
    private var running = false
    private var lastInferenceNs = 0L

    // último resultado convertido (empurrado ao nativo no frame GL)
    private val left = HandState()
    private val right = HandState()

    private class HandState {
        val landmarks = FloatArray(NativeSdk.HAND_LANDMARKS * 3)
        var confidence = 0f
        var gesture = NativeSdk.GESTURE_NONE
        var pinch = 0f
        var timestampNs = 0L
        var valid = false
        var lastWristX = 0f
        var lastWristTime = 0L
    }

    fun start() {
        if (running) return
        running = true
        landmarker = tryCreateLandmarker()
        if (landmarker != null) {
            camera.frameListener = this
        }
        // sem modelo/lib → segue sem hands (gaze cobre a interação)
    }

    fun stop() {
        running = false
        if (camera.frameListener === this) camera.frameListener = null
        try {
            landmarker?.close()
        } catch (_: Exception) {
        }
        landmarker = null
    }

    private fun tryCreateLandmarker(): HandLandmarker? = try {
        val baseOptions = BaseOptions.builder()
            .setModelAssetPath("hand_landmarker.task")
            .build()
        val options = HandLandmarker.HandLandmarkerOptions.builder()
            .setBaseOptions(baseOptions)
            .setRunningMode(RunningMode.LIVE_STREAM)
            .setNumHands(2)
            .setMinHandDetectionConfidence(0.5f)
            .setMinHandPresenceConfidence(0.5f)
            .setMinTrackingConfidence(0.5f)
            .setResultListener { result, _ -> onHandResult(result) }
            .setErrorListener { }
            .build()
        HandLandmarker.createFromOptions(context, options)
    } catch (_: Throwable) {
        // modelo ausente / lib nativa ausente / aparelho sem NEON: hands off
        null
    }

    // ------------------------------------------------------------------
    // Frame da câmera → MediaPipe (detectAsync, ~30 Hz)
    // ------------------------------------------------------------------
    override fun onFrame(bitmap: Bitmap, timestampNs: Long) {
        val lm = landmarker ?: return
        if (timestampNs - lastInferenceNs < 33_000_000L) return
        lastInferenceNs = timestampNs
        try {
            val mpImage = MPImage(bitmap)
            lm.detectAsync(mpImage, timestampNs / 1_000_000L)
        } catch (_: Throwable) {
        }
    }

    // ------------------------------------------------------------------
    // Resultado MediaPipe → landmarks em metros (espaço do mundo)
    // ------------------------------------------------------------------
    private fun onHandResult(result: HandLandmarkerResult) {
        try {
            left.valid = false
            right.valid = false

            val headPose = NativeSdk.getHeadPose()
            val hx = headPose?.getOrNull(3) ?: 0f
            val hy = headPose?.getOrNull(4) ?: 0f
            val hz = headPose?.getOrNull(5) ?: 0f
            val hw = headPose?.getOrNull(7) ?: 1f

            val hands = result.landmarks()
            val handedness = result.handedness()

            for ((handIdx, lms) in hands.withIndex()) {
                if (lms.size < NativeSdk.HAND_LANDMARKS) continue
                // handedness do MediaPipe assume imagem ESPELHADA (selfie);
                // a câmera TRASEIRA não espelha → troca esquerda/direita
                var mirroredRight = handIdx == 0
                val cat = handedness.getOrNull(handIdx)?.firstOrNull()
                val label = cat?.categoryName()
                if (label == "Right") mirroredRight = true
                else if (label == "Left") mirroredRight = false
                // troca: "Right" espelhado = mão ESQUERDA do usuário
                val state = if (mirroredRight) left else right
                fillHand(state, lms, hx, hy, hz, hw)
            }
        } catch (_: Throwable) {
        }
    }

    private fun fillHand(state: HandState, lms: List<com.google.mediapipe.tasks.components.containers.NormalizedLandmark>,
                         hx: Float, hy: Float, hz: Float, hw: Float) {
        val tanHalf = Math.tan(Math.toRadians(fovXDeg * 0.5)).toFloat()
        for (i in 0 until NativeSdk.HAND_LANDMARKS) {
            val lm = lms[i]
            // pinhole: câmera olha -Z do mundo; pixel (nx,ny) origem topo-esq
            val d = handDistance + lm.z * 0.1f
            val cx = (lm.x - 0.5f) * 2f * tanHalf * d
            val cy = -(lm.y - 0.5f) * 2f * tanHalf * d
            val cz = -d

            // câmera→mundo (câmera traseira alinhada à cabeça no VR Box)
            val tx = 2f * (hy * cz - hz * cy)
            val ty = 2f * (hz * cx - hx * cz)
            val tz = 2f * (hx * cy - hy * cx)
            state.landmarks[i * 3] = cx + hw * tx + (hy * tz - hz * ty)
            state.landmarks[i * 3 + 1] = cy + hw * ty + (hz * tx - hx * tz)
            state.landmarks[i * 3 + 2] = cz + hw * tz + (hx * ty - hy * tx)
        }
        state.confidence = 0.9f
        state.valid = true
        state.timestampNs = android.os.SystemClock.elapsedRealtimeNanos()
        classifyGesture(state)
    }

    private fun classifyGesture(s: HandState) {
        fun px(i: Int) = s.landmarks[i * 3]
        fun py(i: Int) = s.landmarks[i * 3 + 1]

        val pinchDist = hypot(
            (px(THUMB_TIP) - px(INDEX_TIP)).toDouble(),
            (py(THUMB_TIP) - py(INDEX_TIP)).toDouble(),
        )
        val palmSize = hypot(
            (px(WRIST) - px(INDEX_MCP)).toDouble(),
            (py(WRIST) - py(INDEX_MCP)).toDouble(),
        ).coerceAtLeast(1e-4)
        val pinchRatio = (pinchDist / palmSize).toFloat()

        fun extended(tip: Int, mcp: Int): Boolean {
            val tipD = hypot((px(tip) - px(WRIST)).toDouble(), (py(tip) - py(WRIST)).toDouble())
            val mcpD = hypot((px(mcp) - px(WRIST)).toDouble(), (py(mcp) - py(WRIST)).toDouble())
            return tipD > mcpD * 1.45
        }
        val indexExt = extended(INDEX_TIP, INDEX_MCP)
        val middleExt = extended(MIDDLE_TIP, 9)
        val ringExt = extended(RING_TIP, 13)
        val pinkyExt = extended(PINKY_TIP, 17)
        val extCount = (if (indexExt) 1 else 0) + (if (middleExt) 1 else 0) +
            (if (ringExt) 1 else 0) + (if (pinkyExt) 1 else 0)

        s.pinch = (1f - (pinchRatio / 0.55f)).coerceIn(0f, 1f)

        val nowMs = s.timestampNs / 1_000_000L
        var swipe = false
        if (s.lastWristTime != 0L && nowMs > s.lastWristTime) {
            val vx = (px(WRIST) - s.lastWristX) / ((nowMs - s.lastWristTime) / 1000f)
            swipe = abs(vx) > 1.2f
        }
        s.lastWristX = px(WRIST)
        s.lastWristTime = nowMs

        s.gesture = when {
            swipe -> NativeSdk.GESTURE_SWIPE
            pinchRatio < 0.35f -> NativeSdk.GESTURE_PINCH
            extCount == 0 -> NativeSdk.GESTURE_FIST
            indexExt && extCount <= 2 -> NativeSdk.GESTURE_POINT
            extCount >= 3 -> {
                val curl = hypot(
                    (px(MIDDLE_TIP) - px(INDEX_MCP)).toDouble(),
                    (py(MIDDLE_TIP) - py(INDEX_MCP)).toDouble(),
                ).toFloat()
                if (curl < palmSize.toFloat() * 1.1f) NativeSdk.GESTURE_GRAB
                else NativeSdk.GESTURE_OPEN_PALM
            }
            else -> NativeSdk.GESTURE_NONE
        }
    }

    /** Chamado no thread GL a cada frame: envia o estado mais recente. */
    fun pushLatestToNative() {
        if (left.valid) {
            NativeSdk.setHandLandmarks(
                true, left.landmarks, left.confidence, left.gesture,
                left.pinch, left.timestampNs,
            )
            pushHandPointer(left)
        }
        if (right.valid) {
            NativeSdk.setHandLandmarks(
                false, right.landmarks, right.confidence, right.gesture,
                right.pinch, right.timestampNs,
            )
            pushHandPointer(right)
        }
    }

    /** Mão apontando/pinchando vira ponteiro (raio índice→mundo). */
    private fun pushHandPointer(s: HandState) {
        if (s.gesture != NativeSdk.GESTURE_POINT && s.gesture != NativeSdk.GESTURE_PINCH) return
        fun px(i: Int) = s.landmarks[i * 3]
        fun py(i: Int) = s.landmarks[i * 3 + 1]
        fun pz(i: Int) = s.landmarks[i * 3 + 2]
        val ox = px(INDEX_MCP); val oy = py(INDEX_MCP); val oz = pz(INDEX_MCP)
        var dx = px(INDEX_TIP) - ox
        var dy = py(INDEX_TIP) - oy
        var dz = pz(INDEX_TIP) - oz
        val len = kotlin.math.sqrt(dx * dx + dy * dy + dz * dz).coerceAtLeast(1e-5f)
        dx /= len; dy /= len; dz /= len
        val ev = if (s.gesture == NativeSdk.GESTURE_PINCH)
            NativeSdk.POINTER_PRESS else NativeSdk.POINTER_HOVER
        NativeSdk.pushPointer(NativeSdk.POINTER_HAND, ox, oy, oz, dx, dy, dz, ev)
    }
}
