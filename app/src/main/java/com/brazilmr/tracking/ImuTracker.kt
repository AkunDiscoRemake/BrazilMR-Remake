/*
 * BrazilMR — fusão de sensores IMU (entrada).
 *
 * Lê giroscópio/acelerômetro/magnetômetro no frame do DISPOSITIVO
 * (Android: X direita, Y topo, Z sai da tela) e remapeia para o frame
 * da CABEÇA do VR Box (paisagem, X direita, Y para cima, -Z à frente),
 * que é a convenção do motor nativo (mundo Y-up).
 *
 * Em repouso (headset nivelado), o acelerômetro no frame da cabeça lê
 * ≈ (0, +9.81, 0) — exatamente o que o ImuFusion nativo espera.
 */
package com.brazilmr.tracking

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import com.brazilmr.sdk.NativeSdk

class ImuTracker(private val context: Context) : SensorEventListener {

    private val sm = context.getSystemService(Context.SENSOR_SERVICE) as SensorManager
    private var running = false
    private var magAvailable = false

    // cache do último evento de cada sensor (fusão por timestamp)
    private val gyro = FloatArray(3)
    private val accel = FloatArray(3)
    private val mag = FloatArray(3)
    private var gyroTs = 0L
    private var accelTs = 0L
    private var magTs = 0L
    private var magValid = false

    // taxa de envio para o nativo (o nativo integra por dt; 240 Hz é overkill)
    private var lastPushNs = 0L
    private val minPushIntervalNs = 2_000_000L // 500 Hz máx

    /** Rotação fixa device(paisagem-90°)→head: (x,y,z)_head = R·(x,y,z)_dev. */
    private val remap = FloatArray(9)

    init {
        // identity até termos a rotação real da tela (updateDisplayRotation)
        android.opengl.Matrix.setIdentityM(remap, 0)
    }

    fun start() {
        if (running) return
        running = true
        val gyroSensor = sm.getDefaultSensor(Sensor.TYPE_GYROSCOPE_UNCALIBRATED)
            ?: sm.getDefaultSensor(Sensor.TYPE_GYROSCOPE)
        val accelSensor = sm.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
        val magSensor = sm.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD)

        // taxa alta = menor latência; o batching do sensor HAL acumula.
        gyroSensor?.let { sm.registerListener(this, it, SensorManager.SENSOR_DELAY_FASTEST) }
        accelSensor?.let { sm.registerListener(this, it, SensorManager.SENSOR_DELAY_GAME) }
        magAvailable = magSensor != null
        magSensor?.let { sm.registerListener(this, it, SensorManager.SENSOR_DELAY_UI) }
    }

    fun stop() {
        if (!running) return
        running = false
        sm.unregisterListener(this)
    }

    /** Recalcula o remap device→head conforme a rotação da tela. */
    fun updateDisplayRotation(rotation: Int) {
        val (xOut, yOut) = when (rotation) {
            android.view.Surface.ROTATION_0 -> SensorManager.AXIS_X to SensorManager.AXIS_Y
            android.view.Surface.ROTATION_90 -> SensorManager.AXIS_Y to SensorManager.AXIS_MINUS_X
            android.view.Surface.ROTATION_180 -> SensorManager.AXIS_MINUS_X to SensorManager.AXIS_MINUS_Y
            else -> SensorManager.AXIS_MINUS_Y to SensorManager.AXIS_X
        }
        val identity = FloatArray(9)
        android.opengl.Matrix.setIdentityM(identity, 0)
        SensorManager.remapCoordinateSystem(identity, xOut, yOut, remap)
    }

    override fun onSensorChanged(event: SensorEvent) {
        when (event.sensor.type) {
            Sensor.TYPE_GYROSCOPE_UNCALIBRATED, Sensor.TYPE_GYROSCOPE -> {
                System.arraycopy(event.values, 0, gyro, 0, 3)
                gyroTs = event.timestamp
            }
            Sensor.TYPE_ACCELEROMETER -> {
                System.arraycopy(event.values, 0, accel, 0, 3)
                accelTs = event.timestamp
            }
            Sensor.TYPE_MAGNETIC_FIELD -> {
                System.arraycopy(event.values, 0, mag, 0, 3)
                magTs = event.timestamp
                magValid = true
            }
        }
        pushIfDue(event.timestamp)
    }

    private fun pushIfDue(nowNs: Long) {
        if (gyroTs == 0L || accelTs == 0L) return
        if (nowNs - lastPushNs < minPushIntervalNs) return
        lastPushNs = nowNs

        val g = remapToHead(gyro)
        val a = remapToHead(accel)
        val m = if (magAvailable && magValid && nowNs - magTs < 200_000_000L)
            remapToHead(mag) else null

        NativeSdk.pushImu(g, a, m, gyroTs)
    }

    private fun remapToHead(v: FloatArray): FloatArray {
        val r = remap
        return floatArrayOf(
            r[0] * v[0] + r[1] * v[1] + r[2] * v[2],
            r[3] * v[0] + r[4] * v[1] + r[5] * v[2],
            r[6] * v[0] + r[7] * v[1] + r[8] * v[2],
        )
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {
        // magnetômetro pouco confiável → ignora correção de yaw
        if (sensor?.type == Sensor.TYPE_MAGNETIC_FIELD && accuracy < SensorManager.SENSOR_STATUS_ACCURACY_LOW) {
            magValid = false
        }
    }
}
