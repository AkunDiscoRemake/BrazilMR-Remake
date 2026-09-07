/*
 * BrazilMR — Activity principal.
 *
 * ÚNICA activity do sistema. Fluxo:
 *   1. Pré-voo (overlay escuro mínimo): consentimentos obrigatórios do
 *      Android (câmera, projeção de mídia) — somente na 1ª execução.
 *   2. Modo VR: GLSurfaceView fullscreen SBS em modo imersivo sticky.
 *      A partir daqui TODA a interação acontece no espaço 3D.
 *
 * Nenhuma UI 2D é usada após o modo VR iniciar (regra da plataforma).
 */
package com.brazilmr.platform

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.os.SystemClock
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import com.brazilmr.BrazilMrApplication
import com.brazilmr.apps.AppBridge
import com.brazilmr.browser.SpatialKeyboard
import com.brazilmr.browser.VrBrowser
import com.brazilmr.camera.Camera2Engine
import com.brazilmr.diagnostics.CrashReporter
import com.brazilmr.hands.HandTracking
import com.brazilmr.input.InputRouter
import com.brazilmr.performance.ThermalManager
import com.brazilmr.sdk.NativeSdk
import com.brazilmr.sdk.lua.LuaSdk
import com.brazilmr.tracking.ImuTracker
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class VrActivity : Activity(), VrOs.Host {

    private lateinit var glView: GLSurfaceView
    private lateinit var renderer: VrRenderer
    private lateinit var imu: ImuTracker
    private lateinit var camera: Camera2Engine
    private lateinit var hands: HandTracking
    private lateinit var thermal: ThermalManager
    private lateinit var input: InputRouter
    private lateinit var os: VrOs
    private lateinit var keyboard: SpatialKeyboard
    private lateinit var browser: VrBrowser
    private lateinit var appBridge: AppBridge
    private lateinit var lua: LuaSdk

    private var vrStarted = false

    // ------------------------------------------------------------------
    // Ciclo de vida
    // ------------------------------------------------------------------
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.attributes.screenBrightness = 1.0f
        enterImmersive()

        // A execução anterior derrubou o processo? Oferece o relatório
        // (fase de pré-voo, antes do VR — consentimento do usuário).
        if (CrashReporter.hasPendingReports()) offerCrashReport()

        try {
            glView = GLSurfaceView(this)
            glView.setEGLContextClientVersion(3)
            // Config EGL EXPLÍCITA: o chooser padrão do GLSurfaceView pede
            // RGB565+depth16 — drivers recentes (Android 15/16, ARMv9) podem
            // NÃO ter mais configs 565 e o contexto EGL falha derrubando o
            // app na abertura ("createContext failed"). RGBA8888+depth24 é
            // universal em ES3.
            glView.setEGLConfigChooser(8, 8, 8, 0, 24, 0)
            glView.preserveEGLContextOnPause = true

            imu = ImuTracker(this)
            thermal = ThermalManager(this)
            camera = Camera2Engine(this)
            hands = HandTracking(this, camera)
            keyboard = SpatialKeyboard()
            os = VrOs(this, application as BrazilMrApplication)
            input = InputRouter { recenter() }
            browser = VrBrowser(this, keyboard)
            appBridge = AppBridge(this)
            lua = LuaSdk(this)
            renderer = VrRenderer(this, os, imu, hands, thermal, input,
                browser, appBridge, lua)

            glView.setRenderer(renderer)
            glView.renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
            setContentView(glView)
        } catch (t: Throwable) {
            // qualquer falha de construção agora vira relatório + tela de
            // diagnóstico em vez de crash silencioso
            CrashReporter.recordError("onCreate", t)
            showStartupFailure(t)
        }
    }

    override fun onResume() {
        super.onResume()
        enterImmersive()
        if (::glView.isInitialized) glView.onResume()
        if (vrStarted) {
            imu.start()
            camera.start(null)
        }
    }

    override fun onPause() {
        if (::glView.isInitialized) glView.onPause()
        if (::imu.isInitialized) imu.stop()
        if (::camera.isInitialized) camera.stop()
        super.onPause()
    }

    override fun onDestroy() {
        if (vrStarted) NativeSdk.shutdown()
        super.onDestroy()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) enterImmersive()
    }

    @SuppressLint("InlinedApi")
    private fun enterImmersive() {
        @Suppress("DEPRECATION")
        window.decorView.systemUiVisibility =
            (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or View.SYSTEM_UI_FLAG_FULLSCREEN or
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION)
    }

    // ------------------------------------------------------------------
    // Input físico do headset:
    //   - toque na tela  = clique do ponteiro (gaze/hand/controller ativo)
    //   - toque longo    = recenter
    //   - volume down/up = recenter
    //   - gamepad A/ok   = clique (Joy-Con etc.)
    // ------------------------------------------------------------------
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!vrStarted) return super.onTouchEvent(event)
        return input.onScreenTouch(event)
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        if (vrStarted && input.onKeyDown(keyCode)) return true
        return super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        if (vrStarted && input.onKeyUp(keyCode)) return true
        return super.onKeyUp(keyCode, event)
    }

    override fun onGenericMotionEvent(event: MotionEvent?): Boolean {
        if (vrStarted && event != null && input.onGenericMotion(event)) return true
        return super.onGenericMotionEvent(event)
    }

    // ------------------------------------------------------------------
    // VrOs.Host — ações do menu principal
    // ------------------------------------------------------------------
    override fun openBrowser() = browser.start()
    override fun openAppLibrary() = appBridge.openLibrary()
    override fun openLuaConsole() {
        lua.openConsole()
        lua.runAsset("hello.lua")
    }

    override fun openKeyboard(onDone: (String) -> Unit) {
        keyboard.onSubmit = onDone
        keyboard.show()
    }

    // ------------------------------------------------------------------
    // Permissões / consentimentos do sistema
    // ------------------------------------------------------------------
    private val cameraRequest = 7001

    fun requestVrPermissions() {
        runOnUiThread {
            if (checkSelfPermission(android.Manifest.permission.CAMERA) !=
                PackageManager.PERMISSION_GRANTED
            ) {
                requestPermissions(
                    arrayOf(android.Manifest.permission.CAMERA), cameraRequest,
                )
            }
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int, permissions: Array<out String>, grantResults: IntArray,
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == cameraRequest) startVrSubsystems()
    }

    @Deprecated("Deprecated in Java")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == AppBridge.PROJECTION_REQUEST && data != null) {
            appBridge.onProjectionResult(resultCode, data)
        }
    }

    /** Chamado pelo renderer quando o contexto GL está pronto. */
    fun onGlReady(w: Int, h: Int) {
        if (vrStarted) return
        vrStarted = true

        val app = application as BrazilMrApplication
        os.host = this
        appBridge.start()

        val profile = app.activeProfile
        NativeSdk.setScreenSize(profile.screenWmm, profile.screenHmm)
        NativeSdk.setStereoProfile(
            profile.fovYDeg, profile.ipdMm, profile.k1, profile.k2, profile.chroma,
            profile.vignette, profile.renderScale, profile.swapEyes,
            NativeSdk.SBS_NORMAL, profile.lensCenterX, profile.lensCenterY,
        )
        NativeSdk.setFilterConfig(NativeSdk.FILTER_BALANCED)

        os.buildScene()
        requestVrPermissions()
        if (checkSelfPermission(android.Manifest.permission.CAMERA) ==
            PackageManager.PERMISSION_GRANTED
        ) startVrSubsystems()

        // primeira execução: wizard de calibração
        if (!app.calibrationDone) os.startCalibration()
    }

    private fun startVrSubsystems() {
        try {
            imu.start()
            NativeSdk.setTrackingBackend(NativeSdk.BACKEND_IMU_3DOF)
            camera.start(null) // preset BALANCED; MediaPipe consome os mesmos frames
            hands.start()
            thermal.start()
        } catch (t: Throwable) {
            // um subsistema que falhe NÃO pode derrubar a plataforma —
            // registra e segue (gaze cobre a interação)
            CrashReporter.recordError("subsistemas VR", t)
        }
    }

    fun recenter() = NativeSdk.recenter()

    // ------------------------------------------------------------------
    // Diagnóstico: relatório de erro
    // ------------------------------------------------------------------

    /**
     * Pergunta se o usuário quer compartilhar o relatório do crash da
     * execução anterior (pré-voo — antes de o VR iniciar).
     */
    private fun offerCrashReport() {
        try {
            android.app.AlertDialog.Builder(this)
                .setTitle("O BrazilMR fechou na última vez")
                .setMessage(
                    "Foi gravado um relatório técnico do que aconteceu " +
                        "(modelo do aparelho, versão do Android e o erro " +
                        "exato — nenhum dado pessoal).\n\n" +
                        "Compartilhar com o desenvolvedor para corrigir o problema?",
                )
                .setPositiveButton("Compartilhar") { d, _ ->
                    d.dismiss()
                    runCatching { startActivity(CrashReporter.shareIntent(this)) }
                    CrashReporter.clear()
                }
                .setNegativeButton("Agora não") { d, _ ->
                    d.dismiss()
                    CrashReporter.clear()
                }
                .setCancelable(false)
                .show()
        } catch (_: Exception) {
            CrashReporter.clear()
        }
    }

    /**
     * Tela de diagnóstico — usada SOMENTE quando o próprio modo VR não
     * conseguiu iniciar. Não é interface do produto (o produto é 100% VR);
     * é o "modo seguro" que garante que o erro chegue ao desenvolvedor.
     * (Chamada também pelo VrRenderer quando a thread GL falha.)
     */
    fun showStartupFailure(t: Throwable) {
        vrStarted = false
        runCatching { glView.onPause() }
        val density = resources.displayMetrics.density
        fun dp(v: Int) = (density * v).toInt()

        val title = android.widget.TextView(this).apply {
            text = "O BrazilMR não conseguiu iniciar o modo VR"
            textSize = 19f
            setTextColor(0xFFE8EDF2.toInt())
        }
        val detail = android.widget.TextView(this).apply {
            text =
                "Erro: ${t.javaClass.name}\n${t.message ?: ""}\n\n" +
                    "Toque em “Compartilhar relatório” e envie para o " +
                    "desenvolvedor — com isso dá para corrigir o problema " +
                    "no seu aparelho."
            textSize = 13f
            setTextColor(0xFF9AA7B4.toInt())
            setPadding(0, dp(12), 0, dp(24))
        }
        val share = android.widget.Button(this).apply {
            text = "Compartilhar relatório"
            setOnClickListener {
                runCatching { startActivity(CrashReporter.shareIntent(this@VrActivity)) }
            }
        }
        val retry = android.widget.Button(this).apply {
            text = "Tentar de novo"
            setOnClickListener {
                CrashReporter.clear()
                recreate()
            }
        }
        val root = android.widget.LinearLayout(this).apply {
            orientation = android.widget.LinearLayout.VERTICAL
            setBackgroundColor(0xFF0B0F14.toInt())
            val pad = dp(20)
            setPadding(pad, pad, pad, pad)
            addView(title)
            addView(detail)
            addView(share)
            addView(retry)
        }
        setContentView(root)
    }

    /** Roteia UM evento nativo por todos os subsistemas UI. */
    fun routeNativeEvent(ev: IntArray) {
        if (ev.size < 6) return
        // input em janelas espaciais (browser / app projetado)
        if (ev[0] == NativeSdk.EVENT_WINDOW_INPUT) {
            val u = ev[3] / 1000f
            val v = ev[4] / 1000f
            val pointerEvent = ev[5]
            when (ev[1]) {
                VrBrowser.WINDOW_ID -> browser.onWindowInput(u, v, pointerEvent)
                AppBridge.WINDOW_ID -> appBridge.onWindowInput(u, v, pointerEvent)
            }
        }
        // ordem: teclado (painel próprio) → biblioteca de apps → wizard → OS/Lua
        if (keyboard.onNativeEvent(ev)) return
        if (appBridge.library.onNativeEvent(ev)) return
        if (browser.onNativeEvent(ev)) return
        os.onNativeEvent(ev)
        lua.dispatchNativeEvent(ev)
    }
}

// ==========================================================================
// Renderer GL — um frame = tracking → cena → estéreo → compositor.
// ==========================================================================
private class VrRenderer(
    private val activity: VrActivity,
    private val os: VrOs,
    private val imu: ImuTracker,
    private val hands: HandTracking,
    private val thermal: ThermalManager,
    private val input: InputRouter,
    private val browser: VrBrowser,
    private val appBridge: AppBridge,
    private val lua: LuaSdk,
) : GLSurfaceView.Renderer {

    private var lastNs = 0L
    private val eventBuf = IntArray(6)
    private var statusTick = 0L

    /** Depois de uma falha: para de renderizar (evita spam de exceção). */
    @Volatile private var broken = false

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        // contexto recriado — reconfigura no próximo onSurfaceChanged
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        if (broken) return
        try {
            if (!NativeSdk.initialize(width, height)) return
            NativeSdk.initGl(width, height)
            NativeSdk.resizeGl(width, height)
            FontAtlas.install()
            activity.onGlReady(width, height)
        } catch (t: Throwable) {
            // exceção na thread GL é fatal por padrão — aqui viram relatório
            broken = true
            CrashReporter.recordError("primeiro frame GL", t)
            activity.runOnUiThread { activity.showStartupFailure(t) }
        }
    }

    override fun onDrawFrame(gl: GL10?) {
        if (broken) return
        try {
            drawFrame()
        } catch (t: Throwable) {
            broken = true
            CrashReporter.recordError("frame GL", t)
            activity.runOnUiThread { activity.showStartupFailure(t) }
        }
    }

    private fun drawFrame() {
        val now = SystemClock.elapsedRealtimeNanos()
        val dt = if (lastNs == 0L) 1f / 60f else (now - lastNs) * 1e-9f
        lastNs = now
        val nowMs = now / 1_000_000L

        // 1) IMU já chega pelo listener do ImuTracker (push direto)

        // 2) mãos: últimos landmarks do MediaPipe
        hands.pushLatestToNative()

        // 3) ponteiro gaze + raio visual
        input.frame()

        // 4) conteúdo dinâmico das janelas (browser / apps projetados)
        browser.frame(nowMs)
        appBridge.frame(nowMs)

        // 5) render estéreo completo (sky→models→windows→UI→hands→compositor)
        NativeSdk.renderFrame(dt.coerceIn(0.0005f, 0.1f), now)

        // 6) eventos → subsistemas
        while (NativeSdk.pollEvent(eventBuf)) {
            activity.routeNativeEvent(eventBuf)
        }

        // 7) status (~0,5 Hz)
        if (nowMs - statusTick > 2000) {
            statusTick = nowMs
            val stats = NativeSdk.getStats()
            val fps = stats?.firstOrNull() ?: 0f
            os.tickStatus("IMU 3DoF", fps, thermal.budget)
        }

        // 8) térmico adaptativo (~1 Hz interno)
        thermal.tick()
    }
}
