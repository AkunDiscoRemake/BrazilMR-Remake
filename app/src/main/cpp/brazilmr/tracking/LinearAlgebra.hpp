// BrazilMR — álgebra linear densa mínima para odometria visual.
// Jacobi eigenvalue (matrizes simétricas pequenas) + utilidades.
// Sem dependências externas (Eigen/OpenCV foram evitados de propósito:
// binário menor, compilação mais rápida, controle de memória total).
#pragma once

#include <cmath>
#include <cstdint>

namespace brazilmr {

// Decomposição espectral de matriz simétrica n×n (n <= 16) pelo método de
// Jacobi. A é modificada. V = autovetores (colunas), em ordem crescente de
// autovalor em eigen[] indexado como (eigIdx, dim) → V[eigIdx * n + dim].
// Retorna número de rotações realizadas.
inline int jacobiEigenSymmetric(float* A, int n, float* eigenvalues, float* V) {
    for (int i = 0; i < n; ++i) {
        eigenvalues[i] = A[i * n + i];
        for (int j = 0; j < n; ++j) V[i * n + j] = (i == j) ? 1.0f : 0.0f;
    }
    const int kMaxSweeps = 30;
    int rotations = 0;
    for (int sweep = 0; sweep < kMaxSweeps; ++sweep) {
        float off = 0.0f;
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j) off += A[i * n + j] * A[i * n + j];
        if (off < 1e-14f) break;
        for (int p = 0; p < n; ++p) {
            for (int q = p + 1; q < n; ++q) {
                float apq = A[p * n + q];
                if (std::fabs(apq) < 1e-15f) continue;
                float theta = (A[q * n + q] - A[p * n + p]) / (2.0f * apq);
                float t = (theta >= 0.0f ? 1.0f : -1.0f) /
                          (std::fabs(theta) + std::sqrt(theta * theta + 1.0f));
                float c = 1.0f / std::sqrt(t * t + 1.0f);
                float s = t * c;
                for (int k = 0; k < n; ++k) {
                    float akp = A[k * n + p], akq = A[k * n + q];
                    A[k * n + p] = c * akp - s * akq;
                    A[k * n + q] = s * akp + c * akq;
                }
                for (int k = 0; k < n; ++k) {
                    float apk = A[p * n + k], aqk = A[q * n + k];
                    A[p * n + k] = c * apk - s * aqk;
                    A[q * n + k] = s * apk + c * aqk;
                }
                for (int k = 0; k < n; ++k) {
                    float vkp = V[k * n + p], vkq = V[k * n + q];
                    V[k * n + p] = c * vkp - s * vkq;
                    V[k * n + q] = s * vkp + c * vkq;
                }
                ++rotations;
            }
        }
        for (int i = 0; i < n; ++i) eigenvalues[i] = A[i * n + i];
    }
    // ordena crescente (seleção simples; n pequeno)
    for (int i = 0; i < n - 1; ++i) {
        int minIdx = i;
        for (int j = i + 1; j < n; ++j)
            if (eigenvalues[j] < eigenvalues[minIdx]) minIdx = j;
        if (minIdx != i) {
            float tmp = eigenvalues[i];
            eigenvalues[i] = eigenvalues[minIdx];
            eigenvalues[minIdx] = tmp;
            for (int k = 0; k < n; ++k) {
                float tv = V[k * n + i];
                V[k * n + i] = V[k * n + minIdx];
                V[k * n + minIdx] = tv;
            }
        }
    }
    return rotations;
}

// SVD 3x3 via EᵀE (autodecomposição de Jacobi). Adequado para forçar
// posto 2 da matriz essencial. A não precisa ser simétrica.
// U, V: colunas; S: singulares (decrescente). Retorna true se ok.
inline bool svd3x3(const float A[9], float U[9], float S[3], float V[9]) {
    // AtA = V D Vᵀ
    float AtA[9];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < 3; ++k) sum += A[k * 3 + i] * A[k * 3 + j];
            AtA[i * 3 + j] = sum;
        }
    float eig[3], Vtmp[9];
    jacobiEigenSymmetric(AtA, 3, eig, Vtmp);
    // autovetores em ordem crescente → invertemos p/ decrescente
    for (int i = 0; i < 3; ++i) {
        int src = 2 - i;
        S[i] = std::sqrt(std::fabs(eig[src]));
        for (int k = 0; k < 3; ++k) V[k * 3 + i] = Vtmp[k * 3 + src];
    }
    if (S[0] < 1e-10f) return false;
    if (S[1] < 1e-10f) S[1] = 1e-10f; // guarda contra divisão por zero
    // U col j = (A * V col j) / S[j]; terceira coluna = produto vetorial
    float u0[3], u1[3];
    for (int k = 0; k < 3; ++k) {
        u0[k] = 0.0f;
        u1[k] = 0.0f;
        for (int m = 0; m < 3; ++m) {
            u0[k] += A[k * 3 + m] * V[m * 3 + 0];
            u1[k] += A[k * 3 + m] * V[m * 3 + 1];
        }
    }
    for (int k = 0; k < 3; ++k) { u0[k] /= S[0]; u1[k] /= S[1]; }
    float u2[3] = {
        u0[1] * u1[2] - u0[2] * u1[1],
        u0[2] * u1[0] - u0[0] * u1[2],
        u0[0] * u1[1] - u0[1] * u1[0]
    };
    for (int k = 0; k < 3; ++k) {
        U[k * 3 + 0] = u0[k];
        U[k * 3 + 1] = u1[k];
        U[k * 3 + 2] = u2[k];
    }
    return true;
}

} // namespace brazilmr
