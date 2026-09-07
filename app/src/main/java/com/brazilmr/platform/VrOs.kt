/*
 * BrazilMR — o "sistema operacional" VR.
 *
 * Constrói o ambiente espacial inicial e roteia eventos do nativo:
 *   - Painel Principal (menu): Navegador, Aplicativos, Calibração,
 *     SDK Lua, Recentrar
 *   - Painel de Status (canto superior): backend de tracking, FPS, térmico
 *   - Logo GLB animado + objetos de demonstração com física
 *   - Biblioteca de apps / navegador / teclado espacial sob demanda
 *
 * O usuário vive AQUI — nunca numa UI 2D do Android.
 */
package com.brazilmr.platform

import android.content.Context
import com.brazilmr.BrazilMrApplication
import com.brazilmr.sdk.NativeSdk
import com.brazilmr.ui.CalibrationWizard

class VrOs(
    private val context: Context,
    private val app: BrazilMrApplication,
) {
    interface Host {
        fun openBrowser()
        fun openAppLibrary()
        fun openLuaConsole()
        fun openKeyboard(onDone: (String) -> Unit)
    }

    var host: Host? = null

    // painéis
    private var menuPanelId = -1
    private var statusPanelId = -1
    private var calibrationPanelId = -1

    private var ctlApps = -1
    private var ctlBrowser = -1
    private var ctlCalibration = -1
    private var ctlLua = -1
    private var ctlRecenter = -1
    private var ctlMenuVisible = -1 // toggle no painel de status
    private var ctlStatusBackend = -1
    private var ctlStatusFps = -1
    private var ctlStatusThermal = -1

    private var logoHandle = 0L
    private val demoObjects = mutableListOf<Long>()

    private var wizard: CalibrationWizard? = null
    private var statusTicks = 0L

    // ------------------------------------------------------------------
    // Cena inicial
    // ------------------------------------------------------------------
    fun buildScene() {
        buildMenuPanel()
        buildStatusPanel()
        loadLogo()
        spawnDemoObjects()
    }

    private fun buildMenuPanel() {
        menuPanelId = NativeSdk.addUiPanel(
            0.0f, 0.15f, -1.8f,
            0f, 0f, 0f, 1f,
            0.62f, 0.52f, "BrazilMR",
        )
        if (menuPanelId < 0) return

        ctlApps = NativeSdk.addUiControl(menuPanelId, NativeSdk.UI_BUTTON,
            0.08f, 0.12f, 0.84f, 0.13f, "Aplicativos")
        ctlBrowser = NativeSdk.addUiControl(menuPanelId, NativeSdk.UI_BUTTON,
            0.08f, 0.29f, 0.84f, 0.13f, "Navegador VR")
        ctlCalibration = NativeSdk.addUiControl(menuPanelId, NativeSdk.UI_BUTTON,
            0.08f, 0.46f, 0.84f, 0.13f,
            if (app.calibrationDone) "Calibração" else "Calibração (inicial)")
        ctlLua = NativeSdk.addUiControl(menuPanelId, NativeSdk.UI_BUTTON,
            0.08f, 0.63f, 0.84f, 0.13f, "SDK Lua")
        ctlRecenter = NativeSdk.addUiControl(menuPanelId, NativeSdk.UI_BUTTON,
            0.08f, 0.80f, 0.84f, 0.13f, "Recentrar vista")
    }

    private fun buildStatusPanel() {
        statusPanelId = NativeSdk.addUiPanel(
            -0.55f, 0.62f, -1.9f,
            0f, 0f, 0f, 1f,
            0.44f, 0.20f, "Status",
        )
        if (statusPanelId < 0) return
        ctlStatusBackend = NativeSdk.addUiControl(statusPanelId, NativeSdk.UI_LABEL,
            0.06f, 0.15f, 0.88f, 0.22f, "Tracking: IMU 3DoF")
        ctlStatusFps = NativeSdk.addUiControl(statusPanelId, NativeSdk.UI_LABEL,
            0.06f, 0.40f, 0.88f, 0.22f, "FPS: --")
        ctlStatusThermal = NativeSdk.addUiControl(statusPanelId, NativeSdk.UI_LABEL,
            0.06f, 0.65f, 0.88f, 0.22f, "Térmico: OK")
    }

    private fun loadLogo() {
        try {
            val bytes = context.assets.open("models/brazilmr_logo.glb").use { it.readBytes() }
            logoHandle = NativeSdk.loadGlb(bytes)
            if (logoHandle != 0L) {
                NativeSdk.setModelTransform(
                    logoHandle,
                    0.55f, -0.05f, -1.75f,   // à direita do menu
                    0f, 0f, 0f, 1f,
                    0.28f, 0.28f, 0.28f,     // escala
                    true,
                )
                if (NativeSdk.modelAnimationCount(logoHandle) > 0) {
                    NativeSdk.setModelAnimation(logoHandle, 0, true)
                }
            }
        } catch (_: Exception) {
            // sem asset: cena segue sem logo
        }
    }

    private fun spawnDemoObjects() {
        // esferas com física nativa (gravidade + colisão esférica)
        val colors = arrayOf(
            floatArrayOf(0f, 0.9f, 0.63f, 1f),
            floatArrayOf(0.95f, 0.75f, 0.1f, 1f),
            floatArrayOf(0.35f, 0.55f, 0.95f, 1f),
        )
        for (i in 0 until 3) {
            val id = NativeSdk.spawnSphere(
                -0.5f + i * 0.35f, 1.4f + i * 0.25f, -2.2f,
                0.07f,
            )
            if (id != 0L) {
                NativeSdk.setObjectColor(id, colors[i][0], colors[i][1], colors[i][2], 1f)
                demoObjects.add(id)
            }
        }
    }

    // ------------------------------------------------------------------
    // Eventos do nativo
    // ------------------------------------------------------------------
    fun onNativeEvent(ev: IntArray) {
        if (ev.size < 6) return
        // o wizard tem painel próprio e consome os eventos dele
        if (wizard?.onNativeEvent(ev, this) == true) return
        val type = ev[0]
        val a = ev[1]
        val b = ev[2]
        when (type) {
            NativeSdk.EVENT_UI_BUTTON -> onMenuButton(a, b)
            NativeSdk.EVENT_UI_PANEL_FOCUS -> { /* painel ganhou foco */ }
            NativeSdk.EVENT_GESTURE -> onGesture(a, b, ev[3] / 1000f)
        }
    }

    private fun onMenuButton(panelId: Int, controlId: Int) {
        when (controlId) {
            ctlApps -> host?.openAppLibrary()
            ctlBrowser -> host?.openBrowser()
            ctlCalibration -> startCalibration()
            ctlLua -> host?.openLuaConsole()
            ctlRecenter -> NativeSdk.recenter()
        }
    }

    private fun onGesture(hand: Int, gesture: Int, strength: Float) {
        when (gesture) {
            NativeSdk.GESTURE_OPEN_PALM -> toggleMenu()
            NativeSdk.GESTURE_GRAB -> host?.openAppLibrary()
        }
    }

    fun toggleMenu() {
        menuVisible = !menuVisible
        NativeSdk.setUiPanelVisible(menuPanelId, menuVisible)
    }

    private var menuVisible = true

    // ------------------------------------------------------------------
    // Calibração (wizard 8 passos)
    // ------------------------------------------------------------------
    fun startCalibration() {
        if (wizard == null) wizard = CalibrationWizard(context, app)
        wizard?.begin(this)
    }

    // ------------------------------------------------------------------
    // Atualização de status (~2 Hz)
    // ------------------------------------------------------------------
    fun tickStatus(backendName: String, fps: Float, thermalBudget: Float) {
        if (statusPanelId < 0) return
        if (++statusTicks % 2 != 0L) return
        NativeSdk.setUiControlLabel(
            statusPanelId, ctlStatusBackend,
            "Tracking: $backendName",
        )
        NativeSdk.setUiControlLabel(
            statusPanelId, ctlStatusFps,
            "FPS: ${fps.toInt()}",
        )
        val therm = when {
            thermalBudget > 0.8f -> "OK"
            thermalBudget > 0.5f -> "Morno"
            else -> "Throttling"
        }
        NativeSdk.setUiControlLabel(
            statusPanelId, ctlStatusThermal,
            "Térmico: $therm (${(thermalBudget * 100).toInt()}%)",
        )
    }

    /** Painel usado pelo wizard (padrão de calibração SBS). */
    fun enterCalibrationMode() {
        NativeSdk.setUiPanelVisible(menuPanelId, false)
        NativeSdk.setUiPanelVisible(statusPanelId, false)
    }

    fun exitCalibrationMode() {
        NativeSdk.setUiPanelVisible(menuPanelId, true)
        NativeSdk.setUiPanelVisible(statusPanelId, true)
    }

    val menuPanel: Int get() = menuPanelId
}
