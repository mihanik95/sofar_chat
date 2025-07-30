#pragma once

#include <JuceHeader.h>

/**
 * Simplified HRIR Database for spatial audio processing
 * In production, this would load actual SOFA HRIR files
 */
class MySofaHrirDatabase
{
public:
    MySofaHrirDatabase() = default;
    ~MySofaHrirDatabase() = default;

    bool isLoaded() const { return true; } // Simplified - always "loaded"

    bool loadSofaFile(const juce::String& filepath)
    {
        // In production would load actual SOFA file
        // For now just return success
        return true;
    }

    struct HrirData
    {
        std::vector<float> left;   // Changed from leftIR
        std::vector<float> right;  // Changed from rightIR
    };

    HrirData getNearestHrir(float azimuth, float elevation)
    {
        HrirData data;
        getHrir(azimuth, elevation, data.left, data.right);
        return data;
    }

    void getHrir(float azimuth, float elevation, 
                 std::vector<float>& leftIR, 
                 std::vector<float>& rightIR)
    {
        // Simplified HRIR - just return dummy impulse responses
        // In production, this would interpolate from actual HRIR measurements
        const int irLength = 128;
        leftIR.resize(irLength);
        rightIR.resize(irLength);
        
        // Simple impulse based on angle
        float delay = (azimuth / 180.0f) * 0.001f; // Simple ITD model
        int delaySamples = static_cast<int>(delay * 44100.0f);
        
        if (delaySamples < irLength)
        {
            leftIR[delaySamples] = azimuth < 0 ? 0.8f : 0.6f;
            rightIR[delaySamples] = azimuth > 0 ? 0.8f : 0.6f;
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MySofaHrirDatabase)
};
