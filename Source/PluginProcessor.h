#pragma once

#include <JuceHeader.h>
#include "DistanceProcessor.h"

//==============================================================================
/**
    SOFAR - Spatial Distance Effect Plugin Processor
    High-performance, crash-proof, thread-safe implementation
*/
class SOFARAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    SOFARAudioProcessor();
    ~SOFARAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    //==============================================================================
    // Room type management
    void setRoomType(int roomType);
    int getCurrentRoomType() const;
    
    // Plugin state queries
    bool isPluginInitialized() const;

    //==============================================================================
    // Public parameter access for UI
    juce::AudioProcessorValueTreeState parameters;

private:
    // Parameter layout creation
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    //==============================================================================
    // Core audio processing
    DistanceProcessor distanceProcessor;
    
    // Plugin state
    int currentRoomType;
    bool isInitialized;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SOFARAudioProcessor)
};