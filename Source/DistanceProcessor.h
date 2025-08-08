#pragma once

#include <JuceHeader.h>
#include <limits>
#include "MySofaHRIR.h"
#include "EarlyReflectionIR.h"

//==============================================================================
/**
    Advanced Scientific Distance Effect Processor
    Based on psychoacoustic research and ISO 9613-1 standards
    Features: Inverse square law, ISO air absorption, proximity compensation,
             early reflections, transient processing, environment modeling
*/
class DistanceProcessor
{
public:
    //==============================================================================
    // Environment types
    enum Environment
    {
        Room = 0,
        Studio,
        Hall,
        Cave,
        numEnvironments
    };
    
    // Air Absorption modes (TDR research)
    enum AirAbsorptionMode
    {
        AirAbsorptionA = 0,  // Gentle high-shelf filter (6 dB/octave)
        AirAbsorptionB       // Aggressive low-pass filter (12 dB/octave)
    };
    
    // Proximity Effect modes (TDR research)
    enum ProximityEffectMode
    {
        ProximityEffectA = 0,  // Sharp high-pass filter (steep cut)
        ProximityEffectB       // Low-shelf filter (gentle bass reduction)
    };

    //==============================================================================
    DistanceProcessor();
    ~DistanceProcessor() = default;

    //==============================================================================
    void prepare(double sampleRate, int samplesPerBlock);
    void reset();
    void processBlock(juce::AudioBuffer<float>& buffer, float distance, float panValue, Environment environment);

    //==============================================================================
    void setEnvironmentType(Environment envType);
    float getMaxDistanceForEnvironment(Environment envType) const;
    
    // Real-time parameter updates from UI
    // Simplified parameter setters
    void setDistance(float distanceMeters);
    void setMaxDistance(float maxDistanceMeters);
    void setRoomWidth(float roomWidthMeters);
    void setRoomHeight(float roomHeightMeters);
    void setRoomLength(float roomLengthMeters);
    void setAirAbsorption(float absorption);
    void setVolumeCompensation(float compensation);
    void setTemperature(float temperatureCelsius);
    void setSourceHeight(float heightPercent);
    void setClarity(float clarity);

    // TDR Proximity research-based parameters
    float originalDistance = 1.0f;  // Reference distance for gain calibration
    bool trueGainEnabled = true;    // Distance gain loss module
    bool trueDelayEnabled = true;   // True delay enabled
    
    AirAbsorptionMode currentAirAbsorptionMode = AirAbsorptionA;
    ProximityEffectMode currentProximityEffectMode = ProximityEffectA;
    
    float speedOfSound = 343.0f; // m/s at 20°C (now mutable for temperature adjustments)

private:
    //==============================================================================
    // Advanced processing pipeline
    void processDistanceEffects(juce::AudioBuffer<float>& buffer, float distance, float panValue, int numSamples);
    void processDelayEffect(juce::AudioBuffer<float>& buffer, float distance, int numSamples);
    void processDistanceGain(juce::AudioBuffer<float>& buffer, float distance, int numSamples);
    void processAirAbsorption(juce::AudioBuffer<float>& buffer, float distance, int numSamples);
    void processStereoWidth(juce::AudioBuffer<float>& buffer, float distance, int numSamples);
    void processPanning(juce::AudioBuffer<float>& buffer, float panValue, int numSamples);
    void processHeightEffects(juce::AudioBuffer<float>& buffer, int numSamples);
    
    void updateEnvironmentParameters(Environment environment);

    //==============================================================================
    // Core audio processing parameters
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;
    
    //==============================================================================
    // Parameter smoothing to prevent artifacts
    juce::SmoothedValue<float> smoothedDistance{0.0f};
    juce::SmoothedValue<float> smoothedPan{0.0f};
    juce::SmoothedValue<float> smoothedGain{1.0f};
    juce::SmoothedValue<float> smoothedCutoffFreq{20000.0f};
    juce::SmoothedValue<float> smoothedStereoWidth{1.0f};
    juce::SmoothedValue<float> smoothedLeftPanGain{0.707f};
    juce::SmoothedValue<float> smoothedRightPanGain{0.707f};
    juce::SmoothedValue<float> smoothedDelayTime{0.0f};
    juce::SmoothedValue<float> smoothedHeight{0.5f};
    juce::SmoothedValue<float> smoothedClarity{1.0f}; // wet mix factor
    
    // Height processing smoothed parameters
    juce::SmoothedValue<float> smoothedTiltGain{0.0f};
    juce::SmoothedValue<float> smoothedHeightWidth{1.0f};
    
    // Panning processing smoothed parameters
    juce::SmoothedValue<float> smoothedShadowCutoff{12000.0f};
    juce::SmoothedValue<float> smoothedFrontBackWidth{1.0f};
    juce::SmoothedValue<float> smoothedPhaseShift{0.0f};
    juce::SmoothedValue<float> smoothedBrightness{1.0f};
    juce::SmoothedValue<float> smoothedIldGainL{0.707f};
    juce::SmoothedValue<float> smoothedIldGainR{0.707f};
    juce::SmoothedValue<float> smoothedEarDelayLeft{0.0f};
    juce::SmoothedValue<float> smoothedEarDelayRight{0.0f};
    
    //==============================================================================
    // Advanced filter chain - separate filters for perfect stereo balance
    juce::dsp::IIR::Filter<float> lowPassFilterLeft;
    juce::dsp::IIR::Filter<float> lowPassFilterRight;
    juce::dsp::IIR::Filter<float> backFilterLeft;
    juce::dsp::IIR::Filter<float> backFilterRight;
    juce::dsp::IIR::Filter<float> heightTiltFilterLeft;
    juce::dsp::IIR::Filter<float> heightTiltFilterRight;
    juce::dsp::DelayLine<float> delayLine;
    juce::dsp::Gain<float> gainProcessor;
    
    // Cache last applied cutoff to avoid per-sample coefficient churn
    float lastCutoffFreq = 20000.0f;
    float lastShadowCutoff = 12000.0f;
    float lastTiltGain = 0.0f;
    float phaseAccumulator = 0.0f;
    
    //==============================================================================
    // Environment and processing state
    Environment currentEnvironment = Room;
    Environment lastEnvironment = Room;
    float currentDistance = 0.0f;
    float currentPan = 0.0f;
    float leftPanGain = 0.707f;
    float rightPanGain = 0.707f;
    
    // Simplified parameter storage
    float currentMaxDistance = 20.0f;
    float currentRoomWidth = 6.0f;
    float currentRoomHeight = 3.0f;
    float currentRoomLength = 8.0f;  // Default room length
    float currentAirAbsorption = 0.5f;
    float currentVolumeCompensation = 0.3f;
    float currentTemperature = 20.0f;
    float currentHeightPercent = 0.5f; // 0-1
    float currentClarity = 1.0f; // 0=dry 1=wet
    
    // Scientific parameters (configurable per environment)
    struct EnvironmentParams
    {
        float maxDistance;            // Maximum realistic distance for this environment
        float roomSize;               // Room size factor
        float decayTime;              // Reverb decay time
        float damping;                // Reverb damping
        float reverbLevel;            // Reverb strength
        float airAbsorptionCoeff;     // Air absorption coefficient
        float diffusion;              // Diffusion amount
        float preDelay;               // Pre-delay in ms
    };
    
    EnvironmentParams environmentSettings[numEnvironments];

    static constexpr float listenerEarHeight = 1.7f; // metres above floor – average ear height when seated/standing

    // Early reflection processor
    EarlyReflectionIR earlyReflection;

    // HRTF binaural convolution
    MySofaHrirDatabase hrirDatabase;
    juce::dsp::Convolution hrtfLeft { juce::dsp::Convolution::NonUniform { 128 } };
    juce::dsp::Convolution hrtfRight{ juce::dsp::Convolution::NonUniform { 128 } };
    juce::AudioBuffer<float> hrtfTempBuffer;
    float lastAzimuthDeg = 0.0f, lastElevationDeg = 0.0f;

    // Cache last geometry state to avoid expensive updates each block
    float lastGeomRoomWidth  = -1.0f;
    float lastGeomRoomLength = -1.0f;
    float lastGeomRoomHeight = -1.0f;
    float lastGeomSrcX       = std::numeric_limits<float>::infinity();
    float lastGeomSrcY       = std::numeric_limits<float>::infinity();
    float lastGeomSrcZ       = std::numeric_limits<float>::infinity();

    void updateHrirFilters(float azimuthDeg, float elevationDeg);
    void processHrtfConvolution(juce::AudioBuffer<float>& buffer);

    // Ear-specific micro-delay lines for ITD (inter-aural time difference)
    juce::dsp::DelayLine<float> earDelayLeft { 480 };  // ≈10 ms @ 48 kHz max
    juce::dsp::DelayLine<float> earDelayRight{ 480 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistanceProcessor)
};