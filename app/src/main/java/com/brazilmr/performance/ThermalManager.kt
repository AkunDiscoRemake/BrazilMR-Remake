/*
 * BrazilMR — gerenciador térmico.
 *
 * Monitora (via BatteryManager) temperatura da bateria, throttling status
 * do sistema e FPS do render, e alimenta o orçamento térmico do motor
 * nativo (0..1). O nativo aplica a ESCADA de degradação:
 *   res → brilho das mãos → partículas → efeitos de janela → detalhe do céu
 * Escada completa e thresholds estão no ThermalManager.hpp nativo.
 */
package com.brazilmr.performance

import android.content.Context
import android.os.BatteryManager
import android.os.Build
import android.os.PowerManager
import com.brazilmr.sdk.NativeSdk

class ThermalManager(private val context: Context) {

    private var running = false
    private var lastTickNs = 0L
    private var emaBudget = 1.0f

    /** Últimas leituras (para o painel de status do VR OS). */
    @Volatile var batteryTempC: Float = 30f
        private set
    @Volatile var systemThrottling: Boolean = false
        private set
    @Volatile var fps: Float = 60f
        private set
    @Volatile var budget: Float = 1f
        private set

    fun start() {
        running = true
        lastTickNs = 0L
    }

    fun stop() {
        running = false
    }

    /** Chamado a cada frame pelo renderer; amostra ~1×/s. */
    fun tick() {
        if (!running) return
        val now = android.os.SystemClock.elapsedRealtimeNanos()
        if (lastTickNs != 0L && now - lastTickNs < 1_000_000_000L) return
        lastTickNs = now

        val bm = context.getSystemService(Context.BATTERY_SERVICE) as BatteryManager
        val temp = bm.getIntProperty(BatteryManager.BATTERY_PROPERTY_TEMPERATURE) / 10f
        if (temp > 0f) batteryTempC = temp

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val pm = context.getSystemService(Context.POWER_SERVICE) as PowerManager
            systemThrottling = pm.getCurrentThermalStatus() >
                PowerManager.THERMAL_STATUS_NONE
        }

        // FPS reportado pelo nativo
        NativeSdk.getStats()?.let { s -> if (s.isNotEmpty()) fps = s[0] }

        // orçamento 1.0 (frio) → 0.0 (crítico)
        val tempBudget = when {
            batteryTempC < 36f -> 1.0f
            batteryTempC < 39f -> 0.85f
            batteryTempC < 41f -> 0.65f
            batteryTempC < 43f -> 0.45f
            else -> 0.25f
        }
        val fpsBudget = when {
            fps >= 55f -> 1.0f
            fps >= 45f -> 0.8f
            fps >= 30f -> 0.6f
            else -> 0.4f
        }
        val throttleBudget = if (systemThrottling) 0.5f else 1.0f

        val target = minOf(tempBudget, fpsBudget, throttleBudget)
        // suaviza (evita pular degraus bruscamente)
        emaBudget += (target - emaBudget) * 0.25f
        budget = emaBudget

        NativeSdk.setThermalBudget(budget)
    }
}
