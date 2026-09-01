#pragma once

#include <cmath>

namespace jogo {

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

constexpr Vec3 operator+(Vec3 a, Vec3 b) { return Vec3{a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr Vec3 operator-(Vec3 a, Vec3 b) { return Vec3{a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr Vec3 operator-(Vec3 v) { return Vec3{-v.x, -v.y, -v.z}; }
constexpr Vec3 operator*(Vec3 v, float e) { return Vec3{v.x * e, v.y * e, v.z * e}; }
constexpr Vec3 operator*(float e, Vec3 v) { return v * e; }
constexpr Vec3& operator+=(Vec3& a, Vec3 b) { a = a + b; return a; }

constexpr float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

constexpr Vec3 cross(Vec3 a, Vec3 b) {
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float comprimento(Vec3 v) { return std::sqrt(dot(v, v)); }

inline Vec3 normalizar(Vec3 v) {
    const float tamanho = comprimento(v);
    return tamanho > 1e-6f ? v * (1.0f / tamanho) : Vec3{0.0f, 0.0f, -1.0f};
}

constexpr Vec3 lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }

/// Base ortonormal 3x3 guardada por colunas. Multiplicar por um vetor leva do
/// espaco local para o espaco pai; a transposta faz o caminho inverso.
struct Mat3 {
    Vec3 colunas[3]{Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.0f, 0.0f, 1.0f}};

    static constexpr Mat3 identidade() { return Mat3{}; }

    /// Rotacao aplicada na ordem roll (Z), pitch (X) e yaw (Y).
    static Mat3 deEuler(float yaw, float pitch, float roll) {
        const float cy = std::cos(yaw);
        const float sy = std::sin(yaw);
        const float cp = std::cos(pitch);
        const float sp = std::sin(pitch);
        const float cr = std::cos(roll);
        const float sr = std::sin(roll);

        Mat3 r;
        r.colunas[0] = Vec3{cy * cr + sy * sp * sr, cp * sr, -sy * cr + cy * sp * sr};
        r.colunas[1] = Vec3{-cy * sr + sy * sp * cr, cp * cr, sy * sr + cy * sp * cr};
        r.colunas[2] = Vec3{sy * cp, -sp, cy * cp};
        return r;
    }

    /// Base de uma camera/objeto que olha em `frente`: colunas direita, cima e
    /// "para tras" (-frente), a convencao usada pela projecao.
    static Mat3 olhandoPara(Vec3 frente, Vec3 cimaReferencia = Vec3{0.0f, 1.0f, 0.0f}) {
        const Vec3 f = normalizar(frente);
        Vec3 direita = cross(f, cimaReferencia);
        if (comprimento(direita) < 1e-4f) {
            // Olhando reto para cima/baixo: escolhe outra referencia.
            direita = cross(f, Vec3{0.0f, 0.0f, 1.0f});
        }
        direita = normalizar(direita);
        const Vec3 cima = cross(direita, f);
        return Mat3{{direita, cima, -f}};
    }

    Vec3 operator*(Vec3 v) const {
        return colunas[0] * v.x + colunas[1] * v.y + colunas[2] * v.z;
    }

    Mat3 operator*(const Mat3& outra) const {
        return Mat3{{*this * outra.colunas[0], *this * outra.colunas[1],
                     *this * outra.colunas[2]}};
    }

    /// Para bases ortonormais, a transposta e a inversa.
    Vec3 aplicarTransposta(Vec3 v) const {
        return Vec3{dot(v, colunas[0]), dot(v, colunas[1]), dot(v, colunas[2])};
    }

    Vec3 direita() const { return colunas[0]; }
    Vec3 cima() const { return colunas[1]; }
    Vec3 frente() const { return -colunas[2]; }
};

}  // namespace jogo
