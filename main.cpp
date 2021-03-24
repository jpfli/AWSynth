
#include <cstdint>
#include <Pokitto.h>
#include "AWSynthSource.h"
#include "SimpleTuneAW.h"

struct RingMod {
    // Callback member function demonstrating ring modulation, pitch slide and vibrato
    std::int32_t callback(std::uint32_t t, std::uint32_t p) {
        using AWSynth = Audio::AWSynthSource;
        
        std::int32_t slide = AWSynth::ramp<800>(800-t); // Ramp that falls from 1<<14 at t=0 down to zero at t=800
        std::int32_t vibrato = AWSynth::lfo<2000>(t);   // Low frequency sine wave that has period of 2000 ticks
        
        // Slide and vibrato are scaled and added to the phase variable. Linear ramp must be squared to get 
        // pitch slide instead of just constant pitch shift.
        p += -(slide*slide >> 18) + vibrato/(1<<8);
        
        std::int32_t o1 = AWSynth::sin(5*p/4);  // 1.25 times the note pitch
        std::int32_t o2 = AWSynth::sin(p/4);    // 0.25 times the note pitch
        
        // _modlvl is used to interpolate between plain o1 and product o1*o2
        return (_modlvl*o1*o2/(1<<7) + (256-_modlvl)*o1) / (1<<8);
    }
    
    std::int32_t _modlvl = 0;
    void modLevel(std::int32_t lvl) { _modlvl = lvl; }
    std::int32_t modLevel() const { return _modlvl; }
};

// Converts level (0...100 %) to gain (Q10 fixed point). You can use the exact same operator output levels
// as in the FM Synth program.
constexpr std::int32_t fmGain(std::int32_t level) {
    constexpr const std::int32_t PERCENT_Q15 = 0.01 * (1<<15);
    std::int32_t lvl_Q15 = level * PERCENT_Q15;
    std::int32_t gain_Q10 = 9 * ((lvl_Q15*lvl_Q15) >> (1+5+15));
    return gain_Q10;
}

// Three operator frequency modulation with feedback
std::int32_t FreqModCallback(std::uint32_t t, std::uint32_t p) {
    static std::int32_t fb = 0;                                 // Static variable to store feeback
    
    using AWSynth = Audio::AWSynthSource;
    
    // Operator 3
    constexpr const std::int32_t R3 = 1;                        // Pitch ratio 1
    constexpr const std::int32_t A3_Q10 = fmGain(25);           // Output level 25%
    std::int32_t o3 = AWSynth::sin(p*R3)*A3_Q10/(1<<10);        // Generate osc 3 output
    
    // Operator 2
    constexpr const std::int32_t R2 = 3;                        // Pitch ratio 3
    constexpr const std::int32_t A2_Q10 = fmGain(45);           // Output level 45%
    std::int32_t o2 = AWSynth::sin(p*R2 + fb)*A2_Q10/(1<<10);   // Use feedback to modulate osc 2
    std::int32_t env2 = AWSynth::ramp<1600>(1600-t);            // Decay envelope for osc 2
    o2 = o2 * ((env2*env2)>>14) / (1<<14);                      // Squared envelope gives nicer result
    
    // Operator 1
    constexpr const std::int32_t R1 = 1;                        // Pitch ratio 1
    std::int32_t o1 = AWSynth::sin(p*R1 + o2+o3);               // Use osc 2 and osc 3 to modulate osc 1
    
    // Feedback
    constexpr const std::int32_t AFB_Q10 = fmGain(25);          // Feedback level 25 %
    fb = o1*AFB_Q10/(1<<10);                                    // Store output of osc 1 to feedback
    
    return o1;
}

constexpr char* EXAMPLE_CASES[] = {"ARCADE", "SYNTHESIS", "BYTEBEAT"};

int main() {
    using Pokitto::Core;
    using Pokitto::Display;
    using Pokitto::Buttons;
    
    using AWSynth = Audio::AWSynthSource;
    using AWPatch = Audio::AWPatch;
    
    std::uint8_t state = 0;
    
    auto arp_patch = AWPatch([](std::uint32_t t, std::uint32_t p)->std::int32_t {  // Lambda callback function
            return AWSynth::sqr(p);         // Square wave generator
        })
        .volume(80).step(6).release(12)     // Volume 80%, step duration 6*8.33ms, release 12*step
        .amplitudes(AWPatch::Envelope(31,31,31,31).loop(32,4))                  // Length 4 steps, then loop (jump) to end
        .semitones(AWPatch::Envelope(   0, 12,  4,  7).smooth(false).loop(0,4));    // Use discrete pitches to create an arpeggio
    
    constexpr auto arp_tune = SIMPLE_TUNE_AW(A-3,A-3,G-3,E-4,E-4,D-4,D-4,A-3,A-3).tempo(120*8);     // Tempo 120, use 8th notes
    
    auto jump_patch = AWPatch([](std::uint32_t t, std::uint32_t p)->std::int32_t {
            return AWSynth::sqr(p);
        })
        .volume(80).step(4).release(0)      // Volume 80%, step duration 5*8.33ms, instant release
        .amplitudes(AWPatch::Envelope(31,31,31,29,28,26,24,22,20,18,16,14,12, 0).loop(32,14))                   // Length 14 steps, then jump to end
        .semitones(AWPatch::Envelope(   0,  0,  2, 4, 6, 8,10,12,14,16,18,20,22,24).smooth(true).loop(13,14));  // Smooth pitch slide
    
    auto powerup_patch = AWPatch([](std::uint32_t t, std::uint32_t p)->std::int32_t {
            return AWSynth::sqr(p);
        })
        .volume(80).step(5).release(0)
        .amplitudes(AWPatch::Envelope(31,31,31,31,31,31,31,31,31,31,31,31).loop(32,12))
        .semitones(AWPatch::Envelope(   0,  7,  8,  1,  8,  9, 2, 9,10, 3,10,11).smooth(false).loop(11,12));
    
    RingMod ringmod = RingMod();                                    // Create an instance of RingMod class
    auto* ringmod_cb = AWPatch::makeCallback(&RingMod::callback);   // Turn member function into a regular function pointer
    
    auto ringmod_patch = AWPatch(ringmod_cb, ringmod)               // Pass the callback pointer, and an instance of the class as user data
        .volume(80).step(4).glide(10).release(20)
        .amplitudes(AWPatch::Envelope(31,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16).loop(16,17));  // Repeat last value as long as note is played
    
    constexpr auto ringmod_tune = SIMPLE_TUNE_AW(A-4,X, C-5,X, C#5,D#5*4,X, D-5,X, C-5,X, C-5,D-5*3,X, C-5,X, G-4,A-4*7, A-2).tempo(120*16);
    
    auto fm_patch = AWPatch(FreqModCallback)                        // Regular function callback
        .volume(80).step(4).release(6)
        .amplitudes(AWPatch::Envelope(29,24,15,24,29,31,31,30,29,27,26,24,23,21,20,18).smooth(true))
        .semitones(AWPatch::Envelope(24,20,16,12, 8, 4, 0,-3,-6,-9,-12,-18,-16,-18,-22,-24).smooth(true));
    
    static constexpr std::uint32_t P1_VALUES[] = {1, 36, 84, 216};
    std::uint32_t parambeat_idx = 0;
    std::uint32_t parambeat_p1 = P1_VALUES[0];          // Variable passed to the callback dunction as user data
    
    auto parambeat_patch = AWPatch([](std::uint32_t t, std::uint32_t p, void* data)->std::int32_t { // Lambda callback function with user data
            std::uint32_t& p1 = *reinterpret_cast<std::uint32_t*>(data);
            std::int32_t o = ((p1*t)>>4)|(t>>5)|t;      // Evaluate bytebeat equation using current value of p1
            return (o&255) - 128;                       // Bytebeat result must be truncated to 8-bits and converted to signed value
        }, parambeat_p1)
        .volume(80).step(4).release(75)
        .amplitudes(AWPatch::Envelope(0,100).loop(1,2));
    
    auto sinebeat_patch = AWPatch([](std::uint32_t t, std::uint32_t p)->std::int32_t {
            std::uint32_t b = t*((t>>9|t>>13)&25&t>>6); // Evaluate bytebeat equation
            return AWSynth::sin(b);                     // Use as phase input for sine wave generator
        })
        .volume(80).step(4).release(25)
        .amplitudes(AWPatch::Envelope(0,100).loop(1,2));
    
    auto bytebeat_patch = AWPatch([](std::uint32_t t, std::uint32_t p)->std::int32_t {
            std::uint32_t b = t*((t>>9|t>>13)&25&t>>6); // Evaluate bytebeat equation
            return (b&255) - 128;                       // Truncate to 8 bits and convert to signed value
        })
        .volume(80).step(4).release(25)
        .amplitudes(AWPatch::Envelope(0,100).loop(1,2));
    
    AWSynth* snd1 = nullptr;
    AWSynth* snd2 = nullptr;
    AWSynth* snd3 = nullptr;
    
    Core::begin();
    while(Core::isRunning()) {
        if(!Core::update()) {
            continue;
        }
        
        switch(state) {
            case 0:     // Pure square wave examples
                if(Buttons::pressed(BTN_A)) {
                    Audio::playTuneAW<0>(arp_tune).patch(arp_patch);
                }
                if(Buttons::pressed(BTN_B)) {
                    AWSynth::play<1>(jump_patch, 61);
                }
                if(Buttons::pressed(BTN_C)) {
                    AWSynth::play<2>(powerup_patch, 62);
                }
                break;
            
            case 1:     // Ring modulation and frequency modulation examples
                if(Buttons::pressed(BTN_A)) {
                    Audio::playTuneAW<0>(ringmod_tune).patch(ringmod_patch);
                }
                if(Buttons::pressed(BTN_B)) {
                    AWSynth::play<1>(fm_patch, 72);
                }
                if(Buttons::pressed(BTN_C)) {
                    AWSynth::play<2>(fm_patch, 60);
                }
                
                if(Buttons::pressed(BTN_UP)) {
                    ringmod.modLevel(ringmod.modLevel()<224 ? ringmod.modLevel()+32 : 256);
                }
                if(Buttons::pressed(BTN_DOWN)) {
                    ringmod.modLevel(ringmod.modLevel()>32 ? ringmod.modLevel()-32 : 0);
                }
                break;
            
            case 2:     // Bytebeat examples
                if(Buttons::pressed(BTN_A)) {
                    snd1 = &AWSynth::play<0>(parambeat_patch);
                }
                else if(Buttons::released(BTN_A)) {
                    if(snd1) snd1->release();
                    snd1 = nullptr;
                }
                if(Buttons::pressed(BTN_B)) {
                    snd2 = &AWSynth::play<1>(sinebeat_patch);
                }
                else if(Buttons::released(BTN_B)) {
                    if(snd2) snd2->release();
                    snd2 = nullptr;
                }
                if(Buttons::pressed(BTN_C)) {
                    snd3 = &AWSynth::play<2>(bytebeat_patch);
                }
                else if(Buttons::released(BTN_C)) {
                    if(snd3) snd3->release();
                    snd3 = nullptr;
                }
                
                if(Buttons::pressed(BTN_UP)) {
                    parambeat_idx = parambeat_idx<3 ? parambeat_idx+1 : 3;
                    parambeat_p1 = P1_VALUES[parambeat_idx];
                }
                if(Buttons::pressed(BTN_DOWN)) {
                    parambeat_idx = parambeat_idx>0 ? parambeat_idx-1 : 0;
                    parambeat_p1 = P1_VALUES[parambeat_idx];
                }
                break;
        }
        
        if(Buttons::pressed(BTN_RIGHT)) {
            state = state<2 ? state+1 : 0;      // Switch to previous example
        }
        if(Buttons::pressed(BTN_LEFT)) {
            state = state>0 ? state-1 : 2;      // Switch to next example
        }
        
        Display::clear();
        Display::print("\nEXAMPLE:\n");
        Display::print(EXAMPLE_CASES[state]);
    }
    
    return 0;
}
