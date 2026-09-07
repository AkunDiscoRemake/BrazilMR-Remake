/*
 * BrazilMR — gerador de atlas de fonte (lado Kotlin).
 *
 * O nativo NÃO renderiza texto: ele desenha quads com o atlas que o Kotlin
 * gera e faz upload via GLES30. Layout das métricas (stride 8):
 *   [count, baseHeightPx, (code, x, y, w, h, xoff, yoff, advance) * count]
 * Convenções:
 *   x,y,w,h — retângulo no atlas (px)
 *   xoff    — deslocamento horizontal a partir do cursor (bearing esq.)
 *   yoff    — ascent (distância da baseline ao TOPO do glifo)
 *   advance — avanço horizontal completo
 */
package com.brazilmr.platform

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Typeface
import com.brazilmr.sdk.NativeSdk

object FontAtlas {

    /** Altura base da fonte em px (referência de escala do UI nativo). */
    const val BASE_HEIGHT_PX = 64f

    /** Charset: ASCII imprimível + acentos PT-BR + símbolos úteis. */
    private const val EXTRA = "ÁÀÂÃÄÇÉÊÍÓÔÕÖÚÜáàâãäçéêíóôõöúüñÑ°±×÷•←→↑↓“”‘’…–—•◦◎●◆★☆"

    fun charset(): CharArray {
        val sb = StringBuilder()
        for (c in 32..126) sb.append(c.toChar()) // ASCII imprimível
        sb.append(EXTRA)
        return sb.toString().toCharArray()
    }

    /** Gera atlas + métricas. Deve rodar no thread GL (upload). */
    fun install() {
        val chars = charset()
        val cellPx = (BASE_HEIGHT_PX * 1.6f).toInt() // célula com folga p/ ascent
        val cols = 16
        val rows = (chars.size + cols - 1) / cols
        val atlasW = cols * cellPx
        val atlasH = rows * cellPx

        val bitmap = Bitmap.createBitmap(atlasW, atlasH, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)
        canvas.drawColor(Color.TRANSPARENT)

        val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            typeface = Typeface.create(Typeface.DEFAULT, Typeface.NORMAL)
            textSize = BASE_HEIGHT_PX
            color = Color.WHITE
        }
        val fm = paint.fontMetrics
        // baseline dentro da célula: topo + folga para ascent
        val ascentPad = (-fm.ascent).coerceAtLeast(1f) + 4f

        val metrics = FloatArray(2 + chars.size * 8)
        metrics[0] = chars.size.toFloat()
        metrics[1] = BASE_HEIGHT_PX

        for ((i, ch) in chars.withIndex()) {
            val col = i % cols
            val row = i / cols
            val cx = col * cellPx
            val cy = row * cellPx
            val baseline = cy + ascentPad

            // glifo branco sobre fundo transparente
            canvas.drawText(ch.toString(), cx + 2f, baseline, paint)

            // limites reais do glifo (evita desperdiçar alpha vazio)
            val w = Math.ceil(paint.measureText(ch.toString()).toDouble()).toInt()
                .coerceIn(1, cellPx - 4)
            // altura real: usar ascent/descent da fonte
            val top = (baseline + fm.ascent).toInt()
            val h = (Math.ceil(fm.descent.toDouble()) -
                Math.floor(fm.ascent.toDouble())).toInt().coerceIn(1, cellPx)

            metrics[2 + i * 8 + 0] = ch.code.toFloat()
            metrics[2 + i * 8 + 1] = (cx + 2).toFloat()
            metrics[2 + i * 8 + 2] = top.toFloat()
            metrics[2 + i * 8 + 3] = w.toFloat()
            metrics[2 + i * 8 + 4] = h.toFloat()
            metrics[2 + i * 8 + 5] = 0f // xoff: drawText já desenha a partir de cx+2
            metrics[2 + i * 8 + 6] = (baseline - top).toFloat() // ascent até o topo
            metrics[2 + i * 8 + 7] = paint.measureText(ch.toString()) // advance
        }

        val texId = NativeSdk.uploadBitmap(bitmap, linear = true)
        bitmap.recycle()
        NativeSdk.setFontAtlas(texId, atlasW, atlasH, metrics)
    }
}
