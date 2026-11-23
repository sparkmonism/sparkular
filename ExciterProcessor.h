#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

// Exciter (Songcraft Audio). Control set follows the Softube Weiss Exciter,
// with oversampling on the harmonics generator.
//
// Per channel (Ch 1 = left or mid, Ch 2 = right or side):
//   Pre HP / Pre LP, Post HP / Post LP, Harmonics, Subtle, 2nd, Exciter Amount,
//   Mastering, Harmonics Mode (Soft, Hard, Poly 3, Poly 5), 2nd Harmonic Level,
//   Subtle Gain, Adaptive Mode.
// Shared: Ganged, M/S, Width, Monitor, Bypass, Filter Slope, Linear Phase,
//   Oversampling.
//
// Signal flow (parallel, the dry signal is never processed):
//   in -> [M/S] -> Pre HP/LP -> adaptive level normalise
//      -> [oversampled] harmonics generator (-> optional fundamental removal)
//      -> DC block -> restore level -> Post HP/LP -> Exciter Amount
//      -> Width (side level) -> added to the dry signal.
//
// Latency is constant. The plugin always reports the linear-phase filter delay
// plus the 8x oversampler delay, and pads the cheaper modes to match, so
// toggling Linear Phase or Oversampling never shifts the host's compensation.
class ExciterProcessor : public juce::AudioProcessor
{
public:
    ExciterProcessor();
    ~ExciterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Sparkular"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Parameter id for a per-channel control, e.g. pid (1, "harm") -> "c1_harm".
    static juce::String pid (int channel, const char* suffix);

    juce::AudioProcessorValueTreeState apvts;

    // For the editor's harmonics LED: 0 = idle, 1 = harmonics, 2 = hard clipping.
    std::atomic<int> ledState { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    //==========================================================================
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void reset() { z1 = z2 = 0.0f; }
        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    // 2nd order (Gentle) or 4th order (Steep) Butterworth.
    struct Cascade
    {
        Biquad s[2];
        int n = 1;

        void reset() { s[0].reset(); s[1].reset(); }
        float process (float x)
        {
            for (int i = 0; i < n; ++i)
                x = s[i].process (x);
            return x;
        }
    };

    // Linear-phase band-pass FIR (windowed sinc), direct form.
    struct Fir
    {
        static constexpr int kTaps = 1023;
        static constexpr int kHalf = (kTaps - 1) / 2; // latency in samples

        std::array<float, kTaps> hr {};         // taps, stored reversed
        std::array<float, 2 * kTaps> buf {};    // mirrored history
        int pos = 0;

        void reset() { buf.fill (0.0f); pos = 0; }
        void design (double fs, double hp, double lp, bool steep);

        float process (float x)
        {
            buf[(size_t) pos] = x;
            buf[(size_t) (pos + kTaps)] = x;

            const float* b = buf.data() + pos + 1;
            float acc = 0.0f;
            for (int k = 0; k < kTaps; ++k)
                acc += hr[(size_t) k] * b[k];

            if (++pos >= kTaps)
                pos = 0;
            return acc;
        }
    };

    struct FirKey
    {
        float hp = -1.0f, lp = -1.0f;
        bool steep = false;
    };

    struct ChannelDsp
    {
        Cascade preHp, preLp, postHp, postLp;
        Fir preFir, postFir;
        FirKey preKey, postKey;
        float dcX = 0.0f, dcY = 0.0f;
        float env = 0.0f;

        void resetAll()
        {
            preHp.reset(); preLp.reset(); postHp.reset(); postLp.reset();
            preFir.reset(); postFir.reset();
            preKey = {}; postKey = {};
            dcX = dcY = 0.0f;
            env = 0.0f;
        }
    };

    //==========================================================================
    struct BlockState
    {
        struct Chan
        {
            float preHp = 2000.0f, preLp = 8000.0f, postHp = 4000.0f, postLp = 16000.0f;
            float harm = 0.5f, amount = 0.3f, secondLvl = 0.3f, subtleGainDb = -6.0f;
            bool subtle = false, second = false, master = false, adaptive = true;
            int mode = 0;
        };

        bool bypass = false, ganged = true, ms = false, steep = false, linear = false, monitor = false;
        float width = 1.0f;
        int order = 2; // oversampling stages: 0 = off, 1 = 2x, 2 = 4x, 3 = 8x
        Chan c[2];
    };

    struct ChanPtrs
    {
        std::atomic<float> *preHp, *preLp, *postHp, *postLp;
        std::atomic<float> *harm, *subtle, *second, *amount, *master, *mode;
        std::atomic<float> *secondLvl, *subtleGain, *adaptive;
    };

    void processChunk (juce::AudioBuffer<float>& buffer, int start, int num, const BlockState& bs);
    void updateFilters (ChannelDsp& d, const BlockState::Chan& p, const BlockState& bs);

    std::array<ChanPtrs, 2> pc {};
    std::atomic<float> *pBypass = nullptr, *pGanged = nullptr, *pMs = nullptr, *pSlope = nullptr,
                       *pLinear = nullptr, *pMonitor = nullptr, *pWidth = nullptr, *pOversampling = nullptr;

    std::array<ChannelDsp, 2> ch;

    // Oversampling (index 0 = 2x, 1 = 4x, 2 = 8x)
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;
    int osLatency[4] { 0, 0, 0, 0 };
    int osMaxLatency = 0;
    int firLatency = 0;     // pre + post linear-phase filters
    int totalLatency = 0;   // reported to host, also the dry delay
    int lastOrder = -1;
    int lastLinear = -1;

    int maxBlock = 512;
    double sr = 44100.0;

    // chunk work buffers (per channel)
    std::array<std::vector<float>, 2> sigBuf, satBuf, envBuf, drvBuf, amtBuf, gdfBuf;

    // delay lines for latency alignment
    std::array<std::vector<float>, 2> dryRing, wetRing;
    int64_t ringMask = 0;
    int64_t writeIndex = 0;

    // adaptive level follower
    float envAtk = 0.0f, envRel = 0.0f;

    // Mastering: gain of the fundamental through each harmonics mode versus the
    // drive amplitude u = drive * |x| (a describing-function table, 0..kGdfMax).
    static constexpr int kGdfSize = 513;
    static constexpr float kGdfMax = 32.0f;
    std::array<std::array<float, kGdfSize>, 4> gdfTable {};
    float gdfLookup (int mode, float au) const;

    // LED clip hold, in samples
    int clipHold = 0;

    std::array<juce::SmoothedValue<float>, 2> drvSm, amtSm;
    juce::SmoothedValue<float> dryGainSm, wetGainSm, widthSm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExciterProcessor)
};
