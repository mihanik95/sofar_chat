#pragma once

#include <JuceHeader.h>
#include <vector>

/**
 * Simplified HRIR Database for spatial audio processing
 * In production, this would load actual SOFA HRIR files
 */
class MySofaHrirDatabase
{
public:
    MySofaHrirDatabase() = default;
    ~MySofaHrirDatabase() = default;

    bool isLoaded() const { return sofaLoaded; }

    bool loadSofaFile (const juce::String& filepath)
    {
        // We do not parse the SOFA content here, but verifying that the file
        // exists gives immediate feedback to the caller. Real loading can be
        // added later without changing the interface.
        sofaLoaded = juce::File (filepath).existsAsFile();
        return sofaLoaded;
    }

    void setSampleRate (double newRate) { sampleRate = newRate; }

    struct HrirData
    {
        std::vector<float> left;
        std::vector<float> right;
    };

    HrirData getNearestHrir (float azimuth, float elevation)
    {
        HrirData data;
        getHrir (azimuth, elevation, data.left, data.right);
        return data;
    }

    /**
     * Very small spherical head model. Generates a pair of impulses with inter
     * aural time and level differences derived from the azimuth. Elevation is
     * currently ignored but kept for API compatibility.
     */
    void getHrir (float azimuth, float elevation,
                  std::vector<float>& leftIR,
                  std::vector<float>& rightIR)
    {
        juce::ignoreUnused (elevation);

        const int irLength = 64;
        leftIR.assign  (irLength, 0.0f);
        rightIR.assign (irLength, 0.0f);

        const float azRad = juce::degreesToRadians (azimuth);
        const float headRadius = 0.0875f;         // metres
        const float c = 343.0f;                   // speed of sound

        const float itdSeconds = headRadius / c * std::sin (azRad);
        const int itdSamples = static_cast<int> (std::round (std::abs (itdSeconds) * sampleRate));

        int leftIndex  = itdSeconds > 0 ? 0 : itdSamples;
        int rightIndex = itdSeconds > 0 ? itdSamples : 0;

        const float ildDb = -6.0f * std::sin (azRad); // crude ILD estimate
        const float leftGain  = juce::Decibels::decibelsToGain (ildDb * 0.5f);
        const float rightGain = juce::Decibels::decibelsToGain (-ildDb * 0.5f);

        if (leftIndex < irLength)  leftIR[leftIndex]  = leftGain;
        if (rightIndex < irLength) rightIR[rightIndex] = rightGain;
    }

private:
    double sampleRate { 44100.0 };
    bool   sofaLoaded { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MySofaHrirDatabase)
};
