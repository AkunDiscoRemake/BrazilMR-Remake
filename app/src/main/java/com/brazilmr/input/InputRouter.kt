/*
 * BrazilMR — arquitetura unificada de entrada.
 *
 * TODA entrada (gaze de cabeça, mãos, Joy-Con/gamepad, toque na tela)
 * converge em raios 3D (origem+direção) + eventos de ponteiro
 * (hover/press/release/move) empurrados para o nativo, que faz o
 * roteamento espacial: janelas → UI → clique no mundo.
 *
 * O usuário nunca precisa de mouse/teclado/touchpad.
 */
package com.brazilmr.input

import android.view.KeyEvent
import android.view.MotionEvent
import com.brazilmr.sdk.NativeSdk

class InputRouter(private val recenterCallback: () -> Unit) {

    // estado do ponteiro de toque (tela = "gatilho" do VR Box)
    private var touchDown = false
    private var downTimeMs = 0L
    private var longPressFired = false

    // gamepad (Joy-Con L+R aparecem como gamepad HID; suportamos genérico)
    private var stickX = 0f
    private var stickY = 0f

    /** Cor do raio ativo (verde BrasilMR). */
    private val rayColor = floatArrayOf(0f, 0.9f, 0.63f)

    /**
     * Chamado 1× por frame ANTES do render:
     * atualiza o ponteiro de cabeça (gaze) e o raio visual.
     */
    fun frame() {
        val pose = NativeSdk.getHeadPose()
        if (pose == null || pose.size < 12) {
            NativeSdk.setActiveRay(false, 0f, 0f, 0f, 0f, 0f, -1f, 0f, 0f, 0f)
            return
        }
        val px = pose[0]; val py = pose[1]; val pz = pose[2]
        val qx = pose[3]; val qy = pose[4]; val qz = pose[5]; val qw = pose[6]

        // direção de visão: q ⊗ (0,0,-1) ⊗ q*
        val dir = rotate(qx, qy, qz, qw, 0f, 0f, -1f)

        // ponteiro gaze em modo hover/move contínuo
        NativeSdk.pushPointer(
            NativeSdk.POINTER_GAZE, px, py, pz, dir[0], dir[1], dir[2],
            NativeSdk.POINTER_MOVE,
        )

        // raio visual (3 m) — origem levemente à frente para não clipar a cabeça
        NativeSdk.setActiveRay(
            true,
            px + dir[0] * 0.05f, py + dir[1] * 0.05f, pz + dir[2] * 0.05f,
            dir[0], dir[1], dir[2],
            rayColor[0], rayColor[1], rayColor[2],
        )
    }

    /** Toque na tela do headset = clique do ponteiro ativo. */
    fun onScreenTouch(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                touchDown = true
                longPressFired = false
                downTimeMs = android.os.SystemClock.uptimeMillis()
                pressPointer()
                return true
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (touchDown) {
                    val held = android.os.SystemClock.uptimeMillis() - downTimeMs
                    if (held > 650L && !longPressFired) {
                        // toque longo = recenter (sem soltar clique)
                        longPressFired = true
                        recenterCallback()
                    } else if (!longPressFired) {
                        releasePointer()
                    }
                    touchDown = false
                }
                return true
            }
        }
        return false
    }

    private fun pressPointer() {
        withGazeRay { ox, oy, oz, dx, dy, dz ->
            NativeSdk.pushPointer(
                NativeSdk.POINTER_GAZE, ox, oy, oz, dx, dy, dz,
                NativeSdk.POINTER_PRESS,
            )
        }
    }

    private fun releasePointer() {
        withGazeRay { ox, oy, oz, dx, dy, dz ->
            NativeSdk.pushPointer(
                NativeSdk.POINTER_GAZE, ox, oy, oz, dx, dy, dz,
                NativeSdk.POINTER_RELEASE,
            )
        }
    }

    private inline fun withGazeRay(block: (Float, Float, Float, Float, Float, Float) -> Unit) {
        val pose = NativeSdk.getHeadPose() ?: return
        if (pose.size < 12) return
        val d = rotate(pose[3], pose[4], pose[5], pose[6], 0f, 0f, -1f)
        block(pose[0], pose[1], pose[2], d[0], d[1], d[2])
    }

    /** Botões físicos: volume-down = recenter, gamepad A = clique. */
    fun onKeyDown(keyCode: Int): Boolean {
        when (keyCode) {
            KeyEvent.KEYCODE_VOLUME_DOWN, KeyEvent.KEYCODE_VOLUME_UP -> {
                recenterCallback()
                return true
            }
            KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_DPAD_CENTER,
            KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_BUTTON_B -> {
                pressPointer()
                return true
            }
        }
        return false
    }

    fun onKeyUp(keyCode: Int): Boolean {
        when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_DPAD_CENTER,
            KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_BUTTON_B -> {
                releasePointer()
                return true
            }
        }
        return false
    }

    /** Eixos de gamepad (Joy-Con: stick esquerdo = mover ponteiro fino). */
    fun onGenericMotion(event: MotionEvent): Boolean {
        if (event.sources and android.view.InputDevice.SOURCE_CLASS_JOYSTICK == 0) return false
        stickX = event.getAxisValue(MotionEvent.AXIS_X)
        stickY = event.getAxisValue(MotionEvent.AXIS_Y)
        return true
    }

    // ------------------------------------------------------------------
    private fun rotate(qx: Float, qy: Float, qz: Float, qw: Float,
                       vx: Float, vy: Float, vz: Float): FloatArray {
        // v' = q v q*  (q unitário)
        val tx = 2f * (qy * vz - qz * vy)
        val ty = 2f * (qz * vx - qx * vz)
        val tz = 2f * (qx * vy - qy * vx)
        return floatArrayOf(
            vx + qw * tx + (qy * tz - qz * ty),
            vy + qw * ty + (qz * tx - qx * tz),
            vz + qw * tz + (qx * ty - qy * tx),
        )
    }
}
