#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>

//==============================================================================
/**
    Simplified SOFAR Audio Processor Editor
    Features only essential distance controls for a clean, intuitive interface
*/
class SOFARAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
  SOFARAudioProcessorEditor(SOFARAudioProcessor &);
  ~SOFARAudioProcessorEditor() override;

  //==============================================================================
  void paint(juce::Graphics &) override;
  void resized() override;

private:
  // This reference is provided as a quick way for your editor to
  // access the processor object that created it.
  SOFARAudioProcessor &audioProcessor;

  // Helper method for slider setup
  void setupSlider(juce::Slider &slider, const juce::String &name,
                   const juce::String &suffix, bool isMainControl = false);

  // Presets system
  struct PresetData {
    juce::String name;
    float roomLength;
    float roomWidth;
    float roomHeight;
    float airAbsorption;
    float temperature;
    // Note: Signal control parameters (distance, panning, volumeCompensation)
    // are NOT included
  };

  void setupPresets();
  void loadPreset(int presetIndex);
  void presetsChanged();

  // Preset definitions based on research data
  static const std::vector<PresetData> presetData;

  // UI Components
  juce::ComboBox presetsComboBox;
  juce::TextButton prevPresetButton; // Left arrow for previous preset
  juce::TextButton nextPresetButton; // Right arrow for next preset
  juce::Label titleLabel;
  juce::Label versionLabel;

  // Category labels
  juce::Label signalCategoryLabel;
  juce::Label roomCategoryLabel;

  // Essential Controls
  juce::Slider distanceSlider;
  juce::Slider roomLengthSlider;
  juce::Slider roomWidthSlider;
  juce::Slider roomHeightSlider;
  juce::Slider airAbsorptionSlider;
  juce::Slider volumeCompensationSlider;
  juce::Slider temperatureSlider;
  juce::Slider panningSlider;
  juce::Slider heightSlider;

  // Parameter Attachments
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      distanceAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      roomLengthAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      roomWidthAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      roomHeightAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      airAbsorptionAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      volumeCompensationAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      temperatureAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      panningAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      heightAttachment;

  // Dynamic labels
  std::vector<std::unique_ptr<juce::Label>> labels;

  // Add TooltipWindow for hover descriptions
  juce::TooltipWindow tooltipWindow;

  // Cached background to avoid recreating gradient every paint
  juce::Image backgroundImage;
  void updateBackgroundImage();

  // Navigation helper
  void navigatePreset(int direction);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SOFARAudioProcessorEditor)
};