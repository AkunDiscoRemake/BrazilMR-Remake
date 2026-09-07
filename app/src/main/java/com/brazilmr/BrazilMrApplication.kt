/*
 * BrazilMR — Application.
 * Mantém estado global da plataforma (perfil do headset, config salva,
 * bridge de apps) e inicializa subsistemas que vivem fora da Activity.
 */
package com.brazilmr

import android.app.Application
import android.content.Context
import com.brazilmr.platform.HeadsetProfile
import com.brazilmr.platform.HeadsetProfiles
import com.brazilmr.sdk.VrPrefs

class BrazilMrApplication : Application() {

    lateinit var prefs: VrPrefs
        private set

    /** Perfil estéreo ativo (IPD, FOV, distorção) — mutável pela calibração. */
    var activeProfile: HeadsetProfile = HeadsetProfiles.defaultProfile()
        private set

    var calibrationDone: Boolean = false
        private set

    override fun onCreate() {
        super.onCreate()
        instance = this
        prefs = VrPrefs(this)
        activeProfile = HeadsetProfiles.load(this, prefs)
        calibrationDone = prefs.getBoolean(VrPrefs.KEY_CALIBRATION_DONE, false)
    }

    fun applyProfile(profile: HeadsetProfile, persist: Boolean = true) {
        activeProfile = profile
        if (persist) HeadsetProfiles.save(this, prefs, profile)
    }

    fun markCalibrationDone() {
        calibrationDone = true
        prefs.putBoolean(VrPrefs.KEY_CALIBRATION_DONE, true)
    }

    companion object {
        @Volatile
        private var instance: BrazilMrApplication? = null

        fun get(): BrazilMrApplication =
            instance ?: error("BrazilMrApplication não inicializada")

        fun context(): Context = get().applicationContext
    }
}
