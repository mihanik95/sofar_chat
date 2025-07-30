#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Updated and expanded environment presets (v0.0043)
// Dramatically different acoustic spaces with clearly distinct characteristics
const std::vector<SOFARAudioProcessorEditor::PresetData> SOFARAudioProcessorEditor::presetData = {
    {"Open Air",         120.0f, 120.0f, 60.0f, 0.20f, 20.0f},  // Vast outdoor space
    {"Large Hall",       45.0f,  28.0f, 10.0f, 0.35f, 18.0f},  // Concert hall acoustics  
    {"Cathedral",        70.0f,  35.0f, 25.0f, 0.30f, 18.0f},  // Huge stone cathedral
    {"Medium Studio",    12.0f,  10.0f, 4.5f, 0.45f, 21.0f},  // Controlled recording studio
    {"Drum Room",         8.0f,   7.0f, 3.0f, 0.55f, 22.0f},  // Tight but lively drum booth
    {"Underground Cave", 60.0f,  30.0f, 20.0f, 0.10f, 15.0f},  // Dark reflective cave
    {"Vehicle Interior",  4.0f,   2.5f, 2.0f, 0.80f, 25.0f},  // Confined vehicle cabin
    {"Closet",            2.5f,   1.8f, 2.2f, 0.90f, 24.0f}   // Very small muffled closet
};

//==============================================================================
SOFARAudioProcessorEditor::SOFARAudioProcessorEditor (SOFARAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), tooltipWindow(this)
{
    // Set larger window size for clean grid layout (4x2 grid for 8 controls + presets)
    setSize (960, 560);
    
    // Set up presets dropdown
    setupPresets();
    
    // Set up main title
    titleLabel.setText("SOFAR - Spatial Distance Effect", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(36.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);
    
    // Version label
    versionLabel.setText("v0.0086", juce::dontSendNotification);
    versionLabel.setFont(juce::Font(18.0f, juce::Font::plain));
    versionLabel.setJustificationType(juce::Justification::centred);
    versionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(versionLabel);
    
    // Set up category labels
    signalCategoryLabel.setText("SIGNAL CONTROL", juce::dontSendNotification);
    signalCategoryLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    signalCategoryLabel.setJustificationType(juce::Justification::centred);
    signalCategoryLabel.setColour(juce::Label::textColourId, juce::Colour(0xff4a90e2));
    addAndMakeVisible(signalCategoryLabel);
    
    roomCategoryLabel.setText("ROOM CONTROL (affected by presets)", juce::dontSendNotification);
    roomCategoryLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    roomCategoryLabel.setJustificationType(juce::Justification::centred);
    roomCategoryLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7b68ee));
    addAndMakeVisible(roomCategoryLabel);
    
    // Distance Control (main control) - larger knob
    setupSlider(distanceSlider, "Distance", "%", true);
    distanceAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "distance", distanceSlider));
    
    // Room Dimensions Controls
    setupSlider(roomLengthSlider, "Room Length", "m");
    roomLengthAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "roomLength", roomLengthSlider));
        
    setupSlider(roomWidthSlider, "Room Width", "m");
    roomWidthAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "roomWidth", roomWidthSlider));
        
    setupSlider(roomHeightSlider, "Room Height", "m");
    roomHeightAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "roomHeight", roomHeightSlider));
    
    // Air Absorption Control
    setupSlider(airAbsorptionSlider, "Air Absorption", "%");
    airAbsorptionAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "airAbsorption", airAbsorptionSlider));
    
    // Volume Compensation Control
    setupSlider(volumeCompensationSlider, "Volume Compensation", "%");
    volumeCompensationAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "volumeCompensation", volumeCompensationSlider));
    
    // Temperature Control
    setupSlider(temperatureSlider, "Temperature", "°C");
    temperatureAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "temperature", temperatureSlider));
    
    // Panning Control
    setupSlider(panningSlider, "Panning", "");
    panningAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
        audioProcessor.parameters, "panning", panningSlider));
    
    // Set consistent, professional colors
    distanceSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff4a90e2));
    distanceSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff4a90e2));
    
    // Room dimensions - different shades of blue/purple
    roomLengthSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff7b68ee));
    roomLengthSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff7b68ee));
    
    roomWidthSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff9370db));
    roomWidthSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff9370db));
    
    roomHeightSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff8a2be2));
    roomHeightSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff8a2be2));
    
    airAbsorptionSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff50c878));
    airAbsorptionSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff50c878));
    
    volumeCompensationSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff32cd32));
    volumeCompensationSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff32cd32));
    
    temperatureSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffff7f50));
    temperatureSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff7f50));
    
    panningSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffffa500));
    panningSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffffa500));

    // Pointer calculation in default LookAndFeel subtracts 90°, so use 0 → 2π to make front (pointer) at 12-o'clock
    panningSlider.setRotaryParameters(0.0f,
                                      juce::MathConstants<float>::twoPi,
                                      false);

    // ────────────────────────────────────────────
    // Preset navigation buttons
    prevPresetButton.setButtonText("<");
    prevPresetButton.onClick = [this] { navigatePreset(-1); };
    prevPresetButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2a));
    prevPresetButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff4a90e2));
    addAndMakeVisible(prevPresetButton);

    nextPresetButton.setButtonText(">");
    nextPresetButton.onClick = [this] { navigatePreset(1); };
    nextPresetButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2a));
    nextPresetButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff4a90e2));
    addAndMakeVisible(nextPresetButton);

    // ────────────────────────────────────────────
    // Add tooltips for user guidance
    distanceSlider.setTooltip("Distance from listener to source (meters).");
    panningSlider.setTooltip("Azimuth around listener (0° front – 360° circle).");
    volumeCompensationSlider.setTooltip("Compensates for perceived loudness loss with distance (%).");
    roomLengthSlider.setTooltip("Length of the simulated room (meters).");
    roomWidthSlider.setTooltip("Width of the simulated room (meters).");
    roomHeightSlider.setTooltip("Height of the simulated room (meters).");
    airAbsorptionSlider.setTooltip("High-frequency attenuation due to air humidity (%).");
    temperatureSlider.setTooltip("Air temperature (°C) – affects speed of sound.");

    // New height slider
    setupSlider(heightSlider, "Height", "%");
    heightAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.parameters, "height", heightSlider);
    heightSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff20b2aa));
    heightSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff20b2aa));
    heightSlider.setTooltip("Vertical position of source (0% floor – 100% ceiling).");
}

SOFARAudioProcessorEditor::~SOFARAudioProcessorEditor()
{
    // Clean up attachments
    distanceAttachment.reset();
    roomLengthAttachment.reset();
    roomWidthAttachment.reset();
    roomHeightAttachment.reset();
    airAbsorptionAttachment.reset();
    volumeCompensationAttachment.reset();
    temperatureAttachment.reset();
    panningAttachment.reset();
    heightAttachment.reset();
}

//==============================================================================
void SOFARAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Clean, modern gradient background
    g.fillAll(juce::Colour(0xff1a1a1a));
    
    juce::ColourGradient gradient(juce::Colour(0xff2a2a2a), 0, 0, 
                                  juce::Colour(0xff1a1a1a), 0, (float)getHeight(), false);
    g.setGradientFill(gradient);
    g.fillRect(0, 0, getWidth(), getHeight());
    
    // Draw subtle border
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRect(getLocalBounds(), 2);
    
    // Draw section dividers for visual organization
    g.setColour(juce::Colour(0xff4a4a4a));
    
    // Horizontal divider between signal and room controls
    int sectionDividerY = 150 + (getHeight() - 180) / 2;
    g.drawLine(40, sectionDividerY, getWidth() - 40, sectionDividerY, 2.0f);
    
    // Draw subtle background boxes for each section
    g.setColour(juce::Colour(0xff1f1f1f));
    
    // Signal control section background
    juce::Rectangle<int> signalBounds(40, 150, getWidth() - 80, (getHeight() - 180) / 2 - 15);
    g.fillRoundedRectangle(signalBounds.toFloat(), 8.0f);
    
    // Room control section background  
    juce::Rectangle<int> roomBounds(40, sectionDividerY + 15, getWidth() - 80, (getHeight() - 180) / 2 - 15);
    g.fillRoundedRectangle(roomBounds.toFloat(), 8.0f);
    
    // Draw subtle outlines for sections
    g.setColour(juce::Colour(0xff4a90e2).withAlpha(0.3f));
    g.drawRoundedRectangle(signalBounds.toFloat(), 8.0f, 1.0f);
    
    g.setColour(juce::Colour(0xff7b68ee).withAlpha(0.3f));
    g.drawRoundedRectangle(roomBounds.toFloat(), 8.0f, 1.0f);
}

void SOFARAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    // Presets area (40px total)
    auto presetsArea = bounds.removeFromTop(40);
    presetsArea.removeFromTop(8);  // Top padding
    presetsArea.removeFromBottom(8); // Bottom padding
    presetsArea.removeFromLeft(50);  // Left padding
    presetsArea.removeFromRight(50); // Right padding

    // Allocate space for navigation buttons (40px each) around the ComboBox
    auto leftButtonArea  = presetsArea.removeFromLeft(40);
    auto rightButtonArea = presetsArea.removeFromRight(40);

    prevPresetButton.setBounds(leftButtonArea);
    nextPresetButton.setBounds(rightButtonArea);

    presetsComboBox.setBounds(presetsArea);
    
    // Title area (80px total)
    auto titleArea = bounds.removeFromTop(80);
    titleLabel.setBounds(titleArea.removeFromTop(40));
    versionLabel.setBounds(titleArea.removeFromTop(25));
    
    // Add padding around the controls  
    bounds.removeFromTop(20);    // Top padding
    bounds.removeFromBottom(20); // Bottom padding
    bounds.removeFromLeft(40);   // Left padding
    bounds.removeFromRight(40);  // Right padding
    
    // Reserve space for category labels (30px each)
    auto signalLabelArea = bounds.removeFromTop(30);
    auto signalControlsArea = bounds.removeFromTop((bounds.getHeight() - 30) / 2);
    auto roomLabelArea = bounds.removeFromTop(30);
    auto roomControlsArea = bounds;
    
    // Position category labels
    signalCategoryLabel.setBounds(signalLabelArea);
    roomCategoryLabel.setBounds(roomLabelArea);
    
    // Signal Controls Section (3 controls: Distance, Panning, Volume Compensation)
    {
        int gridWidth = signalControlsArea.getWidth() / 4;
        int controlSize = std::min(gridWidth - 20, signalControlsArea.getHeight() - 20);
        controlSize = std::min(controlSize, 140);
        
        auto positionSignalControl = [&](juce::Component& control, int col) {
            int x = signalControlsArea.getX() + col * gridWidth + (gridWidth - controlSize) / 2;
            int y = signalControlsArea.getY() + (signalControlsArea.getHeight() - controlSize) / 2;
            control.setBounds(x, y, controlSize, controlSize);
        };
        
        positionSignalControl(distanceSlider, 0);
        positionSignalControl(panningSlider, 1);
        positionSignalControl(heightSlider, 2);
        positionSignalControl(volumeCompensationSlider, 3);
    }
    
    // Room Controls Section (5 controls: Room Length, Width, Height, Air Absorption, Temperature)
    {
        int gridWidth = roomControlsArea.getWidth() / 5;
        int controlSize = std::min(gridWidth - 15, roomControlsArea.getHeight() - 20);
        controlSize = std::min(controlSize, 120); // Slightly smaller for 5 controls
        
        auto positionRoomControl = [&](juce::Component& control, int col) {
            int x = roomControlsArea.getX() + col * gridWidth + (gridWidth - controlSize) / 2;
            int y = roomControlsArea.getY() + (roomControlsArea.getHeight() - controlSize) / 2;
            control.setBounds(x, y, controlSize, controlSize);
        };
        
        positionRoomControl(roomLengthSlider, 0);
        positionRoomControl(roomWidthSlider, 1);
        positionRoomControl(roomHeightSlider, 2);
        positionRoomControl(airAbsorptionSlider, 3);
        positionRoomControl(temperatureSlider, 4);
    }
}

void SOFARAudioProcessorEditor::setupSlider(juce::Slider& slider, const juce::String& name, const juce::String& suffix, bool isMainControl)
{
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 100, 25);
    slider.setName(name);
    
    // Fix parameter formatting - don't add suffix here, it's handled in parameter creation
    // slider.setTextValueSuffix(suffix); // Remove this line
    
    // Make the main control (Distance) larger and more prominent
    if (isMainControl)
    {
        slider.setSize(160, 160);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 120, 30);
    }
    
    // Improved rotary slider appearance
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff404040));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff2a2a2a));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff404040));
    
    // Create and add label with improved styling
    auto label = std::make_unique<juce::Label>();
    label->setText(name, juce::dontSendNotification);
    label->setFont(juce::Font(isMainControl ? 16.0f : 14.0f, juce::Font::bold));
    label->setJustificationType(juce::Justification::centred);
    label->setColour(juce::Label::textColourId, juce::Colours::white);
    label->attachToComponent(&slider, false);
    
    addAndMakeVisible(slider);
    labels.push_back(std::move(label));
}

void SOFARAudioProcessorEditor::setupPresets()
{
    // Set up presets dropdown
    presetsComboBox.addItem("Select Preset...", 1);
    for (int i = 0; i < presetData.size(); ++i) {
        presetsComboBox.addItem(presetData[i].name, i + 2);
    }
    
    presetsComboBox.setSelectedItemIndex(0, juce::dontSendNotification);
    presetsComboBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff2a2a2a));
    presetsComboBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    presetsComboBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff4a90e2));
    presetsComboBox.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff4a90e2));
    
    presetsComboBox.onChange = [this] { presetsChanged(); };
    
    addAndMakeVisible(presetsComboBox);
}

void SOFARAudioProcessorEditor::loadPreset(int presetIndex)
{
    if (presetIndex >= 0 && presetIndex < presetData.size()) {
        const auto& preset = presetData[presetIndex];
        
        // Load only ROOM CONTROL parameters - these define the acoustic environment
        audioProcessor.parameters.getParameterAsValue("roomLength").setValue(preset.roomLength);
        audioProcessor.parameters.getParameterAsValue("roomWidth").setValue(preset.roomWidth);
        audioProcessor.parameters.getParameterAsValue("roomHeight").setValue(preset.roomHeight);
        audioProcessor.parameters.getParameterAsValue("airAbsorption").setValue(preset.airAbsorption);
        audioProcessor.parameters.getParameterAsValue("temperature").setValue(preset.temperature);
        
        // SIGNAL CONTROL parameters are deliberately NOT changed to preserve user's signal positioning:
        // - distance (user's instrument position)
        // - panning (user's stereo positioning) 
        // - volumeCompensation (user's gain preference)
    }
}

void SOFARAudioProcessorEditor::presetsChanged()
{
    int selectedIndex = presetsComboBox.getSelectedItemIndex();
    if (selectedIndex > 0) { // Skip "Select Preset..." item
        loadPreset(selectedIndex - 1); // Adjust for "Select Preset..." item
    }
}

//─────────────────────────────────────────────────────────────────
// Preset navigation helper
void SOFARAudioProcessorEditor::navigatePreset(int direction)
{
    int currentIndex = presetsComboBox.getSelectedItemIndex(); // 0 = "Select Preset..."

    // If no preset chosen yet, start from first (index 1)
    if (currentIndex <= 0)
        currentIndex = 1;

    int totalPresets = static_cast<int>(presetData.size());

    int newIndex = currentIndex + direction;

    if (newIndex < 1)
        newIndex = totalPresets;
    else if (newIndex > totalPresets)
        newIndex = 1;

    // Update ComboBox without triggering its onChange (we'll load manually)
    presetsComboBox.setSelectedItemIndex(newIndex, juce::dontSendNotification);
    loadPreset(newIndex - 1); // Adjust for "Select Preset..."
}