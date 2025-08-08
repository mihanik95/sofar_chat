#pragma once

#include <JuceHeader.h>
#include <array>

/**
 * Early Reflection Impulse Response Generator
 * Creates realistic early reflections for different room sizes
 */
class EarlyReflectionIR
{
public:
    EarlyReflectionIR() = default;
    ~EarlyReflectionIR() = default;

    /**
     * Prepare internal buffers for processing. The processing block is small so
     * we preallocate a temporary buffer once and reuse it to avoid dynamic
     * allocations in the audio thread.
     */
    void prepare (double sampleRate, int samplesPerBlock, int numChannelsIn = 2)
    {
        currentSampleRate = sampleRate;
        numChannels       = numChannelsIn;

        const int maxDelaySamples = static_cast<int> (sampleRate * 0.05); // 50 ms headroom
        reflectionBuffer.setSize (numChannels, samplesPerBlock + maxDelaySamples);
        reflectionBuffer.clear();
    }

    void reset()
    {
        reflectionBuffer.clear();
    }

    void setRoomDimensions (float width, float height, float length)
    {
        roomWidth  = width;
        roomHeight = height;
        roomLength = length;
    }

    /**
     * Add a crude set of first‑order reflections. This is intentionally simple
     * but provides noticeably more spatial impression than the previous stub.
     */
    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const float c = 343.0f; // speed of sound (m/s)

        reflectionBuffer.clear();

        // Copy dry signal
        for (int ch = 0; ch < juce::jmin (numChannels, buffer.getNumChannels()); ++ch)
            reflectionBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        // Distances to six walls from the source placed at centre
        const float distX = roomWidth  * 0.5f;
        const float distY = roomHeight * 0.5f;
        const float distZ = roomLength * 0.5f;

        const std::array<float,6> delays {
            distX / c,  // left wall
            distX / c,  // right wall
            distY / c,  // floor
            distY / c,  // ceiling
            distZ / c,  // front wall
            distZ / c   // back wall
        };

        const std::array<float,6> gains { 0.5f, 0.5f, 0.5f, 0.5f, 0.4f, 0.4f };

        for (size_t i = 0; i < delays.size(); ++i)
        {
            const int delaySamples = static_cast<int> (delays[i] * currentSampleRate);
            for (int ch = 0; ch < juce::jmin (numChannels, buffer.getNumChannels()); ++ch)
                reflectionBuffer.addFrom (ch, delaySamples, buffer, ch, 0, numSamples, gains[i]);
        }

        // Write back the processed block
        for (int ch = 0; ch < juce::jmin (numChannels, buffer.getNumChannels()); ++ch)
            buffer.copyFrom (ch, 0, reflectionBuffer, ch, 0, numSamples);
    }

private:
    double currentSampleRate = 44100.0;
    float  roomWidth  = 6.0f;
    float  roomHeight = 3.0f;
    float  roomLength = 8.0f;
    int    numChannels = 2;

    juce::AudioBuffer<float> reflectionBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarlyReflectionIR)
};
