# WvTable module as JUCE plugin

This module compiles WvTable code as a JUCE VST plugin and standalone EXE.

This project is intended only for debugging of the module.
The synthesizer is monophonic (single voice).
A simple GUI allows for controlling the parameters the same way as in the real synth.

To build, put JUCE files into the JUCE directory
(only modules, extras and CMakeListst.txt are needed)
and run `cmake -B build` and `cmake --build build --config Release`.