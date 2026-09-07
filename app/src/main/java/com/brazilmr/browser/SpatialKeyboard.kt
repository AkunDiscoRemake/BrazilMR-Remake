/*
 * BrazilMR — Teclado espacial.
 *
 * Grade de botões nativos (UI_BUTTON) num painel flutuante — alvos
 * grandes (mínimo ~2 cm no espaço), layout QWERTY + acentos PT-BR.
 * Usado pelo navegador (barra de endereço) e pelo console Lua.
 */
package com.brazilmr.browser

import com.brazilmr.sdk.NativeSdk

class SpatialKeyboard {

    var onSubmit: ((String) -> Unit)? = null

    private var panelId = -1
    private val keyIds = mutableListOf<Int>()
    private val keyChars = mutableListOf<String>()
    private var displayId = -1
    private var backspaceId = -1
    private var enterId = -1
    private var shiftId = -1
    private var spaceId = -1
    private var clearId = -1
    private var shiftOn = false
    private var buffer = StringBuilder()
    private var visible = false

    private val rows = listOf(
        "1234567890",
        "qwertyuiop",
        "asdfghjklç",
        "zxcvbnm,.-",
    )

    fun toggle() = if (visible) hide() else show()

    fun show() {
        if (panelId < 0) build()
        NativeSdk.setUiPanelVisible(panelId, true)
        visible = true
    }

    fun hide() {
        if (panelId >= 0) NativeSdk.setUiPanelVisible(panelId, false)
        visible = false
    }

    private fun build() {
        panelId = NativeSdk.addUiPanel(
            0f, -0.62f, -1.35f,
            0f, 0f, 0f, 1f,
            1.0f, 0.46f, "Teclado",
        )
        if (panelId < 0) return

        var y = 0.06f
        val rowH = 0.155f
        for (row in rows) {
            var x = 0.02f
            val keyW = 0.9f / row.length
            for (ch in row) {
                val id = NativeSdk.addUiControl(
                    panelId, NativeSdk.UI_BUTTON,
                    x, y, keyW * 0.92f, rowH, labelFor(ch.toString()),
                )
                keyIds.add(id)
                keyChars.add(ch.toString())
                x += keyW
            }
            y += rowH + 0.015f
        }
        spaceId = NativeSdk.addUiControl(panelId, NativeSdk.UI_BUTTON,
            0.02f, y, 0.44f, rowH, "espaço")
        shiftId = NativeSdk.addUiControl(panelId, NativeSdk.UI_BUTTON,
            0.50f, y, 0.13f, rowH, "↑")
        backspaceId = NativeSdk.addUiControl(panelId, NativeSdk.UI_BUTTON,
            0.65f, y, 0.13f, rowH, "⌫")
        clearId = NativeSdk.addUiControl(panelId, NativeSdk.UI_BUTTON,
            0.80f, y, 0.08f, rowH, "C")
        enterId = NativeSdk.addUiControl(panelId, NativeSdk.UI_BUTTON,
            0.90f, y, 0.08f, rowH, "OK")
        displayId = NativeSdk.addUiControl(panelId, NativeSdk.UI_LABEL,
            0.02f, 0.80f, 0.96f, 0.14f, "")
    }

    private fun labelFor(ch: String): String =
        if (shiftOn) ch.uppercase() else ch

    private fun refresh() {
        NativeSdk.setUiControlLabel(panelId, displayId, buffer.toString())
    }

    /** Roteia evento UI do teclado. Retorna true se consumiu. */
    fun onNativeEvent(ev: IntArray): Boolean {
        if (panelId < 0 || ev.size < 6 || ev[1] != panelId) return false
        if (ev[0] != NativeSdk.EVENT_UI_BUTTON) return false
        val b = ev[2]

        when {
            b == backspaceId -> {
                if (buffer.isNotEmpty()) buffer.deleteCharAt(buffer.length - 1)
                refresh()
            }
            b == enterId -> {
                onSubmit?.invoke(buffer.toString())
                buffer.clear()
                refresh()
                hide()
            }
            b == spaceId -> {
                buffer.append(' ')
                refresh()
            }
            b == shiftId -> {
                shiftOn = !shiftOn
                NativeSdk.setUiControlLabel(panelId, shiftId, if (shiftOn) "⇧" else "↑")
            }
            b == clearId -> {
                buffer.clear()
                refresh()
            }
            else -> {
                val idx = keyIds.indexOf(b)
                if (idx >= 0) {
                    buffer.append(if (shiftOn) keyChars[idx].uppercase() else keyChars[idx])
                    if (shiftOn) {
                        shiftOn = false
                        NativeSdk.setUiControlLabel(panelId, shiftId, "↑")
                    }
                    refresh()
                } else return false
            }
        }
        return true
    }
}
