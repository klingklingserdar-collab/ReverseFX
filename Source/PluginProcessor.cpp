#include "PluginProcessor.h"
#include <cmath>

namespace
{
    inline float saturate (float x, float drive)
    {
        const float g = 1.0f + drive * 9.0f;
        return std::tanh (x * g) / std::tanh (g);
    }
}

ReverseFXProcessor::ReverseFXProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ReverseFXProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "record", 1 }, "Record", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "play", 1 }, "Play Reversed", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "drive", 1 }, "Saturation",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "size", 1 }, "Reverb Size",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "damping", 1 }, "Reverb Damping",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "reverbMix", 1 }, "Reverb Mix",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "dry", 1 }, "Dry Level",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    return layout;
}

void ReverseFXProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    recordBuffer.setSize (2, (int) (sampleRate * 12.0));
    recordBuffer.clear();
    wetBuffer.setSize (2, juce::jmax (samplesPerBlock, 1));
    wetBuffer.clear();

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    writePos = 0;
    recordedLength = 0;
    readPos = -1;
    wasRecording = false;
    wasPlayPressed = false;
}

bool ReverseFXProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void ReverseFXProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples == 0)
        return;

    const bool recording   = apvts.getRawParameterValue ("record")->load() > 0.5f;
    const bool playPressed = apvts.getRawParameterValue ("play")->load() > 0.5f;
    const float drive      = apvts.getRawParameterValue ("drive")->load();
    const float size       = apvts.getRawParameterValue ("size")->load();
    const float damping    = apvts.getRawParameterValue ("damping")->load();
    const float reverbMix  = apvts.getRawParameterValue ("reverbMix")->load();
    const float dry        = apvts.getRawParameterValue ("dry")->load();

    if (recording && ! wasRecording)
    {
        writePos = 0;
        recordedLength = 0;
        readPos = -1;
    }
    if (! recording && wasRecording)
        recordedLength = writePos;
    wasRecording = recording;

    if (playPressed && ! wasPlayPressed && ! recording && recordedLength > 0)
        readPos = recordedLength - 1;
    wasPlayPressed = playPressed;

    wetBuffer.setSize (2, numSamples, false, false, true);

    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getReadPointer (1);
    float* wL = wetBuffer.getWritePointer (0);
    float* wR = wetBuffer.getWritePointer (1);
    const int cap = recordBuffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        if (recording && writePos < cap)
        {
            recordBuffer.setSample (0, writePos, inL[i]);
            recordBuffer.setSample (1, writePos, inR[i]);
            ++writePos;
        }

        float l = 0.0f, r = 0.0f;
        if (readPos >= 0 && ! recording)
        {
            l = recordBuffer.getSample (0, readPos);
            r = recordBuffer.getSample (1, readPos);
            --readPos;
        }

        wL[i] = saturate (l, drive);
        wR[i] = saturate (r, drive);
    }

    juce::Reverb::Parameters p;
    p.roomSize   = size;
    p.damping    = damping;
    p.wetLevel   = reverbMix * 0.5f;
    p.dryLevel   = (1.0f - reverbMix) * 0.5f;
    p.width      = 1.0f;
    p.freezeMode = 0.0f;
    reverb.setParameters (p);
    reverb.processStereo (wL, wR, numSamples);

    for (int ch = 0; ch < 2; ++ch)
    {
        float* out = buffer.getWritePointer (ch);
        const float* w = wetBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            out[i] = out[i] * dry + w[i];
    }
}

juce::AudioProcessorEditor* ReverseFXProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void ReverseFXProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ReverseFXProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverseFXProcessor();
}
