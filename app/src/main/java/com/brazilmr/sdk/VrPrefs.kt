/*
 * BrazilMR — persistência leve (SharedPreferences) para config do headset.
 * Sem DataStore/Room: footprint mínimo, zero dependências de UI.
 */
package com.brazilmr.sdk

import android.content.Context
import android.content.SharedPreferences

class VrPrefs(context: Context) {
    private val sp: SharedPreferences =
        context.getSharedPreferences("brazilmr_vr", Context.MODE_PRIVATE)

    fun getBoolean(key: String, def: Boolean = false): Boolean = sp.getBoolean(key, def)
    fun putBoolean(key: String, v: Boolean) = sp.edit().putBoolean(key, v).apply()

    fun getFloat(key: String, def: Float): Float = sp.getFloat(key, def)
    fun putFloat(key: String, v: Float) = sp.edit().putFloat(key, v).apply()

    fun getInt(key: String, def: Int): Int = sp.getInt(key, def)
    fun putInt(key: String, v: Int) = sp.edit().putInt(key, v).apply()

    fun getString(key: String, def: String): String = sp.getString(key, def) ?: def
    fun putString(key: String, v: String) = sp.edit().putString(key, v).apply()

    companion object {
        const val KEY_CALIBRATION_DONE = "calibration_done"
        const val KEY_PROFILE_IPD = "profile_ipd_mm"
        const val KEY_PROFILE_FOV = "profile_fov_y_deg"
        const val KEY_PROFILE_K1 = "profile_k1"
        const val KEY_PROFILE_K2 = "profile_k2"
        const val KEY_PROFILE_CHROMA = "profile_chroma"
        const val KEY_PROFILE_RENDER_SCALE = "profile_render_scale"
        const val KEY_PROFILE_SWAP_EYES = "profile_swap_eyes"
        const val KEY_PROFILE_LENS_CX = "profile_lens_cx"
        const val KEY_PROFILE_LENS_CY = "profile_lens_cy"
        const val KEY_PROFILE_SCREEN_W_MM = "profile_screen_w_mm"
        const val KEY_PROFILE_SCREEN_H_MM = "profile_screen_h_mm"
        const val KEY_PROFILE_NAME = "profile_name"
        const val KEY_FILTER_PRESET = "filter_preset"
        const val KEY_SBS_MODE = "sbs_mode"
        const val KEY_SEEN_PREFLIGHT = "seen_preflight"
    }
}
