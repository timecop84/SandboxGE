#ifndef VECTOR_H
#define VECTOR_H

#include <glm/glm.hpp>
#include <iostream>
#include <cmath>

//----------------------------------------------------------------------------------------------------------------------
/// @brief Modern Vector class using GLM as backend instead of ngl_compat
/// @details Interface-compatible replacement for ngl::Vector using glm::vec3
//----------------------------------------------------------------------------------------------------------------------
class Vector {
public:
    float m_x, m_y, m_z;
    
    Vector() : m_x(0.0f), m_y(0.0f), m_z(0.0f) {}
    Vector(float x, float y, float z) : m_x(x), m_y(y), m_z(z) {}
    Vector(float x, float y, float z, float /* w */) : m_x(x), m_y(y), m_z(z) {} // w component ignored for 3D vector
    Vector(const glm::vec3& v) : m_x(v.x), m_y(v.y), m_z(v.z) {}
    
    // Conversion operators
    operator glm::vec3() const { return glm::vec3(m_x, m_y, m_z); }
    
    // Setters
    void set(float x, float y, float z) { m_x = x; m_y = y; m_z = z; }
    void set(const Vector& v) { m_x = v.m_x; m_y = v.m_y; m_z = v.m_z; }
    void setX(float x) { m_x = x; }
    void setY(float y) { m_y = y; }
    void setZ(float z) { m_z = z; }
    
    // Getters (for compatibility)
    float getX() const { return m_x; }
    float getY() const { return m_y; }
    float getZ() const { return m_z; }
    
    // Operators
    Vector operator+(const Vector& v) const { return Vector(m_x + v.m_x, m_y + v.m_y, m_z + v.m_z); }
    Vector operator-(const Vector& v) const { return Vector(m_x - v.m_x, m_y - v.m_y, m_z - v.m_z); }
    Vector operator*(float scalar) const { return Vector(m_x * scalar, m_y * scalar, m_z * scalar); }
    Vector operator*(const Vector& v) const { return Vector(m_x * v.m_x, m_y * v.m_y, m_z * v.m_z); }
    Vector operator/(float scalar) const { return Vector(m_x / scalar, m_y / scalar, m_z / scalar); }
    
    Vector& operator+=(const Vector& v) { m_x += v.m_x; m_y += v.m_y; m_z += v.m_z; return *this; }
    Vector& operator-=(const Vector& v) { m_x -= v.m_x; m_y -= v.m_y; m_z -= v.m_z; return *this; }
    Vector& operator*=(float scalar) { m_x *= scalar; m_y *= scalar; m_z *= scalar; return *this; }
    Vector& operator/=(float scalar) { m_x /= scalar; m_y /= scalar; m_z /= scalar; return *this; }
    
    bool operator==(const Vector& v) const { return m_x == v.m_x && m_y == v.m_y && m_z == v.m_z; }
    bool operator!=(const Vector& v) const { return !(*this == v); }
    
    // Utility functions - inline implementations
    float length() const { return std::sqrt(m_x * m_x + m_y * m_y + m_z * m_z); }
    float lengthSquared() const { return m_x * m_x + m_y * m_y + m_z * m_z; }
    Vector normalize() const {
        float len = length();
        if (len > 0.0001f) return Vector(m_x / len, m_y / len, m_z / len);
        return Vector(0.0f, 0.0f, 0.0f);
    }
    void normalizeIP() {
        float len = length();
        if (len > 0.0001f) { m_x /= len; m_y /= len; m_z /= len; }
    }
    float dot(const Vector& v) const { return m_x * v.m_x + m_y * v.m_y + m_z * v.m_z; }
    Vector cross(const Vector& v) const {
        return Vector(
            m_y * v.m_z - m_z * v.m_y,
            m_z * v.m_x - m_x * v.m_z,
            m_x * v.m_y - m_y * v.m_x
        );
    }
    
    // Static functions for compatibility
    static Vector zero() { return Vector(0, 0, 0); }
    static Vector up() { return Vector(0, 1, 0); }
    static Vector right() { return Vector(1, 0, 0); }
    static Vector forward() { return Vector(0, 0, -1); }
    
    // Stream operators
    friend std::ostream& operator<<(std::ostream& os, const Vector& v) {
        os << "(" << v.m_x << ", " << v.m_y << ", " << v.m_z << ")";
        return os;
    }
};

// Global operators for scalar multiplication
inline Vector operator*(float scalar, const Vector& v) {
    return Vector(v.m_x * scalar, v.m_y * scalar, v.m_z * scalar);
}

#endif // VECTOR_H
