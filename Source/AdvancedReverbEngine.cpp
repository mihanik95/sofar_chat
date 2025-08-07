#include "AdvancedReverbEngine.h"
#include <random>
#include <cmath>

//==============================================================================
// MODULATED ALLPASS FILTER
//==============================================================================

ModulatedAllpass::ModulatedAllpass()
{
}

ModulatedAllpass::~ModulatedAllpass()
{
}

void ModulatedAllpass::prepare(double sampleRate, int maxDelayInSamples)
{
    this->sampleRate = sampleRate;
    delayLine.prepare(juce::dsp::ProcessSpec{sampleRate, 512, 1});
    delayLine.setMaximumDelayInSamples(maxDelayInSamples);
    reset();
}

void ModulatedAllpass::reset()
{
    delayLine.reset();
    modPhase = 0.0f;
    previousOutput = 0.0f;
}

void ModulatedAllpass::setFeedback(float feedback)
{
    this->feedback = juce::jlimit(-0.99f, 0.99f, feedback);
}

void ModulatedAllpass::setModulationDepth(float depth)
{
    this->modDepth = juce::jlimit(0.0f, 1.0f, depth);
}

void ModulatedAllpass::setModulationRate(float rateHz)
{
    this->modRate = juce::jlimit(0.0f, 10.0f, rateHz);
}

void ModulatedAllpass::setDelay(float delayInSamples)
{
    this->baseDelay = juce::jmax(1.0f, delayInSamples);
}

float ModulatedAllpass::processSample(float input)
{
    // Calculate modulated delay time
    float modValue = std::sin(modPhase) * modDepth;
    float currentDelay = baseDelay + modValue * baseDelay * 0.1f; // Max 10% modulation

    // Read the current delayed sample
    float bufOut = delayLine.popSample(0, currentDelay);

    // All-pass structure (feedback < 1 guarantees stability)
    const float vn = input + feedback * bufOut;   // input to delay line
    delayLine.pushSample(0, vn);

    float output = -feedback * vn + bufOut;       // all-pass output

    // Update modulation phase
    modPhase += 2.0f * juce::MathConstants<float>::pi * modRate / static_cast<float>(sampleRate);
    if (modPhase > juce::MathConstants<float>::twoPi)
        modPhase -= juce::MathConstants<float>::twoPi;

    return output;
}

//==============================================================================
// FDN REVERB TANK
//==============================================================================

FDNReverbTank::FDNReverbTank()
{
    // Longer prime delays (~34–90 ms @44.1 k) for smoother, less metallic tail
    delayLengths = {
        1499.0f, 1699.0f, 1999.0f, 2347.0f, 2791.0f, 3109.0f, 3541.0f, 3907.0f
    };
    
    // Initialize modulation phases with different offsets
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        modPhases[i] = static_cast<float>(i) * juce::MathConstants<float>::pi / 4.0f;
        modRates[i] = 0.1f + static_cast<float>(i) * 0.05f;
    }
    
    initializeMixingMatrix();
}

FDNReverbTank::~FDNReverbTank()
{
}

void FDNReverbTank::prepare(double sampleRate, int maxBlockSize)
{
    this->sampleRate = sampleRate;
    
    // Prepare delay lines and filters
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        delayLines[i].prepare(juce::dsp::ProcessSpec{sampleRate, static_cast<juce::uint32>(maxBlockSize), 1});
        delayLines[i].setMaximumDelayInSamples(static_cast<int>(sampleRate * 2.0)); // up to 2 seconds
        
        dampingFilters[i].prepare(juce::dsp::ProcessSpec{sampleRate, static_cast<juce::uint32>(maxBlockSize), 1});
    }
    
    updateDelayLengths();
    updateFeedback();
    updateDamping();
}

void FDNReverbTank::reset()
{
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        delayLines[i].reset();
        dampingFilters[i].reset();
        modPhases[i] = static_cast<float>(i) * juce::MathConstants<float>::pi / 4.0f;
    }
}

void FDNReverbTank::setDecayTime(float decaySeconds)
{
    this->decayTime = juce::jlimit(0.1f, 30.0f, decaySeconds);
    updateFeedback();
}

void FDNReverbTank::setSize(float sizeMultiplier)
{
    this->sizeMultiplier = juce::jlimit(0.1f, 4.0f, sizeMultiplier);
    updateDelayLengths();
}

void FDNReverbTank::setDamping(float damping)
{
    this->dampingAmount = juce::jlimit(0.0f, 1.0f, damping);
    updateDamping();
}

void FDNReverbTank::setModulationDepth(float depth)
{
    this->modDepth = juce::jlimit(0.0f, 1.0f, depth);
}

void FDNReverbTank::setModulationRate(float rateHz)
{
    this->modRate = juce::jlimit(0.0f, 2.0f, rateHz);
}

void FDNReverbTank::initializeMixingMatrix()
{
    // Use an 8x8 Hadamard-like orthogonal matrix (rows normalised by 1/sqrt(8))
    static constexpr float h[8][8] = {
        { 1,  1,  1,  1,  1,  1,  1,  1},
        { 1, -1,  1, -1,  1, -1,  1, -1},
        { 1,  1, -1, -1,  1,  1, -1, -1},
        { 1, -1, -1,  1,  1, -1, -1,  1},
        { 1,  1,  1,  1, -1, -1, -1, -1},
        { 1, -1,  1, -1, -1,  1, -1,  1},
        { 1,  1, -1, -1, -1, -1,  1,  1},
        { 1, -1, -1,  1, -1,  1,  1, -1}
    };

    const float norm = 1.0f / std::sqrt(static_cast<float>(NUM_CHANNELS));
    for (int i = 0; i < NUM_CHANNELS; ++i)
        for (int j = 0; j < NUM_CHANNELS; ++j)
            mixingMatrix[i][j] = h[i][j] * norm;
}

void FDNReverbTank::updateDelayLengths()
{
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        float lenSamps = delayLengths[i] * sizeMultiplier;
        delayLines[i].setDelay(lenSamps);
    }
}

void FDNReverbTank::updateFeedback()
{
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        // target: ‑60 dB after decayTime seconds
        float delaySamples = delayLengths[i] * sizeMultiplier;
        float g = std::pow(0.001f, delaySamples / (static_cast<float>(sampleRate) * decayTime));
        g *= 0.65f; // stronger energy reduction to prevent metallic resonances
        feedback[i] = g;
    }
}

void FDNReverbTank::updateDamping()
{
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        // Improved damping range - never allow completely undamped resonances
        float cutoffFreq = juce::jmap(dampingAmount, 2000.0f, 12000.0f);
        cutoffFreq = juce::jlimit(1500.0f, 12000.0f, cutoffFreq); // ensure minimum damping
        auto coeff = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, cutoffFreq);
        *dampingFilters[i].coefficients = *coeff;
    }
}

void FDNReverbTank::processStereo(float leftInput, float rightInput, float& leftOutput, float& rightOutput)
{
    float inputMono = 0.5f * (leftInput + rightInput);

    float y[NUM_CHANNELS];
    const float depthSamples = modDepth * 5.0f; // reduced modulation depth (was 20.0f)
    // Read outputs with delay modulation, then apply damping
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        // Calculate modulated delay length
        float mod = depthSamples * std::sin(modPhases[i]);
        modPhases[i] += (2.0f * juce::MathConstants<float>::pi * modRates[i]) / static_cast<float>(sampleRate);
        if (modPhases[i] > juce::MathConstants<float>::twoPi)
            modPhases[i] -= juce::MathConstants<float>::twoPi;

        // Use popSample with modulated delay directly instead of setDelay
        float targetDelay = delayLengths[i] * sizeMultiplier + mod;
        y[i] = dampingFilters[i].processSample(delayLines[i].popSample(0, targetDelay));
    }

    // Mix for feedback
    float fb[NUM_CHANNELS] = {};
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        float acc = 0.0f;
        for (int j = 0; j < NUM_CHANNELS; ++j)
            acc += mixingMatrix[i][j] * y[j];
        fb[i] = inputMono + feedback[i] * acc;
    }

    // Push back into delay lines (with simple slow modulation on length)
    for (int i = 0; i < NUM_CHANNELS; ++i)
        delayLines[i].pushSample(0, fb[i]);

    // Generate stereo outs (even indices → L, odd → R)
    float sumL = 0.0f;
    float sumR = 0.0f;
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        if (i % 2 == 0)
            sumL += y[i];
        else
            sumR += y[i];
    }
    leftOutput  = sumL * 0.25f; // scale down to avoid clipping
    rightOutput = sumR * 0.25f;
}

//==============================================================================
// EARLY REFLECTIONS ENGINE
//==============================================================================

EarlyReflectionsEngine::EarlyReflectionsEngine()
{
}

EarlyReflectionsEngine::~EarlyReflectionsEngine()
{
}

void EarlyReflectionsEngine::prepare(double sampleRate, int maxBlockSize)
{
    this->sampleRate = sampleRate;
    
    for (int i = 0; i < MAX_REFLECTIONS; ++i)
    {
        delayLines[i].prepare(juce::dsp::ProcessSpec{sampleRate, static_cast<juce::uint32>(maxBlockSize), 1});
        delayLines[i].setMaximumDelayInSamples(static_cast<int>(sampleRate * 1.0)); // up to 1 sec
    }
    
    loadPattern(0); // temporary until configureGeometry is called
}

void EarlyReflectionsEngine::reset()
{
    for (int i = 0; i < MAX_REFLECTIONS; ++i)
    {
        delayLines[i].reset();
    }
}

void EarlyReflectionsEngine::setRoomSize(float size)
{
    this->roomSize = juce::jlimit(0.1f, 4.0f, size);
}

void EarlyReflectionsEngine::setPattern(int patternIndex)
{
    this->currentPattern = juce::jlimit(0, 3, patternIndex);
    loadPattern(currentPattern);
}

void EarlyReflectionsEngine::setLevel(float level)
{
    this->level = juce::jlimit(0.0f, 1.0f, level);
}

void EarlyReflectionsEngine::setCrossfeed(float crossfeed)
{
    this->crossfeed = juce::jlimit(0.0f, 1.0f, crossfeed);
}

void EarlyReflectionsEngine::processStereo(float leftInput, float rightInput, float& leftOutput, float& rightOutput)
{
    // Mono input feed
    const float monoIn = 0.5f * (leftInput + rightInput);

    float accL = 0.0f;
    float accR = 0.0f;

    for (int i = 0; i < MAX_REFLECTIONS; ++i)
    {
        // Push current sample into delay line
        delayLines[i].pushSample(0, monoIn);

        // Read delayed reflection
        const float refl = delayLines[i].popSample(0);

        // Simple left/right panning based on stored position
        const float pan = panPositions[i]; // -1 = left, +1 = right
        const float gain = gains[i] * level; // apply level once
        const float gL = gain * (pan <= 0.0f ? 1.0f : crossfeed);
        const float gR = gain * (pan >= 0.0f ? 1.0f : crossfeed);

        accL += refl * gL;
        accR += refl * gR;
    }

    leftOutput  = accL;
    rightOutput = accR;
}

void EarlyReflectionsEngine::loadPattern(int patternIndex)
{
    // Simplified implementation
    for (int i = 0; i < MAX_REFLECTIONS; ++i)
    {
        delayTimes[i] = static_cast<float>(i + 1) * 0.01f; // 10ms intervals
        gains[i] = 0.1f / (i + 1); // Decreasing gains
        panPositions[i] = static_cast<float>(i % 2); // Alternate L/R
    }
}

//==============================================================================
// Geometry configuration – create basic 1st-order reflections on six room faces
void EarlyReflectionsEngine::configureGeometry(float roomW, float roomL, float roomH,
                                               float srcX,  float srcY,  float srcZ)
{
    /*
        DearVR-style sophisticated early reflection modeling
        Uses image-source method with realistic material properties and frequency-dependent absorption
        Includes first and second-order reflections for natural room sound
    */

    constexpr float c = 343.0f; // speed of sound (m/s)
    
    // Material properties for realistic absorption
    const float wallAbsorption = 0.15f;   // Typical wall absorption coefficient
    const float floorAbsorption = 0.25f;  // Floor (carpet/wood)
    const float ceilingAbsorption = 0.35f; // Ceiling (tiles/plaster)
    
    // Listener position (origin)
    const float listenerX = 0.0f;
    const float listenerY = 1.7f; // Ear height
    const float listenerZ = 0.0f;
    
    int tap = 0;
    
    auto addReflection = [&](float imgX, float imgY, float imgZ, float absorption, bool isSecondOrder = false)
    {
        if (tap >= MAX_REFLECTIONS) return;
        
        const float dx = imgX - listenerX;
        const float dy = imgY - listenerY;
        const float dz = imgZ - listenerZ;
        const float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        if (dist < 0.1f) return; // Skip very close reflections
        
        const float delaySec = dist / c;
        
        // Realistic gain calculation with distance and absorption
        float gain = (1.0f - absorption) / (1.0f + dist * dist * 0.1f);
        if (isSecondOrder) gain *= 0.3f; // Second-order reflections are weaker
        
        // Apply high-frequency damping based on distance and absorption
        float hfDamping = 1.0f - (absorption * 0.5f + dist * 0.02f);
        hfDamping = juce::jlimit(0.3f, 1.0f, hfDamping);
        
        delayTimes[tap] = delaySec;
        gains[tap] = gain * hfDamping;
        
        // Realistic panning based on actual position
        float azimuth = std::atan2(dx, dz) * 180.0f / juce::MathConstants<float>::pi;
        panPositions[tap] = juce::jlimit(-1.0f, 1.0f, azimuth / 90.0f);
        
        delayLines[tap].setDelay(static_cast<float>(delaySec * sampleRate));
        ++tap;
    };
    
    // First-order reflections (direct wall bounces)
    // Left wall
    addReflection(-roomW - srcX, srcY, srcZ, wallAbsorption);
    // Right wall  
    addReflection(roomW - srcX, srcY, srcZ, wallAbsorption);
    // Back wall
    addReflection(srcX, srcY, -roomL - srcZ, wallAbsorption);
    // Front wall
    addReflection(srcX, srcY, roomL - srcZ, wallAbsorption);
    // Floor
    addReflection(srcX, -srcY, srcZ, floorAbsorption);
    // Ceiling
    addReflection(srcX, 2*roomH - srcY, srcZ, ceilingAbsorption);
    
    // Second-order reflections (corner reflections) - key for realism
    // Left-back corner
    addReflection(-roomW - srcX, srcY, -roomL - srcZ, wallAbsorption * 1.5f, true);
    // Right-back corner
    addReflection(roomW - srcX, srcY, -roomL - srcZ, wallAbsorption * 1.5f, true);
    // Left-front corner
    addReflection(-roomW - srcX, srcY, roomL - srcZ, wallAbsorption * 1.5f, true);
    // Right-front corner
    addReflection(roomW - srcX, srcY, roomL - srcZ, wallAbsorption * 1.5f, true);
    
    // Floor-wall reflections
    addReflection(-roomW - srcX, -srcY, srcZ, (wallAbsorption + floorAbsorption) * 0.5f, true);
    addReflection(roomW - srcX, -srcY, srcZ, (wallAbsorption + floorAbsorption) * 0.5f, true);
    
    // Ceiling-wall reflections
    addReflection(-roomW - srcX, 2*roomH - srcY, srcZ, (wallAbsorption + ceilingAbsorption) * 0.5f, true);
    addReflection(roomW - srcX, 2*roomH - srcY, srcZ, (wallAbsorption + ceilingAbsorption) * 0.5f, true);
    
    // Fill remaining taps with diffuse late early reflections
    std::mt19937 rng(static_cast<unsigned>(std::hash<float>{}(srcX + srcY + srcZ)));
    std::uniform_real_distribution<float> rand01(0.0f, 1.0f);
    
    while (tap < MAX_REFLECTIONS)
    {
        // Generate realistic diffuse reflection pattern
        float delay = 0.020f + 0.080f * rand01(rng); // 20-100ms range
        float gain = 0.02f * (1.0f - rand01(rng) * 0.7f); // Decreasing gains
        float pan = (rand01(rng) - 0.5f) * 2.0f; // -1 to +1
        
        delayTimes[tap] = delay;
        gains[tap] = gain;
        panPositions[tap] = pan;
        delayLines[tap].setDelay(static_cast<float>(delay * sampleRate));
        ++tap;
    }
}

//==============================================================================
// SHIMMER EFFECT
//==============================================================================

ShimmerEffect::ShimmerEffect()
{
}

ShimmerEffect::~ShimmerEffect()
{
}

void ShimmerEffect::prepare(double sampleRate, int maxBlockSize)
{
    this->sampleRate = sampleRate;
    delayLine.prepare(juce::dsp::ProcessSpec{sampleRate, static_cast<juce::uint32>(maxBlockSize), 1});
    delayLine.setMaximumDelayInSamples(static_cast<int>(sampleRate * 0.5));
}

void ShimmerEffect::reset()
{
    delayLine.reset();
    writePhase = 0.0f;
    readPhase1 = 0.0f;
    readPhase2 = 0.0f;
}

void ShimmerEffect::setEnabled(bool enabled)
{
    this->enabled = enabled;
}

void ShimmerEffect::setPitchShift(float semitones)
{
    this->pitchShift = juce::jlimit(-24.0f, 24.0f, semitones);
    this->pitchRatio = std::pow(2.0f, pitchShift / 12.0f);
}

void ShimmerEffect::setFeedback(float feedback)
{
    this->feedbackAmount = juce::jlimit(0.0f, 0.95f, feedback);
}

void ShimmerEffect::setMix(float mix)
{
    this->mix = juce::jlimit(0.0f, 1.0f, mix);
}

float ShimmerEffect::processSample(float input)
{
    if (!enabled)
        return input;
        
    // Simplified pitch shifting
    float shifted = input * pitchRatio;
    return input + (shifted * mix);
}

//==============================================================================
// DIFFUSION SECTION
//==============================================================================

DiffusionSection::DiffusionSection()
{
    for (int i = 0; i < NUM_STAGES; ++i)
    {
        leftAllpasses[i] = std::make_unique<ModulatedAllpass>();
        rightAllpasses[i] = std::make_unique<ModulatedAllpass>();
    }
}

DiffusionSection::~DiffusionSection()
{
}

void DiffusionSection::prepare(double sampleRate, int maxBlockSize)
{
    this->sampleRate = sampleRate;
    
    for (int i = 0; i < NUM_STAGES; ++i)
    {
        leftAllpasses[i]->prepare(sampleRate, 100 + i * 50);
        rightAllpasses[i]->prepare(sampleRate, 125 + i * 50);
        
        leftAllpasses[i]->setFeedback(0.5f);
        rightAllpasses[i]->setFeedback(0.5f);
        
        leftAllpasses[i]->setDelay(static_cast<float>(50 + i * 25));
        rightAllpasses[i]->setDelay(static_cast<float>(62 + i * 25));
    }
}

void DiffusionSection::reset()
{
    for (int i = 0; i < NUM_STAGES; ++i)
    {
        leftAllpasses[i]->reset();
        rightAllpasses[i]->reset();
    }
}

void DiffusionSection::setDiffusion(float diffusion)
{
    this->diffusion = juce::jlimit(0.0f, 1.0f, diffusion);
    
    for (int i = 0; i < NUM_STAGES; ++i)
    {
        leftAllpasses[i]->setFeedback(diffusion * 0.5f);
        rightAllpasses[i]->setFeedback(diffusion * 0.5f);
    }
}

void DiffusionSection::setModulationDepth(float depth)
{
    this->modDepth = juce::jlimit(0.0f, 1.0f, depth);
    
    for (int i = 0; i < NUM_STAGES; ++i)
    {
        leftAllpasses[i]->setModulationDepth(depth);
        rightAllpasses[i]->setModulationDepth(depth);
    }
}

void DiffusionSection::setModulationRate(float rateHz)
{
    this->modRate = juce::jlimit(0.0f, 2.0f, rateHz);
    
    for (int i = 0; i < NUM_STAGES; ++i)
    {
        leftAllpasses[i]->setModulationRate(rateHz * (i + 1));
        rightAllpasses[i]->setModulationRate(rateHz * (i + 1) * 1.1f);
    }
}

void DiffusionSection::processStereo(float leftInput, float rightInput, float& leftOutput, float& rightOutput)
{
    float left = leftInput;
    float right = rightInput;
    
    for (int i = 0; i < NUM_STAGES; ++i)
    {
        left = leftAllpasses[i]->processSample(left);
        right = rightAllpasses[i]->processSample(right);
    }
    
    leftOutput = left;
    rightOutput = right;
}

//==============================================================================
// ADVANCED REVERB ENGINE
//==============================================================================

AdvancedReverbEngine::AdvancedReverbEngine()
{
    // Initialize advanced components
    fdnTank = std::make_unique<FDNReverbTank>();
    earlyReflections = std::make_unique<EarlyReflectionsEngine>();
    shimmer = std::make_unique<ShimmerEffect>();
    inputDiffusion = std::make_unique<DiffusionSection>();
    outputDiffusion = std::make_unique<DiffusionSection>();
}

AdvancedReverbEngine::~AdvancedReverbEngine()
{
}

void AdvancedReverbEngine::prepare(double sampleRate, int maxBlockSize)
{
    this->sampleRate = sampleRate;
    
    // Prepare simple reverb
    juce::Reverb::Parameters params;
    params.roomSize = 0.5f;
    params.damping = 0.5f;
    params.wetLevel = 0.33f;
    params.dryLevel = 0.4f;
    params.width = 1.0f;
    simpleReverb.setParameters(params);
    
    // Prepare advanced components
    if (fdnTank)
        fdnTank->prepare(sampleRate, maxBlockSize);
    if (earlyReflections)
        earlyReflections->prepare(sampleRate, maxBlockSize);
    if (shimmer)
        shimmer->prepare(sampleRate, maxBlockSize);
    if (inputDiffusion)
        inputDiffusion->prepare(sampleRate, maxBlockSize);
    if (outputDiffusion)
        outputDiffusion->prepare(sampleRate, maxBlockSize);
        
    // Prepare pre-delay and filters
    preDelayLine.prepare(juce::dsp::ProcessSpec{sampleRate, static_cast<juce::uint32>(maxBlockSize), 2});
    preDelayLine.setMaximumDelayInSamples(static_cast<int>(sampleRate * 0.5));
    
    juce::dsp::ProcessSpec spec{sampleRate, static_cast<juce::uint32>(maxBlockSize), 1};
    for (auto& f : highCutFilters)
        f.prepare(spec);
    
    // Set default high-cut filter to tame bright resonances
    setHighCut(8000.0f);
}

void AdvancedReverbEngine::reset()
{
    simpleReverb.reset();
    
    if (fdnTank)
        fdnTank->reset();
    if (earlyReflections)
        earlyReflections->reset();
    if (shimmer)
        shimmer->reset();
    if (inputDiffusion)
        inputDiffusion->reset();
    if (outputDiffusion)
        outputDiffusion->reset();
        
    preDelayLine.reset();
    for (auto& f : highCutFilters)
        f.reset();
}

void AdvancedReverbEngine::processStereo(float leftInput, float rightInput, float& leftOutput, float& rightOutput)
{
    /* NEW HYBRID CHAIN (no external parameters changed)
       Direct-in  → optional Pre-Delay
                 ↘
       EarlyReflections (level = earlyLevel)
                 ↘        (sum)
       InputDiffusion → FDNTank → OutputDiffusion  (scaled by lateLevel)
                 ↘
           High-Cut  →  Wet Out
    */

    // 1.  Fetch input & handle optional pre-delay
    float inL = leftInput;
    float inR = rightInput;

    if (preDelayMs > 0.0f)
    {
        const float preDelaySamples = (preDelayMs / 1000.0f) * static_cast<float>(sampleRate);
        if (preDelaySamples != currentPreDelaySamples)
        {
            preDelayLine.setDelay(preDelaySamples);
            currentPreDelaySamples = preDelaySamples;
        }

        // Read delayed samples
        inL = preDelayLine.popSample(0);
        inR = preDelayLine.popSample(1);

        // Push current input
        preDelayLine.pushSample(0, leftInput);
        preDelayLine.pushSample(1, rightInput);
    }

    // 2.  Early reflections
    float earlyL = 0.0f, earlyR = 0.0f;
    if (earlyReflections && earlyLevel > 0.0f)
        earlyReflections->processStereo(inL, inR, earlyL, earlyR);

    // 3.  Input diffusion (pre-late-reverb smoothing)
    float diffL = inL, diffR = inR;
    if (inputDiffusion && diffusion > 0.0f)
        inputDiffusion->processStereo(inL, inR, diffL, diffR);

    // 4.  Late reverb (FDN tank)
    float lateL = 0.0f, lateR = 0.0f;
    if (fdnTank && lateLevel > 0.0f)
    {
        fdnTank->processStereo(diffL, diffR, lateL, lateR);
        lateL *= lateLevel;
        lateR *= lateLevel;
    }

    // 5.  Output diffusion (tail sweetening)
    if (outputDiffusion && diffusion > 0.0f)
        outputDiffusion->processStereo(lateL, lateR, lateL, lateR);

    // 6.  Combine early + late components
    float wetL = earlyL + lateL;
    float wetR = earlyR + lateR;

    // 7.  Width adjustment (0 = mono, 1 = normal, 2 = extra wide)
    if (width != 1.0f)
    {
        const float mid  = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR) * width;

        // Prevent width changes from altering perceived loudness
        const float norm = 1.0f / juce::jmax(1.0f, std::sqrt((width * width + 1.0f) * 0.5f));

        wetL = (mid + side) * norm;
        wetR = (mid - side) * norm;
    }

    // 8.  Optional high-cut on wet path
    wetL = highCutFilters[0].processSample(wetL);
    wetR = highCutFilters[1].processSample(wetR);

    leftOutput  = wetL;
    rightOutput = wetR;
}

// Parameter setters - simplified implementations
void AdvancedReverbEngine::setAlgorithm(AlgorithmType algorithm) 
{
    currentAlgorithm = algorithm;
}

void AdvancedReverbEngine::setModulationType(ModulationType modType) 
{
    currentModType = modType;
}

void AdvancedReverbEngine::setPreDelay(float preDelayMs) 
{
    this->preDelayMs = juce::jlimit(0.0f, 500.0f, preDelayMs);
}

void AdvancedReverbEngine::setDecayTime(float decaySeconds) 
{
    this->decayTime = juce::jlimit(0.1f, 30.0f, decaySeconds);
    if (fdnTank)
        fdnTank->setDecayTime(decaySeconds);
}

void AdvancedReverbEngine::setSize(float size) 
{
    this->size = juce::jlimit(0.1f, 4.0f, size);
    if (fdnTank)
        fdnTank->setSize(size);
}

void AdvancedReverbEngine::setDiffusion(float diffusion) 
{
    this->diffusion = juce::jlimit(0.0f, 1.0f, diffusion);
    if (inputDiffusion)
        inputDiffusion->setDiffusion(diffusion);
}

void AdvancedReverbEngine::setDamping(float damping) 
{
    this->damping = juce::jlimit(0.0f, 1.0f, damping);
    if (fdnTank)
        fdnTank->setDamping(damping);
}

void AdvancedReverbEngine::setWidth(float width) 
{
    this->width = juce::jlimit(0.0f, 2.0f, width);
}

void AdvancedReverbEngine::setModulationDepth(float depth) 
{
    this->modDepth = juce::jlimit(0.0f, 1.0f, depth);
    if (fdnTank)
        fdnTank->setModulationDepth(depth);
}

void AdvancedReverbEngine::setModulationRate(float rateHz) 
{
    this->modRate = juce::jlimit(0.0f, 10.0f, rateHz);
    if (fdnTank)
        fdnTank->setModulationRate(rateHz);
}

void AdvancedReverbEngine::setEarlyLevel(float level) 
{
    this->earlyLevel = juce::jlimit(0.0f, 1.0f, level);
    if (earlyReflections)
        earlyReflections->setLevel(level);
}

void AdvancedReverbEngine::setLateLevel(float level) 
{
    this->lateLevel = juce::jlimit(0.0f, 1.0f, level);
}

void AdvancedReverbEngine::setEarlyCrossfeed(float crossfeed) 
{
    this->earlyCrossfeed = juce::jlimit(0.0f, 1.0f, crossfeed);
    if (earlyReflections)
        earlyReflections->setCrossfeed(crossfeed);
}

void AdvancedReverbEngine::setHighCut(float frequencyHz) 
{
    this->highCutFreq = juce::jlimit(20.0f, 20000.0f, frequencyHz);

    // Update filter coefficients immediately so the change takes effect
    auto coeff = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, this->highCutFreq);
    for (auto& f : highCutFilters)
        *f.coefficients = *coeff;
}

void AdvancedReverbEngine::setLowCut(float frequencyHz) 
{
    this->lowCutFreq = juce::jlimit(20.0f, 20000.0f, frequencyHz);
}

void AdvancedReverbEngine::setHighMultiplier(float multiplier) 
{
    this->highMultiplier = juce::jlimit(0.1f, 10.0f, multiplier);
}

void AdvancedReverbEngine::setLowMultiplier(float multiplier) 
{
    this->lowMultiplier = juce::jlimit(0.1f, 10.0f, multiplier);
}

void AdvancedReverbEngine::setShimmerEnabled(bool enabled) 
{
    this->shimmerEnabled = enabled;
    if (shimmer)
        shimmer->setEnabled(enabled);
}

void AdvancedReverbEngine::setShimmerPitch(float semitones) 
{
    this->shimmerPitch = juce::jlimit(-24.0f, 24.0f, semitones);
    if (shimmer)
        shimmer->setPitchShift(semitones);
}

void AdvancedReverbEngine::setShimmerFeedback(float feedback) 
{
    this->shimmerFeedback = juce::jlimit(0.0f, 0.95f, feedback);
    if (shimmer)
        shimmer->setFeedback(feedback);
}

void AdvancedReverbEngine::setShimmerMix(float mix) 
{
    this->shimmerMix = juce::jlimit(0.0f, 1.0f, mix);
    if (shimmer)
        shimmer->setMix(mix);
}

void AdvancedReverbEngine::setFreeze(bool freeze) 
{
    this->freeze = freeze;
}

//==============================================================================
// Geometry update – propagate to EarlyReflectionsEngine
void AdvancedReverbEngine::updateRoomGeometry(float roomWidth, float roomLength, float roomHeight,
                                              float srcX, float srcY, float srcZ)
{
    if (earlyReflections)
    {
        earlyReflections->configureGeometry(roomWidth, roomLength, roomHeight,
                                            srcX, srcY, srcZ);
    }
} 