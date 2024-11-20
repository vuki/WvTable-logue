/*
    WvTable plugin in JUCE - for testing only
*/

#ifndef JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#endif

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

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
    SynthVoice() { }

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SynthSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* /*sound*/,
        int /*currentPitchWheelPosition*/) override
    {
        currentAngle = 0.0;
        level = velocity * 0.15;
        tailOff = 0.0;

        auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        auto cyclesPerSample = cyclesPerSecond / getSampleRate();

        angleDelta = cyclesPerSample * juce::MathConstants<double>::twoPi;
    }

    void stopNote(float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff) {
            if (juce::approximatelyEqual(tailOff, 0.0)) // we only need to begin a tail-off if it's not already doing so
                                                        // - the stopNote method could be called more than once.
                tailOff = 1.0;
        } else {
            // we're being told to stop playing immediately, so reset everything..
            clearCurrentNote();
            angleDelta = 0.0;
        }
    }

    void pitchWheelMoved(int /*newValue*/) override { }

    void controllerMoved(int /*controllerNumber*/, int /*newValue*/) override { }

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (!juce::approximatelyEqual(angleDelta, 0.0)) {
            if (tailOff > 0.0) {
                while (--numSamples >= 0) {
                    auto currentSample = (float)(sin(currentAngle) * level * tailOff);

                    for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                        outputBuffer.addSample(i, startSample, currentSample);

                    currentAngle += angleDelta;
                    ++startSample;

                    tailOff *= 0.99;

                    if (tailOff <= 0.005) {
                        // tells the synth that this voice has stopped
                        clearCurrentNote();

                        angleDelta = 0.0;
                        break;
                    }
                }
            } else {
                while (--numSamples >= 0) {
                    auto currentSample = (float)(sin(currentAngle) * level);

                    for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                        outputBuffer.addSample(i, startSample, currentSample);

                    currentAngle += angleDelta;
                    ++startSample;
                }
            }
        }
    }

    using SynthesiserVoice::renderNextBlock;

private:
    double currentAngle = 0.0;
    double angleDelta = 0.0;
    double level = 0.0;
    double tailOff = 0.0;
};

//==============================================================================
/** The audio processor. */

class AudioPluginAudioProcessor final : public juce::AudioProcessor, private juce::ValueTree::Listener {
public:
    AudioPluginAudioProcessor()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
        , state(*this, nullptr, "Parameters", createParameterLayout())
    {
        // initialize synth
        for (auto i = 0; i < 4U; ++i)
            synth.addVoice(new SynthVoice());
        synth.addSound(new SynthSound());

        // create pointers to parameters
        paramGain = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter("gain"));

        state.state.addListener(this);
    }

    ~AudioPluginAudioProcessor() override
    {
        state.state.removeListener(this);
    };

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

    using AudioProcessor::processBlock;

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

    // juce::ValueTree::Listener
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override
    {
        //
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add(std::make_unique<juce::AudioParameterFloat>("gain", "Gain",
            juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.8f, juce::AudioParameterFloatAttributes()));

        return layout;
    }

    juce::Synthesiser synth;
    juce::AudioProcessorValueTreeState state;
    juce::AudioParameterFloat* paramGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};

//==============================================================================
/** This creates new instances of the plugin. */
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
