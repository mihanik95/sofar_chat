#include "DistanceProcessor.h"
#include "AdvancedReverbEngine.h"
#include <cmath>
#include <array>

// -------------------------------------------------------------------------

DistanceProcessor::DistanceProcessor()
{
    juce::Logger::writeToLog("DistanceProcessor constructor - Advanced Reverb Research version");
    
    // Initialize environment parameters once
    updateEnvironmentParameters(Generic);
    
    // Initialize advanced reverb
    advancedReverb = std::make_unique<AdvancedReverbEngine>();
    
    // Enable true distance gain and delay for realistic perception
    trueGainEnabled = true;
    trueDelayEnabled = true;
    
    // Initialize other parameters
    currentEnvironment = Generic;
    lastEnvironment = Generic;
    currentDistance = 0.0f;
    currentPan = 0.0f;
    sampleRate = 44100.0;
    leftPanGain = 0.707f;
    rightPanGain = 0.707f;
    
    juce::Logger::writeToLog("DistanceProcessor - Advanced reverb engine initialized");
}
void DistanceProcessor::prepare(double sampleRate, int samplesPerBlock)
{
    try {
        this->sampleRate = sampleRate;
        this->samplesPerBlock = samplesPerBlock;
        
        // Initialize parameter smoothing with optimized times to prevent artifacts
        smoothedDistance.reset(sampleRate, 0.010);  // 10ms smoothing time - balanced responsiveness
        smoothedPan.reset(sampleRate, 0.015);       // 15ms smoothing for panning - prevent artifacts  
        smoothedGain.reset(sampleRate, 0.020);      // 20ms smoothing for gain - prevents zipper noise
        smoothedCutoffFreq.reset(sampleRate, 0.025); // 25ms smoothing for smooth frequency transitions
        smoothedLeftPanGain.reset(sampleRate, 0.015);
        smoothedRightPanGain.reset(sampleRate, 0.015);
        smoothedDelayTime.reset(sampleRate, 0.025);  // 25ms smoothing for delay - prevents artifacts
        smoothedStereoWidth.reset(sampleRate, 0.030); // 30ms smoothing for stereo width - smooth transitions
        smoothedHeight.reset(sampleRate, 0.020);
        smoothedHeight.setCurrentAndTargetValue(currentHeightPercent);
        smoothedClarity.reset(sampleRate, 0.020);
        smoothedClarity.setCurrentAndTargetValue(currentClarity);
        
        // Height processing smoothed parameters
        smoothedTiltGain.reset(sampleRate, 0.050);     // 50ms smoothing for tilt gain - prevent artifacts
        smoothedHeightWidth.reset(sampleRate, 0.030);  // 30ms smoothing for height width
        
        // Panning smoothed parameters for artifact-free processing
        smoothedShadowCutoff.reset(sampleRate, 0.040);     // 40ms smoothing for shadow cutoff
        smoothedFrontBackWidth.reset(sampleRate, 0.035);   // 35ms smoothing for front/back width
        smoothedPhaseShift.reset(sampleRate, 0.030);       // 30ms smoothing for phase shift
        smoothedBrightness.reset(sampleRate, 0.025);       // 25ms smoothing for brightness
        smoothedIldGainL.reset(sampleRate, 0.020);         // 20ms smoothing for ILD gains
        smoothedIldGainR.reset(sampleRate, 0.020);
        
        // ITD smoothing (slower to prevent artifacts)
        smoothedEarDelayLeft.reset(sampleRate, 0.015);
        smoothedEarDelayRight.reset(sampleRate, 0.015);
        
        // Prepare ear delay lines (max 1 ms @ 96 kHz = 96 samples → allocate 480 for headroom)
        earDelayLeft.reset();
        earDelayRight.reset();
        const int maxItdSamples = static_cast<int>(sampleRate * 0.002f); // 2 ms safety
        earDelayLeft.prepare(juce::dsp::ProcessSpec{ sampleRate, (juce::uint32)samplesPerBlock, 1 });
        earDelayRight.prepare(juce::dsp::ProcessSpec{ sampleRate, (juce::uint32)samplesPerBlock, 1 });
        earDelayLeft.setMaximumDelayInSamples(maxItdSamples);
        earDelayRight.setMaximumDelayInSamples(maxItdSamples);
        
        // Set initial values
        smoothedDistance.setCurrentAndTargetValue(0.0f);
        smoothedPan.setCurrentAndTargetValue(0.0f);
        smoothedGain.setCurrentAndTargetValue(1.0f);  // Start at unity gain
        smoothedCutoffFreq.setCurrentAndTargetValue(20000.0f);
        smoothedStereoWidth.setCurrentAndTargetValue(1.0f);
        smoothedLeftPanGain.setCurrentAndTargetValue(0.707f);
        smoothedRightPanGain.setCurrentAndTargetValue(0.707f);
        smoothedDelayTime.setCurrentAndTargetValue(0.0f);
        
        // Set initial values for height processing
        smoothedTiltGain.setCurrentAndTargetValue(0.0f);
        smoothedHeightWidth.setCurrentAndTargetValue(1.0f);
        
        // Set initial values for panning processing
        smoothedShadowCutoff.setCurrentAndTargetValue(12000.0f);
        smoothedFrontBackWidth.setCurrentAndTargetValue(1.0f);
        smoothedPhaseShift.setCurrentAndTargetValue(0.0f);
        smoothedBrightness.setCurrentAndTargetValue(1.0f);
        smoothedIldGainL.setCurrentAndTargetValue(0.707f);
        smoothedIldGainR.setCurrentAndTargetValue(0.707f);
        smoothedEarDelayLeft.setCurrentAndTargetValue(0.0f);
        smoothedEarDelayRight.setCurrentAndTargetValue(0.0f);
        
        // Initialize filters - separate left and right for perfect stereo balance
        lowPassFilterLeft.reset();
        lowPassFilterRight.reset();
        lowPassFilterLeft.prepare(juce::dsp::ProcessSpec{sampleRate, (juce::uint32)samplesPerBlock, 1});
        lowPassFilterRight.prepare(juce::dsp::ProcessSpec{sampleRate, (juce::uint32)samplesPerBlock, 1});
        
        // Prepare rear-hemisphere head-shadow filters (initially bypass-wide)
        backFilterLeft.reset();
        backFilterRight.reset();
        backFilterLeft.prepare(juce::dsp::ProcessSpec{sampleRate, (juce::uint32)samplesPerBlock, 1});
        backFilterRight.prepare(juce::dsp::ProcessSpec{sampleRate, (juce::uint32)samplesPerBlock, 1});
        auto identityCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 20000.0f);
        *backFilterLeft.coefficients = *identityCoeffs;
        *backFilterRight.coefficients = *identityCoeffs;
        
        // Prepare height tilt filters (initially bypass)
        heightTiltFilterLeft.reset();
        heightTiltFilterRight.reset();
        heightTiltFilterLeft.prepare(juce::dsp::ProcessSpec{sampleRate, (juce::uint32)samplesPerBlock, 1});
        heightTiltFilterRight.prepare(juce::dsp::ProcessSpec{sampleRate, (juce::uint32)samplesPerBlock, 1});
        *heightTiltFilterLeft.coefficients = *identityCoeffs;
        *heightTiltFilterRight.coefficients = *identityCoeffs;
        
        // Initialize delay line
        delayLine.reset();
        delayLine.prepare(juce::dsp::ProcessSpec{sampleRate, (juce::uint32)samplesPerBlock, 2});
        delayLine.setMaximumDelayInSamples(static_cast<int>(sampleRate * 0.5)); // 500ms max delay
        
        // Initialize gain processor
        gainProcessor.reset();
        gainProcessor.prepare(juce::dsp::ProcessSpec{sampleRate, (juce::uint32)samplesPerBlock, 2});
        
        // Prepare advanced reverb
        if (advancedReverb)
        {
            advancedReverb->prepare(sampleRate, samplesPerBlock);
        }
        
        // Initialize buffers
        tempBuffer.setSize(2, samplesPerBlock);
        psychoBuffer.setSize(2, samplesPerBlock);
        
        // Prepare early-reflection convolution (mono IR 512 taps default)
        earlyReflections.prepare (sampleRate, samplesPerBlock, 2);

        // Load unity (Dirac) impulse as placeholder early reflection IR
        earlyReflections.loadDirac (512, sampleRate);
        
        // Prepare HRTF convolvers
        juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 1 };
        hrtfLeft.prepare (spec);
        hrtfRight.prepare (spec);

        // Load default SOFA database (fallback to unity if not found)
        const juce::File sofaFile (juce::File::getCurrentWorkingDirectory().getChildFile ("libs/libmysofa/share/default.sofa"));
        hrirDatabase.loadSofaFile (sofaFile.getFullPathName());

        updateHrirFilters (0.0f, 0.0f);
        
        juce::Logger::writeToLog("DistanceProcessor prepared successfully");
    }
    catch (const std::exception& e) {
        juce::Logger::writeToLog("DistanceProcessor prepare error: " + juce::String(e.what()));
    }
}

void DistanceProcessor::reset()
{
    try {
        if (advancedReverb)
            advancedReverb->reset();
        lowPassFilterLeft.reset();
        lowPassFilterRight.reset();
        heightTiltFilterLeft.reset();
        heightTiltFilterRight.reset();
        delayLine.reset();
        earDelayLeft.reset();
        earDelayRight.reset();
        gainProcessor.reset();
        tempBuffer.clear();
        psychoBuffer.clear();
        
        // Reset smoothed values to current values (no sudden jumps)
        smoothedDistance.setCurrentAndTargetValue(smoothedDistance.getCurrentValue());
        smoothedPan.setCurrentAndTargetValue(smoothedPan.getCurrentValue());
        smoothedGain.setCurrentAndTargetValue(smoothedGain.getCurrentValue());
        smoothedCutoffFreq.setCurrentAndTargetValue(smoothedCutoffFreq.getCurrentValue());
        smoothedLeftPanGain.setCurrentAndTargetValue(smoothedLeftPanGain.getCurrentValue());
        smoothedRightPanGain.setCurrentAndTargetValue(smoothedRightPanGain.getCurrentValue());
        smoothedDelayTime.setCurrentAndTargetValue(smoothedDelayTime.getCurrentValue());
        
        juce::Logger::writeToLog("DistanceProcessor reset");
    }
    catch (const std::exception& e) {
        juce::Logger::writeToLog("DistanceProcessor reset error: " + juce::String(e.what()));
    }
}

void DistanceProcessor::processBlock(juce::AudioBuffer<float>& buffer, float distance, float panValue, Environment environment)
{
    try {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        
        if (numSamples <= 0 || numChannels <= 0) return;
        
        // Set target values for tracking (but use actual values for processing)
        smoothedDistance.setTargetValue(distance);
        smoothedPan.setTargetValue(panValue);
        smoothedClarity.setTargetValue(currentClarity);
        
        // Process effects with immediate response
        processDistanceEffects(buffer, distance, panValue, numSamples);
        
    }
    catch (const std::exception& e) {
        juce::Logger::writeToLog("DistanceProcessor::processBlock error: " + juce::String(e.what()));
    }
}

void DistanceProcessor::processDistanceEffects(juce::AudioBuffer<float>& buffer, float distance, float panValue, int numSamples)
{
    try {
        // PERFECT TRANSPARENCY - Completely fixed 0% to 1% jump
        // =====================================================
        
        // Create smooth distance factor relative to the current room size
        const float distanceFactor = juce::jlimit(0.0f, 1.0f,
                                                 currentMaxDistance > 0.0f
                                                     ? distance / currentMaxDistance
                                                     : 0.0f);

        // CRITICAL: At exact zero distance, do nothing but basic panning
        if (distanceFactor <= 0.0f) {
            // Only basic equal-power panning - no spatial processing at all
            if (buffer.getNumChannels() >= 2) {
                const float azRad = panValue * juce::MathConstants<float>::pi / 180.0f;
                const float panNorm = juce::jlimit(-1.0f, 1.0f, std::sin(azRad));
                
                const float gainL = std::sqrt(0.5f * (1.0f - panNorm));
                const float gainR = std::sqrt(0.5f * (1.0f + panNorm));
                
                auto* left = buffer.getWritePointer(0);
                auto* right = buffer.getWritePointer(1);
                for (int n = 0; n < numSamples; ++n) {
                    const float leftSample = left[n] * gainL;
                    const float rightSample = right[n] * gainR;
                    left[n] = leftSample;
                    right[n] = rightSample;
                }
            }
            return; // EXIT - no spatial processing whatsoever
        }
        
        // Scale all spatial processing directly with the distance factor
        const float spatialProcessingAmount = distanceFactor;
        
        // ROOM-CONNECTED SPATIAL PROCESSING with smooth scaling
        // ====================================================
        
        // 1. ROOM-SCALED DISTANCE - Smooth scaling by room size
        const float roomDepth = juce::jmax(1.0f, currentRoomLength); // Minimum 1m to avoid division by zero
        const float actualDistanceMeters = distanceFactor * roomDepth; // 0-1 maps to 0-roomDepth meters
        
        // 2. ROOM-CONSTRAINED PANNING - Smooth panning with room boundaries
        const float panRad = panValue * juce::MathConstants<float>::pi / 180.0f;
        const float maxLateralDistance = juce::jmax(0.5f, currentRoomWidth * 0.5f); // Minimum 0.5m
        const float lateralDistanceMeters = std::sin(panRad) * maxLateralDistance;
        const float absoluteLateralDistance = std::abs(lateralDistanceMeters);
        
        // 3. ROOM-SCALED HEIGHT - Smooth height scaling
        const float sourceHeightMeters = currentHeightPercent * juce::jmax(2.0f, currentRoomHeight); // Minimum 2m
        const float verticalOffsetMeters = sourceHeightMeters - DistanceProcessor::listenerEarHeight;
        
        // 4. SMOOTH 3D POSITION CALCULATION
        const float true3DDistance = std::sqrt(actualDistanceMeters * actualDistanceMeters + 
                                              absoluteLateralDistance * absoluteLateralDistance + 
                                              verticalOffsetMeters * verticalOffsetMeters);
        
        // 5. ROOM SIZE PERCEPTION - Smooth scaling without hard thresholds
        const float roomVolume = currentRoomWidth * currentRoomLength * currentRoomHeight;
        
        // 6. SMOOTH DISTANCE PERCEPTION SCALING
        const float basePerceptualFactor = 1.0f + (roomDepth - 3.0f) * 0.15f; // Smooth scaling
        const float perceptualDistanceFactor = juce::jlimit(0.5f, 2.5f, basePerceptualFactor);
        
        // 7. APPLY SMOOTH SPATIAL PROCESSING
        // =================================

        // Scale all effects by spatialProcessingAmount for smooth onset
        const float effectiveDistance = true3DDistance * perceptualDistanceFactor;

        // Skip heavy convolution/reverb when extremely far or in huge rooms
        const bool heavyLoad = (effectiveDistance > 30.0f || currentRoomLength > 50.0f);

        // In heavy-load scenarios use a simplified path to avoid CPU spikes
        if (heavyLoad)
        {
            if (trueGainEnabled)
                processDistanceGain(buffer, effectiveDistance, numSamples);

            smoothedPan.setTargetValue(panValue);
            processPanning(buffer, panValue, numSamples);

            processAirAbsorption(buffer, effectiveDistance, numSamples);
            // lightweight height cues still apply
            processHeightEffects(buffer, numSamples);
            return; // skip expensive processing
        }
        
        // Height effects - always process for smooth height movement
        processHeightEffects(buffer, numSamples);
        
        // Delay effect with smooth scaling
        if (trueDelayEnabled && spatialProcessingAmount > 0.01f) {
            processDelayEffect(buffer, effectiveDistance * spatialProcessingAmount, numSamples);
        }
        
        // Distance gain with smooth scaling
        if (trueGainEnabled && spatialProcessingAmount > 0.01f) {
            processDistanceGain(buffer, effectiveDistance * spatialProcessingAmount, numSamples);
        }
        
        // Air absorption with smooth scaling
        if (spatialProcessingAmount > 0.01f) {
            processAirAbsorption(buffer, effectiveDistance * spatialProcessingAmount, numSamples);
        }
        
        // ROOM WIDTH PERCEPTION - smooth and continuous
        // =====================================================

        // Clamp to a realistic range to avoid extreme values
        const float safeRoomWidth = juce::jlimit(2.0f, 100.0f, currentRoomWidth);

        // Map room width (2m..20m) to stereo width (0.6x..1.8x) with linear interpolation
        const float widthNorm = juce::jlimit(0.0f, 1.0f, (safeRoomWidth - 2.0f) / 18.0f);
        float safeStereoWidth = 0.6f + widthNorm * 1.2f;

        // Ensure bounds for safety
        safeStereoWidth = juce::jlimit(0.6f, 1.8f, safeStereoWidth);

        // Width effect should only be noticeable when panned off centre
        const float lateralPanFactor = std::abs(std::sin(panRad));
        safeStereoWidth = 1.0f + (safeStereoWidth - 1.0f) * lateralPanFactor;
        
        // SAFE M/S processing - smooth width without channel swapping
        if (buffer.getNumChannels() >= 2 && std::abs(safeStereoWidth - 1.0f) > 0.05f)
        {
            // Target width evolves with distance instead of collapsing to mono
            const float targetWidth = 1.0f + (safeStereoWidth - 1.0f) * spatialProcessingAmount;
            smoothedStereoWidth.setTargetValue(targetWidth);

            for (int n = 0; n < numSamples; ++n)
            {
                const float leftSample  = buffer.getSample(0, n);
                const float rightSample = buffer.getSample(1, n);

                const float midSample  = (leftSample + rightSample) * 0.5f;
                const float sideSample = (leftSample - rightSample) * 0.5f;

                const float widthValue = smoothedStereoWidth.getNextValue();
                const float processedSide = sideSample * widthValue;

                // Normalise output to avoid perceived loudness changes
                const float norm = 1.0f / juce::jmax(1.0f, std::sqrt((widthValue * widthValue + 1.0f) * 0.5f));

                const float newLeft  = juce::jlimit(-2.0f, 2.0f, (midSample + processedSide) * norm);
                const float newRight = juce::jlimit(-2.0f, 2.0f, (midSample - processedSide) * norm);

                buffer.setSample(0, n, newLeft);
                buffer.setSample(1, n, newRight);
            }
        }
        
        // SAFE ROOM-CONNECTED PANNING - Improved artifact elimination
        smoothedPan.setTargetValue(panValue);
        processPanning(buffer, panValue, numSamples);
        
        // Psychoacoustic effects with smooth scaling
        if (spatialProcessingAmount > 0.01f) {
            processPsychoacousticEffects(buffer, effectiveDistance * spatialProcessingAmount, numSamples);
        }
        
        // Early reflections with smooth scaling
        if (!heavyLoad && spatialProcessingAmount > 0.1f) {
            earlyReflections.process(buffer);
        }
        
        // ULTRA-SAFE REVERB PROCESSING - Fixed resonance issues
        // =====================================================
        if (!heavyLoad && spatialProcessingAmount > 0.1f) {
            // ULTRA-CONSERVATIVE reverb processing
            juce::AudioBuffer<float> cleanInput(buffer.getNumChannels(), numSamples);
            cleanInput.makeCopyOf(buffer, true);

            tempBuffer.makeCopyOf(cleanInput, true);
            processLateReverb(effectiveDistance * spatialProcessingAmount, numSamples, buffer.getNumChannels());
            
            // ULTRA-CONSERVATIVE blend - prevent any resonance
            const float ultraSafeReverbMix = juce::jlimit(0.0f, 0.1f, spatialProcessingAmount * 0.08f); // Max 8%
            
            for (int channel = 0; channel < buffer.getNumChannels() && channel < tempBuffer.getNumChannels(); ++channel) {
                auto* output = buffer.getWritePointer(channel);
                const auto* original = cleanInput.getReadPointer(channel);
                const auto* reverb = tempBuffer.getReadPointer(channel);
                
                for (int n = 0; n < numSamples; ++n) {
                    const float originalSample = original[n];
                    const float reverbSample = juce::jlimit(-1.0f, 1.0f, reverb[n]); // Limit reverb
                    
                    // Ultra-conservative blend
                    output[n] = originalSample * (1.0f - ultraSafeReverbMix) + reverbSample * ultraSafeReverbMix;
                    
                    // Ultra-safe limiting
                    output[n] = juce::jlimit(-1.2f, 1.2f, output[n]);
                }
            }
        }

        // OPTIONAL: Final HRTF convolution with ultra-safe scaling
        if (!heavyLoad && spatialProcessingAmount > 0.2f) {
            juce::AudioBuffer<float> dryCopy(buffer);
            processHrtfConvolution(buffer);

            // Ultra-conservative HRTF blend
            const float ultraSafeHrtfAmount = spatialProcessingAmount * 0.3f; // Max 30%
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                float* wet = buffer.getWritePointer(ch);
                const float* dry = dryCopy.getReadPointer(ch);
                for (int n = 0; n < numSamples; ++n) {
                    wet[n] = wet[n] * ultraSafeHrtfAmount + dry[n] * (1.0f - ultraSafeHrtfAmount);
                    wet[n] = juce::jlimit(-1.2f, 1.2f, wet[n]); // Final safety limiting
                }
            }
        }
        
    }
    catch (const std::exception& e) {
        juce::Logger::writeToLog("processDistanceEffects error: " + juce::String(e.what()));
        // Clear buffer on error to prevent artifacts
        buffer.clear();
    }
}

void DistanceProcessor::processDelayEffect(juce::AudioBuffer<float>& buffer, float distance, int numSamples)
{
    try {
        // distance is already in meters, use it directly
        const float actualDistance = distance;
        
        // CRITICAL: Complete bypass at 0% distance for perfect transparency
        if (actualDistance <= 0.0f) {
            // Ensure smoothed delay is set to zero for perfect bypass
            smoothedDelayTime.setCurrentAndTargetValue(0.0f);
            return; // No processing at all - completely transparent
        }
        
        // FIXED: Smooth gradual onset instead of hard cutoff
        // Calculate smooth transition factor from 0m to 1m
        float delayStrength = 1.0f;
        if (actualDistance <= 1.0f) {
            // Smooth cubic curve from 0 to 1 over 1 meter
            delayStrength = actualDistance * actualDistance * actualDistance; // cubic ease-in
            delayStrength = juce::jlimit(0.0f, 1.0f, delayStrength);
        }
        
        // Apply minimal delay even at very close distances for smoothness
        float minDelayTime = 0.001f; // 1ms minimum delay
        
        // FIXED: Calculate delay time using temperature-adjusted speed of sound
        const float delayTimeSeconds = (actualDistance / speedOfSound) + minDelayTime;
        const float delayTimeSamples = delayTimeSeconds * static_cast<float>(sampleRate);
        
        // Smart delay time handling to prevent artifacts
        float currentDelayTime = smoothedDelayTime.getCurrentValue();
        float targetDelayTime = delayTimeSamples;
        float delayChange = std::abs(targetDelayTime - currentDelayTime);
        
        // If change is very small (< 1 sample), apply immediately
        if (delayChange < 1.0f) {
            smoothedDelayTime.setCurrentAndTargetValue(targetDelayTime);
        } else {
            smoothedDelayTime.setTargetValue(targetDelayTime);
        }
        
        // Process delay with smooth strength factor
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float currentDelayTimeSamples = smoothedDelayTime.getNextValue();
            
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                float inputSample = buffer.getSample(channel, sample);
                
                // Get delayed sample
                float delayedSample = delayLine.popSample(channel, currentDelayTimeSamples);
                
                // Mix original and delayed signal with smooth strength
                float mixedSample = inputSample * (1.0f - delayStrength * 0.1f) + delayedSample * delayStrength * 0.1f;
                
                // Push input into delay line
                delayLine.pushSample(channel, inputSample);
                
                // Set output sample
                buffer.setSample(channel, sample, mixedSample);
            }
        }
        
    }
    catch (const std::exception& e) {
        juce::Logger::writeToLog("processDelayEffect error: " + juce::String(e.what()));
    }
}

void DistanceProcessor::processDistanceGain(juce::AudioBuffer<float>& buffer, float distance, int numSamples)
{
    try {
        const float actualDistance = distance;
 
        // Perfect transparency at zero distance
        if (actualDistance <= 0.0f)
        {
            smoothedGain.setCurrentAndTargetValue(1.0f);
            return;
        }
 
        float finalGain = 1.0f;
 
        // 1 / d law beyond 1 m, unity within 1 m (no boosting)
        if (actualDistance > 1.0f)
            finalGain = 1.0f / actualDistance; // linear amplitude, −6 dB per doubling
 
        // Optional volume-compensation curve (0..1)
        // Instead of forcing the gain toward unity, scale the attenuation
        // exponent so that higher values reduce the rate at which volume
        // decreases with distance.
        if (currentVolumeCompensation > 0.0f)
        {
            const float exponent = 1.0f - juce::jlimit(0.0f, 1.0f, currentVolumeCompensation);
            finalGain = std::pow(finalGain, exponent);
        }
 
        // Floor at −60 dB to avoid denormals
        finalGain = juce::jmax(finalGain, 0.001f);
 
        smoothedGain.setTargetValue(finalGain);
 
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* ptr = buffer.getWritePointer(ch);
            for (int s = 0; s < numSamples; ++s)
                ptr[s] *= smoothedGain.getNextValue();
        }
    }
    catch (const std::exception& e) {
        juce::Logger::writeToLog("processDistanceGain error: " + juce::String(e.what()));
    }
}

void DistanceProcessor::processAirAbsorption(juce::AudioBuffer<float>& buffer, float distance, int numSamples)
{
    try {
        const float actualDistance = distance;
 
        if (actualDistance <= 0.0f)
        {
            smoothedCutoffFreq.setCurrentAndTargetValue(20000.0f);
            lowPassFilterLeft.reset();
            lowPassFilterRight.reset();
            lastCutoffFreq = 20000.0f;
            return;
        }
 
        // If the user has set Air Absorption to 0 %, keep the full bandwidth.
        if (currentAirAbsorption <= 0.0001f)
        {
            smoothedCutoffFreq.setCurrentAndTargetValue(20000.0f);
            lowPassFilterLeft.reset();
            lowPassFilterRight.reset();
            lastCutoffFreq = 20000.0f;
            return; // Bypass filtering entirely
        }
 
        // MUCH MORE SUBTLE air absorption - like dearVR MICRO
        // Only subtle high-frequency roll-off, never aggressive frequency deletion
        float targetCutoff = 20000.0f; // Start with full bandwidth
        
        // Very gentle distance-based HF roll-off
        if (actualDistance > 1.0f) {
            // Gentle logarithmic curve - much more subtle than before
            float distanceFactor = std::log(actualDistance) / std::log(20.0f); // 0 to 1 over 1m to 20m
            distanceFactor = juce::jlimit(0.0f, 1.0f, distanceFactor);
            
            // Subtle cutoff reduction: 20kHz down to 8kHz maximum
            targetCutoff = 20000.0f - distanceFactor * 12000.0f;
        }
        
        // User air-absorption parameter adds subtle extra HF roll-off
        const float userEffect = currentAirAbsorption * 0.3f; // Much more subtle multiplier
        const float distanceRatio = currentMaxDistance > 0.0f ? juce::jlimit(0.0f, 1.0f, actualDistance / currentMaxDistance) : 0.0f;
        targetCutoff -= userEffect * 3000.0f * distanceRatio; // Maximum 3kHz reduction instead of 4kHz
        
        // CRITICAL: Never go below 5kHz - preserve musical content
        targetCutoff = juce::jlimit(5000.0f, 20000.0f, targetCutoff);
 
        smoothedCutoffFreq.setTargetValue(targetCutoff);
        const float currentCutoff = smoothedCutoffFreq.getNextValue();
        if (std::abs(currentCutoff - lastCutoffFreq) > 50.0f) // Less frequent updates
        {
            // Use gentler filter slope (0.5 Q instead of 0.707)
            auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, currentCutoff, 0.5f);
            *lowPassFilterLeft.coefficients  = *coeffs;
            *lowPassFilterRight.coefficients = *coeffs;
            lastCutoffFreq = currentCutoff;
        }
 
        // Apply filter only if cutoff is below 18kHz (avoid unnecessary processing)
        if (currentCutoff < 18000.0f) {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto* ptr = buffer.getWritePointer(ch);
                for (int s = 0; s < numSamples; ++s)
                    ptr[s] = (ch == 0 ? lowPassFilterLeft : lowPassFilterRight).processSample(ptr[s]);
            }
        }
    }
    catch (const std::exception& e) {
        juce::Logger::writeToLog("processAirAbsorption error: " + juce::String(e.what()));
    }
}

void DistanceProcessor::processStereoWidth(juce::AudioBuffer<float>& buffer, float distance, int numSamples)
{
    try {
        // Only process stereo buffers
        if (buffer.getNumChannels() < 2) return;
        
        const float actualDistance = distance;
        
        // Moderately adjust stereo width based on room size and distance
        float roomWidthFactor = juce::jlimit(0.7f, 1.3f, currentRoomWidth / 6.0f);

        float distanceWidth = 1.0f;
        if (actualDistance < 2.0f)
        {
            distanceWidth = 1.3f - 0.3f * (actualDistance / 2.0f); // 1.3x to 1.0x
        }
        else
        {
            float t = juce::jlimit(0.0f, 1.0f, (actualDistance - 2.0f) / 8.0f);
            distanceWidth = 1.0f - 0.5f * t; // 1.0x to 0.5x
        }

        float finalWidth = juce::jlimit(0.5f, 1.5f, roomWidthFactor * distanceWidth);
        
        // Smooth the width parameter
        smoothedStereoWidth.setTargetValue(finalWidth);
        
        // Process using Mid/Side technique for dramatic stereo width control
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float currentWidth = smoothedStereoWidth.getNextValue();
            
            // Get left and right samples
            float leftSample = buffer.getSample(0, sample);
            float rightSample = buffer.getSample(1, sample);
            
            // Convert to Mid/Side
            float midSample = (leftSample + rightSample) * 0.5f;
            float sideSample = (leftSample - rightSample) * 0.5f;
            
            // Apply width factor with gentle scaling
            sideSample *= currentWidth;

            // Normalise to prevent width changes altering level
            const float norm = 1.0f / juce::jmax(1.0f, std::sqrt((currentWidth * currentWidth + 1.0f) * 0.5f));

            // Convert back to Left/Right and apply normalisation
            float newLeftSample = (midSample + sideSample) * norm;
            float newRightSample = (midSample - sideSample) * norm;

            // Set the processed samples
            buffer.setSample(0, sample, newLeftSample);
            buffer.setSample(1, sample, newRightSample);
        }
        
    }
    catch (const std::exception& e) {
        juce::Logger::writeToLog("processStereoWidth error: " + juce::String(e.what()));
    }
}

void DistanceProcessor::processPanning(juce::AudioBuffer<float>& buffer, float panValue, int numSamples)
{
    try {
        if (buffer.getNumChannels() < 2) return;

        // ROOM-AWARE PANNING - Panning feels connected to actual room dimensions
        const float azRad = panValue * juce::MathConstants<float>::pi / 180.0f;

        // Keep HRIR filters current for later binaural convolution
        const float elDeg = (currentHeightPercent - 0.5f) * 60.0f;
        updateHrirFilters (panValue, elDeg);

        // ROOM BOUNDARY AWARENESS - Calculate position relative to room walls
        const float maxLateralDistance = currentRoomWidth * 0.5f; // Half room width from center
        const float lateralPosition = std::sin(azRad) * maxLateralDistance; // -roomWidth/2 to +roomWidth/2
        const float distanceToLeftWall = maxLateralDistance + lateralPosition; // 0 to roomWidth
        const float distanceToRightWall = maxLateralDistance - lateralPosition; // 0 to roomWidth
        const float closestWallDistance = juce::jmin(distanceToLeftWall, distanceToRightWall);
        
        // ROOM SIZE PERCEPTION - Larger rooms feel more spacious
        const float roomSizeFactor = juce::jlimit(0.5f, 2.5f, currentRoomWidth / 6.0f); // 6m = reference room width
        
        // WALL PROXIMITY EFFECTS - Sources very close to walls sound different
        float wallProximityFactor = 1.0f;
        if (closestWallDistance < 1.0f) {
            wallProximityFactor = 0.5f + (closestWallDistance / 1.0f) * 0.5f; // 0.5x to 1.0x stereo width near walls
        }
        
        // ROOM-CONSTRAINED PANNING LIMITS
        // In small rooms, extreme panning feels more dramatic
        // In large rooms, same pan angle feels less dramatic
        float panScale = 1.0f;
        if (currentRoomWidth <= 10.0f)
        {
            // 2m..10m -> 0.7x..1.0x
            const float t = juce::jlimit(0.0f, 1.0f, (currentRoomWidth - 2.0f) / 8.0f);
            panScale = 0.7f + t * 0.3f;
        }
        else
        {
            // 10m..20m+ -> 1.0x..1.3x
            const float t = juce::jlimit(0.0f, 1.0f, (currentRoomWidth - 10.0f) / 10.0f);
            panScale = 1.0f + t * 0.3f;
        }

        float roomConstrainedPan = juce::jlimit(-180.0f, 180.0f, panValue * panScale);

        const float roomAwareAzRad = roomConstrainedPan * juce::MathConstants<float>::pi / 180.0f;

        // SMOOTH FRONT/BACK DISTINCTION with room awareness
        const float frontBackFactor = std::cos(roomAwareAzRad); // +1 = front, -1 = back
        const bool isRearSource = frontBackFactor < 0.0f;
        const float frontBackAmount = std::abs(frontBackFactor); // 0 = side, 1 = front/back
        
        // ROOM-AWARE HEAD SHADOW FILTERING for rear sources
        if (isRearSource)
        {
            // Calculate head shadow attenuation with room size awareness
            const float rearAmount = -frontBackFactor; // 0 = side, 1 = directly behind
            
            // Room size affects head shadow - larger rooms have more diffuse rear sources
            float roomAwareShadowIntensity = rearAmount;
            if (currentRoomLength > 8.0f) {
                // Large rooms: less intense head shadow (more diffuse)
                roomAwareShadowIntensity *= 0.6f;
            } else if (currentRoomLength < 4.0f) {
                // Small rooms: more intense head shadow (more direct)
                roomAwareShadowIntensity *= 1.4f;
            }
            roomAwareShadowIntensity = juce::jlimit(0.0f, 1.0f, roomAwareShadowIntensity);
            
            // Conservative cutoff range with room awareness
            const float baseCutoff = 12000.0f - roomAwareShadowIntensity * 4000.0f; // 4kHz to 12kHz
            const float roomAwareCutoff = baseCutoff * (1.0f + (roomSizeFactor - 1.0f) * 0.3f); // Room size affects cutoff
            const float shadowCutoff = juce::jlimit(4000.0f, 15000.0f, roomAwareCutoff);
            
            // Smooth cutoff changes to prevent artifacts
            smoothedShadowCutoff.setTargetValue(shadowCutoff);
            const float currentShadowCutoff = smoothedShadowCutoff.getNextValue();
            
            // Update head shadow filters only when needed and with smooth transitions
            if (std::abs(currentShadowCutoff - lastShadowCutoff) > 200.0f) // Less frequent updates
            {
                // Very gentle high-shelf reduction with room-aware intensity
                const float attenuationDb = -2.0f * roomAwareShadowIntensity; // Max -2dB, room-aware
                auto shadowCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, currentShadowCutoff, 0.707f, 
                                                                                       juce::Decibels::decibelsToGain(attenuationDb));
                *backFilterLeft.coefficients = *shadowCoeffs;
                *backFilterRight.coefficients = *shadowCoeffs;
                lastShadowCutoff = currentShadowCutoff;
            }
            
            // Apply gentle head shadow filtering
            auto* left = buffer.getWritePointer(0);
            auto* right = buffer.getWritePointer(1);
            
            for (int n = 0; n < numSamples; ++n)
            {
                left[n] = backFilterLeft.processSample(left[n]);
                right[n] = backFilterRight.processSample(right[n]);
            }
        }
        
        // ROOM-AWARE FRONT/BACK SPATIAL PROCESSING
        // Room size dramatically affects front/back perception
        
        // Smooth front/back width changes with room awareness
        float frontBackWidth = 1.0f;
        if (isRearSource) {
            // Back sources: room size affects stereo width
            float baseWidth = 0.7f + (1.0f - frontBackAmount) * 0.2f; // 0.7x to 0.9x
            frontBackWidth = baseWidth * roomSizeFactor * 0.8f; // Scale by room size
        } else {
            // Front sources: wider stereo image, scaled by room size
            float baseWidth = 1.0f + frontBackAmount * 0.4f; // 1.0x to 1.4x
            frontBackWidth = baseWidth * roomSizeFactor * 0.6f; // Scale by room size
        }
        
        // Apply wall proximity effect
        frontBackWidth *= wallProximityFactor;
        frontBackWidth = juce::jlimit(0.3f, 2.5f, frontBackWidth);
        
        // Smooth width parameter changes to prevent artifacts
        smoothedFrontBackWidth.setTargetValue(frontBackWidth);
        
        // ROOM-AWARE PHASE EFFECTS for back sources
        float phaseShiftAmount = 0.0f;
        if (isRearSource) {
            // Room size affects phase shift intensity
            float basePhase = frontBackAmount * 0.15f;
            phaseShiftAmount = basePhase * (2.0f - roomSizeFactor * 0.5f); // Larger rooms = less phase shift
            phaseShiftAmount = juce::jlimit(0.0f, 0.3f, phaseShiftAmount);
        }
        smoothedPhaseShift.setTargetValue(phaseShiftAmount);
        
        // ROOM-AWARE BRIGHTNESS CHANGES
        float brightnessFactor = 1.0f;
        if (isRearSource) {
            // Back sources: room size affects brightness reduction
            float baseBrightness = 0.95f + (1.0f - frontBackAmount) * 0.05f; // 0.95x to 1.0x
            brightnessFactor = baseBrightness + (roomSizeFactor - 1.0f) * 0.02f; // Larger rooms = brighter
        } else {
            // Front sources: room size affects brightness enhancement
            float baseBrightness = 1.0f + frontBackAmount * 0.05f; // 1.0x to 1.05x
            brightnessFactor = baseBrightness + (roomSizeFactor - 1.0f) * 0.03f; // Larger rooms = brighter
        }
        brightnessFactor = juce::jlimit(0.9f, 1.15f, brightnessFactor);
        smoothedBrightness.setTargetValue(brightnessFactor);
        
        // PROCESS ROOM-AWARE SPATIAL EFFECTS
        for (int n = 0; n < numSamples; ++n)
        {
            float leftSample = buffer.getSample(0, n);
            float rightSample = buffer.getSample(1, n);
            
            // Apply smooth room-aware front/back stereo width
            const float currentWidth = smoothedFrontBackWidth.getNextValue();
            float midSample = (leftSample + rightSample) * 0.5f;
            float sideSample = (leftSample - rightSample) * 0.5f;
            sideSample *= currentWidth;
            
            // Apply smooth room-aware phase effects for back sources - FIXED artifacts
            const float currentPhaseShift = smoothedPhaseShift.getNextValue();
            if (currentPhaseShift > 0.001f) {
                // SAFE phase accumulation with proper bounds
                phaseAccumulator += currentPhaseShift * 0.005f; // Even slower accumulation
                phaseAccumulator = std::fmod(phaseAccumulator, 1.0f); // Safe modulo
                phaseAccumulator = juce::jlimit(0.0f, 1.0f, phaseAccumulator); // Safety bounds
                
                // SAFE phase effect - much more conservative
                const float safePhaseEffect = phaseAccumulator * 0.05f; // Reduced from 0.1f
                sideSample = sideSample * (1.0f - safePhaseEffect);
                sideSample = juce::jlimit(-2.0f, 2.0f, sideSample); // Safety bounds
            }
            
            // Apply smooth room-aware brightness changes
            const float currentBrightness = smoothedBrightness.getNextValue();
            leftSample = leftSample * currentBrightness;
            rightSample = rightSample * currentBrightness;
            
            // SAFE stereo reconstruction with bounds checking
            const float newLeft = juce::jlimit(-2.0f, 2.0f, midSample + sideSample);
            const float newRight = juce::jlimit(-2.0f, 2.0f, midSample - sideSample);
            buffer.setSample(0, n, newLeft);
            buffer.setSample(1, n, newRight);
        }

        // ROOM-AWARE ILD/ITD PROCESSING
        // Inter-aural level difference (ILD) using equal-power law with room scaling
        const float panNorm = juce::jlimit (-1.0f, 1.0f, std::sin (roomAwareAzRad));
        float gainL = std::sqrt (0.5f * (1.0f - panNorm));
        float gainR = std::sqrt (0.5f * (1.0f + panNorm));
        
        // Room size affects ILD intensity - larger rooms have more dramatic ILD
        const float ildIntensity = 0.5f + roomSizeFactor * 0.5f; // 0.5x to 2.0x intensity
        gainL = juce::jlimit(0.1f, 1.0f, 0.5f + (gainL - 0.5f) * ildIntensity);
        gainR = juce::jlimit(0.1f, 1.0f, 0.5f + (gainR - 0.5f) * ildIntensity);
        
        // Smooth ILD gains to prevent artifacts
        smoothedIldGainL.setTargetValue(gainL);
        smoothedIldGainR.setTargetValue(gainR);

        // ROOM-AWARE ITD - Inter-aural time difference with room scaling
        constexpr float maxITD = 0.0007f; // seconds base ITD
        const float roomAwareMaxITD = maxITD * (0.8f + roomSizeFactor * 0.4f); // Scale ITD by room size
        const float itdSec = roomAwareMaxITD * panNorm; // −L lead … +R lead

        const float delayLeftSec  = itdSec < 0.0f ? -itdSec : 0.0f;
        const float delayRightSec = itdSec > 0.0f ?  itdSec : 0.0f;

        const float delayLeftSamples  = delayLeftSec  * static_cast<float> (sampleRate);
        const float delayRightSamples = delayRightSec * static_cast<float> (sampleRate);

        // Smooth ITD changes to prevent artifacts
        smoothedEarDelayLeft.setTargetValue (delayLeftSamples);
        smoothedEarDelayRight.setTargetValue(delayRightSamples);

        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        // Process with smooth room-aware ITD + ILD
        for (int n = 0; n < numSamples; ++n)
        {
            const float delayL = smoothedEarDelayLeft.getNextValue();
            const float delayR = smoothedEarDelayRight.getNextValue();
            
            const float currentGainL = smoothedIldGainL.getNextValue();
            const float currentGainR = smoothedIldGainR.getNextValue();

            // Apply ITD via delay lines
            earDelayLeft.pushSample (0, left[n]);
            earDelayRight.pushSample(0, right[n]);

            const float delayedL = earDelayLeft.popSample (0, delayL);
            const float delayedR = earDelayRight.popSample(0, delayR);

            // Apply smooth room-aware ILD gains
            left[n]  = delayedL * currentGainL;
            right[n] = delayedR * currentGainR;
        }
    }
    catch (const std::exception& e) {
        juce::Logger::writeToLog("DistanceProcessor processPanning error: " + juce::String(e.what()));
    }
}

void DistanceProcessor::processLateReverb(float distance, int numSamples, int channels)
{
    try {
        if (!advancedReverb) return;
        
        const auto& env = environmentSettings[currentEnvironment];
        const float actualDistance = distance;
        
        // Skip reverb processing at 0 distance to preserve original sound
        if (actualDistance <= 0.0f) {
            return;
        }
        
        // ------------------------------------------------------------------
        // Update early-reflection geometry only when parameters change
        if (advancedReverb)
        {
            const float panDeg = smoothedPan.getCurrentValue();
            const float panRad = panDeg * juce::MathConstants<float>::pi / 180.0f;

            const float srcX = std::sin(panRad) * actualDistance; // left(+)/right(-)
            const float srcZ = std::cos(panRad) * actualDistance; // front(+)
            const float srcY = currentHeightPercent * currentRoomHeight; // metres

            const bool geometryChanged =
                std::abs(currentRoomWidth  - lastGeomRoomWidth)  > 0.05f ||
                std::abs(currentRoomLength - lastGeomRoomLength) > 0.05f ||
                std::abs(currentRoomHeight - lastGeomRoomHeight) > 0.05f ||
                std::abs(srcX - lastGeomSrcX) > 0.02f ||
                std::abs(srcY - lastGeomSrcY) > 0.02f ||
                std::abs(srcZ - lastGeomSrcZ) > 0.02f;

            if (geometryChanged)
            {
                advancedReverb->updateRoomGeometry(currentRoomWidth, currentRoomLength, currentRoomHeight,
                                                   srcX, srcY, srcZ);
                lastGeomRoomWidth  = currentRoomWidth;
                lastGeomRoomLength = currentRoomLength;
                lastGeomRoomHeight = currentRoomHeight;
                lastGeomSrcX = srcX;
                lastGeomSrcY = srcY;
                lastGeomSrcZ = srcZ;
            }
        }
        
        // DearVR-style distance-dependent reverb characteristics
        // Closer sources: less reverb, more direct sound
        // Distant sources: more reverb, less direct sound
        const float distanceRatio = juce::jlimit(0.0f, 1.0f, actualDistance / currentMaxDistance);
        
        // Distance-based reverb level scaling
        // Close sources (0-2m): minimal reverb
        // Medium sources (2-10m): moderate reverb  
        // Distant sources (10m+): full reverb
        float reverbLevelScale = 1.0f;
        if (actualDistance < 2.0f)
        {
            // Very close sources - minimal reverb
            reverbLevelScale = 0.1f + (actualDistance / 2.0f) * 0.4f; // 0.1 to 0.5
        }
        else if (actualDistance < 10.0f)
        {
            // Medium distance - gradual reverb increase
            reverbLevelScale = 0.5f + ((actualDistance - 2.0f) / 8.0f) * 0.4f; // 0.5 to 0.9
        }
        else
        {
            // Distant sources - full reverb
            reverbLevelScale = 0.9f + (juce::jmin(actualDistance - 10.0f, 10.0f) / 10.0f) * 0.1f; // 0.9 to 1.0
        }
        
        // DRAMATIC FRONT/BACK REVERB CHARACTERISTICS
        // Front sources: more early reflections, less late reverb (more direct)
        // Back sources: less early reflections, more late reverb (more diffuse)
        const float panDeg = smoothedPan.getCurrentValue();
        const float panRad = panDeg * juce::MathConstants<float>::pi / 180.0f;
        const float frontBackFactor = std::cos(panRad); // +1 = front, -1 = back
        const bool isRearSource = frontBackFactor < 0.0f;
        
        // Distance-based early/late balance with front/back modification
        float earlyToLateRatio = juce::jlimit(0.2f, 0.8f, 0.8f - distanceRatio * 0.6f);
        
        // DRAMATIC front/back modification
        if (isRearSource) {
            // Back sources: much more late reverb, less early reflections
            earlyToLateRatio *= 0.5f; // Reduce early reflections dramatically
        } else {
            // Front sources: more early reflections, less late reverb
            earlyToLateRatio *= 1.3f; // Increase early reflections
        }
        earlyToLateRatio = juce::jlimit(0.1f, 0.9f, earlyToLateRatio);
        
        const float earlyLevel = env.reverbLevel * reverbLevelScale * earlyToLateRatio;
        const float lateLevel = env.reverbLevel * reverbLevelScale * (1.0f - earlyToLateRatio);
        
        // Distance-based pre-delay (closer sources have shorter pre-delay)
        const float preDelayScale = juce::jlimit(0.3f, 1.0f, 0.3f + distanceRatio * 0.7f);
        const float scaledPreDelay = env.preDelay * preDelayScale;
        
        // Distance-based diffusion (distant sources more diffuse)
        const float diffusionScale = juce::jlimit(0.4f, 1.0f, 0.4f + distanceRatio * 0.6f);
        const float scaledDiffusion = env.diffusion * diffusionScale;
        
        // Distance-based decay time (distant sources seem to have longer decay)
        const float decayScale = juce::jlimit(0.7f, 1.3f, 0.7f + distanceRatio * 0.6f);
        const float scaledDecayTime = env.decayTime * decayScale;
        
        // DRAMATIC ROOM SIZE EFFECTS - Configure reverb based on environment and distance
        // Room size now has much more dramatic effect on reverb characteristics
        
        // Calculate dramatic room size factor
        const float roomVolume = currentRoomWidth * currentRoomLength * currentRoomHeight;
        const float roomSizeFactor = juce::jlimit(0.1f, 3.0f, roomVolume / 100.0f); // Normalize to 0.1-3.0 range
        
        if (currentEnvironment != lastEnvironment || true) // Always update for room size changes
        {
            AdvancedReverbEngine::AlgorithmType algorithm = AdvancedReverbEngine::Hall;
            AdvancedReverbEngine::ModulationType modulation = AdvancedReverbEngine::RandomMod;
            
            // Use a single algorithm (Hall) with moderate modulation
            algorithm = AdvancedReverbEngine::Hall;
            modulation = AdvancedReverbEngine::RandomMod;
            
            advancedReverb->setAlgorithm(algorithm);
            advancedReverb->setModulationType(modulation);
            
            // DRAMATIC room size effect on reverb size parameter
            const float dramaticRoomSize = env.roomSize * roomSizeFactor;
            advancedReverb->setSize(juce::jlimit(0.1f, 1.0f, dramaticRoomSize));
            
            lastEnvironment = currentEnvironment;
        }
        
        // Apply distance-dependent parameters - FIXED: Conservative levels to prevent buildup
        const float safeEarlyLevel = juce::jlimit(0.0f, 0.3f, earlyLevel * roomSizeFactor * 0.2f); // Much more conservative
        const float safeLateLevel = juce::jlimit(0.0f, 0.4f, lateLevel * roomSizeFactor * 0.3f); // Much more conservative
        const float safeDecayTime = juce::jlimit(0.1f, 8.0f, scaledDecayTime * juce::jmin(roomSizeFactor, 2.0f)); // Limit decay time
        
        advancedReverb->setEarlyLevel(safeEarlyLevel);
        advancedReverb->setLateLevel(safeLateLevel);
        advancedReverb->setPreDelay(juce::jlimit(0.0f, 0.1f, scaledPreDelay * juce::jmin(roomSizeFactor, 1.5f))); // Limit pre-delay
        advancedReverb->setDiffusion(juce::jlimit(0.3f, 0.8f, scaledDiffusion * juce::jmin(roomSizeFactor, 1.2f))); // Limit diffusion
        advancedReverb->setDecayTime(safeDecayTime); // Use safe decay time
        advancedReverb->setDamping(juce::jlimit(0.1f, 0.9f, env.damping * juce::jmax(0.5f, 1.0f / roomSizeFactor))); // More damping for stability
        
        // Process reverb - FIXED: Clean processing without internal feedback
        float* leftChannel = tempBuffer.getWritePointer(0);
        float* rightChannel = tempBuffer.getNumChannels() > 1 ? tempBuffer.getWritePointer(1) : nullptr;
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            // CRITICAL: Read clean input samples
            const float leftIn = leftChannel[sample];
            const float rightIn = rightChannel ? rightChannel[sample] : leftIn;
            
            float leftOut = 0.0f;
            float rightOut = 0.0f;
            
            // Process through advanced reverb engine
            advancedReverb->processStereo(leftIn, rightIn, leftOut, rightOut);
            
            // CRITICAL: Apply conservative wet/dry mix to prevent buildup
            const float wetLevel = juce::jlimit(0.0f, 0.6f, distanceRatio * 0.4f + 0.05f); // Much more conservative
            const float dryLevel = 1.0f - wetLevel * 0.3f; // Preserve more dry signal
            
            // CRITICAL: Safety limiting on reverb output
            leftOut = juce::jlimit(-1.0f, 1.0f, leftOut);
            rightOut = juce::jlimit(-1.0f, 1.0f, rightOut);
            
            // Store mixed output with safe levels
            leftChannel[sample] = leftIn * dryLevel + leftOut * wetLevel;
            if (rightChannel)
                rightChannel[sample] = rightIn * dryLevel + rightOut * wetLevel;
                
            // Final safety limiting per sample
            leftChannel[sample] = juce::jlimit(-1.5f, 1.5f, leftChannel[sample]);
            if (rightChannel)
                rightChannel[sample] = juce::jlimit(-1.5f, 1.5f, rightChannel[sample]);
        }
    }
    catch (const std::exception& e) {
        juce::Logger::writeToLog("DistanceProcessor processLateReverb error: " + juce::String(e.what()));
    }
}

void DistanceProcessor::processHeightEffects(juce::AudioBuffer<float>& buffer, int numSamples)
{
    try {
        if (buffer.getNumChannels() < 2 || numSamples <= 0) return;
        
        
        // Calculate height position with room connection
        const float heightFactor = juce::jlimit(0.0f, 1.0f, currentHeightPercent);
        const float roomHeight = juce::jmax(2.0f, currentRoomHeight);
        const float actualHeightMeters = heightFactor * roomHeight;
        const float roomCenterHeight = roomHeight * 0.5f;
        
        // Height deviation from center (-1 = floor, 0 = center, +1 = ceiling)
        const float heightDeviation = (actualHeightMeters - roomCenterHeight) / roomCenterHeight;
        const float clampedHeightDeviation = juce::jlimit(-1.0f, 1.0f, heightDeviation);
        
        const float roomHeightFactor = juce::jlimit(1.0f, 2.0f, roomHeight / 3.0f);
        
        const float tiltFreq = 800.0f;
        const float baseTiltGain = std::tanh(clampedHeightDeviation * 1.2f) * 8.0f;
        const float dramaticTiltGain = juce::jlimit(-10.0f, 10.0f, baseTiltGain * roomHeightFactor);
        
        // Smooth tilt gain changes
        smoothedTiltGain.setTargetValue(dramaticTiltGain);
        const float currentTiltGain = smoothedTiltGain.getNextValue();
        
        // Update filter coefficients when needed
        if (std::abs(currentTiltGain - lastTiltGain) > 0.5f) {
            try {
                if (sampleRate > 0.0 && tiltFreq > 0.0f && tiltFreq < sampleRate * 0.5f) {
                    if (currentTiltGain > 0) {
                        // Above center: high-shelf boost
                        auto tiltCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                            sampleRate, tiltFreq, 0.707f, juce::Decibels::decibelsToGain(currentTiltGain));
                        if (tiltCoeffs != nullptr) {
                            *heightTiltFilterLeft.coefficients = *tiltCoeffs;
                            *heightTiltFilterRight.coefficients = *tiltCoeffs;
                        }
                    } else if (currentTiltGain < 0) {
                        // Below center: low-pass filtering
                        auto tiltCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                            sampleRate, tiltFreq, 0.707f, juce::Decibels::decibelsToGain(-currentTiltGain));
                        if (tiltCoeffs != nullptr) {
                            *heightTiltFilterLeft.coefficients = *tiltCoeffs;
                            *heightTiltFilterRight.coefficients = *tiltCoeffs;
                        }
                    }
                    lastTiltGain = currentTiltGain;
                }
            }
            catch (...) {
                // Skip filtering on error
            }
        }
        
        const float baseWidthFactor = 1.0f - clampedHeightDeviation * 0.4f;
        const float dramaticWidthFactor = juce::jlimit(0.7f, 1.3f, baseWidthFactor * roomHeightFactor);
        
        smoothedHeightWidth.setTargetValue(dramaticWidthFactor);
        
        const float phaseShiftAmount = clampedHeightDeviation * 0.15f; // ±15% phase shift
        const float phaseShiftRadians = phaseShiftAmount * juce::MathConstants<float>::pi;
        
        const float heightGainModulation = 1.0f + clampedHeightDeviation * 0.05f;
        
        // Process height effects with dramatic changes
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float leftSample = buffer.getSample(0, sample);
            float rightSample = buffer.getSample(1, sample);
            
            if (std::abs(currentTiltGain) > 0.1f) {
                leftSample = heightTiltFilterLeft.processSample(leftSample);
                rightSample = heightTiltFilterRight.processSample(rightSample);
            }
            
            // Apply dramatic height-based stereo width
            const float currentWidthFactor = smoothedHeightWidth.getNextValue();
            float midSample = (leftSample + rightSample) * 0.5f;
            float sideSample = (leftSample - rightSample) * 0.5f;
            sideSample *= currentWidthFactor;
            
            if (std::abs(phaseShiftAmount) > 0.03f) {
                float phaseShiftedSide = sideSample * std::cos(phaseShiftRadians);
                sideSample = sideSample * 0.8f + phaseShiftedSide * 0.2f;
            }
            
            // Reconstruct left/right with dramatic changes
            leftSample = midSample + sideSample;
            rightSample = midSample - sideSample;
            
            // Apply dramatic height-based gain modulation
            leftSample *= heightGainModulation;
            rightSample *= heightGainModulation;
            
            // Apply safe output limiting
            leftSample = juce::jlimit(-2.0f, 2.0f, leftSample);
            rightSample = juce::jlimit(-2.0f, 2.0f, rightSample);
            
            buffer.setSample(0, sample, leftSample);
            buffer.setSample(1, sample, rightSample);
        }
        
    }
    catch (const std::exception& e) {
        juce::Logger::writeToLog("processHeightEffects error: " + juce::String(e.what()));
        // Clear buffer on error to prevent further issues
        buffer.clear();
    }
}

void DistanceProcessor::processPsychoacousticEffects(juce::AudioBuffer<float>& buffer, float distance, int numSamples)
{
    try {
        // distance is already in meters, use it directly
        const float actualDistance = distance;

        // Quick exit for zero distance (perfect transparency)
        if (actualDistance <= 0.0f || buffer.getNumChannels() < 2)
            return;

        /*
            Externalization Booster & Near-field ILD (no new UI parameters)
            ----------------------------------------------------------------
            1. Very small cross-feed (<7 %) that grows with distance to mimic
               early reflections / room leakage – helps move sound outside the head.
            2. Automatic near-field ILD (<1 m): the ear closest to the source is
               gently boosted while the far ear is slightly attenuated, providing
               realistic proximity cues without altering the public parameter set.
        */

        // Distance-based cross-feed coefficient (0 → 0.07)
        const float crossfeed = juce::jlimit (0.0f, 0.07f, 0.07f * (1.0f - std::exp (-actualDistance * 0.3f)));

        // Near-field closeness factor (0 at ≥1 m, 1 at 0 m)
        const float closeness = juce::jlimit (0.0f, 1.0f, 1.0f - actualDistance);

        // Early exit if no effects are necessary
        if (crossfeed <= 0.0f && closeness <= 0.0f)
            return;

        // Determine which ear is closer using current smoothed pan value
        const float panDeg  = smoothedPan.getCurrentValue();
        const float panRad  = panDeg * juce::MathConstants<float>::pi / 180.0f;
        const bool rightIsNear = (std::sin (panRad) >= 0.0f);

        // Pre-compute ILD gain factors (max ±3 dB ≈ 1.41x)
        const float nearBoost = 1.0f + 0.3f * closeness;    // up to +30 % (≈ +2.5 dB)
        const float farCut   = 1.0f / nearBoost;             // energy compensation

        // Process per-sample (lightweight operations)
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float left  = buffer.getSample (0, sample);
            float right = buffer.getSample (1, sample);

            // Cross-feed (use original samples to avoid cumulative bleed)
            const float newLeft  = left  + crossfeed * right;
            const float newRight = right + crossfeed * left;

            float procLeft  = newLeft;
            float procRight = newRight;

            // Apply near-field ILD only when within 1 m
            if (closeness > 0.0f)
            {
                if (rightIsNear)
                {
                    procRight *= nearBoost;
                    procLeft  *= farCut;
                }
                else
                {
                    procLeft  *= nearBoost;
                    procRight *= farCut;
                }
            }

            buffer.setSample (0, sample, procLeft);
            buffer.setSample (1, sample, procRight);
        }
    }
    catch (const std::exception& e) {
        juce::Logger::writeToLog("processPsychoacousticEffects error: " + juce::String (e.what()));
    }
}

void DistanceProcessor::updateEnvironmentParameters(Environment /*unused*/)
{
    // DearVR-style advanced room modeling with frequency-dependent absorption
    EnvironmentParams params;
    
    // Calculate room volume and surface area for realistic acoustics
    const float roomVolume = currentRoomWidth * currentRoomLength * currentRoomHeight;
    const float surfaceArea = 2.0f * (currentRoomWidth * currentRoomLength + 
                                     currentRoomWidth * currentRoomHeight + 
                                     currentRoomLength * currentRoomHeight);
    
    // Realistic RT60 calculation using Sabine's formula
    // RT60 = 0.161 * V / (A * α) where V = volume, A = surface area, α = absorption
    const float avgAbsorption = 0.1f + currentAirAbsorption * 0.4f; // 0.1 to 0.5 range
    const float rt60 = 0.161f * roomVolume / (surfaceArea * avgAbsorption);
    
    // Room size factor based on volume (more realistic than simple average)
    params.roomSize = juce::jlimit(0.1f, 4.0f, std::pow(roomVolume / 150.0f, 0.33f)); // Cube root scaling
    
    // Decay time based on RT60 calculation
    params.decayTime = juce::jlimit(0.2f, 8.0f, rt60);
    
    // Frequency-dependent damping (HF absorbed more than LF)
    // Higher frequencies are absorbed more by air and materials
    const float hfAbsorption = avgAbsorption * (1.0f + currentAirAbsorption * 2.0f);
    params.damping = juce::jlimit(0.02f, 0.95f, hfAbsorption);
    
    // Reverb level based on room acoustics (larger, less absorptive rooms = more reverb)
    const float reverbFactor = (1.0f - avgAbsorption) * params.roomSize * 0.15f;
    params.reverbLevel = juce::jlimit(0.0f, 0.4f, reverbFactor);
    
    // Pre-delay based on room dimensions (time for first reflection)
    const float maxDimension = juce::jmax(currentRoomWidth, currentRoomLength, currentRoomHeight);
    params.preDelay = juce::jlimit(1.0f, 100.0f, maxDimension * 2.9f); // Speed of sound factor
    
    // Diffusion based on room shape (more square = more diffuse)
    const float aspectRatio = juce::jmax(currentRoomWidth, currentRoomLength) / 
                             juce::jmin(currentRoomWidth, currentRoomLength);
    params.diffusion = juce::jlimit(0.1f, 1.0f, 1.0f / aspectRatio);
    
    // Store calculated parameters
    params.airAbsorptionCoeff = currentAirAbsorption;
    params.maxDistance = currentMaxDistance;

    environmentSettings[Generic] = params;

    // Apply frequency-dependent processing to reverb engine
    if (advancedReverb)
    {
        advancedReverb->setSize(params.roomSize);
        advancedReverb->setDamping(params.damping);
        advancedReverb->setDecayTime(params.decayTime);
        advancedReverb->setDiffusion(params.diffusion);
        advancedReverb->setPreDelay(params.preDelay);
        
        // Set frequency-dependent characteristics
        // High frequencies decay faster in real rooms
        const float highCutFreq = juce::jlimit(2000.0f, 20000.0f, 
                                              20000.0f - currentAirAbsorption * 15000.0f);
        advancedReverb->setHighCut(highCutFreq);
        
        // Low frequencies build up more in larger rooms
        const float lowCutFreq = juce::jlimit(20.0f, 200.0f, 
                                             200.0f - params.roomSize * 50.0f);
        advancedReverb->setLowCut(lowCutFreq);
        
        // Adjust early/late balance based on room size
        const float earlyLevel = juce::jlimit(0.1f, 0.6f, 0.4f - params.roomSize * 0.1f);
        const float lateLevel = juce::jlimit(0.3f, 0.8f, 0.5f + params.roomSize * 0.1f);
        advancedReverb->setEarlyLevel(earlyLevel);
        advancedReverb->setLateLevel(lateLevel);
    }
}

void DistanceProcessor::setEnvironmentType(Environment envType)
{
    juce::ignoreUnused(envType);
}

float DistanceProcessor::getMaxDistanceForEnvironment(Environment envType) const
{
    juce::ignoreUnused(envType);
    return currentMaxDistance;
}

//==============================================================================
// Simplified parameter setters
void DistanceProcessor::setDistance(float distanceMeters)
{
    currentDistance = juce::jlimit(0.0f, currentMaxDistance, distanceMeters);
}

void DistanceProcessor::setMaxDistance(float maxDistanceMeters)
{
    currentMaxDistance = juce::jlimit(5.0f, 100.0f, maxDistanceMeters);
    // Update the current environment's max distance
    environmentSettings[currentEnvironment].maxDistance = currentMaxDistance;
}

void DistanceProcessor::setAirAbsorption(float absorption)
{
    currentAirAbsorption = juce::jlimit(0.0f, 1.0f, absorption);
    // Update the current environment's air absorption
    environmentSettings[currentEnvironment].airAbsorptionCoeff = currentAirAbsorption;
}

void DistanceProcessor::setVolumeCompensation(float compensation)
{
    currentVolumeCompensation = juce::jlimit(0.0f, 1.0f, compensation);
}

void DistanceProcessor::setRoomWidth(float roomWidthMeters)
{
    currentRoomWidth = juce::jlimit(2.0f, 100.0f, roomWidthMeters);

    float widthFactor = juce::jlimit(0.5f, 1.5f, currentRoomWidth / 6.0f);

    environmentSettings[currentEnvironment].diffusion = juce::jlimit(0.1f, 1.0f, widthFactor);
    environmentSettings[currentEnvironment].reverbLevel = juce::jlimit(0.05f, 0.5f, widthFactor * 0.2f);

    if (advancedReverb)
    {
        advancedReverb->setDiffusion(environmentSettings[currentEnvironment].diffusion);
        float earlyLevel = juce::jlimit(0.05f, 0.4f, widthFactor * 0.2f);
        advancedReverb->setEarlyLevel(earlyLevel);
    }
}

void DistanceProcessor::setRoomHeight(float roomHeightMeters)
{
    currentRoomHeight = juce::jlimit(2.0f, 20.0f, roomHeightMeters);
    
    float heightFactor = juce::jlimit(0.5f, 3.0f, currentRoomHeight / 3.0f);

    float roomSizeFactor = juce::jlimit(0.5f, 2.0f, heightFactor);
    environmentSettings[currentEnvironment].roomSize = roomSizeFactor;

    float decayTimeFactor = juce::jlimit(0.5f, 6.0f, heightFactor * 2.5f);
    environmentSettings[currentEnvironment].decayTime = decayTimeFactor;
    
    float heightReverbBoost = juce::jlimit(0.0f, 0.6f, (heightFactor - 0.5f) * 0.1f);
    environmentSettings[currentEnvironment].reverbLevel = juce::jmax(environmentSettings[currentEnvironment].reverbLevel, heightReverbBoost);
    
    float preDelay = juce::jlimit(2.0f, 80.0f, currentRoomHeight * 4.0f);

    float damping = juce::jlimit(0.2f, 0.8f, 1.0f - (heightFactor * 0.1f));
    environmentSettings[currentEnvironment].damping = damping;
    
    // Update reverb parameters if reverb is active
    if (advancedReverb)
    {
        advancedReverb->setSize(roomSizeFactor);
        advancedReverb->setDecayTime(decayTimeFactor);
        advancedReverb->setPreDelay(preDelay);
        advancedReverb->setDamping(damping);
    }
}

void DistanceProcessor::setRoomLength(float roomLengthMeters)
{
    currentRoomLength = juce::jlimit(2.0f, 100.0f, roomLengthMeters);
    
    float lengthFactor = juce::jlimit(0.5f, 3.0f, currentRoomLength / 10.0f);
    float sizeMultiplier = juce::jlimit(0.7f, 1.5f, lengthFactor);
    environmentSettings[currentEnvironment].roomSize *= sizeMultiplier;
    
    float lateReverbLevel = juce::jlimit(0.1f, 0.6f, lengthFactor * 0.1f);
    
    // DRAMATIC frequency response - longer rooms have more low-end buildup
    
    // Update reverb parameters if reverb is active
    if (advancedReverb)
    {
        advancedReverb->setSize(environmentSettings[currentEnvironment].roomSize);
        advancedReverb->setLateLevel(lateReverbLevel);
        
        // Set early reflections pattern based on length
        if (lengthFactor > 2.0f)
            advancedReverb->setEarlyLevel(0.3f);
        else if (lengthFactor > 1.0f)
            advancedReverb->setEarlyLevel(0.2f);
        else
            advancedReverb->setEarlyLevel(0.1f);
    }
}

void DistanceProcessor::setTemperature(float temperatureCelsius)
{
    currentTemperature = juce::jlimit(-40.0f, 60.0f, temperatureCelsius);
    // Standard speed of sound calculation
    float adjustedSpeedOfSound = 331.3f * std::sqrt(1.0f + currentTemperature / 273.15f);
    speedOfSound = juce::jlimit(330.0f, 360.0f, adjustedSpeedOfSound);
    
    // Temperature also affects reverb characteristics for additional effect
    if (advancedReverb)
    {
        // Cold air → slightly darker; hot air → slightly brighter
        float temperatureNorm  = juce::jlimit(-1.0f, 1.0f, currentTemperature / 50.0f); // −1..+1 for −50..50 °C

        const float baseHighCut = 15000.0f;
        float  highCutShift     = temperatureNorm * 5000.0f; // ±5 kHz
        float  newHighCut       = juce::jlimit(5000.0f, 20000.0f, baseHighCut + highCutShift);

        advancedReverb->setHighCut(newHighCut);

        // Mild damping variation
        float tempDamping = juce::jlimit(0.2f, 0.8f, 0.7f - temperatureNorm * 0.2f);
        environmentSettings[currentEnvironment].damping = tempDamping;
        advancedReverb->setDamping(tempDamping);

        // Slight wet level variation
        float baselineLate = environmentSettings[currentEnvironment].reverbLevel;
        float tempWetMult  = juce::jlimit(0.5f, 1.5f, 1.0f + temperatureNorm * 0.5f);
        advancedReverb->setLateLevel(juce::jlimit(0.0f, 1.0f, baselineLate * tempWetMult));
    }
}

void DistanceProcessor::setSourceHeight(float heightPercent)
{
    currentHeightPercent = juce::jlimit(0.0f, 1.0f, heightPercent);
    smoothedHeight.setTargetValue(currentHeightPercent);
}

void DistanceProcessor::setClarity(float clarity)
{
    currentClarity = juce::jlimit(0.0f, 1.0f, clarity);
    smoothedClarity.setTargetValue(currentClarity);
}

//==============================================================================
void DistanceProcessor::updateHrirFilters(float azDeg, float elDeg)
{
    // Smooth interpolation threshold - update more frequently for natural movement
    if (std::abs(azDeg - lastAzimuthDeg) < 0.5f && std::abs(elDeg - lastElevationDeg) < 0.5f)
        return; // Reduced threshold for smoother updates

    lastAzimuthDeg   = azDeg;
    lastElevationDeg = elDeg;

    // DearVR-style HRTF interpolation
    // Instead of nearest neighbor, interpolate between 4 surrounding positions
    
    // Find surrounding grid positions
    float azStep = 15.0f; // Typical HRTF database resolution
    float elStep = 15.0f;
    
    float azLow = std::floor(azDeg / azStep) * azStep;
    float azHigh = azLow + azStep;
    float elLow = std::floor(elDeg / elStep) * elStep;
    float elHigh = elLow + elStep;
    
    // Interpolation weights
    float azWeight = (azDeg - azLow) / azStep;
    float elWeight = (elDeg - elLow) / elStep;
    
    // Get 4 corner HRIRs
    auto hrir00 = hrirDatabase.getNearestHrir(azLow, elLow);   // Bottom-left
    auto hrir01 = hrirDatabase.getNearestHrir(azLow, elHigh);  // Top-left
    auto hrir10 = hrirDatabase.getNearestHrir(azHigh, elLow);  // Bottom-right
    auto hrir11 = hrirDatabase.getNearestHrir(azHigh, elHigh); // Top-right
    
    // Bilinear interpolation
    const int len = std::max({(int)hrir00.left.size(), (int)hrir01.left.size(), 
                             (int)hrir10.left.size(), (int)hrir11.left.size()});
    
    if (len == 0) return;
    
    std::vector<float> interpLeft(len, 0.0f);
    std::vector<float> interpRight(len, 0.0f);
    
    for (int i = 0; i < len; ++i)
    {
        // Ensure we don't go out of bounds
        float h00L = (i < (int)hrir00.left.size()) ? hrir00.left[i] : 0.0f;
        float h01L = (i < (int)hrir01.left.size()) ? hrir01.left[i] : 0.0f;
        float h10L = (i < (int)hrir10.left.size()) ? hrir10.left[i] : 0.0f;
        float h11L = (i < (int)hrir11.left.size()) ? hrir11.left[i] : 0.0f;
        
        float h00R = (i < (int)hrir00.right.size()) ? hrir00.right[i] : 0.0f;
        float h01R = (i < (int)hrir01.right.size()) ? hrir01.right[i] : 0.0f;
        float h10R = (i < (int)hrir10.right.size()) ? hrir10.right[i] : 0.0f;
        float h11R = (i < (int)hrir11.right.size()) ? hrir11.right[i] : 0.0f;
        
        // Bilinear interpolation
        float topL = h01L * (1.0f - azWeight) + h11L * azWeight;
        float bottomL = h00L * (1.0f - azWeight) + h10L * azWeight;
        interpLeft[i] = bottomL * (1.0f - elWeight) + topL * elWeight;
        
        float topR = h01R * (1.0f - azWeight) + h11R * azWeight;
        float bottomR = h00R * (1.0f - azWeight) + h10R * azWeight;
        interpRight[i] = bottomR * (1.0f - elWeight) + topR * elWeight;
    }
    
    // Create buffers and load interpolated HRIRs
    auto makeBuffer = [len](const std::vector<float>& data) {
        juce::AudioBuffer<float> buf;
        buf.setSize(1, len);
        std::memcpy(buf.getWritePointer(0), data.data(), sizeof(float) * len);
        return buf;
    };

    hrtfLeft.loadImpulseResponse(makeBuffer(interpLeft), sampleRate,
                                 juce::dsp::Convolution::Stereo::no,
                                 juce::dsp::Convolution::Trim::no,
                                 juce::dsp::Convolution::Normalise::no);
    hrtfRight.loadImpulseResponse(makeBuffer(interpRight), sampleRate,
                                  juce::dsp::Convolution::Stereo::no,
                                  juce::dsp::Convolution::Trim::no,
                                  juce::dsp::Convolution::Normalise::no);
}

void DistanceProcessor::processHrtfConvolution(juce::AudioBuffer<float>& buffer)
{
    // DearVR-style HRTF processing with subtle crosstalk cancellation
    juce::dsp::AudioBlock<float> block (buffer);
    
    // Store original for crosstalk cancellation
    juce::AudioBuffer<float> originalBuffer;
    originalBuffer.makeCopyOf(buffer);
    
    // Process left channel
    auto blockL = block.getSingleChannelBlock (0);
    juce::dsp::ProcessContextReplacing<float> ctxL (blockL);
    hrtfLeft.process (ctxL);

    // Process right channel
    if (buffer.getNumChannels() > 1)
    {
        auto blockR = block.getSingleChannelBlock (1);
        juce::dsp::ProcessContextReplacing<float> ctxR (blockR);
        hrtfRight.process (ctxR);
    }
    
    // Apply subtle crosstalk cancellation for better externalization
    // This helps move the sound outside the head when using headphones
    if (buffer.getNumChannels() >= 2)
    {
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);
        
        // Crosstalk cancellation parameters
        const float crossfeedAmount = 0.15f; // Subtle amount
        const float crossfeedDelay = 0.0003f; // 0.3ms delay
        const int delaySamples = static_cast<int>(crossfeedDelay * sampleRate);
        
        // Simple delay-based crosstalk cancellation
        juce::AudioBuffer<float> crossfeedBuffer(2, delaySamples + buffer.getNumSamples());
        crossfeedBuffer.clear();
        
        const int numSamples = buffer.getNumSamples();
        
        for (int i = 0; i < numSamples; ++i)
        {
            // Get delayed crossfeed signals
            float delayedLeft = 0.0f;
            float delayedRight = 0.0f;
            
            if (i >= delaySamples)
            {
                delayedLeft = crossfeedBuffer.getSample(0, i - delaySamples);
                delayedRight = crossfeedBuffer.getSample(1, i - delaySamples);
            }
            
            // Store current samples for future crossfeed
            crossfeedBuffer.setSample(0, i, left[i]);
            crossfeedBuffer.setSample(1, i, right[i]);
            
            // Apply inverted crossfeed (cancellation)
            left[i] -= delayedRight * crossfeedAmount;
            right[i] -= delayedLeft * crossfeedAmount;
            
            // Add subtle direct crossfeed for natural sound
            left[i] += right[i] * crossfeedAmount * 0.3f;
            right[i] += left[i] * crossfeedAmount * 0.3f;
        }
        
    }
}

 