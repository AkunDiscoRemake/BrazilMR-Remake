/*
 * BrazilMR — Navegador VR.
 *
 * WebView renderizada OFFSCREEN (software) → bitmap → textura GL →
 * janela espacial CONTENT_BROWSER. A navegação acontece toda no espaço
 * 3D: barra de endereço espacial + teclado espacial; páginas viram
 * painéis flutuantes com profundidade. Nenhum browser 2D do Android.
 *
 * Input: eventos WINDOW_INPUT(u,v) da janela → MotionEvents sintéticos
 * no WebView (escala uv → pixels).
 */
package com.brazilmr.browser

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.os.Handler
import android.os.Looper
import android.view.MotionEvent
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import com.brazilmr.sdk.NativeSdk
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.withLock

@SuppressLint("SetJavaScriptEnabled")
class VrBrowser(
    private val context: Context,
    private val keyboard: SpatialKeyboard,
) {
    companion object {
        const val WINDOW_ID = 100
        const val TEX_W = 1024
        const val TEX_H = 640
        private const val REDRAW_HZ = 15
    }

    private val main = Handler(Looper.getMainLooper())
    private var webView: WebView? = null
    private var canvas: Canvas? = null

    // double-buffer de bitmap: main desenha no back, GL publica o front
    private val bitmapBack = Bitmap.createBitmap(TEX_W, TEX_H, Bitmap.Config.ARGB_8888)
    private val bitmapFront = Bitmap.createBitmap(TEX_W, TEX_H, Bitmap.Config.ARGB_8888)
    private val bitmapLock = ReentrantLock()
    private val dirty = AtomicBoolean(false)

    private var textureId = 0
    private var windowCreated = false
    private var addressPanelId = -1
    private var lastRedrawMs = 0L
    private var pageTitle = ""
    private var started = false

    /** Ids dos botões da barra (Digitar, Voltar, Frente, Fechar). */
    private val addressButtonIds = mutableListOf<Int>()

    val addressPanel: Int get() = addressPanelId

    // histórico / favoritos (simples, em memória + persistido levemente)
    private val history = mutableListOf<String>()
    private var homeUrl = "https://duckduckgo.com"

    fun start(initialUrl: String? = null) {
        if (started) {
            NativeSdk.updateWindowFlags(WINDOW_ID, true, true, 5, textureId)
            NativeSdk.setUiPanelVisible(addressPanelId, true)
            return
        }
        started = true
        main.post {
            WebView.enableSlowWholeDocumentDraw()
            val wv = WebView(context)
            wv.layoutParams = android.view.ViewGroup.LayoutParams(TEX_W, TEX_H)
            wv.layout(0, 0, TEX_W, TEX_H)
            setupSettings(wv.settings)
            wv.webViewClient = object : WebViewClient() {
                override fun shouldOverrideUrlLoading(view: WebView, request: WebResourceRequest): Boolean {
                    return false // navega dentro do WebView
                }

                override fun onPageFinished(view: WebView, url: String) {
                    pageTitle = view.title ?: url
                    history.add(url)
                }
            }
            wv.webChromeClient = WebChromeClient()
            wv.loadUrl(initialUrl ?: homeUrl)
            webView = wv
        }
        if (textureId == 0) {
            val tex = IntArray(1)
            android.opengl.GLES30.glGenTextures(1, tex, 0)
            textureId = tex[0]
        }
        createSpatialWindow()
        keyboard.onSubmit = { text -> onAddressEntered(text) }
        showAddressBar()
    }

    @SuppressLint("SetJavaScriptEnabled")
    private fun setupSettings(s: WebSettings) {
        s.javaScriptEnabled = true
        s.domStorageEnabled = true
        s.loadWithOverviewMode = true
        s.useWideViewPort = true
        s.builtInZoomControls = false
        s.displayZoomControls = false
        s.mediaPlaybackRequiresUserGesture = false
        s.cacheMode = WebSettings.LOAD_DEFAULT
    }

    private fun createSpatialWindow() {
        if (windowCreated) return
        val id = NativeSdk.createWindow(
            WINDOW_ID,
            -0.1f, 0.05f, -2.1f,       // à frente e acima do centro
            0f, 0f, 0f, 1f,
            1.15f, 0.72f,               // ~16:10
            NativeSdk.CONTENT_BROWSER,
        )
        if (id >= 0) {
            windowCreated = true
            NativeSdk.updateWindowFlags(WINDOW_ID, true, true, 5, textureId)
        }
    }

    private fun showAddressBar() {
        if (addressPanelId < 0) {
            addressPanelId = NativeSdk.addUiPanel(
                -0.1f, 0.62f, -1.9f,
                0f, 0f, 0f, 1f,
                0.9f, 0.16f, "Endereço",
            )
            for ((i, label) in listOf("Digitar", "Voltar", "Frente", "Fechar").withIndex()) {
                addressButtonIds.add(
                    NativeSdk.addUiControl(addressPanelId, NativeSdk.UI_BUTTON,
                        0.02f + i * 0.24f, 0.15f, 0.22f, 0.7f, label),
                )
            }
        }
        NativeSdk.setUiPanelVisible(addressPanelId, true)
    }

    // ------------------------------------------------------------------
    // Loop de render (chamado pelo VrActivity no thread GL)
    // ------------------------------------------------------------------
    fun frame(nowMs: Long) {
        if (webView == null) return
        if (nowMs - lastRedrawMs < 1000 / REDRAW_HZ) return
        lastRedrawMs = nowMs

        // desenha o WebView no bitmap back (thread principal)
        main.post {
            val wv = webView ?: return@post
            bitmapBack.eraseColor(android.graphics.Color.DKGRAY)
            if (canvas == null) canvas = Canvas(bitmapBack)
            try {
                wv.draw(canvas)
            } catch (_: Exception) {
            }
            bitmapLock.withLock {
                bitmapFront.eraseColor(0)
                bitmapFront.drawBitmap(bitmapBack, 0f, 0f, null)
            }
            dirty.set(true)
        }

        if (dirty.compareAndSet(true, false)) {
            android.opengl.GLES30.glBindTexture(android.opengl.GLES30.GL_TEXTURE_2D, textureId)
            bitmapLock.withLock {
                android.opengl.GLUtils.texImage2D(
                    android.opengl.GLES30.GL_TEXTURE_2D, 0, bitmapFront, 0,
                )
            }
            android.opengl.GLES30.glTexParameteri(
                android.opengl.GLES30.GL_TEXTURE_2D,
                android.opengl.GLES30.GL_TEXTURE_MIN_FILTER,
                android.opengl.GLES30.GL_LINEAR,
            )
            android.opengl.GLES30.glTexParameteri(
                android.opengl.GLES30.GL_TEXTURE_2D,
                android.opengl.GLES30.GL_TEXTURE_MAG_FILTER,
                android.opengl.GLES30.GL_LINEAR,
            )
            android.opengl.GLES30.glBindTexture(android.opengl.GLES30.GL_TEXTURE_2D, 0)
        }
    }

    // ------------------------------------------------------------------
    // Input: janela (u,v) → WebView
    // ------------------------------------------------------------------
    fun onWindowInput(u: Float, v: Float, pointerEvent: Int) {
        val wv = webView ?: return
        val x = u.coerceIn(0f, 1f) * TEX_W
        val y = v.coerceIn(0f, 1f) * TEX_H
        val now = android.os.SystemClock.uptimeMillis()
        val action = when (pointerEvent) {
            NativeSdk.POINTER_PRESS -> MotionEvent.ACTION_DOWN
            NativeSdk.POINTER_RELEASE -> MotionEvent.ACTION_UP
            NativeSdk.POINTER_MOVE -> MotionEvent.ACTION_MOVE
            else -> MotionEvent.ACTION_HOVER_MOVE
        }
        main.post {
            @Suppress("DEPRECATION")
            wv.dispatchTouchEvent(
                MotionEvent.obtain(now, now, action, x, y, 0),
            )
        }
    }

    fun onAddressEntered(text: String) {
        val url = if (text.startsWith("http") || text.contains(' ') || !text.contains('.'))
            text else "https://$text"
        main.post { webView?.loadUrl(url) }
    }

    /** Consome eventos da barra de endereço. Retorna true se consumiu. */
    fun onNativeEvent(ev: IntArray): Boolean {
        if (addressPanelId < 0 || ev.size < 6) return false
        if (ev[1] != addressPanelId || ev[0] != NativeSdk.EVENT_UI_BUTTON) return false
        val idx = addressButtonIds.indexOf(ev[2])
        if (idx >= 0) {
            onAddressButton(idx)
            return true
        }
        return false
    }

    /** Eventos dos botões da barra de endereço (index conforme criado). */
    fun onAddressButton(controlIndex: Int) {
        when (controlIndex) {
            0 -> keyboard.toggle()                    // Digitar
            1 -> main.post { webView?.goBack() }      // Voltar
            2 -> main.post { webView?.goForward() }   // Frente
            3 -> close()                              // Fechar
        }
    }

    fun close() {
        started = false
        windowCreated = false
        NativeSdk.removeWindow(WINDOW_ID)
        NativeSdk.setUiPanelVisible(addressPanelId, false)
        keyboard.hide()
        main.post { webView?.destroy(); webView = null }
    }
}
