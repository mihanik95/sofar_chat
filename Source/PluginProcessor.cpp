#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SOFARAudioProcessor::SOFARAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
      parameters(*this, nullptr),
      currentRoomType(0),
      isInitialized(false)
#else
    : parameters(*this, nullptr, juce::Identifier("SOFAR"), createParameterLayout()),
      currentRoomType(0),
      isInitialized(false)
#endif
{
    // Optimized constructor - minimal work, thread-safe initialization
    juce::Logger::writeToLog("SOFAR plugin constructor - optimized version");
}

SOFARAudioProcessor::~SOFARAudioProcessor()
{
    // Optimized destructor with proper cleanup order
    isInitialized = false;
    distanceProcessor.reset();
}

//==============================================================================
const juce::String SOFARAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SOFARAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SOFARAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SOFARAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SOFARAudioProcessor::getTailLengthSeconds() const
{
    return 2.0; // Optimized: Reduced tail time for better performance
}

int SOFARAudioProcessor::getNumPrograms()
{
    return 1;
}

int SOFARAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SOFARAudioProcessor::setCurrentProgram (int index)
{
    // Optimized: No-op for single program
}

const juce::String SOFARAudioProcessor::getProgramName (int index)
{
    return {};
}

void SOFARAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    // Optimized: No-op for single program
}

//==============================================================================
void SOFARAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::Logger::writeToLog("SOFAR prepareToPlay - optimized version");
    
    // Enhanced parameter validation with early exit
    if (sampleRate <= 0.0 || samplesPerBlock <= 0) {
        juce::Logger::writeToLog("Invalid prepareToPlay parameters - aborting");
        return;
    }
    
    // Reasonable bounds checking
    if (sampleRate < 8000.0 || sampleRate > 192000.0) {
        juce::Logger::writeToLog("Sample rate out of bounds: " + juce::String(sampleRate));
        return;
    }
    
    if (samplesPerBlock > 8192) {
        juce::Logger::writeToLog("Block size too large: " + juce::String(samplesPerBlock));
        return;
    }
    
    try {
        // Thread-safe preparation with new interface
        distanceProcessor.prepare(sampleRate, samplesPerBlock);
        
        // Mark as successfully initialized
        isInitialized = true;
        
        juce::Logger::writeToLog("SOFAR prepareToPlay completed successfully - optimized");
        
    } catch (const std::exception& e) {
        juce::Logger::writeToLog("Exception in prepareToPlay: " + juce::String(e.what()));
        isInitialized = false;
    } catch (...) {
        juce::Logger::writeToLog("Unknown exception in prepareToPlay");
        isInitialized = false;
    }
}

void SOFARAudioProcessor::releaseResources()
{
    // Optimized cleanup sequence
    isInitialized = false;
    distanceProcessor.reset();
    juce::Logger::writeToLog("SOFAR resources released");
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SOFARAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // Optimized: Support only mono and stereo for best performance
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    const auto& mainInput = layouts.getMainInputChannelSet();
    
    if (mainOutput != juce::AudioChannelSet::mono() && 
        mainOutput != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (mainOutput != mainInput)
        return false;
   #endif

    return true;
  #endif
}
#endif

void SOFARAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    try {
        juce::ScopedNoDenormals noDenormals;
        
        auto totalNumInputChannels  = getTotalNumInputChannels();
        auto totalNumOutputChannels = getTotalNumOutputChannels();
        
        // Clear any output channels that don't contain input data
        for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
            buffer.clear (i, 0, buffer.getNumSamples());
        
        // Get parameter values including new room dimensions
        float distance = *parameters.getRawParameterValue("distance");
        float panning = *parameters.getRawParameterValue("panning");
        float heightPct = *parameters.getRawParameterValue("height");
        float roomLength = *parameters.getRawParameterValue("roomLength");
        float roomWidth = *parameters.getRawParameterValue("roomWidth");
        float roomHeight = *parameters.getRawParameterValue("roomHeight");
        float airAbsorption = *parameters.getRawParameterValue("airAbsorption");
        float volumeCompensation = *parameters.getRawParameterValue("volumeCompensation");
        float temperature = *parameters.getRawParameterValue("temperature");
        
        // Convert azimuth to lateral factor (|sin| gives 0–1 based on side offset)
        const float panAmount = std::abs(std::sin(panning * juce::MathConstants<float>::pi / 180.0f));
        
        // Calculate effective max distance based on panning and room dimensions
        // When centered (pan=0): use room length
        // When panned left/right: interpolate between length and width
        float effectiveMaxDistance = roomLength + panAmount * (roomWidth - roomLength);
        effectiveMaxDistance = juce::jmax(effectiveMaxDistance, 2.0f); // Minimum 2m
        
        // Convert distance from percentage to actual meters based on current room size
        float actualDistance = distance * effectiveMaxDistance;
        
        // Update distance processor with room-based parameters
        distanceProcessor.setDistance(actualDistance);
        distanceProcessor.setMaxDistance(effectiveMaxDistance);
        distanceProcessor.setRoomWidth(roomWidth);
        distanceProcessor.setRoomHeight(roomHeight);
        distanceProcessor.setRoomLength(roomLength);
        distanceProcessor.setAirAbsorption(airAbsorption);
        distanceProcessor.setVolumeCompensation(volumeCompensation);
        distanceProcessor.setTemperature(temperature);
        distanceProcessor.setSourceHeight(heightPct);
        
        // Process with actual distance in meters (not percentage)
        auto env = static_cast<DistanceProcessor::Environment> (currentRoomType);
        distanceProcessor.processBlock(buffer, actualDistance, panning, env);
        
    } catch (const std::exception& e) {
        juce::Logger::writeToLog("Exception in processBlock: " + juce::String(e.what()));
        // Continue with unprocessed audio rather than crash
    } catch (...) {
        juce::Logger::writeToLog("Unknown exception in processBlock");
    }
}

//==============================================================================
bool SOFARAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SOFARAudioProcessor::createEditor()
{
    try {
        return new SOFARAudioProcessorEditor(*this);
    } catch (...) {
        juce::Logger::writeToLog("Failed to create editor");
        return nullptr;
    }
}

//==============================================================================
void SOFARAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    try {
        // Optimized state saving with validation
        auto state = parameters.copyState();
        
        // Add room type to state
        state.setProperty("roomType", currentRoomType, nullptr);
        
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        if (xml != nullptr)
            copyXmlToBinary (*xml, destData);
            
    } catch (const std::exception& e) {
        juce::Logger::writeToLog("Exception saving state: " + juce::String(e.what()));
    } catch (...) {
        juce::Logger::writeToLog("Unknown exception saving state");
    }
}

void SOFARAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    try {
        // Optimized state loading with validation
        if (data == nullptr || sizeInBytes <= 0)
            return;
            
        std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
        
        if (xmlState.get() != nullptr && xmlState->hasTagName (parameters.state.getType()))
        {
            auto newState = juce::ValueTree::fromXml (*xmlState);
            parameters.replaceState (newState);
            
            // Restore room type safely
            if (newState.hasProperty("roomType"))
            {
                int roomType = newState.getProperty("roomType", 0);
                setRoomType(juce::jlimit(0, DistanceProcessor::numEnvironments - 1, roomType));
            }
        }
        
    } catch (const std::exception& e) {
        juce::Logger::writeToLog("Exception loading state: " + juce::String(e.what()));
    } catch (...) {
        juce::Logger::writeToLog("Unknown exception loading state");
    }
}

//==============================================================================
// Optimized room type management
void SOFARAudioProcessor::setRoomType(int roomType)
{
    currentRoomType = juce::jlimit(0, DistanceProcessor::numEnvironments - 1, roomType);
    distanceProcessor.setEnvironmentType (static_cast<DistanceProcessor::Environment>(currentRoomType));
}

int SOFARAudioProcessor::getCurrentRoomType() const
{
    return currentRoomType;
}

bool SOFARAudioProcessor::isPluginInitialized() const
{
    return isInitialized;
}

//==============================================================================
// Parameter layout creation
juce::AudioProcessorValueTreeState::ParameterLayout SOFARAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    // Distance Control (main control) - percentage of room size, converted to meters in processing
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "distance", "Distance", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.1f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { 
            // Show percentage - actual meters will be calculated based on current room size
            return juce::String(value * 100.0f, 1) + "%"; 
        }));
    
    // Room Dimensions Controls
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "roomLength", "Room Length", 
        juce::NormalisableRange<float>(2.0f, 100.0f, 0.5f), 8.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + "m"; }));
        
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "roomWidth", "Room Width", 
        juce::NormalisableRange<float>(2.0f, 100.0f, 0.5f), 6.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + "m"; }));
        
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "roomHeight", "Room Height", 
        juce::NormalisableRange<float>(2.0f, 20.0f, 0.1f), 3.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + "m"; }));
    
    // Air Absorption Control
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "airAbsorption", "Air Absorption", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value * 100.0f, 1) + "%"; }));
    
    // Volume Compensation Control
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "volumeCompensation", "Volume Compensation", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value * 100.0f, 1) + "%"; }));
    
    // Temperature Control
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "temperature", "Temperature", 
        juce::NormalisableRange<float>(-100.0f, 200.0f, 1.0f), 20.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + "°C"; }));
    
    // Now a 360° azimuth control (0° = front, 90° = right, 180° = back, 270° = left)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "panning", "Panning",
        juce::NormalisableRange<float>(0.0f, 360.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            int deg = int(std::round(value));
            switch (deg) {
                case 0:  case 360: return juce::String("Front");
                case 90:           return juce::String("Right");
                case 180:          return juce::String("Back");
                case 270:          return juce::String("Left");
                default:           return juce::String(deg) + juce::String("°");
            }
        }));
    
    // Height Control (vertical position as percentage of room height)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "height", "Height",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value * 100.0f, 1) + "%"; }));
    
    return layout;
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    try {
        return new SOFARAudioProcessor();
    } catch (const std::exception& e) {
        juce::Logger::writeToLog("Exception creating plugin: " + juce::String(e.what()));
        return nullptr;
    } catch (...) {
        juce::Logger::writeToLog("Unknown exception creating plugin");
        return nullptr;
    }
}

