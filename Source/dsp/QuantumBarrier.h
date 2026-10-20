#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

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

// A region of constant potential. The particle comes in from the left where V = 0.
struct Segment
{
    double length;
    double potential;
};

// Fixed capacity, so building one on the audio thread never allocates.
struct Potential
{
    static constexpr int maxSegments = 15;

    std::array<Segment, maxSegments> segments {};
    int count = 0;
    double right = 0.0;  // potential beyond the last segment (1 for a potential step)

    void add (Segment s)
    {
        if (count < maxSegments)
            segments[(size_t) count++] = s;
    }

    double length() const
    {
        double total = 0.0;
        for (int j = 0; j < count; ++j)
            total += segments[(size_t) j].length;
        return total;
    }
};

enum class BarrierShape
{
    single,   // one rectangular barrier
    twin,     // two barriers with a gap between them: resonant tunnelling
    lattice,  // a row of identical barriers: a tiny crystal with energy bands
    well,     // a deep square well instead of a barrier: Ramsauer-Townsend resonances
    step      // the potential steps up to V0 and stays there
};

struct BarrierGeometry
{
    double width = 3.0;  // thickness of each barrier (or of the well)
    double gap = 6.0;    // distance between neighbouring barriers (twin and lattice)
    BarrierShape shape = BarrierShape::single;
    int latticeCells = 5;
    double wellDepth = 6.0;  // in units of V0
};

inline Potential makePotential (const BarrierGeometry& g)
{
    Potential p;
    switch (g.shape)
    {
        case BarrierShape::single:
            p.add ({ g.width, 1.0 });
            break;
        case BarrierShape::twin:
            p.add ({ g.width, 1.0 });
            p.add ({ g.gap, 0.0 });
            p.add ({ g.width, 1.0 });
            break;
        case BarrierShape::lattice:
            for (int n = 0; n < g.latticeCells; ++n)
            {
                if (n > 0)
                    p.add ({ g.gap, 0.0 });
                p.add ({ g.width, 1.0 });
            }
            break;
        case BarrierShape::well:
            p.add ({ g.width, -g.wellDepth });
            break;
        case BarrierShape::step:
            p.right = 1.0;
            break;
    }
    return p;
}

struct Scattering
{
    double transmitted = 0.0;  // amplitude of what gets through, sqrt of the transmitted flux
    double reflected = 1.0;    // |r|, amplitude of the wave that bounces back
};

namespace detail
{
using C = std::complex<double>;

struct M2
{
    C a, b, c, d;
    M2 operator* (const M2& o) const
    {
        return { a * o.a + b * o.c, a * o.b + b * o.d,
                 c * o.a + d * o.c, c * o.b + d * o.d };
    }
};

// Wavenumber in a region; avoids k == 0 exactly, where the plane-wave basis degenerates.
inline C wavenumber (double energy, double potential)
{
    double kinetic = energy - potential;
    if (std::abs (kinetic) < 1.0e-9)
        kinetic = 2.0e-9;
    return std::sqrt (C (kinetic, 0.0));
}

// Maps the (A, B) coefficients of psi = A e^{ikx} + B e^{-ikx} to (psi, psi') at x.
inline M2 waveMatrix (C k, double x)
{
    const C i (0.0, 1.0);
    const C e = std::exp (i * k * x);
    return M2 { e, 1.0 / e, i * k * e, -i * k / e };
}

inline M2 inverse (const M2& m)
{
    const C det = m.a * m.d - m.b * m.c;
    return M2 { m.d / det, -m.b / det, -m.c / det, m.a / det };
}

// Matching psi and psi' at x between a region with kLeft and one with kRight: takes the
// right region's coefficients to the left region's.
inline M2 boundary (C kLeft, C kRight, double x)
{
    return inverse (waveMatrix (kLeft, x)) * waveMatrix (kRight, x);
}

// Wavenumbers of every region, from the free region on the left to the one on the right.
struct Wavenumbers
{
    std::array<C, Potential::maxSegments + 2> k;
    size_t size;
};

inline Wavenumbers regionWavenumbers (double energy, const Potential& p)
{
    Wavenumbers w {};
    w.k[0] = wavenumber (energy, 0.0);
    for (int j = 0; j < p.count; ++j)
        w.k[(size_t) j + 1] = wavenumber (energy, p.segments[(size_t) j].potential);
    w.size = (size_t) p.count + 2;
    w.k[w.size - 1] = wavenumber (energy, p.right);
    return w;
}
} // namespace detail

inline Scattering scatter (double energy, const Potential& p)
{
    using namespace detail;

    if (energy <= 1.0e-12)
        return {};

    const auto [k, size] = regionWavenumbers (energy, p);
    M2 m { 1.0, 0.0, 0.0, 1.0 };
    double x = 0.0;
    for (size_t j = 0; j + 1 < size; ++j)
    {
        m = m * boundary (k[j], k[j + 1], x);
        if (j < (size_t) p.count)
            x += p.segments[j].length;
    }

    // Incoming from the left, nothing incoming from the right: (A_out, B_out) = (t, 0).
    const C t = 1.0 / m.a;
    const C r = m.c / m.a;

    // Flux carried on the right relative to the incoming flux (zero if the wave is evanescent there).
    const double fluxRatio = std::max (0.0, k[size - 1].real()) / k[0].real();
    return { std::sqrt (fluxRatio) * std::abs (t), std::abs (r) };
}

inline Scattering scatter (double energy, const BarrierGeometry& g)
{
    return scatter (energy, makePotential (g));
}

// Real part of the stationary wavefunction for a wave of unit amplitude coming in from the
// left, at each position in xs (the potential starts at x = 0).
inline std::vector<double> wavefunction (double energy, const Potential& p, const std::vector<double>& xs)
{
    using namespace detail;

    const auto [k, size] = regionWavenumbers (energy, p);
    std::vector<double> edges { 0.0 };
    for (int j = 0; j < p.count; ++j)
        edges.push_back (edges.back() + p.segments[(size_t) j].length);

    // Walk from the right, where only the transmitted wave exists, back to the left.
    std::vector<std::pair<C, C>> coeffs (size);
    coeffs.back() = { 1.0, 0.0 };
    for (size_t j = size - 1; j > 0; --j)
    {
        const auto m = boundary (k[j - 1], k[j], edges[j - 1]);
        const auto& [a, b] = coeffs[j];
        coeffs[j - 1] = { m.a * a + m.b * b, m.c * a + m.d * b };
    }

    // Normalise so the incoming wave has amplitude 1.
    const C scale = 1.0 / coeffs.front().first;
    const C i (0.0, 1.0);

    std::vector<double> psi;
    psi.reserve (xs.size());
    for (double x : xs)
    {
        size_t region = 0;
        while (region < edges.size() && x >= edges[region])
            ++region;
        const auto& [a, b] = coeffs[region];
        psi.push_back ((scale * (a * std::exp (i * k[region] * x) + b * std::exp (-i * k[region] * x))).real());
    }
    return psi;
}

// A smooth barrier V(x) = sech^2(alpha x) (Poschl-Teller), which has an exact solution.
// No sharp edges means no over-the-barrier ripples: tunnelling at its cleanest.
inline Scattering scatterSoft (double energy, double alpha)
{
    if (energy <= 1.0e-12)
        return {};

    const double pi = 3.14159265358979323846;
    const double kArg = pi * std::sqrt (energy) / alpha;
    if (kArg > 300.0)
        return { 1.0, 0.0 };

    const double s = 4.0 / (alpha * alpha) - 1.0;
    const double edge = s > 0.0 ? std::cosh (0.5 * pi * std::sqrt (s)) : std::cos (0.5 * pi * std::sqrt (-s));
    const double sinhK = std::sinh (kArg);
    const double transmission = sinhK * sinhK / (sinhK * sinhK + edge * edge);
    return { std::sqrt (transmission), std::sqrt (std::max (0.0, 1.0 - transmission)) };
}

// Bound systems: instead of scattering, these have discrete energy levels. Each level is a
// spectral line with a Lorentzian shape whose width stands for the state's lifetime.
enum class Spectrum
{
    box,         // particle in a box: E_n = n^2 E_1
    oscillator,  // harmonic oscillator: E_n = (n + 1/2) hbar omega, evenly spaced
    hydrogen     // Rydberg series E_n = 1 - 1/n^2 crowding up to the ionisation limit at E = 1
};

inline double lorentzian (double energy, double level, double halfWidth)
{
    const double d = (energy - level) / halfWidth;
    return 1.0 / (1.0 + d * d);
}

// Line strength at this energy, 0..1 (the strongest nearby line wins). The line width is
// relative: to each level for the box and hydrogen, to the level spacing for the oscillator.
inline double spectrum (double energy, Spectrum kind, double relativeWidth)
{
    if (energy <= 0.0)
        return 0.0;

    double strongest = 0.0;
    switch (kind)
    {
        case Spectrum::box:
        {
            const int nearest = (int) std::lround (std::sqrt (energy));
            for (int n = std::max (1, nearest - 2); n <= nearest + 2; ++n)
                strongest = std::max (strongest, lorentzian (energy, (double) n * n, relativeWidth * n * n));
            break;
        }
        case Spectrum::oscillator:
        {
            // Ground state at E = 1/2, so the lines sit on the odd harmonics of half the barrier.
            const int nearest = (int) std::lround (energy - 0.5);
            for (int n = std::max (0, nearest - 2); n <= nearest + 2; ++n)
                strongest = std::max (strongest, lorentzian (energy, n + 0.5, relativeWidth));  // spacing is 1
            break;
        }
        case Spectrum::hydrogen:
        {
            if (energy >= 1.0)
                return 1.0;  // above the ionisation limit: the continuum absorbs everything
            relativeWidth *= 0.25;  // the series is dense, so its lines need to be sharper to stay apart
            const int nearest = (int) std::lround (1.0 / std::sqrt (1.0 - energy));
            for (int n = std::max (2, nearest - 2); n <= nearest + 2; ++n)
            {
                const double level = 1.0 - 1.0 / ((double) n * n);
                strongest = std::max (strongest, lorentzian (energy, level, relativeWidth * level));
            }
            // The series limit itself: lines merge into the continuum just below E = 1.
            strongest = std::max (strongest, lorentzian (energy, 1.0, relativeWidth));
            break;
        }
    }
    return strongest;
}

} // namespace qs
