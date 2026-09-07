/*
 * BrazilMR — captura de crashes (diagnóstico).
 *
 * Grava em arquivo (armazenamento interno, sem permissões) tudo o que
 * derrubar o processo, para que a CAUSA REAL chegue ao desenvolvedor:
 *   - java-crash-*.txt   → exceção não capturada (qualquer thread,
 *                          inclusive a thread GL)
 *   - native-crash-*.txt → SIGSEGV/SIGABRT etc. (handler C++ em
 *                          brazilmr_jni/CrashHandler.cpp)
 *   - startup-*.txt      → falha registrada em ponto controlado
 *   - device.txt         → modelo/fingerprint/ABI/versão (sem dados
 *                          pessoais — apenas informação técnica)
 *
 * Na abertura seguinte, o VrActivity oferece compartilhar o relatório
 * (fase de pré-voo, ANTES do modo VR — não é interface do produto).
 */
package com.brazilmr.diagnostics

import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Process
import android.util.Log
import com.brazilmr.sdk.NativeSdk
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object CrashReporter {

    private const val TAG = "BrazilMRCrash"
    private const val DIR = "crashes"
    private const val MAX_REPORT_CHARS = 200_000

    /** "ok" ou o motivo da falha de carregamento da lib nativa. */
    @Volatile var nativeLibStatus: String = ""
        private set

    @Volatile private var crashDir: File? = null
    @Volatile private var installed = false

    /**
     * Deve ser a PRIMEIRA coisa executada do processo
     * (BrazilMrApplication.onCreate, antes de qualquer outro código).
     */
    fun install(context: Context) {
        if (installed) return
        installed = true
        val appContext = context.applicationContext
        val dir = File(appContext.filesDir, DIR).apply { mkdirs() }
        crashDir = dir

        // informações do aparelho (técnico, sem dados pessoais)
        runCatching { writeDeviceInfo(appContext, dir) }

        // lib nativa + handler de sinais o mais cedo possível.
        // System.loadLibrary acontece no init do objeto NativeSdk — se a lib
        // falhar (o caso clássico de "app abre e fecha"), o motivo fica
        // gravado em arquivo em vez de virar um crash silencioso.
        nativeLibStatus = try {
            NativeSdk.installCrashHandler(dir.absolutePath)
            "ok"
        } catch (t: Throwable) {
            val msg = "libbrazilmr.so FALHOU AO CARREGAR: " +
                "${t.javaClass.name}: ${t.message}"
            recordError("lib-nativa", RuntimeException(msg, t))
            msg
        }

        // exceções não capturadas em QUALQUER thread (inclui a thread GL,
        // onde roda o renderer — exceção lá é fatal por padrão)
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            runCatching { writeJavaCrash(dir, thread, throwable) }
            previous?.uncaughtException(thread, throwable)
        }
        Log.i(TAG, "CrashReporter ativo (lib nativa: $nativeLibStatus)")
    }

    /** Registra uma falha em ponto controlado, sem derrubar o app. */
    fun recordError(stage: String, t: Throwable) {
        runCatching {
            val dir = crashDir ?: return
            File(dir, "startup-${stamp()}.txt").writeText(
                "ponto=$stage\n" +
                    "erro=${t.javaClass.name}: ${t.message}\n\n" +
                    Log.getStackTraceString(t),
            )
        }
        Log.e(TAG, "Falha em '$stage'", t)
    }

    /** Há relatórios de crash da execução anterior? */
    fun hasPendingReports(): Boolean =
        crashDir?.listFiles { f: File ->
            f.isFile && f.length() > 0 && f.name != "device.txt"
        }?.isNotEmpty() == true

    /** Texto completo do relatório (aparelho + todos os crashes). */
    fun buildReport(): String = runCatching {
        val sb = StringBuilder()
        sb.append("== BrazilMR — relatório de erro ==\n")
        sb.append("gerado=")
            .append(SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(Date()))
            .append('\n')
        sb.append("libNativa=").append(nativeLibStatus.ifEmpty { "n/a" }).append('\n')
        crashDir?.listFiles()
            ?.sortedBy { it.name }
            ?.forEach { f ->
                if (!f.isFile || f.length() == 0L) return@forEach
                sb.append("\n== ARQUIVO: ${f.name} ==\n")
                sb.append(f.readText())
            }
        if (sb.length > MAX_REPORT_CHARS) sb.setLength(MAX_REPORT_CHARS)
        sb.toString()
    }.getOrDefault("(falha ao gerar relatório)")

    /** Intent de compartilhamento (e-mail, WhatsApp, GitHub…). */
    fun shareIntent(context: Context): Intent {
        val send = Intent(Intent.ACTION_SEND).apply {
            type = "text/plain"
            putExtra(Intent.EXTRA_SUBJECT, "BrazilMR — relatório de erro")
            putExtra(Intent.EXTRA_TEXT, buildReport())
        }
        return Intent.createChooser(send, "Compartilhar relatório de erro")
    }

    /** Apaga os relatórios (após compartilhar ou ignorar). */
    fun clear() {
        runCatching { crashDir?.listFiles()?.forEach { it.delete() } }
    }

    // ------------------------------------------------------------------

    private fun stamp(): String =
        SimpleDateFormat("yyyyMMdd-HHmmss-SSS", Locale.US).format(Date())

    private fun writeDeviceInfo(context: Context, dir: File) {
        val version = runCatching {
            @Suppress("DEPRECATION")
            context.packageManager.getPackageInfo(context.packageName, 0).versionName
        }.getOrNull() ?: "?"
        File(dir, "device.txt").writeText(
            buildString {
                append("modelo=${Build.MANUFACTURER} ${Build.MODEL}\n")
                append("dispositivo=${Build.DEVICE}\n")
                append("android=${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})\n")
                append("fingerprint=${Build.FINGERPRINT}\n")
                append("abis=${Build.SUPPORTED_ABIS.contentToString()}\n")
                append("processo64bits=${Process.is64Bit()}\n")
                append("versaoApp=$version\n")
                append("targetSdk=${context.applicationInfo.targetSdkVersion}\n")
            },
        )
    }

    private fun writeJavaCrash(dir: File, thread: Thread, t: Throwable) {
        val f = File(dir, "java-crash-${stamp()}.txt")
        f.writeText(
            "thread=${thread.name} (id ${thread.id})\n" +
                "erro=${t.javaClass.name}: ${t.message}\n\n" +
                Log.getStackTraceString(t),
        )
        Log.e(TAG, "Exceção não capturada — gravada em ${f.name}", t)
    }
}
