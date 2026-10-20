#pragma once

#include <cmath>
#include <complex>

namespace qs
{

// Quantum scattering off a piecewise-constant potential, solved with transfer matrices.
//
// Units: hbar^2 / 2m = 1 and the barrier height V0 = 1, so a plane wave with energy E
// has wavenumber k = sqrt(E - V). Inside a barrier with E < 1, k is imaginary and the
// wave decays exponentially: that is quantum tunnelling.
//
// The filter maps audio frequency to particle energy (E = h f, normalised so that the
// "barrier" frequency is E = 1). Frequencies above the barrier pass easily, frequencies
// below it have to tunnel through.

enum class BarrierShape
{
    single,  // one rectangular barrier
    twin     // two barriers with a gap between them: resonant tunnelling
};

struct BarrierGeometry
{
    double width = 3.0;  // thickness of each barrier
    double gap = 6.0;    // distance between the two barriers (twin only)
    BarrierShape shape = BarrierShape::single;
};

struct Scattering
{
    double transmitted = 0.0;  // |t|, amplitude of the wave that gets through
    double reflected = 1.0;    // |r|, amplitude of the wave that bounces back
};

inline Scattering scatter (double energy, const BarrierGeometry& g)
{
    using C = std::complex<double>;

    if (energy <= 1.0e-12)
        return {};

    // Avoid k == 0 exactly at the top of the barrier, where the plane-wave basis degenerates.
    if (std::abs (energy - 1.0) < 1.0e-9)
        energy += 2.0e-9;

    const C i (0.0, 1.0);
    const C kOut = std::sqrt (C (energy, 0.0));
    const C kIn = std::sqrt (C (energy - 1.0, 0.0));

    struct M2
    {
        C a, b, c, d;
        M2 operator* (const M2& o) const
        {
            return { a * o.a + b * o.c, a * o.b + b * o.d,
                     c * o.a + d * o.c, c * o.b + d * o.d };
        }
    };

    // D maps the (A, B) coefficients of psi = A e^{ikx} + B e^{-ikx} to (psi, psi') at x.
    auto waveMatrix = [&] (C k, double x)
    {
        const C e = std::exp (i * k * x);
        return M2 { e, 1.0 / e, i * k * e, -i * k / e };
    };

    auto inverse = [] (const M2& m)
    {
        const C det = m.a * m.d - m.b * m.c;
        return M2 { m.d / det, -m.b / det, -m.c / det, m.a / det };
    };

    // Matching psi and psi' at the boundary x between a region with kLeft and one with kRight.
    auto boundary = [&] (C kLeft, C kRight, double x)
    {
        return inverse (waveMatrix (kLeft, x)) * waveMatrix (kRight, x);
    };

    const double w = g.width;
    M2 m = boundary (kOut, kIn, 0.0) * boundary (kIn, kOut, w);

    if (g.shape == BarrierShape::twin)
        m = m * boundary (kOut, kIn, w + g.gap) * boundary (kIn, kOut, 2.0 * w + g.gap);

    // Incoming from the left, nothing incoming from the right: (A_out, B_out) = (t, 0).
    const C t = 1.0 / m.a;
    const C r = m.c / m.a;
    return { std::abs (t), std::abs (r) };
}

} // namespace qs
