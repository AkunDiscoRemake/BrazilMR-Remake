// BrazilMR — fusão de sensores complementar (gyro + accel + mag opcional).
// 3DoF: orientação da cabeça estável, sem jitter, sem drift perceptível de yaw
// quando há magnetômetro; com magnetômetro ausente o yaw deriva lentamente
// (comportamento padrão de headsets Cardboard) e pode ser recentrado.
#pragma once

#include "../math/MathTypes.hpp"

namespace brazilmr {

class ImuFusion {
public:
    // @param gravityWorld  vetor da gravidade no referencial do mundo (ex.: {0,0,9.81})
    // @param northWorld    direção do norte magnético no mundo (quando disponível)
    // Mundo do engine: X direita, Y PARA CIMA, -Z à frente (convenção GL).
    // Em repouso o acelerômetro (frame da cabeça, já remapeado pela camada
    // Java) lê +9.81 em Y.
    ImuFusion()
        : gravityWorld_{0.0f, 9.81f, 0.0f},
          northWorld_{1.0f, 0.0f, 0.0f} {}

    void reset() {
        q_ = Quat::identity();
        bias_ = {0, 0, 0};
        initialized_ = false;
        staticFrames_ = 0;
    }

    // Última taxa de giroscópio (rad/s), para predição de pose.
    Vec3 lastGyro() const { return lastGyro_; }

    // @param gyro   rad/s, eixos do SENSOR (já remapeado p/ cabeça na camada Java)
    // @param accel  m/s²
    // @param mag    µT (opcional — nullptr se indisponível)
    // @param dtSec  delta desde a última chamada
    Quat update(const Vec3& gyro, const Vec3& accel, const Vec3* mag, float dtSec) {
        if (dtSec <= 0.0f) return q_;
        if (!initialized_) {
            // Inicializa com a direção da gravidade (z do mundo contra gravidade).
            Vec3 a = accel.normalized();
            if (a.lengthSq() > 0.5f) {
                // alinha a direção "para cima" do corpo com +Y do mundo
                Vec3 from = a;            // aponta "para cima" no corpo
                Vec3 to{0.0f, 1.0f, 0.0f}; // para cima no mundo (Y-up)
                q_ = quatBetween(from, to);
                initialized_ = true;
            }
            lastGyro_ = gyro;
            return q_;
        }

        lastGyro_ = gyro;
        Vec3 w = gyro - bias_;

        // 1) Integração giroscópica (ω no FRAME DO CORPO → pós-multiplica)
        float angle = w.length() * dtSec;
        if (angle > 1e-7f) {
            Quat dq = Quat::fromAxisAngle(w, angle);
            q_ = (q_ * dq).normalized();
        }

        // 2) Correção por acelerômetro (roll/pitch).
        float accelMag = accel.length();
        float gMag = gravityWorld_.length();
        // Confiança alta só quando |a| ≈ g (aparelho sem aceleração linear).
        float dev = std::fabs(accelMag - gMag) / gMag;
        float accelTrust = dev < 0.15f ? 1.0f - dev * 4.0f : 0.0f;
        accelTrust = clampf(accelTrust, 0.0f, 1.0f);
        if (accelTrust > 0.0f) {
            Vec3 aBody = accel.normalized();
            Vec3 expected = q_.conjugate().rotate(Vec3{0.0f, 1.0f, 0.0f});
            Vec3 err = aBody.cross(expected); // eixo no frame do corpo
            float gain = 0.05f * accelTrust;
            Quat corr = Quat::fromAxisAngle(err, err.length() * gain * dtSec * 60.0f);
            q_ = (q_ * corr).normalized();
        }

        // 3) Correção por magnetômetro (yaw) — só quando o campo é plausível.
        if (mag) {
            float mMag = mag->length();
            if (mMag > 15.0f && mMag < 90.0f) {
                // Projeta o campo no plano horizontal do mundo (Y-up → plano XZ).
                Vec3 mWorld = q_.rotate(*mag);
                Vec3 mHoriz = Vec3{mWorld.x, 0.0f, mWorld.z};
                Vec3 nHoriz = Vec3{northWorld_.x, 0.0f, northWorld_.z};
                if (mHoriz.lengthSq() > 1e-4f && nHoriz.lengthSq() > 1e-4f) {
                    mHoriz = mHoriz.normalized();
                    nHoriz = nHoriz.normalized();
                    float dot = clampf(mHoriz.dot(nHoriz), -1.0f, 1.0f);
                    float angle = std::acos(dot);
                    Vec3 axis = mHoriz.cross(nHoriz);
                    if (axis.lengthSq() > 1e-6f && std::fabs(angle) > 0.001f) {
                        // Corrige apenas yaw, com ganho conservador.
                        float gain = 0.02f;
                        if (axis.z < 0.0f) angle = -angle;
                        Quat corr = Quat::fromAxisAngle(Vec3{0, 0, 1}, angle * gain);
                        q_ = (corr * q_).normalized(); // frame do MUNDO
                    }
                }
            }
        }
        return q_;
    }

    // Calibração de bias do giroscópio: chamar com o aparelho parado.
    // Retorna true quando concluída.
    bool calibrateGyroBias(const Vec3& gyro, int samples = 40) {
        if (staticFrames_ == 0) biasSum_ = {0, 0, 0};
        biasSum_ += gyro;
        ++staticFrames_;
        if (staticFrames_ >= samples) {
            bias_ = biasSum_ * (1.0f / static_cast<float>(samples));
            staticFrames_ = 0;
            return true;
        }
        return false;
    }

    // Recenter: zera o yaw mantendo pitch/roll (o clássico "olhe para frente").
    void recenterYaw() {
        float yaw, pitch, roll;
        q_.toEulerYXZ(yaw, pitch, roll);
        q_ = Quat::fromEulerYXZ(0.0f, pitch, roll);
    }

    Quat orientation() const { return q_; }
    Vec3 gyroBias() const { return bias_; }

    void setGravityWorld(const Vec3& g) { gravityWorld_ = g; }

private:
    // Quaternion de menor rotação entre dois vetores.
    static Quat quatBetween(const Vec3& from, const Vec3& to) {
        Vec3 f = from.normalized();
        Vec3 t = to.normalized();
        float d = clampf(f.dot(t), -1.0f, 1.0f);
        if (d > 0.99999f) return Quat::identity();
        if (d < -0.99999f) {
            // vetores opostos: eixo ortogonal arbitrário
            Vec3 axis = std::fabs(f.x) < 0.9f ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
            Vec3 ortho = f.cross(axis).normalized();
            return Quat::fromAxisAngle(ortho, kPi);
        }
        Vec3 axis = f.cross(t);
        float s = std::sqrt((1.0f + d) * 2.0f);
        return Quat{axis.x / s, axis.y / s, axis.z / s, s * 0.5f}.normalized();
    }

    Quat q_ = Quat::identity();
    Vec3  bias_{0, 0, 0};
    Vec3  biasSum_{0, 0, 0};
    Vec3  lastGyro_{0, 0, 0};
    Vec3  gravityWorld_;
    Vec3  northWorld_;
    bool  initialized_ = false;
    int   staticFrames_ = 0;
};

} // namespace brazilmr
