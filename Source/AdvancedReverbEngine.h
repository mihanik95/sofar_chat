#pragma once

#include <JuceHeader.h>
#include <array>

//==============================================================================
// Forward declarations
class ModulatedAllpass;
class FDNReverbTank;
class EarlyReflectionsEngine;
class ShimmerEffect;
class DiffusionSection;

//==============================================================================
/**
    Modulated Allpass Filter
*/
class ModulatedAllpass
{
public:
    ModulatedAllpass();
    ~ModulatedAllpass();
    
    void prepare(double sampleRate, int maxDelayInSamples);
    void reset();
    void setFeedback(float feedback);
    void setModulationDepth(float depth);
    void setModulationRate(float rateHz);
    void setDelay(float delayInSamples);
    float processSample(float input);
    
private:
    juce::dsp::DelayLine<float> delayLine;
    double sampleRate = 44100.0;
    float feedback = 0.5f;
    float modDepth = 0.0f;
    float modRate = 0.0f;
    float baseDelay = 1.0f;
    float modPhase = 0.0f;
    float previousOutput = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulatedAllpass)
};

//==============================================================================
/**
    FDN Reverb Tank
*/
class FDNReverbTank
{
public:
    FDNReverbTank();
    ~FDNReverbTank();
    
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setDecayTime(float decaySeconds);
    void setSize(float sizeMultiplier);
    void setDamping(float damping);
    void setModulationDepth(float depth);
    void setModulationRate(float rateHz);
    void processStereo(float leftInput, float rightInput, float& leftOutput, float& rightOutput);
    
private:
    static constexpr int NUM_CHANNELS = 8;
    
    std::array<juce::dsp::DelayLine<float>, NUM_CHANNELS> delayLines;
    std::array<juce::dsp::IIR::Filter<float>, NUM_CHANNELS> dampingFilters;
    std::array<float, NUM_CHANNELS> delayLengths;
    std::array<float, NUM_CHANNELS> feedback;
    std::array<float, NUM_CHANNELS> modPhases;
    std::array<float, NUM_CHANNELS> modRates;
    std::array<std::array<float, NUM_CHANNELS>, NUM_CHANNELS> mixingMatrix;
    
    double sampleRate = 44100.0;
    float decayTime = 2.0f;
    float sizeMultiplier = 1.0f;
    float dampingAmount = 0.6f; // increased damping to reduce resonances
    float modDepth = 0.1f; // reduced modulation depth
    float modRate = 0.15f;
    
    void initializeMixingMatrix();
    void updateDelayLengths();
    void updateFeedback();
    void updateDamping();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FDNReverbTank)
};

//==============================================================================
/**
    Early Reflections Engine
*/
class EarlyReflectionsEngine;

class EarlyReflectionsEngine
{
public:
    EarlyReflectionsEngine();
    ~EarlyReflectionsEngine();
    
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setRoomSize(float size);
    void setPattern(int patternIndex);
    void setLevel(float level);
    void setCrossfeed(float crossfeed);
    void configureGeometry(float roomWidth, float roomLength, float roomHeight,
                           float srcX, float srcY, float srcZ);
    void processStereo(float leftInput, float rightInput, float& leftOutput, float& rightOutput);
    
private:
    static constexpr int MAX_REFLECTIONS = 24;
    
    std::array<juce::dsp::DelayLine<float>, MAX_REFLECTIONS> delayLines;
    std::array<float, MAX_REFLECTIONS> delayTimes;
    std::array<float, MAX_REFLECTIONS> gains;
    std::array<float, MAX_REFLECTIONS> panPositions;
    
    double sampleRate = 44100.0;
    float roomSize = 1.0f;
    float level = 0.3f;
    float crossfeed = 0.0f;
    int currentPattern = 0;
    
    void loadPattern(int patternIndex);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EarlyReflectionsEngine)
};

//==============================================================================
/**
    Shimmer Effect
*/
class ShimmerEffect
{
public:
    ShimmerEffect();
    ~ShimmerEffect();
    
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setEnabled(bool enabled);
    void setPitchShift(float semitones);
    void setFeedback(float feedback);
    void setMix(float mix);
    float processSample(float input);
    
private:
    juce::dsp::DelayLine<float> delayLine;
    double sampleRate = 44100.0;
    bool enabled = false;
    float pitchShift = 12.0f;
    float feedbackAmount = 0.3f;
    float mix = 0.1f;
    
    // Simple pitch shifting variables
    float writePhase = 0.0f;
    float readPhase1 = 0.0f;
    float readPhase2 = 0.0f;
    float pitchRatio = 1.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShimmerEffect)
};

//==============================================================================
/**
    Diffusion Section
*/
class DiffusionSection
{
public:
    DiffusionSection();
    ~DiffusionSection();
    
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setDiffusion(float diffusion);
    void setModulationDepth(float depth);
    void setModulationRate(float rateHz);
    void processStereo(float leftInput, float rightInput, float& leftOutput, float& rightOutput);
    
private:
    static constexpr int NUM_STAGES = 4;
    
    std::array<std::unique_ptr<ModulatedAllpass>, NUM_STAGES> leftAllpasses;
    std::array<std::unique_ptr<ModulatedAllpass>, NUM_STAGES> rightAllpasses;
    
    double sampleRate = 44100.0;
    float diffusion = 0.5f;
    float modDepth = 0.0f;
    float modRate = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiffusionSection)
};

//==============================================================================
/**
    Advanced Reverb Engine (Stub Implementation)
    This is a simplified version that uses JUCE's built-in reverb
    and provides the interface for future advanced implementation.
*/
class AdvancedReverbEngine
{
public:
    //==============================================================================
    // Algorithm types (for future implementation)
    enum AlgorithmType
    {
        Hall = 0,
        Room,
        Plate,
        Spring,
        Chamber,
        Cathedral,
        Random,
        numAlgorithms
    };
    
    // Modulation types (for future implementation)
    enum ModulationType
    {
        Off = 0,
        Pitch,
        PitchFat,
        RandomMod,
        RandomFat,
        Chorus,
        Spin,
        numModulationTypes
    };
    
    //==============================================================================
    AdvancedReverbEngine();
    ~AdvancedReverbEngine();
    
    //==============================================================================
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void processStereo(float leftInput, float rightInput, float& leftOutput, float& rightOutput);
    
    //==============================================================================
    // Parameter setters
    void setAlgorithm(AlgorithmType algorithm);
    void setModulationType(ModulationType modType);
    void setPreDelay(float preDelayMs);
    void setDecayTime(float decaySeconds);
    void setSize(float size);
    void setDiffusion(float diffusion);
    void setDamping(float damping);
    void setWidth(float width);
    void setModulationDepth(float depth);
    void setModulationRate(float rateHz);
    void setEarlyLevel(float level);
    void setLateLevel(float level);
    void setEarlyCrossfeed(float crossfeed);
    void setHighCut(float frequencyHz);
    void setLowCut(float frequencyHz);
    void setHighMultiplier(float multiplier);
    void setLowMultiplier(float multiplier);
    void setShimmerEnabled(bool enabled);
    void setShimmerPitch(float semitones);
    void setShimmerFeedback(float feedback);
    void setShimmerMix(float mix);
    void setFreeze(bool freeze);
    
    // Update early-reflection geometry (called from DistanceProcessor)
    void updateRoomGeometry(float roomWidth, float roomLength, float roomHeight,
                             float srcX, float srcY, float srcZ);
    
private:
    //==============================================================================
    double sampleRate = 44100.0;
    juce::Reverb simpleReverb;
    
    // Advanced reverb components
    std::unique_ptr<FDNReverbTank> fdnTank;
    std::unique_ptr<EarlyReflectionsEngine> earlyReflections;
    std::unique_ptr<ShimmerEffect> shimmer;
    std::unique_ptr<DiffusionSection> inputDiffusion;
    std::unique_ptr<DiffusionSection> outputDiffusion;
    
    // Pre-delay
    juce::dsp::DelayLine<float> preDelayLine;
    
    // Filtering
    std::array<juce::dsp::IIR::Filter<float>, 2> highCutFilters;
    juce::dsp::IIR::Filter<float> lowCutFilter;
    
    // Parameters
    AlgorithmType currentAlgorithm = Hall;
    ModulationType currentModType = Off;
    float preDelayMs = 0.0f;
    float currentPreDelaySamples = 0.0f;
    float decayTime = 2.0f;
    float size = 1.0f;
    float diffusion = 0.5f;
    float damping = 0.5f;
    float width = 1.0f;
    float modDepth = 0.3f;
    float modRate = 0.15f;
    float earlyLevel = 0.3f;
    float lateLevel = 0.7f;
    float earlyCrossfeed = 0.0f;
    float highCutFreq = 8000.0f;
    float lowCutFreq = 80.0f;
    float highMultiplier = 1.0f;
    float lowMultiplier = 1.0f;
    bool shimmerEnabled = false;
    float shimmerPitch = 12.0f;
    float shimmerFeedback = 0.3f;
    float shimmerMix = 0.1f;
    bool freeze = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedReverbEngine)
}; 