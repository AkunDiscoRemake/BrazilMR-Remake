/*
 * BrazilMR — VR SDK Lua (LuaJ).
 *
 * API exposta ao script (namespace `vr`):
 *   vr.createWindow(id, largura, altura [, px, py, pz])
 *   vr.removeWindow(id)
 *   vr.setWindowTransform(id, px, py, pz, yawDeg, escala)
 *   vr.loadModel("assets/models/logo.glb") → handle
 *   vr.setModelTransform(handle, px,py,pz, yawDeg, escala, visivel)
 *   vr.setModelAnimation(handle, animIdx, loop)
 *   vr.spawnSphere(x, y, z, raio) / vr.spawnCube(x, y, z, metade)
 *   vr.setColor(id, r, g, b, a)
 *   vr.removeObject(id)
 *   vr.getHeadPose() → px,py,pz, qx,qy,qz,qw
 *   vr.getHand(esquerda) → table de 21 landmarks {x,y,z} + gesto/pinça
 *   vr.raycast() → hit (u,v,windowId) sob o ponteiro ativo (via evento)
 *   vr.onGesture(fn)  — callback: (hand, gestureName, strength)
 *   vr.onEvent(fn)    — callback: (type, a, b, x, y, z)
 *   vr.setThermalBudget(b)
 *   vr.recenter()
 *
 * Scripts de exemplo: assets/lua (hello.lua, window_demo.lua).
 */
package com.brazilmr.sdk.lua

import android.content.Context
import com.brazilmr.sdk.NativeSdk
import org.luaj.vm2.LuaBoolean
import org.luaj.vm2.LuaDouble
import org.luaj.vm2.LuaInteger
import org.luaj.vm2.LuaString
import org.luaj.vm2.LuaTable
import org.luaj.vm2.LuaValue
import org.luaj.vm2.Varargs
import org.luaj.vm2.lib.OneArgFunction
import org.luaj.vm2.lib.ThreeArgFunction
import org.luaj.vm2.lib.TwoArgFunction
import org.luaj.vm2.lib.jse.JsePlatform

class LuaSdk(private val context: Context) {

    private var globals: org.luaj.vm2.Globals? = null
    private var gestureCallback: LuaValue? = null
    private var eventCallback: LuaValue? = null

    // painel do console Lua (erro/log dentro do VR)
    private var consolePanelId = -1
    private var consoleLabelId = -1

    fun initialize() {
        if (globals != null) return
        val g = JsePlatform.standardGlobals()
        globals = g

        val vr = LuaTable.tableOf()

        // ------------------------------------------------------------------
        vr.set("createWindow", object : VarArgFunctionCompat() {
            override fun invoke(args: Varargs): LuaValue {
                val id = args.arg(1).checkint()
                val w = args.arg(2).checkdouble().toFloat()
                val h = args.arg(3).checkdouble().toFloat()
                val px = if (args.narg() >= 4) args.arg(4).checkdouble().toFloat() else 0f
                val py = if (args.narg() >= 5) args.arg(5).checkdouble().toFloat() else 0.1f
                val pz = if (args.narg() >= 6) args.arg(6).checkdouble().toFloat() else -2f
                val r = NativeSdk.createWindow(
                    id, px, py, pz, 0f, 0f, 0f, 1f, w, h,
                    NativeSdk.CONTENT_LUA_UI,
                )
                return LuaInteger.valueOf(r)
            }
        })

        vr.set("removeWindow", object : OneArgFunction() {
            override fun call(id: LuaValue): LuaValue {
                NativeSdk.removeWindow(id.checkint())
                return LuaValue.NIL
            }
        })

        vr.set("setWindowTransform", object : VarArgFunctionCompat() {
            override fun invoke(args: Varargs): LuaValue {
                val id = args.arg(1).checkint()
                val px = args.arg(2).checkdouble().toFloat()
                val py = args.arg(3).checkdouble().toFloat()
                val pz = args.arg(4).checkdouble().toFloat()
                val yaw = Math.toRadians(args.arg(5).checkdouble())
                val scale = if (args.narg() >= 6) args.arg(6).checkdouble().toFloat() else 1f
                val half = yaw / 2
                NativeSdk.updateWindowTransform(
                    id, px, py, pz,
                    0f, Math.sin(half).toFloat(), 0f, Math.cos(half).toFloat(),
                    scale,
                )
                return LuaValue.NIL
            }
        })

        vr.set("loadModel", object : OneArgFunction() {
            override fun call(path: LuaValue): LuaValue {
                val asset = path.checkjstring().removePrefix("assets/")
                return try {
                    val bytes = context.assets.open(asset).use { it.readBytes() }
                    LuaInteger.valueOf(NativeSdk.loadGlb(bytes).toInt())
                } catch (_: Exception) {
                    LuaInteger.valueOf(0)
                }
            }
        })

        vr.set("setModelTransform", object : VarArgFunctionCompat() {
            override fun invoke(args: Varargs): LuaValue {
                val handle = args.arg(1).checkint().toLong()
                val px = args.arg(2).checkdouble().toFloat()
                val py = args.arg(3).checkdouble().toFloat()
                val pz = args.arg(4).checkdouble().toFloat()
                val yawDeg = args.arg(5).checkdouble()
                val scale = if (args.narg() >= 6) args.arg(6).checkdouble().toFloat() else 1f
                val visible = if (args.narg() >= 7) args.arg(7).checkboolean() else true
                val half = Math.toRadians(yawDeg) / 2
                val ok = NativeSdk.setModelTransform(
                    handle, px, py, pz,
                    0f, Math.sin(half).toFloat(), 0f, Math.cos(half).toFloat(),
                    scale, scale, scale, visible,
                )
                return LuaBoolean.valueOf(ok)
            }
        })

        vr.set("setModelAnimation", object : ThreeArgFunction() {
            override fun call(h: LuaValue, i: LuaValue, l: LuaValue): LuaValue =
                LuaBoolean.valueOf(NativeSdk.setModelAnimation(h.checklong(), i.checkint(), l.checkboolean()))
        })

        vr.set("spawnSphere", object : VarArgFunctionCompat() {
            override fun invoke(args: Varargs): LuaValue {
                val id = NativeSdk.spawnSphere(
                    args.arg(1).checkdouble().toFloat(), args.arg(2).checkdouble().toFloat(),
                    args.arg(3).checkdouble().toFloat(), args.arg(4).checkdouble().toFloat(),
                )
                return LuaInteger.valueOf(id.toInt())
            }
        })

        vr.set("spawnCube", object : VarArgFunctionCompat() {
            override fun invoke(args: Varargs): LuaValue {
                val id = NativeSdk.spawnCube(
                    args.arg(1).checkdouble().toFloat(), args.arg(2).checkdouble().toFloat(),
                    args.arg(3).checkdouble().toFloat(), args.arg(4).checkdouble().toFloat(),
                )
                return LuaInteger.valueOf(id.toInt())
            }
        })

        vr.set("setColor", object : VarArgFunctionCompat() {
            override fun invoke(args: Varargs): LuaValue {
                val ok = NativeSdk.setObjectColor(
                    args.arg(1).checklong(),
                    args.arg(2).checkdouble().toFloat(), args.arg(3).checkdouble().toFloat(),
                    args.arg(4).checkdouble().toFloat(),
                    if (args.narg() >= 5) args.arg(5).checkdouble().toFloat() else 1f,
                )
                return LuaBoolean.valueOf(ok)
            }
        })

        vr.set("removeObject", object : OneArgFunction() {
            override fun call(id: LuaValue): LuaValue =
                LuaBoolean.valueOf(NativeSdk.removeObject(id.checklong()))
        })

        vr.set("getHeadPose", object : ZeroArgFunctionCompat() {
            override fun call(): LuaValue {
                val pose = NativeSdk.getHeadPose() ?: return LuaValue.NIL
                val t = LuaTable()
                for (i in 0 until 12) t.set(i + 1, LuaDouble.valueOf(pose[i].toDouble()))
                return t
            }
        })

        vr.set("getHand", object : TwoArgFunction() {
            override fun call(self: LuaValue, left: LuaValue): LuaValue {
                // como os landmarks não são consultáveis de volta pelo JNI,
                // expomos o último estado conhecido via eventos GESTURE
                return lastHandState(left.checkboolean())
            }
        })

        vr.set("raycast", object : ZeroArgFunctionCompat() {
            override fun call(): LuaValue {
                // último hit de janela reportado via evento WINDOW_INPUT
                return lastRayHit ?: LuaValue.NIL
            }
        })

        vr.set("onGesture", object : OneArgFunction() {
            override fun call(fn: LuaValue): LuaValue {
                gestureCallback = fn
                return LuaValue.NIL
            }
        })

        vr.set("onEvent", object : OneArgFunction() {
            override fun call(fn: LuaValue): LuaValue {
                eventCallback = fn
                return LuaValue.NIL
            }
        })

        vr.set("setThermalBudget", object : OneArgFunction() {
            override fun call(b: LuaValue): LuaValue {
                NativeSdk.setThermalBudget(b.checkdouble().toFloat())
                return LuaValue.NIL
            }
        })

        vr.set("recenter", object : ZeroArgFunctionCompat() {
            override fun call(): LuaValue {
                NativeSdk.recenter()
                return LuaValue.NIL
            }
        })

        vr.set("log", object : OneArgFunction() {
            override fun call(msg: LuaValue): LuaValue {
                appendLog(msg.checkjstring())
                return LuaValue.NIL
            }
        })

        g.set("vr", vr)
    }

    // ------------------------------------------------------------------
    // Execução de scripts
    // ------------------------------------------------------------------
    /** Executa um script Lua do diretório assets. Retorna erro ou null. */
    fun runAsset(script: String): String? {
        initialize()
        return try {
            val src = context.assets.open("lua/$script").bufferedReader().use { it.readText() }
            globals!!.load(src, script).call()
            null
        } catch (e: Exception) {
            appendLog("ERRO: ${e.message}")
            e.message
        }
    }

    fun runSource(source: String): String? {
        initialize()
        return try {
            globals!!.load(source, "console").call()
            null
        } catch (e: Exception) {
            appendLog("ERRO: ${e.message}")
            e.message
        }
    }

    // ------------------------------------------------------------------
    // Eventos nativos → callbacks Lua
    // ------------------------------------------------------------------
    fun dispatchNativeEvent(ev: IntArray) {
        if (ev.size < 6) return
        val type = ev[0]
        val x = ev[3] / 1000.0
        val y = ev[4] / 1000.0
        val z = ev[5] / 1000.0

        if (type == NativeSdk.EVENT_GESTURE) {
            handGesture[ev[1] == 0] = Pair(ev[2], x.toFloat())
            gestureCallback?.invoke(
                LuaValue.varargsOf(
                    arrayOf(
                        LuaInteger.valueOf(ev[1]),
                        LuaString.valueOf(gestureName(ev[2])),
                        LuaDouble.valueOf(x),
                    ),
                ),
            )
        }
        if (type == NativeSdk.EVENT_WINDOW_INPUT) {
            // registra o último hit para vr.raycast()
            val t = LuaTable()
            t.set(1, LuaDouble.valueOf(x)) // u
            t.set(2, LuaDouble.valueOf(y)) // v
            t.set(3, LuaInteger.valueOf(ev[1])) // windowId
            lastRayHit = t
        }
        try {
            eventCallback?.invoke(
                LuaValue.varargsOf(
                    arrayOf(
                        LuaInteger.valueOf(type), LuaInteger.valueOf(ev[1]),
                        LuaInteger.valueOf(ev[2]), LuaDouble.valueOf(x),
                        LuaDouble.valueOf(y), LuaDouble.valueOf(z),
                    ),
                ),
            )
        } catch (_: Exception) {
        }
    }

    private var lastRayHit: LuaValue? = null

    // últimos gestos reportados (alimentado por dispatchNativeEvent)
    private val handGesture = HashMap<Boolean, Pair<Int, Float>>()

    private fun lastHandState(left: Boolean): LuaValue {
        val (g, s) = handGesture[left] ?: (NativeSdk.GESTURE_NONE to 0f)
        val t = LuaTable()
        t.set("gesture", LuaString.valueOf(gestureName(g)))
        t.set("pinch", LuaDouble.valueOf(s.toDouble()))
        return t
    }

    private fun gestureName(g: Int): String = when (g) {
        NativeSdk.GESTURE_OPEN_PALM -> "OPEN_PALM"
        NativeSdk.GESTURE_PINCH -> "PINCH"
        NativeSdk.GESTURE_GRAB -> "GRAB"
        NativeSdk.GESTURE_FIST -> "FIST"
        NativeSdk.GESTURE_POINT -> "POINT"
        NativeSdk.GESTURE_SWIPE -> "SWIPE"
        else -> "NONE"
    }

    // ------------------------------------------------------------------
    // Console (painel espacial nativo)
    // ------------------------------------------------------------------
    fun openConsole() {
        if (consolePanelId < 0) {
            consolePanelId = NativeSdk.addUiPanel(
                0.6f, -0.25f, -1.6f,
                0f, 0f, 0f, 1f,
                0.72f, 0.4f, "SDK Lua",
            )
            consoleLabelId = NativeSdk.addUiControl(consolePanelId, NativeSdk.UI_LABEL,
                0.04f, 0.08f, 0.92f, 0.84f, "Lua pronto.")
        }
        NativeSdk.setUiPanelVisible(consolePanelId, true)
    }

    fun closeConsole() {
        if (consolePanelId >= 0) NativeSdk.setUiPanelVisible(consolePanelId, false)
    }

    private val logBuffer = StringBuilder()

    private fun appendLog(msg: String) {
        if (consolePanelId < 0) return
        logBuffer.append(msg).append('\n')
        if (logBuffer.length > 400) logBuffer.delete(0, logBuffer.length - 400)
        NativeSdk.setUiControlLabel(
            consolePanelId, consoleLabelId,
            logBuffer.toString().replace("\n", "  ·  "),
        )
    }

    // compat: VarArgFunction com invoke(Varargs)
    private abstract class VarArgFunctionCompat : org.luaj.vm2.lib.VarArgFunction()
    private abstract class ZeroArgFunctionCompat : org.luaj.vm2.lib.ZeroArgFunction()
}
