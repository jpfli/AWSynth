#pragma once

#include <LibSchedule>
#include <LibAudio>
#include "AWSynthSource.h"

#define SIMPLE_TUNE_AW(x...)                                            \
    []{                                                                 \
        struct  constexpr_string_type { const char * chars = #x; };     \
        using result = Audio::internal::apply_range<sizeof(#x)-1, Audio::internal::count_elements<constexpr_string_type>::produce >::result; \
        return Audio::internal::genSimpleTuneAW<std::array<Audio::u8, (result::value+1)*2 >>( constexpr_string_type{}.chars ); \
    }()


namespace Audio {

template<u32 channel, u32 timerId>
class SimpleTuneAW {

    public:
    
        constexpr void patch(const AWPatch& patch) { _patch = &patch; }
        
        constexpr void tempo(u32 tempo){ _tempo = 4 * 60000 / tempo; }
        
        void setup(u32 tempo, const AWPatch& patch, const u8 *data, u32 length) {
            _patch = &patch;
            _source = nullptr;
            
            _tempo = tempo;
            this->data = data;
            this->length = length;
            this->position = 0;
            Schedule::after<timerId>(0, &SimpleTuneAW::play, *this);
        }
    
    private:
    
        void play() {
            if(position >= length) {
                if(_source) {
                    _source->release();
                }
                return;
            }
            
            auto note_number = data[position++];
            
            note_number &= 0x7F;
            
            signed char noteDuration = data[position++];
            u32 duration = _tempo;
            
            if(noteDuration < 0)
                duration /= -noteDuration;
            else if(noteDuration > 0)
                duration *= noteDuration;
            
            if(note_number <= 88){
                if(_patch) {
                    _source = &AWSynthSource::play<channel, false>(*_patch, 23+note_number);
                }
            }
            else if(_source) {
                _source->release();
            }
            
            Schedule::after<timerId>(duration, &SimpleTuneAW::play, *this);
        }
        
        const AWPatch* _patch;
        AWSynthSource* _source;
        
        u32 _tempo;
        u32 position;
        u32 length;
        const u8* data;
};

namespace internal {

    template<typename Array>
    constexpr auto genSimpleTuneAW(const char *str){
        constexpr const u8 noteIndex[] = {
            12, // a
            14, // b
            3,  // c
            5,  // d
            7,  // e
            8,  // f
            10  // g
        };
        
        u32 pos = 0;
        
        struct Extended : Array {
            const AWPatch* _patch = nullptr;
            constexpr auto& patch(const AWPatch& patch) { _patch = &patch; return *this; }
            constexpr const AWPatch& patch() const { return *_patch; }
            
            u32 _tempo = 4 * 60000 / 120;
            constexpr auto& tempo(u32 tempo) { _tempo = 4 * 60000 / tempo; return *this; }
            constexpr u32 tempo() const { return _tempo; }
        } array = {};
        
        for(u32 i=0; str[i];){
            u32 octave = 4;
            u32 note = 0;
            s8 duration = 0;
            
            i += countWhitespace(str+i);
            if(str[i] >= 'A' && str[i] <= 'G'){
                note = noteIndex[ str[i++] - 'A' ];
                
                i += countWhitespace(str+i);
                if(str[i] == '#'){
                    note++;
                    i++;
                } else if(str[i] == '-')
                    i++;
                
                i += countWhitespace(str+i);
                if(str[i] >= '0' && str[i] <= '9')
                    octave = str[i++] - '0';
                note = (note + octave * 12) - 14;
            } else {
                note = 89;
                i++;
            }
            
            i += countWhitespace(str+i);
            if(str[i] == '/'){
                i++;
                i += countWhitespace(str+i);
                while(str[i] >= '0' && str[i] <= '9')
                    duration = (duration * 10) + str[i++] - '0';
                duration = -duration;
            } else if( str[i] == '*' ){
                i++;
                i += countWhitespace(str+i);
                while(str[i] >= '0' && str[i] <= '9')
                    duration = (duration * 10) + str[i++] - '0';
            }
            
            array[pos++] = note;
            array[pos++] = duration;
            
            i += countWhitespace(str+i);
            if(str[i] == ',') i++;
        }
        return array;
    }
    
    template <u32 channel, u32 timerId>
    inline SimpleTuneAW<channel, timerId> simpleTuneAW;
}

template<u32 channel=0, u32 timerId=~0U - channel, typename TuneType>
auto& playTuneAW(const TuneType& tune){
    auto& source = internal::simpleTuneAW<channel, timerId>;
    source.setup(tune.tempo(), tune.patch(), &tune[0], tune.size());
    return source;
}

}  // namespace Audio
