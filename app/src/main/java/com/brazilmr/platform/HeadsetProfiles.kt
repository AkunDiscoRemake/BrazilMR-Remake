/*
 * BrazilMR — perfis de headset (VR Box / Cardboard).
 *
 * O perfil define a geometria óptica usada pelo compositor estéreo nativo:
 *   FOV, IPD, distorção k1/k2, aberração cromática, escala de render e
 *   centros de lente. Perfis conhecidos vêm de assets/device_profiles.json;
 *   a calibração wizard ajusta em cima e persiste em SharedPreferences.
 */
package com.brazilmr.platform

import android.content.Context
import com.brazilmr.sdk.VrPrefs
import org.json.JSONArray
import org.json.JSONObject
import java.io.InputStream

data class HeadsetProfile(
    val name: String,
    /** FOV vertical por olho, graus. */
    val fovYDeg: Float = 90f,
    /** Distância interpupilar, mm. */
    val ipdMm: Float = 63.5f,
    /** Coeficientes de distorção barrel (Cardboard: ~0.34 / ~0.55 se usado). */
    val k1: Float = 0.215f,
    val k2: Float = 0.125f,
    /** Aberração cromática relativa (0 = nenhuma). */
    val chroma: Float = 0.008f,
    /** Escala de render (1.0 = resolução nativa por olho). */
    val renderScale: Float = 1.0f,
    /** Olhos trocados (lentes do headset invertidas). */
    val swapEyes: Boolean = false,
    /** Centro da lente esquerda (fração da metade da tela: 0.5 = centro). */
    val lensCenterX: Float = 0.5f,
    val lensCenterY: Float = 0.5f,
    /** Tamanho físico da tela (mm) — necessário p/ escala do mundo. */
    val screenWmm: Float = 130f,
    val screenHmm: Float = 68f,
    /** Vignette 0..1 (escurecimento borda p/ suavizar borda de lente). */
    val vignette: Float = 0.35f,
)

object HeadsetProfiles {

    /** Perfil "genérico" usado antes da calibração. */
    fun defaultProfile(): HeadsetProfile = HeadsetProfile(name = "VR Box (padrão)")

    /**
     * Carrega perfis embutidos de assets/device_profiles.json e aplica
     * overrides persistidos da calibração.
     */
    fun load(context: Context, prefs: VrPrefs): HeadsetProfile {
        val builtin = builtinProfiles(context)
        val savedName = prefs.getString(VrPrefs.KEY_PROFILE_NAME, "")
        var profile = builtin.firstOrNull { it.name == savedName } ?: defaultProfile()

        // overrides da calibração (se existirem)
        if (prefs.getFloat(VrPrefs.KEY_PROFILE_IPD, -1f) >= 0f) {
            profile = profile.copy(
                ipdMm = prefs.getFloat(VrPrefs.KEY_PROFILE_IPD, profile.ipdMm),
                fovYDeg = prefs.getFloat(VrPrefs.KEY_PROFILE_FOV, profile.fovYDeg),
                k1 = prefs.getFloat(VrPrefs.KEY_PROFILE_K1, profile.k1),
                k2 = prefs.getFloat(VrPrefs.KEY_PROFILE_K2, profile.k2),
                chroma = prefs.getFloat(VrPrefs.KEY_PROFILE_CHROMA, profile.chroma),
                renderScale = prefs.getFloat(VrPrefs.KEY_PROFILE_RENDER_SCALE, profile.renderScale),
                swapEyes = prefs.getBoolean(VrPrefs.KEY_PROFILE_SWAP_EYES, profile.swapEyes),
                lensCenterX = prefs.getFloat(VrPrefs.KEY_PROFILE_LENS_CX, profile.lensCenterX),
                lensCenterY = prefs.getFloat(VrPrefs.KEY_PROFILE_LENS_CY, profile.lensCenterY),
                screenWmm = prefs.getFloat(VrPrefs.KEY_PROFILE_SCREEN_W_MM, profile.screenWmm),
                screenHmm = prefs.getFloat(VrPrefs.KEY_PROFILE_SCREEN_H_MM, profile.screenHmm),
            )
        }
        return profile
    }

    fun save(context: Context, prefs: VrPrefs, p: HeadsetProfile) {
        prefs.apply {
            putString(VrPrefs.KEY_PROFILE_NAME, p.name)
            putFloat(VrPrefs.KEY_PROFILE_IPD, p.ipdMm)
            putFloat(VrPrefs.KEY_PROFILE_FOV, p.fovYDeg)
            putFloat(VrPrefs.KEY_PROFILE_K1, p.k1)
            putFloat(VrPrefs.KEY_PROFILE_K2, p.k2)
            putFloat(VrPrefs.KEY_PROFILE_CHROMA, p.chroma)
            putFloat(VrPrefs.KEY_PROFILE_RENDER_SCALE, p.renderScale)
            putBoolean(VrPrefs.KEY_PROFILE_SWAP_EYES, p.swapEyes)
            putFloat(VrPrefs.KEY_PROFILE_LENS_CX, p.lensCenterX)
            putFloat(VrPrefs.KEY_PROFILE_LENS_CY, p.lensCenterY)
            putFloat(VrPrefs.KEY_PROFILE_SCREEN_W_MM, p.screenWmm)
            putFloat(VrPrefs.KEY_PROFILE_SCREEN_H_MM, p.screenHmm)
        }
    }

    /** Perfis de fábrica (assets/device_profiles.json). */
    fun builtinProfiles(context: Context): List<HeadsetProfile> {
        return try {
            val stream: InputStream = context.assets.open("device_profiles.json")
            val json = stream.bufferedReader().use { it.readText() }
            stream.close()
            val arr = JSONArray(json)
            (0 until arr.length()).mapNotNull { i ->
                val o = arr.optJSONObject(i) ?: return@mapNotNull null
                profileFromJson(o)
            }
        } catch (_: Exception) {
            emptyList()
        }
    }

    private fun profileFromJson(o: JSONObject): HeadsetProfile = HeadsetProfile(
        name = o.optString("name", "VR Box"),
        fovYDeg = o.optDouble("fovYDeg", 90.0).toFloat(),
        ipdMm = o.optDouble("ipdMm", 63.5).toFloat(),
        k1 = o.optDouble("k1", 0.215).toFloat(),
        k2 = o.optDouble("k2", 0.125).toFloat(),
        chroma = o.optDouble("chroma", 0.008).toFloat(),
        renderScale = o.optDouble("renderScale", 1.0).toFloat(),
        swapEyes = o.optBoolean("swapEyes", false),
        lensCenterX = o.optDouble("lensCenterX", 0.5).toFloat(),
        lensCenterY = o.optDouble("lensCenterY", 0.5).toFloat(),
        screenWmm = o.optDouble("screenWmm", 130.0).toFloat(),
        screenHmm = o.optDouble("screenHmm", 68.0).toFloat(),
        vignette = o.optDouble("vignette", 0.35).toFloat(),
    )
}
