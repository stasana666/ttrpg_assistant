#pragma once

#include <pf2e_engine/position.h>

#include <cmath>
#include <numbers>

struct TVector2D {
    double x;
    double y;

    TVector2D() : x(0), y(0) {}
    TVector2D(double x, double y) : x(x), y(y) {}
    explicit TVector2D(TPosition pos) : x(pos.x), y(pos.y) {}

    TVector2D operator+(const TVector2D& other) const {
        return TVector2D(x + other.x, y + other.y);
    }

    TVector2D operator-(const TVector2D& other) const {
        return TVector2D(x - other.x, y - other.y);
    }

    TVector2D operator*(double scalar) const {
        return TVector2D(x * scalar, y * scalar);
    }

    TVector2D operator/(double scalar) const {
        return TVector2D(x / scalar, y / scalar);
    }

    double Dot(const TVector2D& other) const {
        return x * other.x + y * other.y;
    }

    double Cross(const TVector2D& other) const {
        return x * other.y - y * other.x;
    }

    double Length() const {
        return std::sqrt(x * x + y * y);
    }

    double LengthSquared() const {
        return x * x + y * y;
    }

    TVector2D Normalized() const {
        double len = Length();
        if (len < kEpsilon) {
            return TVector2D(0, 0);
        }
        return *this / len;
    }

    bool IsZero() const {
        return Length() < kEpsilon;
    }

    static constexpr double kEpsilon = 0.001;
    // cos(pi/4) = 1/sqrt(2) = sqrt(2)/2
    static constexpr double kConeHalfAngleCos = std::numbers::sqrt2 / 2.0;
};
