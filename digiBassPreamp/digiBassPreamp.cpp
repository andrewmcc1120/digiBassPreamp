#include "daisy_seed.h"
#include "daisysp.h"
#include <cmath>

using namespace daisy;
using namespace daisy::seed;
using namespace daisysp;

// Custom Biquad class

struct MyBiquad
{
    float a0 = 0, a1 = 0, a2 = 0, b1 = 0, b2 = 0;
    float z1 = 0, z2 = 0;

    void Init() { z1 = z2 = 0; }

    float Process(float x)
    {
        float y = a0 * x + a1 * z1 + a2 * z2 - b1 * z1 - b2 * z2;
        z2      = z1;
        z1      = y;
        return y;
    }

    void SetCoeffs(float _a0, float _a1, float _a2, float _b1, float _b2)
    {
        a0 = _a0;
        a1 = _a1;
        a2 = _a2;
        b1 = _b1;
        b2 = _b2;
    }
};

// 3-Band EQ class

class EQ3Band
{
  public:
    void Init(float sr)
    {
        sr_ = sr;

        // Fixed center frequencies
        bass_freq_   = 80.0f;
        mid_freq_    = 800.0f;
        treble_freq_ = 4000.0f;

        // Q factors
        bass_q_   = 0.707f;
        mid_q_    = 1.0f;
        treble_q_ = 0.707f;

        bass.Init();
        mid.Init();
        treble.Init();
    }

    float Process(float x)
    {
        float y = bass.Process(x);
        y       = mid.Process(y);
        y       = treble.Process(y);
        return y;
    }

    // Potentiometer input: floating point from 0 to 1
    void SetBass(float pot)
    {
        float db = PotToDb(pot, 15.0f);
        SetLowShelf(bass, bass_freq_, bass_q_, db);
    }

    void SetMid(float pot)
    {
        float db = PotToDb(pot, 12.0f);
        SetPeak(mid, mid_freq_, mid_q_, db);
    }

    void SetTreble(float pot)
    {
        float db = PotToDb(pot, 15.0f);
        SetHighShelf(treble, treble_freq_, treble_q_, db);
    }

  private:
    float    sr_;
    MyBiquad bass, mid, treble;
    float    bass_freq_, mid_freq_, treble_freq_;
    float    bass_q_, mid_q_, treble_q_;

    float PotToDb(float pot, float maxDb)
    {
        return (0.5f - pot) * 2.0f * maxDb;
    }

    // RBJ Cookbook Coefficients

    void SetLowShelf(MyBiquad &f, float freq, float Q, float dbGain)
    {
        float A     = powf(10.0f, dbGain / 40.0f);
        float w0    = 2.0f * M_PI * freq / sr_;
        float alpha = sinf(w0) / (2.0f * Q);
        float cosw0 = cosf(w0);

        float b0 = A * ((A + 1) - (A - 1) * cosw0 + 2 * sqrtf(A) * alpha);
        float b1 = 2 * A * ((A - 1) - (A + 1) * cosw0);
        float b2 = A * ((A + 1) - (A - 1) * cosw0 - 2 * sqrtf(A) * alpha);
        float a0 = (A + 1) + (A - 1) * cosw0 + 2 * sqrtf(A) * alpha;
        float a1 = -2 * ((A - 1) + (A + 1) * cosw0);
        float a2 = (A + 1) + (A - 1) * cosw0 - 2 * sqrtf(A) * alpha;

        f.SetCoeffs(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
    }

    void SetHighShelf(MyBiquad &f, float freq, float Q, float dbGain)
    {
        float A     = powf(10.0f, dbGain / 40.0f);
        float w0    = 2.0f * M_PI * freq / sr_;
        float alpha = sinf(w0) / (2.0f * Q);
        float cosw0 = cosf(w0);

        float b0 = A * ((A + 1) + (A - 1) * cosw0 + 2 * sqrtf(A) * alpha);
        float b1 = -2 * A * ((A - 1) + (A + 1) * cosw0);
        float b2 = A * ((A + 1) + (A - 1) * cosw0 - 2 * sqrtf(A) * alpha);
        float a0 = (A + 1) - (A - 1) * cosw0 + 2 * sqrtf(A) * alpha;
        float a1 = 2 * ((A - 1) - (A + 1) * cosw0);
        float a2 = (A + 1) - (A - 1) * cosw0 - 2 * sqrtf(A) * alpha;

        f.SetCoeffs(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
    }

    void SetPeak(MyBiquad &f, float freq, float Q, float dbGain)
    {
        float A     = powf(10.0f, dbGain / 40.0f);
        float w0    = 2.0f * M_PI * freq / sr_;
        float alpha = sinf(w0) / (2.0f * Q);
        float cosw0 = cosf(w0);

        float b0 = 1 + alpha * A;
        float b1 = -2 * cosw0;
        float b2 = 1 - alpha * A;
        float a0 = 1 + alpha / A;
        float a1 = -2 * cosw0;
        float a2 = 1 - alpha / A;

        f.SetCoeffs(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
    }
};

// Daisy Seed setup

DaisySeed        hw;
EQ3Band          eq;
AdcChannelConfig adcConfig[3];

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        float sig = in[0][i];
        sig       = eq.Process(sig);
        out[0][i] = sig;
        out[1][i] = sig;
    }
}

int main(void)
{
    hw.Init();
    hw.StartLog();

    adcConfig[0].InitSingle(A0);
    adcConfig[1].InitSingle(A1);
    adcConfig[2].InitSingle(A2);

    hw.adc.Init(adcConfig, sizeof(adcConfig) / sizeof(adcConfig[0]));

    hw.adc.Start();

    eq.Init(hw.AudioSampleRate());
    hw.StartAudio(AudioCallback);

    while(1)
    {
        // Do not attempt to print a float directly to the monitor...STM32 chips do not have printf enabled
        int value    = hw.adc.Get(0);
        int floatVal = hw.adc.GetFloat(0) * 1000;
        hw.PrintLine("ADC Value: %d", value);
        hw.PrintLine("ADC Float: %d", floatVal);

        float bass   = hw.adc.GetFloat(0);
        float mid    = hw.adc.GetFloat(1);
        float treble = hw.adc.GetFloat(2);

        eq.SetBass(bass);
        eq.SetMid(mid);
        eq.SetTreble(treble);

        System::Delay(1);
    }
}
