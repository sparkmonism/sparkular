#include "ExciterProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;

// The static nonlinearity of each harmonics mode, before the 1/drive scaling.
inline float shapeCore (int mode, float u)
{
    switch (mode)
    {
        default:
        case 0: // Soft: smooth knee, many harmonics
            return std::tanh (u);

        case 1: // Hard: harder knee, many harmonics. u / (1 + u^8)^(1/8)
        {
            const float u2 = u * u;
            const float u4 = u2 * u2;
            const float u8 = u4 * u4;
            return u / std::sqrt (std::sqrt (std::sqrt (1.0f + u8)));
        }

        case 2: // Poly 3: 3rd harmonic only, flat above 1.0
        {
            const float uc = juce::jlimit (-1.0f, 1.0f, u);
            return uc - uc * uc * uc * (1.0f / 3.0f);
        }

        case 3: // Poly 5: 3rd and 5th harmonic, flat above 1.0
        {
            const float uc = juce::jlimit (-1.0f, 1.0f, u);
            const float u2 = uc * uc;
            return uc * (1.0f - u2 * (0.5f - 0.1f * u2));
        }
    }
}

// The harmonics generator. `x` is the (level-normalised) input, `d` the drive.
// Returns the generator output at unity small-signal gain. `fundGain` is the
// estimated gain of the fundamental through the generator (0 when Mastering is
// off); subtracting fundGain * x leaves only the added harmonics.
inline float generator (float x, float d, int mode, bool second, float secondLvl,
                        float fundGain, float& maxU)
{
    const float u = d * x;
    const float au = std::abs (u);
    if (au > maxU)
        maxU = au;

    float y = shapeCore (mode, u);

    if (second)
        y += 0.5f * secondLvl * y * y; // even harmonics (DC is removed later)

    y /= d;

    return y - fundGain * x;
}
} // namespace

//==============================================================================
juce::String ExciterProcessor::pid (int channel, const char* suffix)
{
    return "c" + juce::String (channel) + "_" + suffix;
}

juce::AudioProcessorValueTreeState::ParameterLayout ExciterProcessor::createLayout()
{
    using P  = juce::AudioParameterFloat;
    using B  = juce::AudioParameterBool;
    using C  = juce::AudioParameterChoice;
    using NR = juce::NormalisableRange<float>;
    using Attr = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto freqRange = []
    {
        return NR (20.0f, 20000.0f,
                   [] (float s, float e, float v) { return s * std::pow (e / s, v); },
                   [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); });
    };

    auto freqAttr = []
    {
        return Attr()
            .withStringFromValueFunction ([] (float v, int)
                                          {
                                              return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                                                                  : juce::String ((int) std::round (v)) + " Hz";
                                          })
            .withValueFromStringFunction ([] (const juce::String& t)
                                          {
                                              const float v = t.getFloatValue();
                                              return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
                                          });
    };

    // ---- shared controls
    layout.add (std::make_unique<B> (juce::ParameterID { "bypass", 1 }, "Bypass", false));
    layout.add (std::make_unique<B> (juce::ParameterID { "ganged", 1 }, "Ganged", true));
    layout.add (std::make_unique<B> (juce::ParameterID { "ms", 1 }, "M/S", false));
    layout.add (std::make_unique<C> (juce::ParameterID { "slope", 1 }, "Filter Slope",
                                     juce::StringArray { "Gentle", "Steep" }, 0));
    layout.add (std::make_unique<B> (juce::ParameterID { "linear", 1 }, "Linear Phase", false));
    layout.add (std::make_unique<B> (juce::ParameterID { "monitor", 1 }, "Monitor", false));
    layout.add (std::make_unique<P> (juce::ParameterID { "width", 1 }, "Width",
                                     NR (0.0f, 2.0f, 0.001f), 1.0f,
                                     Attr().withStringFromValueFunction ([] (float v, int)
                                                                         { return juce::String ((int) std::round (v * 100.0f)) + " %"; })
                                           .withValueFromStringFunction ([] (const juce::String& t)
                                                                         { return t.getFloatValue() * 0.01f; })));
    layout.add (std::make_unique<C> (juce::ParameterID { "oversampling", 1 }, "Oversampling",
                                     juce::StringArray { "Off", "2x", "4x", "8x" }, 2));

    // ---- per-channel controls
    for (int n = 1; n <= 2; ++n)
    {
        auto id = [n] (const char* s) { return juce::ParameterID { pid (n, s), 1 }; };
        auto nm = [n] (const char* s) { return "Ch" + juce::String (n) + " " + s; };

        layout.add (std::make_unique<P> (id ("prehp"),  nm ("Pre HP"),  freqRange(), 2000.0f,  freqAttr()));
        layout.add (std::make_unique<P> (id ("prelp"),  nm ("Pre LP"),  freqRange(), 8000.0f,  freqAttr()));
        layout.add (std::make_unique<P> (id ("posthp"), nm ("Post HP"), freqRange(), 4000.0f,  freqAttr()));
        layout.add (std::make_unique<P> (id ("postlp"), nm ("Post LP"), freqRange(), 16000.0f, freqAttr()));

        layout.add (std::make_unique<P> (id ("harm"), nm ("Harmonics"), NR (0.0f, 1.0f, 0.001f), 0.5f));
        layout.add (std::make_unique<B> (id ("subtle"), nm ("Subtle"), false));
        layout.add (std::make_unique<B> (id ("2nd"), nm ("2nd"), false));
        layout.add (std::make_unique<P> (id ("amount"), nm ("Exciter Amount"), NR (0.0f, 1.0f, 0.001f), 0.3f));
        layout.add (std::make_unique<B> (id ("master"), nm ("Mastering"), false));
        layout.add (std::make_unique<C> (id ("mode"), nm ("Harmonics Mode"),
                                         juce::StringArray { "Soft", "Hard", "Poly 3", "Poly 5" }, 0));
        layout.add (std::make_unique<P> (id ("2ndlvl"), nm ("2nd Harmonic Level"), NR (0.0f, 1.0f, 0.001f), 0.3f));
        layout.add (std::make_unique<P> (id ("subtlegain"), nm ("Subtle Gain"), NR (-24.0f, 0.0f, 0.01f), -6.0f,
                                         Attr().withLabel ("dB")));
        layout.add (std::make_unique<B> (id ("adaptive"), nm ("Adaptive Mode"), true));
    }

    return layout;
}

ExciterProcessor::ExciterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto ptr = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };

    pBypass       = ptr ("bypass");
    pGanged       = ptr ("ganged");
    pMs           = ptr ("ms");
    pSlope        = ptr ("slope");
    pLinear       = ptr ("linear");
    pMonitor      = ptr ("monitor");
    pWidth        = ptr ("width");
    pOversampling = ptr ("oversampling");

    for (int c = 0; c < 2; ++c)
    {
        const int n = c + 1;
        auto& p = pc[(size_t) c];
        p.preHp      = ptr (pid (n, "prehp"));
        p.preLp      = ptr (pid (n, "prelp"));
        p.postHp     = ptr (pid (n, "posthp"));
        p.postLp     = ptr (pid (n, "postlp"));
        p.harm       = ptr (pid (n, "harm"));
        p.subtle     = ptr (pid (n, "subtle"));
        p.second     = ptr (pid (n, "2nd"));
        p.amount     = ptr (pid (n, "amount"));
        p.master     = ptr (pid (n, "master"));
        p.mode       = ptr (pid (n, "mode"));
        p.secondLvl  = ptr (pid (n, "2ndlvl"));
        p.subtleGain = ptr (pid (n, "subtlegain"));
        p.adaptive   = ptr (pid (n, "adaptive"));
    }
}

bool ExciterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

juce::AudioProcessorEditor* ExciterProcessor::createEditor()
{
    return new ExciterEditor (*this);
}

//==============================================================================
void ExciterProcessor::Fir::design (double fs, double hp, double lp, bool steep)
{
    const int M = kHalf;
    const int winHalf = steep ? M : 127; // shorter window = gentler skirts
    const double fcLp = juce::jlimit (1.0, 0.49 * fs, lp) / fs;
    const double fcHp = juce::jlimit (1.0, 0.49 * fs, hp) / fs;

    auto sinc = [] (double fc, int k)
    {
        return k == 0 ? 2.0 * fc : std::sin (2.0 * kPi * fc * (double) k) / (kPi * (double) k);
    };

    std::array<float, kTaps> h {};
    for (int n = 0; n < kTaps; ++n)
    {
        const int k = n - M;
        double w = 0.0;

        if (std::abs (k) <= winHalf)
        {
            const double t = (double) (k + winHalf) / (double) (2 * winHalf);
            w = 0.42 - 0.5 * std::cos (2.0 * kPi * t) + 0.08 * std::cos (4.0 * kPi * t); // Blackman
        }

        h[(size_t) n] = (float) (w * (sinc (fcLp, k) - sinc (fcHp, k)));
    }

    for (int n = 0; n < kTaps; ++n)
        hr[(size_t) n] = h[(size_t) (kTaps - 1 - n)];
}

void ExciterProcessor::updateFilters (ChannelDsp& d, const BlockState::Chan& p, const BlockState& bs)
{
    const double nyq = 0.45 * sr;

    auto fix = [nyq] (double& hp, double& lp)
    {
        hp = juce::jlimit (10.0, nyq, hp);
        lp = juce::jlimit (10.0, nyq, lp);
        if (lp < hp * 1.05)
        {
            lp = hp * 1.05;
            if (lp > nyq)
            {
                lp = nyq;
                hp = lp / 1.05;
            }
        }
    };

    double preHp = p.preHp, preLp = p.preLp, postHp = p.postHp, postLp = p.postLp;
    fix (preHp, preLp);
    fix (postHp, postLp);

    if (bs.linear)
    {
        auto changed = [] (const FirKey& k, double hp, double lp, bool steep)
        {
            return std::abs ((double) k.hp - hp) > 0.01 || std::abs ((double) k.lp - lp) > 0.01 || k.steep != steep;
        };

        if (changed (d.preKey, preHp, preLp, bs.steep))
        {
            d.preFir.design (sr, preHp, preLp, bs.steep);
            d.preKey = { (float) preHp, (float) preLp, bs.steep };
        }

        if (changed (d.postKey, postHp, postLp, bs.steep))
        {
            d.postFir.design (sr, postHp, postLp, bs.steep);
            d.postKey = { (float) postHp, (float) postLp, bs.steep };
        }
        return;
    }

    // Minimum phase: Butterworth, 2nd order (Gentle) or 4th order (Steep).
    const int n = bs.steep ? 2 : 1;
    const double qs[2] = { n == 1 ? 0.70710678 : 0.5411961, 1.3065630 };

    auto design = [this] (Biquad& b, double f, double q, bool highPass)
    {
        const double w0 = 2.0 * kPi * f / sr;
        const double c = std::cos (w0);
        const double s = std::sin (w0);
        const double alpha = s / (2.0 * q);
        const double a0 = 1.0 + alpha;

        if (highPass)
        {
            b.b0 = (float) ((1.0 + c) * 0.5 / a0);
            b.b1 = (float) (-(1.0 + c) / a0);
            b.b2 = b.b0;
        }
        else
        {
            b.b0 = (float) ((1.0 - c) * 0.5 / a0);
            b.b1 = (float) ((1.0 - c) / a0);
            b.b2 = b.b0;
        }

        b.a1 = (float) (-2.0 * c / a0);
        b.a2 = (float) ((1.0 - alpha) / a0);
    };

    auto setCascade = [&] (Cascade& cas, double f, bool highPass)
    {
        cas.n = n;
        for (int i = 0; i < n; ++i)
            design (cas.s[i], f, qs[i], highPass);
    };

    setCascade (d.preHp,  preHp,  true);
    setCascade (d.preLp,  preLp,  false);
    setCascade (d.postHp, postHp, true);
    setCascade (d.postLp, postLp, false);
}

float ExciterProcessor::gdfLookup (int mode, float au) const
{
    const auto& t = gdfTable[(size_t) mode];

    if (au >= kGdfMax)
        return t[kGdfSize - 1] * (kGdfMax / au);

    const float pos = au * ((float) (kGdfSize - 1) / kGdfMax);
    const int i = (int) pos;
    const float f = pos - (float) i;
    return t[(size_t) i] + f * (t[(size_t) (i + 1)] - t[(size_t) i]);
}

//==============================================================================
void ExciterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = std::max (1, samplesPerBlock);

    osLatency[0] = 0;
    for (int i = 0; i < 3; ++i)
    {
        oversamplers[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> (
            2, (size_t) (i + 1),
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            true, true);
        oversamplers[(size_t) i]->initProcessing ((size_t) maxBlock);
        oversamplers[(size_t) i]->reset();
        osLatency[i + 1] = (int) std::lround (oversamplers[(size_t) i]->getLatencyInSamples());
    }
    osMaxLatency = osLatency[3];
    firLatency   = 2 * Fir::kHalf;
    totalLatency = firLatency + osMaxLatency;

    for (size_t c = 0; c < 2; ++c)
    {
        sigBuf[c].assign ((size_t) maxBlock, 0.0f);
        satBuf[c].assign ((size_t) maxBlock, 0.0f);
        envBuf[c].assign ((size_t) maxBlock, 1.0f);
        drvBuf[c].assign ((size_t) maxBlock, 1.0f);
        amtBuf[c].assign ((size_t) maxBlock, 0.0f);
        gdfBuf[c].assign ((size_t) maxBlock, 0.0f);

        const int ringSize = juce::nextPowerOfTwo (totalLatency + 16);
        dryRing[c].assign ((size_t) ringSize, 0.0f);
        wetRing[c].assign ((size_t) ringSize, 0.0f);
        ringMask = ringSize - 1;

        ch[c].resetAll();
    }

    // Mastering table: fundamental amplitude / drive amplitude for each mode,
    // measured by pushing a sine of amplitude `au` through the nonlinearity.
    for (int m = 0; m < 4; ++m)
    {
        gdfTable[(size_t) m][0] = 1.0f;

        for (int k = 1; k < kGdfSize; ++k)
        {
            const double au = (double) k * (double) kGdfMax / (double) (kGdfSize - 1);
            constexpr int N = 256;
            double acc = 0.0;

            for (int i = 0; i < N; ++i)
            {
                const double th = 2.0 * kPi * ((double) i + 0.5) / (double) N;
                const double sn = std::sin (th);
                acc += (double) shapeCore (m, (float) (au * sn)) * sn;
            }

            gdfTable[(size_t) m][(size_t) k] = (float) (2.0 * acc / (double) N / au);
        }
    }

    writeIndex = 0;
    lastOrder = -1;
    lastLinear = -1;
    clipHold = 0;

    envAtk = (float) (1.0 - std::exp (-1.0 / (0.0005 * sr))); // 0.5 ms attack
    envRel = (float) std::exp (-1.0 / (0.150 * sr));          // 150 ms release

    for (size_t c = 0; c < 2; ++c)
    {
        drvSm[c].reset (sr, 0.02);
        amtSm[c].reset (sr, 0.02);
        drvSm[c].setCurrentAndTargetValue (1.0f);
        amtSm[c].setCurrentAndTargetValue (0.0f);
    }

    dryGainSm.reset (sr, 0.01);
    wetGainSm.reset (sr, 0.01);
    widthSm.reset (sr, 0.02);
    dryGainSm.setCurrentAndTargetValue (1.0f);
    wetGainSm.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 0.0f : 1.0f);
    widthSm.setCurrentAndTargetValue (pWidth->load());

    setLatencySamples (totalLatency);
}

//==============================================================================
void ExciterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int nch = std::min (buffer.getNumChannels(), 2);
    const int ns  = buffer.getNumSamples();

    for (int c = nch; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, ns);

    BlockState bs;
    bs.bypass  = pBypass->load()  > 0.5f;
    bs.ganged  = pGanged->load()  > 0.5f;
    bs.ms      = pMs->load()      > 0.5f;
    bs.steep   = pSlope->load()   > 0.5f;
    bs.linear  = pLinear->load()  > 0.5f;
    bs.monitor = pMonitor->load() > 0.5f;
    bs.width   = pWidth->load();
    bs.order   = juce::jlimit (0, 3, (int) std::lround (pOversampling->load()));

    for (size_t c = 0; c < 2; ++c)
    {
        const auto& p = pc[c];
        auto& o = bs.c[c];
        o.preHp        = p.preHp->load();
        o.preLp        = p.preLp->load();
        o.postHp       = p.postHp->load();
        o.postLp       = p.postLp->load();
        o.harm         = p.harm->load();
        o.amount       = p.amount->load();
        o.secondLvl    = p.secondLvl->load();
        o.subtleGainDb = p.subtleGain->load();
        o.subtle       = p.subtle->load()   > 0.5f;
        o.second       = p.second->load()   > 0.5f;
        o.master       = p.master->load()   > 0.5f;
        o.adaptive     = p.adaptive->load() > 0.5f;
        o.mode         = juce::jlimit (0, 3, (int) std::lround (p.mode->load()));
    }

    const int linearNow = bs.linear ? 1 : 0;
    if (linearNow != lastLinear)
    {
        for (auto& d : ch)
            d.resetAll();
        lastLinear = linearNow;
    }

    if (bs.order != lastOrder)
    {
        if (bs.order > 0)
            oversamplers[(size_t) (bs.order - 1)]->reset();
        lastOrder = bs.order;
    }

    dryGainSm.setTargetValue ((bs.bypass || ! bs.monitor) ? 1.0f : 0.0f);
    wetGainSm.setTargetValue (bs.bypass ? 0.0f : 1.0f);
    widthSm.setTargetValue (bs.width);

    int done = 0;
    while (done < ns)
    {
        const int n = std::min (ns - done, maxBlock);
        processChunk (buffer, done, n, bs);
        done += n;
    }
}

//==============================================================================
void ExciterProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int num,
                                     const BlockState& bs)
{
    const int nch = std::min (buffer.getNumChannels(), 2);

    float* data[2] = { nullptr, nullptr };
    for (int c = 0; c < nch; ++c)
        data[c] = buffer.getWritePointer (c) + start;

    const bool useMs = bs.ms && nch > 1;
    const int pathLatency = (bs.linear ? firLatency : 0) + osLatency[bs.order];
    const int wetExtra = totalLatency - pathLatency; // always >= 0

    auto params = [&bs] (int c) -> const BlockState::Chan& { return bs.c[bs.ganged ? 0 : c]; };

    for (int c = 0; c < nch; ++c)
        updateFilters (ch[(size_t) c], params (c), bs);

    // ---- 1. keep the dry signal, route L/R or M/S, run the pre filters
    for (int s = 0; s < num; ++s)
    {
        const float l = data[0][s];
        const float r = nch > 1 ? data[1][s] : 0.0f;
        const int64_t idx = writeIndex + s;

        dryRing[0][(size_t) (idx & ringMask)] = l;
        dryRing[1][(size_t) (idx & ringMask)] = r;

        if (useMs)
        {
            sigBuf[0][(size_t) s] = 0.5f * (l + r);
            sigBuf[1][(size_t) s] = 0.5f * (l - r);
        }
        else
        {
            sigBuf[0][(size_t) s] = l;
            sigBuf[1][(size_t) s] = r;
        }
    }

    for (int c = 0; c < nch; ++c)
    {
        auto& d = ch[(size_t) c];
        float* x = sigBuf[(size_t) c].data();

        if (bs.linear)
            for (int s = 0; s < num; ++s)
                x[s] = d.preFir.process (x[s]);
        else
            for (int s = 0; s < num; ++s)
                x[s] = d.preLp.process (d.preHp.process (x[s]));
    }

    // ---- 2. level follower (adaptive mode), drive and amount ramps
    for (int c = 0; c < nch; ++c)
    {
        const auto& p = params (c);
        auto& d = ch[(size_t) c];

        float drive = 0.25f + 3.75f * p.harm * p.harm;
        if (p.subtle)
            drive *= juce::Decibels::decibelsToGain (p.subtleGainDb);
        drive = std::max (drive, 0.02f);

        drvSm[(size_t) c].setTargetValue (drive);
        amtSm[(size_t) c].setTargetValue (p.amount * 2.0f);

        float* x = sigBuf[(size_t) c].data();
        float* e = envBuf[(size_t) c].data();

        for (int s = 0; s < num; ++s)
        {
            const float ax = std::abs (x[s]);

            if (ax > d.env)
                d.env += envAtk * (ax - d.env);
            else
                d.env = std::max (ax, d.env * envRel);

            const float level = p.adaptive ? std::max (d.env, 1.0e-3f) : 1.0f;
            e[s] = level;
            x[s] /= level;

            const float dv = drvSm[(size_t) c].getNextValue();
            drvBuf[(size_t) c][(size_t) s] = dv;
            amtBuf[(size_t) c][(size_t) s] = amtSm[(size_t) c].getNextValue();

            // Mastering: estimated gain of the fundamental through the generator,
            // from the amplitude of the (normalised) signal at the generator input.
            gdfBuf[(size_t) c][(size_t) s] = p.master ? gdfLookup (p.mode, dv * (d.env / level)) : 0.0f;
        }
    }

    // ---- 3. harmonics generator, oversampled
    float* sigPtr[2] = { sigBuf[0].data(), sigBuf[1].data() };
    float* satPtr[2] = { satBuf[0].data(), satBuf[1].data() };
    float maxU[2] = { 0.0f, 0.0f };

    if (bs.order == 0)
    {
        for (int c = 0; c < nch; ++c)
        {
            const auto& p = params (c);
            const float* dv = drvBuf[(size_t) c].data();
            const float* gv = gdfBuf[(size_t) c].data();

            for (int s = 0; s < num; ++s)
                satPtr[c][s] = generator (sigPtr[c][s], dv[s], p.mode, p.second, p.secondLvl,
                                          gv[s], maxU[c]);
        }
    }
    else
    {
        auto& os = *oversamplers[(size_t) (bs.order - 1)];

        juce::dsp::AudioBlock<float> inBlock (sigPtr, (size_t) nch, (size_t) num);
        auto up = os.processSamplesUp (inBlock);

        const size_t upNum = (size_t) num << bs.order;

        for (int c = 0; c < nch; ++c)
        {
            const auto& p = params (c);
            const float* dv = drvBuf[(size_t) c].data();
            const float* gv = gdfBuf[(size_t) c].data();
            float* q = up.getChannelPointer ((size_t) c);

            for (size_t j = 0; j < upNum; ++j)
                q[j] = generator (q[j], dv[j >> bs.order], p.mode, p.second, p.secondLvl,
                                  gv[j >> bs.order], maxU[c]);
        }

        juce::dsp::AudioBlock<float> outBlock (satPtr, (size_t) nch, (size_t) num);
        os.processSamplesDown (outBlock);
    }

    // ---- 4. DC block, restore level, post filters, Exciter Amount
    for (int c = 0; c < nch; ++c)
    {
        auto& d = ch[(size_t) c];
        const float* e = envBuf[(size_t) c].data();
        const float* a = amtBuf[(size_t) c].data();
        float* out = sigBuf[(size_t) c].data();

        for (int s = 0; s < num; ++s)
        {
            const float y = satPtr[c][s];
            const float dc = y - d.dcX + 0.995f * d.dcY;
            d.dcX = y;
            d.dcY = dc;

            float w = dc * e[s];
            w = bs.linear ? d.postFir.process (w)
                          : d.postLp.process (d.postHp.process (w));

            out[s] = w * a[s];
        }
    }

    // ---- 5. harmonics LED
    {
        bool clipped = false, active = false;

        for (int c = 0; c < nch; ++c)
        {
            const auto& p = params (c);
            const float limit = p.mode >= 2 ? 1.0f : 4.0f;

            if (p.amount > 0.001f)
            {
                active = true;
                if (maxU[c] > limit)
                    clipped = true;
            }
        }

        if (bs.bypass)
            active = clipped = false;

        if (clipped)
            clipHold = (int) (0.25 * sr);
        else
            clipHold = std::max (0, clipHold - num);

        ledState.store (clipHold > 0 ? 2 : (active ? 1 : 0));
    }

    // ---- 6. width, latency alignment, and sum with the dry signal
    for (int s = 0; s < num; ++s)
    {
        const int64_t idx = writeIndex + s;

        float wl, wr = 0.0f;

        if (nch > 1)
        {
            const float w0 = sigBuf[0][(size_t) s];
            const float w1 = sigBuf[1][(size_t) s];

            float m, side;
            if (useMs)
            {
                m = w0;
                side = w1;
            }
            else
            {
                m = 0.5f * (w0 + w1);
                side = 0.5f * (w0 - w1);
            }

            side *= widthSm.getNextValue();
            wl = m + side;
            wr = m - side;
        }
        else
        {
            wl = sigBuf[0][(size_t) s];
        }

        wetRing[0][(size_t) (idx & ringMask)] = wl;
        wetRing[1][(size_t) (idx & ringMask)] = wr;

        const float dryG = dryGainSm.getNextValue();
        const float wetG = wetGainSm.getNextValue();

        for (int c = 0; c < nch; ++c)
        {
            const float dry = dryRing[(size_t) c][(size_t) ((idx - totalLatency) & ringMask)];
            const float wet = wetRing[(size_t) c][(size_t) ((idx - wetExtra) & ringMask)];
            data[c][s] = dry * dryG + wet * wetG;
        }
    }

    writeIndex += num;
}

//==============================================================================
void ExciterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ExciterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ExciterProcessor();
}
