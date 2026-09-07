/*
 * BrazilMR — Biblioteca de aplicativos.
 *
 * Lista apps launchable do aparelho (nome, ícone, pacote) e os apresenta
 * como ÍCONES nativos num painel espacial. Tocar um ícone → AppBridge
 * inicia o app e o projeta numa janela espacial.
 *
 * Aqui NÃO existe "launcher 2D": a biblioteca é um painel 3D com
 * profundidade, e os apps vivem em janelas flutuantes no espaço.
 */
package com.brazilmr.apps

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.drawable.Drawable
import com.brazilmr.sdk.NativeSdk

class AppLibrary(private val context: Context) {

    data class AppEntry(
        val label: String,
        val packageName: String,
        val activityName: String,
        val iconTexId: Int,
    )

    var onLaunch: ((AppEntry) -> Unit)? = null

    private var panelId = -1
    private val entries = mutableListOf<AppEntry>()
    private val entryControlIds = mutableListOf<Int>()
    private val pages = 12 // ícones por página
    private var page = 0
    private var prevId = -1
    private var nextId = -1
    private var closeId = -1

    fun toggle() = if (panelId >= 0) close() else open()

    fun open() {
        if (panelId < 0) {
            loadApps()
            buildPanel()
        }
        NativeSdk.setUiPanelVisible(panelId, true)
    }

    fun close() {
        if (panelId >= 0) NativeSdk.setUiPanelVisible(panelId, false)
    }

    private fun loadApps() {
        val pm = context.packageManager
        val intent = Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER)
        val resolved = pm.queryIntentActivities(intent, 0)
            .sortedBy { it.loadLabel(pm).toString().lowercase() }

        for (ri in resolved) {
            val label = ri.loadLabel(pm).toString()
            val pkg = ri.activityInfo.packageName
            if (pkg == context.packageName) continue // o BrazilMR já está rodando
            val icon = drawableToBitmap(ri.loadIcon(pm), 96)
            val tex = NativeSdk.uploadBitmap(icon)
            icon.recycle()
            entries.add(
                AppEntry(label, pkg, ri.activityInfo.name, tex),
            )
            if (entries.size >= 60) break // biblioteca inicial razoável
        }
    }

    private fun buildPanel() {
        panelId = NativeSdk.addUiPanel(
            0f, 0.0f, -1.9f,
            0f, 0f, 0f, 1f,
            1.1f, 0.75f, "Aplicativos",
        )
        if (panelId < 0) return
        prevId = NativeSdk.addUiControl(panelId, NativeSdk.UI_BUTTON,
            0.02f, 0.85f, 0.2f, 0.12f, "◀")
        nextId = NativeSdk.addUiControl(panelId, NativeSdk.UI_BUTTON,
            0.26f, 0.85f, 0.2f, 0.12f, "▶")
        closeId = NativeSdk.addUiControl(panelId, NativeSdk.UI_BUTTON,
            0.78f, 0.85f, 0.2f, 0.12f, "Fechar")
        showPage(0)
    }

    private fun showPage(p: Int) {
        // remove controles da página anterior
        for (id in entryControlIds) NativeSdk.setUiControlVisible(panelId, id, false)
        entryControlIds.clear()
        val lastPage = ((entries.size + pages - 1) / pages - 1).coerceAtLeast(0)
        page = p.coerceIn(0, lastPage)

        val start = page * pages
        val end = minOf(start + pages, entries.size)
        val cols = 4
        val cw = 0.22f
        val ch = 0.25f
        for (i in start until end) {
            val e = entries[i]
            val idx = i - start
            val col = idx % cols
            val row = idx / cols
            val id = NativeSdk.addUiControl(
                panelId, NativeSdk.UI_ICON,
                0.05f + col * cw, 0.08f + row * ch,
                cw * 0.9f, ch * 0.8f,
                e.label, 0f, e.iconTexId,
            )
            entryControlIds.add(id)
        }
    }

    /** Roteia eventos UI da biblioteca. Retorna true se consumiu. */
    fun onNativeEvent(ev: IntArray): Boolean {
        if (panelId < 0 || ev.size < 6 || ev[1] != panelId) return false
        if (ev[0] != NativeSdk.EVENT_UI_BUTTON) return false
        val b = ev[2]
        when (b) {
            prevId -> showPage(page - 1)
            nextId -> showPage(page + 1)
            closeId -> close()
            else -> {
                val idx = entryControlIds.indexOf(b)
                if (idx >= 0) {
                    val entry = entries[page * pages + idx]
                    onLaunch?.invoke(entry)
                    close()
                }
            }
        }
        return true
    }

    private fun drawableToBitmap(d: Drawable, sizePx: Int): Bitmap {
        val bmp = Bitmap.createBitmap(sizePx, sizePx, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bmp)
        d.setBounds(0, 0, sizePx, sizePx)
        d.draw(canvas)
        return bmp
    }
}
