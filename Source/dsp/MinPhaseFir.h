#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace qs
{

// Small in-place radix-2 FFT. The inverse is scaled by 1/N.
class Fft
{
public:
    explicit Fft (int order)
        : size (1 << order), twiddles ((size_t) size / 2), bitReversed ((size_t) size)
    {
        const double pi = 3.14159265358979323846;

        for (int k = 0; k < size / 2; ++k)
            twiddles[(size_t) k] = std::polar (1.0, -2.0 * pi * k / size);

        for (int n = 0; n < size; ++n)
        {
            int r = 0;
            for (int b = 0; b < order; ++b)
                r |= ((n >> b) & 1) << (order - 1 - b);
            bitReversed[(size_t) n] = r;
        }
    }

    int getSize() const { return size; }

    void perform (std::complex<double>* data, bool inverse) const
    {
        for (int n = 0; n < size; ++n)
            if (n < bitReversed[(size_t) n])
                std::swap (data[n], data[bitReversed[(size_t) n]]);

        for (int len = 2; len <= size; len <<= 1)
        {
            const int half = len / 2;
            const int step = size / len;

            for (int start = 0; start < size; start += len)
            {
                for (int k = 0; k < half; ++k)
                {
                    auto w = twiddles[(size_t) (k * step)];
                    if (inverse)
                        w = std::conj (w);

                    const auto u = data[start + k];
                    const auto v = data[start + k + half] * w;
                    data[start + k] = u + v;
                    data[start + k + half] = u - v;
                }
            }
        }

        if (inverse)
            for (int n = 0; n < size; ++n)
                data[n] /= (double) size;
    }

private:
    int size;
    std::vector<std::complex<double>> twiddles;
    std::vector<int> bitReversed;
};

// Turns a magnitude response into a minimum-phase FIR using the real cepstrum.
// Minimum phase means no added latency, so the filter can be played live.
class MinPhaseDesigner
{
public:
    explicit MinPhaseDesigner (int fftOrder)
        : fft (fftOrder), buffer ((size_t) fft.getSize())
    {
    }

    int getFftSize() const { return fft.getSize(); }
    int getNumBins() const { return fft.getSize() / 2 + 1; }

    // magnitudes: getNumBins() values from DC to Nyquist. taps: numTaps <= fftSize / 2.
    void design (const double* magnitudes, float* taps, int numTaps)
    {
        const int n = fft.getSize();
        const int half = n / 2;

        for (int k = 0; k <= half; ++k)
        {
            const double logMag = std::log (std::max (magnitudes[k], 1.0e-5));
            buffer[(size_t) k] = logMag;
            if (k > 0 && k < half)
                buffer[(size_t) (n - k)] = logMag;
        }

        fft.perform (buffer.data(), true);  // real cepstrum

        // Fold the cepstrum onto positive quefrencies to get the minimum-phase version.
        for (int q = 1; q < half; ++q)
        {
            buffer[(size_t) q] = 2.0 * buffer[(size_t) q].real();
            buffer[(size_t) (n - q)] = 0.0;
        }
        buffer[0] = buffer[0].real();
        buffer[(size_t) half] = buffer[(size_t) half].real();

        fft.perform (buffer.data(), false);

        for (auto& z : buffer)
            z = std::exp (z);

        fft.perform (buffer.data(), true);

        // Short fade at the tail so truncation does not click.
        const int fadeLength = numTaps / 4;
        const double pi = 3.14159265358979323846;

        for (int j = 0; j < numTaps; ++j)
        {
            double g = 1.0;
            const int intoFade = j - (numTaps - fadeLength);
            if (intoFade >= 0)
                g = 0.5 * (1.0 + std::cos (pi * (intoFade + 1) / (fadeLength + 1)));

            taps[j] = (float) (buffer[(size_t) j].real() * g);
        }
    }

private:
    Fft fft;
    std::vector<std::complex<double>> buffer;
};

} // namespace qs
