#pragma once

#include <cstdint>
#include <LibAudio>

namespace Audio {

struct AWPatch {
    constexpr AWPatch(std::int32_t (*callback)(std::uint32_t t, std::uint32_t p)) : _callback(callback), _data(nullptr) {}
    
    template<typename T>
    constexpr AWPatch(std::int32_t (*callback)(std::uint32_t t, std::uint32_t p, void* data), T& obj) : _callback_with_data(callback), _data(reinterpret_cast<void*>(&obj)) {}
    
    union {
        std::int32_t (*_callback)(std::uint32_t, std::uint32_t);
        std::int32_t (*_callback_with_data)(std::uint32_t, std::uint32_t, void* ptr);
    };
    void* _data = nullptr;
    
    constexpr AWPatch& algorithm(std::int32_t (*callback)(std::uint32_t t, std::uint32_t p)) {
        _callback = callback;
        _data = nullptr;
        return *this;
    }
    
    template<typename T>
    constexpr AWPatch& algorithm(std::int32_t (*callback)(std::uint32_t t, std::uint32_t p, void* data), T& obj) {
        _callback_with_data = callback;
        _data = reinterpret_cast<void*>(&obj);
        return *this;
    }
    
    std::uint8_t _volume = 100;
    constexpr AWPatch& volume(std::uint8_t val) { _volume = val < 100 ? val : 100; return *this; }
    constexpr std::uint8_t volume() const { return _volume; }
    
    std::uint8_t _step = 1;
    constexpr AWPatch& step(uint8_t val) { _step = val>0 ? val : 1; return *this; }
    constexpr std::uint8_t step() const { return _step; }
    
    std::uint8_t _glide = 0;
    constexpr AWPatch& glide(uint8_t val) { _glide = val; return *this; }
    constexpr std::uint8_t glide() const { return _glide; }
    
    std::uint8_t _release = 0;
    constexpr AWPatch& release(uint8_t val) { _release = val; return *this; }
    constexpr std::uint8_t release() const { return _release; }
    
    struct Envelope {
        static constexpr std::uint32_t SIZE = 32;
        
        std::int8_t _data[SIZE];
        
        constexpr Envelope(const std::int8_t (&array)[SIZE]) : _data{} {
            for(std::uint32_t idx=0; idx<SIZE; ++idx) {
                std::int8_t val = (array[idx] < 32) ? ((array[idx] >= -32) ? array[idx] : -32) : 31;
                _data[idx] = static_cast<std::uint8_t>(val)<<2;
            }
        }
        
        template<typename... Args>
        constexpr explicit Envelope(const Args&... args) : _data{} {
            const std::int8_t array[SIZE] = {args...};
            for(std::uint32_t idx=0; idx<SIZE; ++idx) {
                std::int8_t val = (array[idx] < 32) ? ((array[idx] >= -32) ? array[idx] : -32) : 31;
                _data[idx] = static_cast<std::uint8_t>(val)<<2;
            }
        }
        
        constexpr const std::int8_t* data() const { return _data; }
        constexpr std::uint32_t size() const { return SIZE; }
        
        constexpr Envelope& smooth(bool val) {
            for(std::uint32_t idx=0; idx<SIZE; ++idx) {
                _data[idx] = (_data[idx]&(-4)) | (val ? 3 : 0);
            }
            return *this;
        }
        
        template<typename... Args>
        constexpr Envelope& effects(const Args&... args) {
            const std::uint8_t array[SIZE] = {args...};
            for(std::uint32_t idx=0; idx<SIZE; ++idx) {
                std::uint8_t effect = (array[idx] < 4) ? array[idx] : 0;
                _data[idx] = (_data[idx]&(-4)) | effect;
            }
            return *this;
        }
        
        std::uint8_t _loop = 0;
        std::uint8_t _end = 0;
        constexpr Envelope& loop(uint8_t loop, uint8_t end) { _loop = loop; _end = end; return *this; }
        constexpr std::uint8_t loop() const { return _loop; }
        constexpr std::uint8_t end() const { return _end; }
    };
    
    Envelope _amplitude_env = Envelope(124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124);
    constexpr AWPatch& amplitudes(const Envelope& env) { _amplitude_env = env; return *this; }
    constexpr const Envelope& amplitudes() const { return _amplitude_env; }
    
    Envelope _semitone_env;
    constexpr AWPatch& semitones(const Envelope& env) { _semitone_env = env; return *this; }
    constexpr const Envelope& semitones() const { return _semitone_env; }
    
    // Takes a class member function and wraps it into a regular function pointer
    template<typename T>
    static auto makeCallback(std::int32_t (T::*method)(std::uint32_t t, std::uint32_t p)) {
        static decltype(method) static_ptr;
        static_ptr = method;
        
        return +[](std::uint32_t t_, std::uint32_t p_, void* data)->std::int32_t {
            T& obj = *reinterpret_cast<T*>(data);
            return (obj.*static_ptr)(t_, p_);
        };
    }
};

class AWSynthSource {
    
    public:
    
        template<unsigned channel>
        static AWSynthSource& getInstance() { static AWSynthSource self; return self; }
        
        template<unsigned channel=0, bool lowLatency=true>
        static AWSynthSource& play(const AWPatch& patch, std::uint8_t midikey=48) {
            auto& self = getInstance<channel>();
            self.init(patch, midikey);
            
            if(patch._data == nullptr) {
                // Plain callback function, no user data
                self._callback = patch._callback;
                self._data = nullptr;
                
                if(lowLatency) {
                    lowLatencyMix<channel>(self);
                }
                
                Audio::connect(channel, &self, channel == 0 ? copy : mix<channel>);
            }
            else {
                // Callback function with user data
                self._callback_with_data = patch._callback_with_data;
                self._data = patch._data;
                
                if(lowLatency) {
                    lowLatencyMixAlt<channel>(self);
                }
                
                Audio::connect(channel, &self, channel == 0 ? copyAlt : mixAlt<channel>);
            }
            
            return self;
        }
        
        inline void release() {
            _released = true;
        }
    
    public:
    
        // Waveform generators turn input variable 'p' (phase, 256 is full period) into a waveform.
        // Output is in the range -128..128
        
        // Sine wave, triangluar wave and square wave generators
        static constexpr std::int32_t sin(std::uint32_t p) { std::int32_t o = ((p&127) * (-p&127)) >> 5; return p&128 ? -o : o; }
        static constexpr std::int32_t tri(std::uint32_t p) { std::int32_t o = (2*(p+64)) & 255; return (p+64)&128 ? 128-o : o-128; }
        static constexpr std::int32_t sqr(std::uint32_t p) { return (p&128) ? -128 : 128; }
        
        // Pulse wave generator with variable pulse width 'w'
        static constexpr std::int32_t sqr(std::uint32_t p, std::uint32_t w) { return (p&255) < w ? 128 : -128; }
        
        // Sawtooth or modified triangle wave generator. Optional template parameter 'XPeak' defines the x location (0 to 256) of the peak.
        // XPeak values 0 and 256 (default) give a sawtooth wave. XPeak=128 gives a symmetric triangle wave, similar to 'tri()'.
        template<signed XPeak=256>
        static constexpr std::int32_t saw(std::uint32_t p) {
            static_assert(XPeak>=0 && XPeak<=256);
            constexpr std::int32_t RATE_A = XPeak>1 ? ((1<<20)/XPeak) : (1<<20);
            constexpr std::int32_t RATE_B = XPeak<255 ? ((1<<20)/(256-XPeak)) : (1<<20);
            std::int32_t o = (p+XPeak/2) & 255;
            return ((o<XPeak ? o*RATE_A : (256-o)*RATE_B) >> 12) - 128;
        }
        
        // Noise generator
        static inline std::int32_t noise(std::uint32_t p) {
            std::uint32_t z = (p>>7)^0x5DEECE66;
            return (((1664525*z + 1013904223)>>12) & 255) - 128;
        }
        
        // Envelope generator takes input variable 't' (ticks) and template parameter 'T' (duration).
        // Returned value is 0 when t<=0, 1<<14 when t>=T and increases linearly when 0<t<T
        template<signed T>
        static constexpr std::int32_t ramp(std::int32_t t) {
            static_assert(T>0 && T<=(1<<30));
            return t<=0 ? 0 : (t>=T ? (1<<14) : (t * ((1<<(14+16))/T) >> 16));
        }
        
        // LFO is a sine wave generator that takes input variable 't' (ticks) and template parameter 'T' (full period).
        // Output is in the range -1<<14..1<<14 for greater resolution
        template<signed T>
        static constexpr std::int32_t lfo(std::uint32_t t) {
            t = t*(((1<<(14+16))+T-1)/T) >> 16;
            std::uint32_t o = ((t&8191)*(-t&8191)) >> 10;
            return t&8192 ? -o : o;
        }
    
    private:
    
        enum struct Effect { STEP=0, ATTACK=1, DECAY=2, SLIDE=3 };
        
        static constexpr std::uint32_t _RATE_1HZ_Q32 = (static_cast<std::uint64_t>(1) << 32) / POK_AUD_FREQ;
        
        static constexpr std::int32_t _ONE_Q20 = 1<<20;
        static constexpr std::int32_t _ONE_Q24 = 1<<24;
        
        // Control values are updated 120 times per second
        static constexpr std::uint32_t _CV_RATE_Q20 = (240*_ONE_Q20 + POK_AUD_FREQ-1) / POK_AUD_FREQ;
        
        static constexpr std::int32_t _LEVEL_SCALE_Q10 = (1<<5);                    // Scales amplitude level to fixed point Q10
        static constexpr std::int32_t _SEMITONE_SCALE_Q15 = ((1<<15) + 12-1) / 12;  // Scales semitones to octaves as fixed point Q15
        
        static constexpr std::uint16_t _SEMITONES_Q15[13] = {
            static_cast<std::uint16_t>((1<<15) * 0.50000), 
            static_cast<std::uint16_t>((1<<15) * 0.52973), 
            static_cast<std::uint16_t>((1<<15) * 0.56123), 
            static_cast<std::uint16_t>((1<<15) * 0.59460), 
            static_cast<std::uint16_t>((1<<15) * 0.62996), 
            static_cast<std::uint16_t>((1<<15) * 0.66742), 
            static_cast<std::uint16_t>((1<<15) * 0.70711), 
            static_cast<std::uint16_t>((1<<15) * 0.74915), 
            static_cast<std::uint16_t>((1<<15) * 0.79370), 
            static_cast<std::uint16_t>((1<<15) * 0.84090), 
            static_cast<std::uint16_t>((1<<15) * 0.89090), 
            static_cast<std::uint16_t>((1<<15) * 0.94387), 
            static_cast<std::uint16_t>((1<<15) * 1.00000)
        };
        
        constexpr explicit AWSynthSource() :
            _t(0), _p(0), 
            _rate_Q24(0), _phase_Q24(0),
            _cv_accu_Q20(0), _step_rate_Q24(0), _step_accu_Q24(0),_step_div_Q24(0), 
            _levels(nullptr), _levels_idx(0), _levels_loop(0), _levels_end(0), _base_level_Q10(0), _delta_level_Q10(0),
            _semitones(nullptr), _semitones_idx(0), _semitones_loop(0), _semitones_end(0), _base_pitchbend_Q10(0), _delta_pitchbend_Q10(0),
            _target_gain_Q10(0), _delta_gain_Q10(0),
            _release_rate_Q14(0), _volume_Q14(0),
            _glide_interval_Q10(0), _glide_rate_Q14(0), _glide_accu_Q14(0),
            _midikey(0), _released(true), 
            _callback(nullptr),
            _data(nullptr)
        {
        }
        
        inline void init(const AWPatch& patch, std::uint8_t midikey) {
            if(patch.glide() > 0 && !_released) {
                // Calculate remaining glide interval in case the current patch hasn't finished it's pitch glide
                _glide_interval_Q10 = _glide_interval_Q10*_glide_accu_Q14 / (1<<14);
                
                // Then add interval from this to previous midikey
                _glide_interval_Q10 += (1<<10) * (_midikey-midikey) / 12;
                _glide_rate_Q14 = (1<<14) / (patch.glide()*patch.step());
                _glide_accu_Q14 = 1<<14;   // Envelope starts from full glide interval and glides down to zero to current pitch
            }
            else {
                _t = 0;
                _p = 0;
                
                _cv_accu_Q20 = _ONE_Q20;
                
                _step_rate_Q24 = ((_CV_RATE_Q20<<4) + patch.step()-1) / (2*patch.step());
                _step_div_Q24 = _ONE_Q24 / (2*patch.step());
                _step_accu_Q24 = 0;
                
                _volume_Q14 = (patch.volume() * ((1<<30) / 100)) >> 16;
                _release_rate_Q14 = patch.release() > 0 ? (_volume_Q14 / (patch.release()*2*patch.step())) : _volume_Q14;
                
                _levels = patch.amplitudes().data();
                _levels_loop = patch.amplitudes().loop();
                _levels_end = patch.amplitudes().end();
                _levels_idx = 0;
                
                _base_level_Q10 = (_levels[0]>>2)*_LEVEL_SCALE_Q10;
                _delta_level_Q10 = 0;
                
                switch(static_cast<Effect>(_levels[0]&3)) {
                    case Effect::ATTACK:
                        _delta_level_Q10 = _base_level_Q10;
                        _base_level_Q10 = 0;
                        break;
                    
                    case Effect::DECAY:
                        _delta_level_Q10 = -_base_level_Q10;
                        break;
                    
                    default:
                        break;
                }
                
                std::int32_t level_Q10 = _base_level_Q10 + _delta_level_Q10*(_step_div_Q24>>4)/_ONE_Q20;
                _target_gain_Q10 = levelToGain((_volume_Q14*level_Q10) >> 14);
                _delta_gain_Q10 = _target_gain_Q10 - 0;
                
                _semitones = patch.semitones().data();
                _semitones_loop = patch.semitones().loop();
                _semitones_end = patch.semitones().end();
                _semitones_idx = 0;
                
                _base_pitchbend_Q10 = ((_semitones[0]>>2)*_SEMITONE_SCALE_Q15)>>5;
                _delta_pitchbend_Q10 = 0;
                
                switch(static_cast<Effect>(_semitones[0]&3)) {
                    case Effect::ATTACK:
                        _delta_pitchbend_Q10 = _base_pitchbend_Q10;
                        _base_pitchbend_Q10 = 0;
                        break;
                    
                    case Effect::DECAY:
                        _delta_pitchbend_Q10 = -_base_pitchbend_Q10;
                        break;
                    
                    default:
                        break;
                }
                
                std::int32_t pitchbend_Q15 = (_base_pitchbend_Q10<<5) + _delta_pitchbend_Q10*(_step_div_Q24>>(1+9))/(1<<10);
                _rate_Q24 = (static_cast<std::uint64_t>(_RATE_1HZ_Q32) * 440 * pow2((midikey-69)*_SEMITONE_SCALE_Q15 + pitchbend_Q15)) >> (8+15);
                _phase_Q24 = 0;
            
                _glide_interval_Q10 = 0;
                _glide_rate_Q14 = 0;
                _glide_accu_Q14 = 0;
            }
            
            _midikey = midikey;
            _released = false;
        }
        
        inline void update() {
            _p += _phase_Q24>>(24-8);
            _phase_Q24 &= (1<<(24-8)) - 1;
            
            if(_released) {
                _volume_Q14 -= _release_rate_Q14;
                if(_volume_Q14 <= 0) {
                    _volume_Q14 = 0;
                }
            }
            
            std::int32_t level_Q10 = _base_level_Q10;
            std::int32_t pitchbend_Q15 = _base_pitchbend_Q10<<5;
            
            if(_step_accu_Q24+_step_div_Q24 >= _ONE_Q24) {
                _step_accu_Q24 = -_step_div_Q24;
                _t += 256;
                
                level_Q10 += _delta_level_Q10;
                _delta_level_Q10 = 0;
                
                std::uint32_t len = (_levels_end > 0 && _levels_end < AWPatch::Envelope::SIZE) ? _levels_end : AWPatch::Envelope::SIZE;
                if(_levels_idx < len) {
                    ++_levels_idx;
                    if(_levels_idx >= len) {
                        _levels_idx = (_levels_loop < _levels_end) ? _levels_loop : AWPatch::Envelope::SIZE;
                    }
                    
                    if(_levels_idx < AWPatch::Envelope::SIZE) {
                        _base_level_Q10 = (_levels[_levels_idx]>>2)*_LEVEL_SCALE_Q10;
                        
                        switch(static_cast<Effect>(_levels[_levels_idx]&3)) {
                            case Effect::ATTACK:
                                _delta_level_Q10 = _base_level_Q10;
                                _base_level_Q10 = 0;
                                break;
                            
                            case Effect::DECAY:
                                _delta_level_Q10 = -_base_level_Q10;
                                break;
                            
                            case Effect::SLIDE:
                                _delta_level_Q10 = _base_level_Q10 - level_Q10;
                                _base_level_Q10 = level_Q10;
                                break;
                            
                            default:
                                break;
                        }
                        
                        if(_base_level_Q10 < level_Q10) {
                            level_Q10 = _base_level_Q10;
                        }
                    }
                    else {
                        _base_level_Q10 = level_Q10;
                        if(!_released) {
                            release();  // Trigger release when we reach the end of the level envelope
                            if(_release_rate_Q14 >= _volume_Q14) {
                                _volume_Q14 = 0;
                            }
                        }
                    }
                }
                
                pitchbend_Q15 += (_delta_pitchbend_Q10<<5) - _delta_pitchbend_Q10*(_step_div_Q24>>(1+9))/(1<<10);
                
                _base_pitchbend_Q10 += _delta_pitchbend_Q10;
                _delta_pitchbend_Q10 = 0;
                
                len = (_semitones_end > 0 && _semitones_end < AWPatch::Envelope::SIZE) ? _semitones_end : AWPatch::Envelope::SIZE;
                if(_semitones_idx < len) {
                    ++_semitones_idx;
                    if(_semitones_idx >= len) {
                        _semitones_idx = (_semitones_loop < _semitones_end) ? _semitones_loop : AWPatch::Envelope::SIZE;
                    }
                    
                    if(_semitones_idx < AWPatch::Envelope::SIZE) {
                        std::int32_t next_pitchbend_Q10 = ((_semitones[_semitones_idx]>>2)*_SEMITONE_SCALE_Q15)>>5;
                        
                        switch(static_cast<Effect>(_semitones[_semitones_idx]&3)) {
                            case Effect::STEP:
                                _base_pitchbend_Q10 = next_pitchbend_Q10;
                                break;
                            
                            case Effect::ATTACK:
                                _delta_pitchbend_Q10 = next_pitchbend_Q10;
                                _base_pitchbend_Q10 = 0;
                                break;
                            
                            case Effect::DECAY:
                                _delta_pitchbend_Q10 = -next_pitchbend_Q10;
                                _base_pitchbend_Q10 = next_pitchbend_Q10;
                                break;
                            
                            case Effect::SLIDE:
                                _delta_pitchbend_Q10 = next_pitchbend_Q10 - _base_pitchbend_Q10;
                                break;
                            
                            default:
                                break;
                        }
                    }
                }
            }
            else {
                level_Q10 += _delta_level_Q10*((_step_accu_Q24 + _step_div_Q24)>>4)/_ONE_Q20;
                pitchbend_Q15 += _delta_pitchbend_Q10*((_step_accu_Q24 + (_step_div_Q24>>1))>>9)/(1<<10);
            }
            
            std::int32_t prev_gain_Q10 = _target_gain_Q10;
            _target_gain_Q10 = levelToGain((_volume_Q14*level_Q10) >> 14);
            _delta_gain_Q10 = _target_gain_Q10 - prev_gain_Q10;
            
            if(_glide_accu_Q14 > 0) {
                _glide_accu_Q14 -= _glide_rate_Q14;
                if(_glide_accu_Q14 > 0) {
                    pitchbend_Q15 += (_glide_interval_Q10*_glide_accu_Q14) / (1<<9);
                }
                else {
                    _glide_accu_Q14 = 0;
                }
            }
            
            _rate_Q24 = (static_cast<std::uint64_t>(_RATE_1HZ_Q32) * 440 * pow2((_midikey-69)*_SEMITONE_SCALE_Q15 + pitchbend_Q15)) >> (8+15);
        }
        
        inline std::uint8_t tick() {
            std::int32_t gain_Q10 = _target_gain_Q10 - _delta_gain_Q10*_cv_accu_Q20 / _ONE_Q20;
            std::int32_t val = _callback(_t+(_step_accu_Q24>>16), _p+(_phase_Q24>>16)) * gain_Q10 / (1<<10);
            
            _step_accu_Q24 += _step_rate_Q24;
            _phase_Q24 += _rate_Q24;
            
            _cv_accu_Q20 -= _CV_RATE_Q20;
            if(_cv_accu_Q20 <= 0) {
                _cv_accu_Q20 = _ONE_Q20;
                update();
            }
            
            val = val > -128 ? (val < 127 ? val : 127) : -128;  // Clip to 8-bits
            return val + 128;                                   // Convert to unsigned value
        }
        
        inline std::uint8_t tickAlt() {
            std::int32_t gain_Q10 = _target_gain_Q10 - _delta_gain_Q10*_cv_accu_Q20 / _ONE_Q20;
            std::int32_t val = _callback_with_data(_t+(_step_accu_Q24>>16), _p+(_phase_Q24>>16), _data) * gain_Q10 / (1<<10);
            
            _step_accu_Q24 += _step_rate_Q24;
            _phase_Q24 += _rate_Q24;
            
            _cv_accu_Q20 -= _CV_RATE_Q20;
            if(_cv_accu_Q20 <= 0) {
                _cv_accu_Q20 = _ONE_Q20;
                update();
            }
            
            val = val > -128 ? (val < 127 ? val : 127) : -128;  // Clip to 8-bits
            return val + 128;                                   // Convert to unsigned value
        }
        
        static void copy(u8* buffer, void* ptr) {
            auto& self = *reinterpret_cast<AWSynthSource*>(ptr);
            for(uint32_t i = 0; i < 512; ++i) {
                buffer[i] = self.tick();
            }
            
            if(self._volume_Q14 <= 0) {
                Audio::stop<0>();
            }
        }
        
        static void copyAlt(u8* buffer, void* ptr) {
            auto& self = *reinterpret_cast<AWSynthSource*>(ptr);
            for(uint32_t i = 0; i < 512; ++i) {
                buffer[i] = self.tickAlt();
            }
            
            if(self._volume_Q14 <= 0) {
                Audio::stop<0>();
            }
        }
        
        template<std::uint32_t channel>
        static void mix(std::uint8_t* buffer, void* ptr) {
            auto& self = *reinterpret_cast<AWSynthSource*>(ptr);
            for(uint32_t i = 0; i < 512; ++i) {
                buffer[i] = Audio::mix(buffer[i], self.tick());
            }
            
            if(self._volume_Q14 <= 0) {
                Audio::stop<channel>();
            }
        }
        
        template<std::uint32_t channel>
        static void mixAlt(std::uint8_t* buffer, void* ptr) {
            auto& self = *reinterpret_cast<AWSynthSource*>(ptr);
            for(uint32_t i = 0; i < 512; ++i) {
                buffer[i] = Audio::mix(buffer[i], self.tickAlt());
            }
            
            if(self._volume_Q14 <= 0) {
                Audio::stop<channel>();
            }
        }
        
        template<std::uint32_t channel>
        static void lowLatencyMix(AWSynthSource& self) {
            std::uint32_t idx = audio_playHead >> 9;
            std::uint32_t last = (idx - 1) & (bufferCount - 1);
            if(audio_state[last]) {
                mix<channel>(audio_buffer + last*512, &self);
            }
        }
        
        template<std::uint32_t channel>
        static void lowLatencyMixAlt(AWSynthSource& self) {
            // Check if the last audio buffer has already been filled, and if so, add to it
            std::uint32_t idx = audio_playHead >> 9;
            std::uint32_t last = (idx - 1) & (bufferCount - 1);
            if(audio_state[last]) {
                mixAlt<channel>(audio_buffer + last*512, &self);
            }
        }
        
        // Takes logarithmic level (0 to 1024) and returns gain as Q10 fixed point
        static constexpr std::int32_t levelToGain(std::int32_t level) {
            constexpr std::int32_t SCALE_Q15 = 3.32193*35/20 * (1<<15);   // 3.32193=log2(10); 35=scales level to 0...35 dB
            return level > 0 ? (pow2(-((1024-level)*SCALE_Q15) >> 10) >> 5) : 0;
        }
        
        // Input range is -9...8.999 as Q15 fixed point
        static constexpr std::uint32_t pow2(std::int32_t exp_Q15) {
            std::uint32_t semitone_Q15 = 12 * (exp_Q15 & 0x7fff);
            std::uint32_t semitone = semitone_Q15 >> 15;
            std::uint32_t ratio_a_Q15 = _SEMITONES_Q15[semitone];
            std::uint32_t ratio_b_Q15 = _SEMITONES_Q15[semitone + 1];
            std::uint32_t fraction_Q15 = semitone_Q15 & 0x7fff;
            
            std::uint32_t shift = 9 + (exp_Q15 >> 15);
            return ((ratio_a_Q15 + (((ratio_b_Q15 - ratio_a_Q15) * fraction_Q15) >> 15)) << shift) >> 8;
        }
        
        std::uint32_t _t;
        std::uint32_t _p;
        
        std::uint32_t _rate_Q24;
        std::uint32_t _phase_Q24;
        
        std::int32_t _cv_accu_Q20;
        
        std::int32_t _step_rate_Q24;
        std::int32_t _step_accu_Q24;
        std::int32_t _step_div_Q24;
        
        const std::int8_t* _levels;
        std::uint8_t _levels_idx;
        std::uint8_t _levels_loop;
        std::uint8_t _levels_end;
        std::int16_t _base_level_Q10;
        std::int16_t _delta_level_Q10;
        
        const std::int8_t* _semitones;
        std::uint8_t _semitones_idx;
        std::uint8_t _semitones_loop;
        std::uint8_t _semitones_end;
        std::int16_t _base_pitchbend_Q10;
        std::int16_t _delta_pitchbend_Q10;
        
        std::int16_t _target_gain_Q10;
        std::int16_t _delta_gain_Q10;
        
        std::int16_t _release_rate_Q14;
        std::int16_t _volume_Q14;
        
        std::int16_t _glide_interval_Q10;
        std::int16_t _glide_rate_Q14;
        std::int16_t _glide_accu_Q14;
        
        std::uint8_t _midikey;
        
        bool _released;
        
        union {
            std::int32_t (*_callback)(std::uint32_t, std::uint32_t);
            std::int32_t (*_callback_with_data)(std::uint32_t, std::uint32_t, void*);
        };
        void* _data;
};

} // namespace Audio
