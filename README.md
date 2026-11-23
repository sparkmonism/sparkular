# Sparkular

Songcraft Audio. Harmonic exciter (AU / VST3 / Standalone), built with JUCE.
To rename it, change `PRODUCT_NAME` in CMakeLists.txt,
`getName()` in ExciterProcessor.h and the title in `ExciterEditor::paint`.

The control set follows the Softube Weiss Exciter, with oversampling added.

## Build (macOS)

Requires Xcode command line tools and CMake (`brew install cmake`). From inside this folder:

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j 8

The AU is copied to `~/Library/Audio/Plug-Ins/Components/` automatically.

    killall -9 AudioComponentRegistrar
    auval -v aufx Exct Sngc

## Controls

Per channel (Ch 1 = left or mid, Ch 2 = right or side). Pick the channel with
the CH 1 / CH 2 buttons.

- Frequency window (top): drag the edges of the PRE band (input filter, lime) and
  POST band (output filter). The PRE band is where harmonics are generated from;
  the POST band is where they are allowed back out. Keep Post HP at least twice Pre HP.
- Harmonics: how hard the generator is driven. The LED is lime while harmonics are
  being added and red on hard clipping (occasional red flashes are fine).
- Subtle: lowers the drive into the generator by Subtle Gain.
- 2nd: adds even harmonics at the 2nd Level.
- Exciter Amount: level of the generated signal added to the dry signal. The dry
  path is never reduced.
- Mastering: removes the estimated fundamental from the generator output, so only
  the added harmonics are left. Exact for steady tones, an approximation on
  complex material.
- Adaptive: normalises the level into the generator so the harmonic content stays
  consistent whatever the input level.
- Mode: Soft, Hard, Poly 3 (3rd harmonic only), Poly 5 (3rd and 5th).

Shared:

- Ganged: both channels use Ch 1's settings. While ganged, only Ch 1 is editable.
- M/S: process mid/side instead of left/right.
- Width: level of the side signal in the added harmonics (dry path unaffected).
- Monitor: listen to the added harmonics only.
- Bypass: soft bypass, latency compensated.
- Slope: Gentle (2nd order) or Steep (4th order) filters.
- Linear Phase: linear-phase FIR filters instead of minimum-phase.
- Oversampling: Off / 2x / 4x / 8x around the harmonics generator.
- Copy 1 > 2 / Copy 2 > 1: copy all per-channel settings across.

## Notes

- Reported latency is constant (about 22 ms at 48 kHz): the linear-phase filter
  delay plus the 8x oversampler delay. Other modes are padded to match.
- Minimum-phase mode does not phase-match the dry path the way the Weiss does.
  If you blend broad overlapping bands, use Linear Phase.
- Not included from the Weiss: the output analyzer, the harmonic-difference graph
  and its range/tilt options.
