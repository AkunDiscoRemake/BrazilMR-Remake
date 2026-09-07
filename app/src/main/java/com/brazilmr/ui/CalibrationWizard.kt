/*
 * BrazilMR — Assistente de Calibração (8 passos), 100% dentro do VR.
 *
 * Passos:
 *  1. Posicionamento do headset (verifique selagem/conforto)
 *  2. Lentes (padrão de grid — alinhar centro da lente)
 *  3. IPD (slider até as imagens fundirem)
 *  4. Orientação (horizonte nivelado — recenter automático)
 *  5. Tracking (girar a cabeça, medir jitter)
 *  6. Teste de mãos (se disponível)
 *  7. Teste de controle (toque/gamepad)
 *  8. Entrar no VR (salva config)
 *
 * A UI é um painel espacial nativo; o padrão de calibração (grid + IPD
 * markers) é o modo SBS_CALIBRATION do compositor nativo.
 */
package com.brazilmr.ui

import android.content.Context
import com.brazilmr.BrazilMrApplication
import com.brazilmr.platform.VrOs
import com.brazilmr.sdk.NativeSdk

class CalibrationWizard(
    private val context: Context,
    private val app: BrazilMrApplication,
) {
    private var step = 0
    private var panelId = -1
    private var titleId = -1
    private var bodyId = -1
    private var nextId = -1
    private var skipId = -1
    private var ipdSliderId = -1
    private var working = app.activeProfile

    private val titles = arrayOf(
        "1/8 — Posicione o headset",
        "2/8 — Centre as lentes",
        "3/8 — Ajuste o IPD",
        "4/8 — Nivele o horizonte",
        "5/8 — Teste de tracking",
        "6/8 — Teste de mãos",
        "7/8 — Teste de controle",
        "8/8 — Pronto!",
    )

    private val bodies = arrayOf(
        "Coloque o celular no VR Box e ajuste as alças.\n" +
            "Verifique se o encaixe está firme e o celular centralizado.\n" +
            "Toque em AVANÇAR quando estiver confortável.",
        "Olhe o grid de calibração.\n" +
            "Cada olho deve ver o círculo centrado na sua lente.\n" +
            "Se as bordas distorcerem muito, ajuste a posição do celular.",
        "Arraste o controle até os círculos dos DOIS olhos se fundirem\n" +
            "num único círculo nítido. Se não fundirem, priorize conforto.",
        "Mantenha a cabeça em posição natural de uso\n" +
            "e toque AVANÇAR — o yaw será recentrado agora.",
        "Gire a cabeça devagar: esquerda, direita, cima, baixo.\n" +
            "O cruz deve seguir suavemente, sem tremer.",
        "Estique a mão à frente da câmera traseira.\n" +
            "Se as mãos não aparecerem, o aparelho usa apontador\nde cabeça (toque para continuar).",
        "Toque a tela do headset uma vez.\n" +
            "Com Joy-Con: aperte o botão A.\n" +
            "O anel deve piscar verde.",
        "Calibração salva!\n" +
            "Bem-vindo ao BrazilMR — seu espaço VR.",
    )

    fun begin(os: VrOs) {
        step = 0
        working = app.activeProfile
        os.enterCalibrationMode()

        if (panelId < 0) {
            panelId = NativeSdk.addUiPanel(
                0f, 0.32f, -1.5f,
                0f, 0f, 0f, 1f,
                0.78f, 0.58f, "Calibração",
            )
            titleId = NativeSdk.addUiControl(panelId, NativeSdk.UI_LABEL,
                0.06f, 0.06f, 0.88f, 0.14f, titles[0])
            bodyId = NativeSdk.addUiControl(panelId, NativeSdk.UI_LABEL,
                0.06f, 0.22f, 0.88f, 0.50f, bodies[0])
            ipdSliderId = NativeSdk.addUiControl(panelId, NativeSdk.UI_SLIDER,
                0.06f, 0.58f, 0.60f, 0.07f, "IPD (mm)")
            nextId = NativeSdk.addUiControl(panelId, NativeSdk.UI_BUTTON,
                0.60f, 0.74f, 0.34f, 0.16f, "Avançar")
            skipId = NativeSdk.addUiControl(panelId, NativeSdk.UI_BUTTON,
                0.06f, 0.74f, 0.34f, 0.16f, "Pular")
        }
        NativeSdk.setUiPanelVisible(panelId, true)
        applyStep(os)
    }

    private fun applyStep(os: VrOs) {
        NativeSdk.setUiControlLabel(panelId, titleId, titles[step])
        NativeSdk.setUiControlLabel(panelId, bodyId, bodies[step])
        val showIpd = step == 2
        NativeSdk.setUiControlVisible(panelId, ipdSliderId, showIpd)
        // padrão de grid nos passos de lente/IPD
        NativeSdk.setStereoProfile(
            working.fovYDeg, working.ipdMm, working.k1, working.k2, working.chroma,
            working.vignette, working.renderScale, working.swapEyes,
            if (step == 1 || step == 2) NativeSdk.SBS_CALIBRATION else NativeSdk.SBS_NORMAL,
            working.lensCenterX, working.lensCenterY,
        )
        if (showIpd) {
            NativeSdk.setUiControlValue(
                panelId, ipdSliderId,
                ((working.ipdMm - 52f) / (78f - 52f)).coerceIn(0f, 1f),
            )
        }
    }

    /** Chamado pelo host com eventos UI do painel do wizard. */
    fun onNativeEvent(ev: IntArray, os: VrOs): Boolean {
        if (step !in 0..7) return false
        if (panelId < 0 || ev.size < 6) return false
        val type = ev[0]
        val a = ev[1]
        val b = ev[2]
        if (a != panelId) return false

        when (type) {
            NativeSdk.EVENT_UI_BUTTON -> when (b) {
                nextId -> advance(os)
                skipId -> finish(os)
            }
            NativeSdk.EVENT_UI_SLIDER -> if (b == ipdSliderId) {
                val t = ev[3] / 1000f
                working = working.copy(ipdMm = 52f + t * (78f - 52f))
                NativeSdk.setStereoProfile(
                    working.fovYDeg, working.ipdMm, working.k1, working.k2,
                    working.chroma, working.vignette, working.renderScale,
                    working.swapEyes, NativeSdk.SBS_CALIBRATION,
                    working.lensCenterX, working.lensCenterY,
                )
            }
        }
        return true
    }

    private fun advance(os: VrOs) {
        when (step) {
            3 -> NativeSdk.recenter() // nivela no momento do avanço
        }
        if (step >= 7) {
            finish(os)
        } else {
            step++
            applyStep(os)
        }
    }

    private fun finish(os: VrOs) {
        // restaura modo normal e persiste
        NativeSdk.setStereoProfile(
            working.fovYDeg, working.ipdMm, working.k1, working.k2,
            working.chroma, working.vignette, working.renderScale,
            working.swapEyes, NativeSdk.SBS_NORMAL,
            working.lensCenterX, working.lensCenterY,
        )
        app.applyProfile(working)
        app.markCalibrationDone()
        NativeSdk.setUiPanelVisible(panelId, false)
        os.exitCalibrationMode()
    }
}
