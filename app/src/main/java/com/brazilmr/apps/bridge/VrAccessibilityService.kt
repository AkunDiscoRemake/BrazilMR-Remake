/*
 * BrazilMR — Serviço de acessibilidade (injeção de input nos apps).
 *
 * Traduz interações espaciais em toques REAIS no Android:
 *   raycast→touch  (clique na janela = tap na tela)
 *   pinch→tap      (pinça = toque)
 *   drag→swipe     (arrasto na janela = swipe)
 *   controller→virtual touch (Joy-Con move o toque)
 *
 * É a única forma SEM ROOT de injetar input em apps de terceiros
 * (dispatchGesture da AccessibilityService API).
 */
package com.brazilmr.apps.bridge

import android.accessibilityservice.AccessibilityService
import android.accessibilityservice.GestureDescription
import android.graphics.Path
import android.os.SystemClock
import android.view.accessibility.AccessibilityEvent

class VrAccessibilityService : AccessibilityService() {

    companion object {
        @Volatile var instance: VrAccessibilityService? = null
            private set

        fun available(): Boolean = instance != null

        /** Tap em coordenadas de tela. */
        fun tap(x: Float, y: Float, durationMs: Long = 60): Boolean {
            val svc = instance ?: return false
            return svc.dispatch(
                GestureDescription.Builder()
                    .addStroke(stroke(x, y, x, y, 0, durationMs))
                    .build(),
            )
        }

        /** Swipe entre dois pontos (drag na janela espacial). */
        fun swipe(x0: Float, y0: Float, x1: Float, y1: Float,
                  durationMs: Long = 220): Boolean {
            val svc = instance ?: return false
            return svc.dispatch(
                GestureDescription.Builder()
                    .addStroke(stroke(x0, y0, x1, y1, 0, durationMs))
                    .build(),
            )
        }

        private fun stroke(x0: Float, y0: Float, x1: Float, y1: Float,
                           startDelayMs: Long, durationMs: Long): GestureDescription.StrokeDescription {
            val path = Path().apply {
                moveTo(x0, y0)
                lineTo(x1, y1)
            }
            return GestureDescription.StrokeDescription(path, startDelayMs, durationMs)
        }
    }

    override fun onServiceConnected() {
        super.onServiceConnected()
        instance = this
    }

    override fun onAccessibilityEvent(event: AccessibilityEvent?) {
        // não consome eventos: apenas injeta (dispatchGesture)
    }

    override fun onInterrupt() {}

    override fun onDestroy() {
        instance = null
        super.onDestroy()
    }
}
