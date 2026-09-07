// BrazilMR — One Euro Filter (Casiez et al. 2012).
// Filtro adaptativo de passa-baixa: corta jitter em baixa velocidade,
// mantém baixa latência em alta velocidade. Base de todo o tracking.
#pragma once

#include "../math/MathTypes.hpp"

namespace brazilmr {

class LowPassFilter {
public:
    void reset() { hasPrev_ = false; prev_ = 0.0f; }
    bool hasPrev() const { return hasPrev_; }

    float filter(float x, float alpha) {
        float y = hasPrev_ ? lerpf(prev_, x, alpha) : x;
        prev_ = y;
        hasPrev_ = true;
        return y;
    }

    float last() const { return prev_; }

private:
    bool  hasPrev_ = false;
    float prev_    = 0.0f;
};

// Configuração exposta ao usuário (painel de calibração de filtros).
struct OneEuroConfig {
    float minCutoffHz = 1.2f;  // corte mínimo (menor = mais suave parado)
    float beta        = 0.05f; // coeficiente de velocidade (maior = mais responsivo)
    float dCutoffHz   = 1.0f;  // corte do filtro de derivada
};

class OneEuroFilter {
public:
    explicit OneEuroFilter(const OneEuroConfig& cfg = OneEuroConfig()) : cfg_(cfg) {}

    void configure(const OneEuroConfig& cfg) { cfg_ = cfg; }
    const OneEuroConfig& config() const { return cfg_; }

    void reset() { xFilter_.reset(); dxFilter_.reset(); first_ = true; }

    // @param x      valor cru
    // @param dtSec  delta de tempo desde a última amostra
    float filter(float x, float dtSec) {
        if (dtSec <= 0.0f) dtSec = 1e-4f;
        if (first_) {
            first_ = false;
            xFilter_.reset();
            dxFilter_.reset();
            return xFilter_.filter(x, 1.0f);
        }
        float dx = (x - xFilter_.last()) / dtSec;
        float edx = dxFilter_.filter(dx, alphaForCutoff(cfg_.dCutoffHz, dtSec));
        float cutoff = cfg_.minCutoffHz + cfg_.beta * std::fabs(edx);
        return xFilter_.filter(x, alphaForCutoff(cutoff, dtSec));
    }

    float last() const { return xFilter_.last(); }

private:
    static float alphaForCutoff(float cutoffHz, float dtSec) {
        float tau = 1.0f / (2.0f * kPi * cutoffHz);
        return 1.0f / (1.0f + tau / dtSec);
    }

    OneEuroConfig cfg_;
    LowPassFilter  xFilter_;
    LowPassFilter  dxFilter_;
    bool           first_ = true;
};

// Três canais independentes.
struct Vec3OneEuro {
    OneEuroFilter x, y, z;

    void configure(const OneEuroConfig& cfg) { x.configure(cfg); y.configure(cfg); z.configure(cfg); }
    void reset() { x.reset(); y.reset(); z.reset(); }
    Vec3 filter(const Vec3& v, float dtSec) {
        return {x.filter(v.x, dtSec), y.filter(v.y, dtSec), z.filter(v.z, dtSec)};
    }
};

// Filtragem de quaternion no espaço de erro (evita artefatos de filtrar
// componentes independentes): mede o "erro" rotacional, suaviza, aplica de volta.
struct QuatOneEuro {
    void configure(const OneEuroConfig& cfg) { errX.configure(cfg); errY.configure(cfg); errZ.configure(cfg); }
    void reset() { errX.reset(); errY.reset(); errZ.reset(); hasPrev_ = false; }

    Quat filter(const Quat& q, float dtSec) {
        if (!hasPrev_) {
            hasPrev_ = true;
            q_ = q.normalized();
            return q_;
        }
        // erro = log(q_meas * q_est^-1)
        Quat qErrLocal = q * q_.conjugate();
        if (qErrLocal.w < 0.0f) qErrLocal = {-qErrLocal.x, -qErrLocal.y, -qErrLocal.z, -qErrLocal.w};
        Vec3 ev = quatLog(qErrLocal);
        Vec3 smoothed{errX.filter(ev.x, dtSec), errY.filter(ev.y, dtSec), errZ.filter(ev.z, dtSec)};
        q_ = (quatExp(smoothed) * q_).normalized();
        return q_;
    }

    Quat current() const { return q_; }

private:
    OneEuroFilter errX, errY, errZ;
    Quat q_ = Quat::identity();
    bool hasPrev_ = false;
};

} // namespace brazilmr
