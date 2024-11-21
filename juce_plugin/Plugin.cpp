/*
    WvTable plugin in JUCE - for testing only
*/

#ifndef JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#endif

#include <cstdint>
#include <array>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <userosc2.h>

static constexpr uint32_t blockSize = 32;

//==============================================================================
/** A dummy synth sound. */

class SynthSound final : public juce::SynthesiserSound {
public:
    SynthSound() { }

    bool appliesToNote(int /*midiNoteNumber*/) override
    {
        return true;
    }
    bool appliesToChannel(int /*midiChannel*/) override
    {
        return true;
    }
};

//==============================================================================
/** The synthesizer voice. */

class SynthVoice final : public juce::SynthesiserVoice {
public:
    SynthVoice()
    {
        oscParam.pitch = 69;
        oscParam.shape_lfo = 0;
        oscParam.cutoff = 0;
        oscParam.resonance = 0;
        // buffer.fill(0);
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SynthSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float /*velocity*/, juce::SynthesiserSound* /*sound*/,
        int /*currentPitchWheelPosition*/) override
    {
        oscParam.pitch = static_cast<uint16_t>(midiNoteNumber) << 8;
        OSC_NOTEON(&oscParam);
        gain = 1.f;
    }

    void stopNote(float /*velocity*/, bool allowTailOff) override
    {
        if (!isVoiceActive())
            return; // voice not running, no need to process
        // clearCurrentNote(); // temporary
        OSC_NOTEOFF(&oscParam);
        if (allowTailOff) { // start tail off
            gain = tailAlpha.get();
        } else { // stop the note now
            clearCurrentNote();
        }
    }

    void pitchWheelMoved(int /*newValue*/) override { }

    void controllerMoved(int /*controllerNumber*/, int /*newValue*/) override { }

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        // We need to run the oscillator, but we should not fill the buffer if the voice
        // is inactive. Otherwise, the sound never stops.
        const auto active = isVoiceActive();

        // sample rendering loop
        while (--numSamples >= 0) {
            if (bufIndex == blockSize) {
                // generate new samples
                OSC_CYCLE(&oscParam, buffer.data(), blockSize);
                bufIndex = 0;
            }
            if (active) {
                const float currentSample = static_cast<float>(buffer[bufIndex++]) * 4.656612873077393e-10f * gain;
                for (auto i = outputBuffer.getNumChannels(); --i >= 0;) {
                    outputBuffer.addSample(i, startSample, currentSample);
                }
                if (gain < 1.f) {
                    gain *= tailAlpha.get();
                    if (gain < 0.0067f) { // about 5*tau
                        clearCurrentNote();
                        gain = 0;
                    }
                }
            } else {
                // no note playing in this voice
                bufIndex++;
            }
            ++startSample;
        }
    }

    // Set alpha value for tail off
    static void setTailAlpha(float alpha)
    {
        tailAlpha.set(alpha);
    }

private:
    std::array<int32_t, blockSize> buffer = {};
    uint32_t bufIndex = blockSize;
    user_osc_param_t oscParam;
    float gain = 0.f; // local gain used for tail off
    inline static juce::Atomic<float> tailAlpha { 0.f };
};

//==============================================================================
/** The audio processor. */

class AudioPluginAudioProcessor final : public juce::AudioProcessor,
                                        private juce::AudioProcessorValueTreeState::Listener {
public:
    AudioPluginAudioProcessor()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
        , state(*this, nullptr, "Parameters", createParameterLayout())
    {
        // initialize the module (it's shared between the voices)
        OSC_INIT(0, 0);
        for (uint16_t i = 0; i < k_num_user_osc_param_id; ++i) {
            OSC_PARAM(i, 0);
        }

        // initialize the synth
        // for (auto i = 0; i < 4U; ++i)
        synth.addVoice(new SynthVoice());
        synth.addSound(new SynthSound());
        SynthVoice::setTailAlpha(0.9997916883665486f); // 0.5 s

        // create pointers to parameters
        paramWave = dynamic_cast<juce::AudioParameterInt*>(state.getParameter("wave"));
        paramSkew = dynamic_cast<juce::AudioParameterInt*>(state.getParameter("skew"));
        paramWavetable = dynamic_cast<juce::AudioParameterInt*>(state.getParameter("wavetable"));
        paramEnvAttack = dynamic_cast<juce::AudioParameterInt*>(state.getParameter("env_attack"));
        paramEnvDecay = dynamic_cast<juce::AudioParameterInt*>(state.getParameter("env_decay"));
        paramEnvAmount = dynamic_cast<juce::AudioParameterInt*>(state.getParameter("env_amount"));
        paramLfoRate = dynamic_cast<juce::AudioParameterInt*>(state.getParameter("lfo_rate"));
        paramLfoAmount = dynamic_cast<juce::AudioParameterInt*>(state.getParameter("lfo_amount"));
        paramRelease = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter("release"));
        paramGain = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter("gain"));

        // add parameter listeners
        state.addParameterListener("wave", this);
        state.addParameterListener("skew", this);
        state.addParameterListener("wavetable", this);
        state.addParameterListener("env_attack", this);
        state.addParameterListener("env_decay", this);
        state.addParameterListener("env_amount", this);
        state.addParameterListener("lfo_rate", this);
        state.addParameterListener("lfo_amount", this);
        state.addParameterListener("release", this);
    }

    ~AudioPluginAudioProcessor() override { };

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        // Only mono/stereo and input/output must have same layout
        const auto& mainOutput = layouts.getMainOutputChannelSet();
        const auto& mainInput = layouts.getMainInputChannelSet();
        // input and output layout must either be the same or the input must be disabled altogether
        if (!mainInput.isDisabled() && mainInput != mainOutput)
            return false;
        // only allow stereo and mono
        if (mainOutput.size() > 2)
            return false;
        return true;
    }

    void prepareToPlay(double sampleRate, int /*samplesPerBlock*/) override
    {
        synth.setCurrentPlaybackSampleRate(sampleRate);
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        juce::ScopedNoDenormals noDenormals;
        const auto numSamples = buffer.getNumSamples();
        for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
            buffer.clear(i, 0, numSamples);

        synth.renderNextBlock(buffer, midiMessages, 0, numSamples);

        for (auto channel = 0; channel < getTotalNumOutputChannels(); ++channel)
            // buffer.applyGain(channel, 0, numSamples, state.getParameter("gain")->getValue());
            buffer.applyGain(channel, 0, numSamples, paramGain->get());
    }

    void releaseResources() override { };

    juce::AudioProcessorEditor* createEditor() override
    {
        juce::AudioProcessorEditor* editor = new juce::GenericAudioProcessorEditor(*this);
        // editor->setSize(800, 600);
        return editor;
    }

    bool hasEditor() const override
    {
        return true;
    }

    const juce::String getName() const override
    {
        return "WvTable";
    }

    bool acceptsMidi() const override
    {
        return true;
    }

    bool producesMidi() const override
    {
        return false;
    }

    bool isMidiEffect() const override
    {
        return false;
    }

    double getTailLengthSeconds() const override
    {
        return 0.0;
    }

    int getNumPrograms() override
    {
        return 1;
    }

    int getCurrentProgram() override
    {
        return 0;
    }

    void setCurrentProgram(int /*index*/) override { }

    const juce::String getProgramName(int /*index*/) override
    {
        return "";
    }

    void changeProgramName(int /*index*/, const juce::String& /*newName*/) override { }

    void getStateInformation(juce::MemoryBlock& /*destData*/) override { }

    void setStateInformation(const void* /*data*/, int /*sizeInBytes*/) override { }

    // Callback for partameter changes
    void parameterChanged(const juce::String& id, float) override
    {
        if (id == "release") {
            const float tailTime = paramRelease->get();
            const float alpha = std::exp(-5.f / (tailTime * 48000.f));
            SynthVoice::setTailAlpha(alpha);
        } else if (id == "wave") {
            OSC_PARAM(k_user_osc_param_shape, static_cast<uint16_t>(paramWave->get()));
        } else if (id == "skew") {
            OSC_PARAM(k_user_osc_param_shiftshape, static_cast<uint16_t>(paramSkew->get()));
        } else if (id == "wavetable") {
            OSC_PARAM(k_user_osc_param_id1, static_cast<uint16_t>(paramWavetable->get()));
        } else if (id == "env_attack") {
            OSC_PARAM(k_user_osc_param_id2, static_cast<uint16_t>(paramEnvAttack->get()));
        } else if (id == "env_decay") {
            OSC_PARAM(k_user_osc_param_id3, static_cast<uint16_t>(paramEnvDecay->get() + 100));
        } else if (id == "env_amount") {
            OSC_PARAM(k_user_osc_param_id4, static_cast<uint16_t>(paramEnvAmount->get() + 100));
        } else if (id == "lfo_rate") {
            OSC_PARAM(k_user_osc_param_id5, static_cast<uint16_t>(paramLfoRate->get()));
        } else if (id == "lfo_amount") {
            OSC_PARAM(k_user_osc_param_id6, static_cast<uint16_t>(paramLfoAmount->get()));
        }
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add(std::make_unique<juce::AudioParameterInt>("wave", "Wave", 0, 1023, 0));
        layout.add(std::make_unique<juce::AudioParameterInt>("skew", "Skew", 0, 1023, 0));
        layout.add(std::make_unique<juce::AudioParameterInt>("wavetable", "Wavetable", 0, 95, 0));
        layout.add(std::make_unique<juce::AudioParameterInt>("env_attack", "Env Attack", 0, 100, 0));
        layout.add(std::make_unique<juce::AudioParameterInt>("env_decay", "Env Decay", -99, 100, 0));
        layout.add(std::make_unique<juce::AudioParameterInt>("env_amount", "Env Amount", -99, 100, 0));
        layout.add(std::make_unique<juce::AudioParameterInt>("lfo_rate", "LFO2 Rate", 0, 100, 0));
        layout.add(std::make_unique<juce::AudioParameterInt>("lfo_amount", "LFO2 Amount", 0, 100, 0));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            "release", "Release", juce::NormalisableRange<float>(0.f, 3.f, 0.05f, 0.5f), 0.5f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            "gain", "Gain", juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.8f));

        return layout;
    }

    juce::Synthesiser synth;
    juce::AudioProcessorValueTreeState state;

    juce::AudioParameterInt* paramWave;
    juce::AudioParameterInt* paramSkew;
    juce::AudioParameterInt* paramWavetable;
    juce::AudioParameterInt* paramEnvAttack;
    juce::AudioParameterInt* paramEnvDecay;
    juce::AudioParameterInt* paramEnvAmount;
    juce::AudioParameterInt* paramLfoRate;
    juce::AudioParameterInt* paramLfoAmount;
    juce::AudioParameterFloat* paramRelease;
    juce::AudioParameterFloat* paramGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};

//==============================================================================
/** This creates new instances of the plugin. */
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
