
// Web Audio API check
window.AudioContext = window.AudioContext || window.webkitAudioContext;
if (!AudioContext) {
    alert("Sorry, your browser doesn't support the Web Audio APIs.");
    throw new Error("Sorry, your browser doesn't support the Web Audio APIs. Execution Aborted."); // ABORT ALL
}
let audio_ctx = new AudioContext();

let midikeyByKeyCode = new Map([
    //  G#2        A2       A#2        B2
    [65, 44], [90, 45], [83, 46], [88, 47],
    //   C3       C#3        D3       D#3        E3        F3       F#3        G3       G#3         A3       A#3         B3
    [67, 48], [70, 49], [86, 50], [71, 51], [66, 52], [78, 53], [74, 54], [77, 55], [75, 56], [188, 57], [76, 58], [190, 59],
    //   C4       C#4        D4       D#4        E4        F4       F#4        G4       G#4        A4       A#4        B4
    [81, 60], [50, 61], [87, 62], [51, 63], [69, 64], [82, 65], [53, 66], [84, 67], [54, 68], [89, 69], [55, 70], [85, 71],
    //   C5       C#5        D5       D#5        E5
    [73, 72], [57, 73], [79, 74], [48, 75], [80, 76]
]);

let keySymbolByMidikey = new Map([
    //   G#2         A2        A#2         B2
    [44, 'A'], [45, 'Z'], [46, 'S'], [47, 'X'],
    //    C3        C#3         D3        D#3         E3         F3        F#3         G3        G#3         A3        A#3         B3
    [48, 'C'], [49, 'F'], [50, 'V'], [51, 'G'], [52, 'B'], [53, 'N'], [54, 'J'], [55, 'M'], [56, 'K'], [57, ','], [58, 'L'], [59, '.'],
    //    C4        C#4         D4        D#4         E4         F4        F#4         G4        G#4         A4        A#4         B4
    [60, 'Q'], [61, '2'], [62, 'W'], [63, '3'], [64, 'E'], [65, 'R'], [66, '5'], [67, 'T'], [68, '6'], [69, 'Y'], [70, '7'], [71, 'U'],
    //    C5        C#5         D5        D#5         E5
    [72, 'I'], [73, '9'], [74, 'O'], [75, '0'], [76, 'P']
]);

const palette = [
    "#1a1c2c","#5d275d","#b13e53","#ef7d57","#ffcd75","#a7f070","#38b764","#257179",
    "#29366f","#3b5dc9","#41a6f6","#73eff7","#f4f4f4","#94b0c2","#566c86","#333c57"
];

let waveform_types = [ "square", "pulse", "sawtooth", "softsaw", "triangle", "sine", "noise" ];
let waveform_colors = [ palette[6], palette[5], palette[4], palette[3], palette[2], palette[1], palette[13] ];

let effect_types = [ "step", "attack", "decay", "slide" ];
let effect_colors = [palette[2], palette[4], palette[6], palette[9]];

let sources = [];

let awpatch = new AWPatch();
let saved_awpatch = null;

let waveform_view = new WaveformView(document.querySelector("#waveform-canvas"), waveform_colors);
let pitch_view = new EnvelopeView(document.querySelector("#pitchenv-canvas"), 32, 32.0/32, awpatch.semitones, effect_colors.concat([palette[1]]));
let amplitude_view = new EnvelopeView(document.querySelector("#ampenv-canvas"), 17, 32.0/16, awpatch.amplitudes, effect_colors.concat([palette[8]]));

function refreshView() {    
    document.getElementById("volume").value = awpatch.volume;
    document.getElementById("stepsize").value = awpatch.step;
    document.getElementById("loop_start").value = awpatch.amplitudes.loop_start;
    document.getElementById("env_length").value = awpatch.amplitudes.length;
    document.getElementById("release").value = awpatch.release;
    document.getElementById("glide").value = awpatch.glide;
    
    pitch_view.setEnvelope(awpatch.semitones);    
    amplitude_view.setEnvelope(awpatch.amplitudes);
    
    waveform_view.draw();
    pitch_view.draw();
    amplitude_view.draw();
}

function waveformSelected(type) {
    waveform_view.selected_waveform = type;
    
    // Highlight the selected waveform icon
    document.querySelectorAll("#waveforms .item").forEach((elem, idx) => {
        elem.style.backgroundColor = (elem.children[0].id == type) ? "#555" : "";
    });
}

function waveformSet(type) {
    for(let idx=0; idx<awpatch.waveform.length; ++idx) {
        awpatch.waveform[idx] = waveform_view.selected_waveform;
    }
    waveform_view.draw();
}

function pitchEffectSelected(type) {
    pitch_view.selected_effect = type;
    
    // Highlight the selected waveform icon
    document.querySelectorAll("#pitch-effects .item").forEach((elem, idx) => {
        elem.style.backgroundColor = (elem.children[0].id == type) ? "#555" : "";
    });
}

function pitchEffectSet(type) {
    for(let idx=0; idx<awpatch.semitones.data.length; ++idx) {
        awpatch.semitones.effects[idx] = type;
    }
    pitch_view.draw();
}

function amplitudeEffectSelected(type) {
    amplitude_view.selected_effect = type;
    
    // Highlight the selected waveform icon
    document.querySelectorAll("#amplitude-effects .item").forEach((elem, idx) => {
        elem.style.backgroundColor = (elem.children[0].id == type) ? "#555" : "";
    });
}

function amplitudeEffectSet(type) {
    for(let idx=0; idx<awpatch.amplitudes.data.length; ++idx) {
        awpatch.amplitudes.effects[idx] = type;
    }
    amplitude_view.draw();
}

function volumeChanged() {
    awpatch.volume = document.getElementById("volume").value;
}

function stepChanged() {
    awpatch.step = document.getElementById("stepsize").value;
}

function releaseChanged() {
    awpatch.release = document.getElementById("release").value;
}

function glideChanged() {
    awpatch.glide = document.getElementById("glide").value;
}

function loopChanged() {
    val = document.getElementById("loop_start").value;
    awpatch.semitones.loop_start = parseInt(val);
    awpatch.amplitudes.loop_start = parseInt(val);
    
    val = document.getElementById("env_length").value;
    awpatch.semitones.length = parseInt(val);
    awpatch.amplitudes.length = parseInt(val);
    
    waveform_view.draw();
    pitch_view.draw();
    amplitude_view.draw();
}

function semitonesChanged() {
    pitch_view.draw();
}

function amplitudesChanged() {
    amplitude_view.draw();
}

function createWhiteKey(midikey, slot) {
    let key = document.createElement("div");
    key.className = "white pianokey";
    key.style.height = "120px";
    key.style.width = "24px";

    key.dataset["midikey"] = midikey;

    return key;
}

function createBlackKey(midikey, slot) {
    let key = document.createElement("div");
    key.className = "black pianokey";
    key.style.height = "70px";
    key.style.width = "14px";

    key.dataset["midikey"] = midikey;

    return key;
}

function setupVirtualPiano(eventHandler) {
    const semitones = { white: [0,2,4,5,7,9,11], black: [1,3,6,8,10] };
    
    let keyboard = document.getElementById('pianokeyb');
    
    const idx_lo = 5;       // A0
    const idx_hi = 8*7 + 0; // C8
    let idx_white = idx_lo;
    while(idx_white <= idx_hi) {
        const octave = (idx_white/7) | 0;
        const midikey = 12 + 12*octave + semitones.white[idx_white%7];
        let key = createWhiteKey(midikey);
        key.style.left = 24*(idx_white-idx_lo) + "px";
        keyboard.appendChild(key);
        
        let label = document.createElement('div');
        label.className = 'label';
        if(keySymbolByMidikey.has(midikey)) label.innerHTML = "<b>"+keySymbolByMidikey.get(midikey)+"</b>";
        key.appendChild(label);
        
        key.addEventListener("mousedown", eventHandler, false);
        key.addEventListener("mouseup", eventHandler, false);
        key.addEventListener("mouseenter", eventHandler, false);
        key.addEventListener("mouseleave", eventHandler, false);
        
        ++idx_white;
    }
    
    let idx_black = 5 * ((idx_lo/7)|0);
    while(semitones.black[idx_black%5] < semitones.white[idx_lo%7]) {
        ++idx_black;
        if(idx_black%5 == 0) break;
    }
    
    const semitone_hi = 12*((idx_hi/7)|0) + semitones.white[idx_hi%7]
    while(12*((idx_black/5)|0) + semitones.black[idx_black%5] < semitone_hi) {
        const octave = (idx_black/5)|0;
        const midikey = 12 + 12*octave + semitones.black[idx_black%5];
        let key = createBlackKey(midikey);
        key.style.left = 14*(midikey-12) - 24*(idx_lo) + "px";
        keyboard.appendChild(key);
        
        let label = document.createElement('div');
        label.className = 'label';
        if(keySymbolByMidikey.has(midikey)) label.innerHTML = "<b>"+keySymbolByMidikey.get(midikey)+"</b>";
        key.appendChild(label);
        
        key.addEventListener("mousedown", eventHandler, false);
        key.addEventListener("mouseup", eventHandler, false);
        key.addEventListener("mouseenter", eventHandler, false);
        key.addEventListener("mouseleave", eventHandler, false);
        
        ++idx_black;
    }
    
    // Scroll C4 to the middle of the piano keyboard view
    document.querySelector("div[data-midikey='60']").scrollIntoView({ behavior:'auto', block:'center', inline:'center' });
}

function AWPatch() {
    this.volume = 80;
    this.step = 4;
    this.release = 0;
    this.glide = 0;
    this.waveform = Array(32).fill(waveform_types[0]);
    this.semitones = { loop_start: 32, length: 32, data: Array(32).fill(0), effects: Array(32).fill(effect_types[0]) };
    this.amplitudes = { loop_start: 32, length: 32, data: Array(32).fill(32), effects: Array(32).fill(effect_types[0]) };
}

AWPatch.prototype.parse = function(json) {
    if(!json) return;
    
    ob = JSON.parse(json);
    this.volume = ob.volume;
    this.step = ob.step;
    this.release = ob.release;
    this.glide = ob.glide;
    
    this.waveform = (ob.waveform.constructor === Array) ? ob.waveform.map((x) => x) : Array(32).fill(ob.waveform);
    
    this.semitones.loop_start = ob.semitones.loop_start;
    this.semitones.length = ob.semitones.length;
    this.semitones.data = ob.semitones.data.map((x) => x);
    if('smooth' in ob.semitones) {
        ob.semitones.effects = Array(32).fill(ob.semitones.smooth ? "slide" : "step");
    }
    this.semitones.effects = ob.semitones.effects.map((x) => x);
    
    this.amplitudes.loop_start = ob.amplitudes.loop_start;
    this.amplitudes.length = ob.amplitudes.length;
    this.amplitudes.data = (ob.amplitudes.data.findIndex(item => item > 32) >= 0) ?
        ob.amplitudes.data.map((item) => ((item*31/100)|0)) :
        ob.amplitudes.data.map((x) => x);
    if('smooth' in ob.amplitudes) {
        ob.amplitudes.effects = Array(32).fill(ob.amplitudes.smooth ? "slide" : "step");
    }
    this.amplitudes.effects = ob.amplitudes.effects.map((x) => x);
}

AWPatch.prototype.copy = function() {
    let other = new AWPatch();
    other.volume = this.volume;
    other.step = this.step;
    other.release = this.release;
    other.glide = this.glide;
    other.waveform = this.waveform.map((x) => x);
    other.semitones.loop_start = this.semitones.loop_start;
    other.semitones.length = this.semitones.length;
    other.semitones.data = this.semitones.data.map((x) => x);
    other.semitones.effects = this.semitones.effects.map((x) => x);
    other.amplitudes.loop_start = this.amplitudes.loop_start;
    other.amplitudes.length = this.amplitudes.length;
    other.amplitudes.data = this.amplitudes.data.map((x) => x);
    other.amplitudes.effects = this.amplitudes.effects.map((x) => x);
    return other;
}

AWPatch.prototype.match = function(other) {
    return this.volume == other.volume &&
        this.step == other.step && this.release == other.release && this.glide == other.glide &&
        this.waveform.every((val,idx) => val === other.waveform[idx]) &&
        this.semitones.loop_start == other.semitones.loop_start &&
        this.semitones.length == other.semitones.length &&
        this.semitones.data.every((val,idx) => val === other.semitones.data[idx]) &&
        this.semitones.effects.every((val,idx) => val === other.semitones.effects[idx]) &&
        this.amplitudes.loop_start == other.amplitudes.loop_start &&
        this.amplitudes.length == other.amplitudes.length &&
        this.amplitudes.data.every((val,idx) => val === other.amplitudes.data[idx]) &&
        this.amplitudes.effects.every((val,idx) => val === other.amplitudes.effects[idx]);
}

function PixelCanvas(canvas) {
    this.canvas = canvas;
    this.w = this.canvas.width;
    this.h = this.canvas.height;
    this.ctx = this.canvas.getContext("2d");
    this.ctx.globalAlpha = 1;
    
    let dpi = window.devicePixelRatio;
    let style_width = +window.getComputedStyle(this.canvas).getPropertyValue("width").slice(0, -2);
    let style_height = +window.getComputedStyle(this.canvas).getPropertyValue("height").slice(0, -2);
    this.canvas.setAttribute('width', style_width * dpi);
    this.canvas.setAttribute('height', style_height * dpi);
}

PixelCanvas.prototype.fill = function(color) {
    this.ctx.fillStyle = color;
    this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
}

PixelCanvas.prototype.fillRect = function(color, x, y, w, h) {
    x = (x*this.canvas.width/this.w) | 0;
    y = (y*this.canvas.height/this.h) | 0;
    w = (w*this.canvas.width/this.w) | 0;
    h = (h*this.canvas.height/this.h) | 0;
    this.ctx.fillStyle = color;
    this.ctx.fillRect(x, y, w, h);
}

PixelCanvas.prototype.pset = function(x, y, color) {
    x = x|0;
    y = y|0;
    
    const resX = this.canvas.width/this.w;
    let x0 = (x*resX)|0;
    let x1 = ((x+1)*resX)|0;
    
    const resY = this.canvas.height/this.h;
    let y0 = (y*resY)|0;
    let y1 = ((y+1)*resY)|0;
    
    if (x1 >= 0 && x0 < this.canvas.width && y1 >= 0 && y0 < this.canvas.height) {
        if(color) this.ctx.fillStyle = color;
        this.ctx.fillRect(x0, y0, x1-x0, y1-y0);
    }
}

PixelCanvas.prototype.getPixelXY = function(event) {
    let rect = this.canvas.getBoundingClientRect();
    let px = event.clientX - rect.left;
    px = this.w * px / this.canvas.clientWidth;
    let py = event.clientY - rect.top;
    py = this.h * py / this.canvas.clientHeight;
    
    return {x: px|0, y: py|0};
}

function WaveformView(canvas, colors) {
    this.selected_waveform = waveform_types[0];
    
    this.pcanvas = new PixelCanvas(canvas);
    this.colors = colors;
    
    canvas.addEventListener("mousedown", this);
    canvas.addEventListener("mousemove", this);
}

WaveformView.prototype.handleEvent = function(event) {
    const coord = this.pcanvas.getPixelXY(event);
    let step = (coord.x/4)|0;
    if(step > awpatch.waveform.length-1) step = awpatch.waveform.length-1;
    
    if(event.type == "mousedown" || (event.type == "mousemove" && (event.buttons&1))) {
        awpatch.waveform[step] = this.selected_waveform;
    }
    else {
        return;
    }
    
    this.draw();
}

WaveformView.prototype.draw = function() {
    this.pcanvas.fill("#000");
    
    for(let step=0; step<32; ++step) {
        const idx = waveform_types.findIndex(type => type == awpatch.waveform[step]);
        let color = this.colors[idx];
        for(j=1; j<this.pcanvas.h-1; ++j) {
            this.pcanvas.pset(4*step+1, j, color);
            this.pcanvas.pset(4*step+2, j, color);
        }
    }
    
    // Draw loop start and length markers
    for(let j=0; j<this.pcanvas.h; j+=2) {
        this.pcanvas.pset(4*awpatch.semitones.loop_start, j, "#888");
        this.pcanvas.pset(4*awpatch.semitones.length-1, j, "#888");
    }
}

function EnvelopeView(canvas, rowzero, scale, envelope, colors) {
    this.selected_effect = effect_types[0];
    
    this.pcanvas = new PixelCanvas(canvas);
    this.rowzero = rowzero;
    this.scale = scale;
    this.envelope = envelope;
    this.colors = colors;
    
    canvas.addEventListener("mousedown", this);
    canvas.addEventListener("mousemove", this);
}

EnvelopeView.prototype.handleEvent = function(evt) {
    const coord = this.pcanvas.getPixelXY(event);
    let step = (coord.x/4)|0;
    if(step > this.envelope.data.length-1) step = this.envelope.data.length-1;
    
    let val = this.rowzero - ((coord.y < 1) ? 1 : ((coord.y < this.pcanvas.h-1) ? coord.y : this.pcanvas.h-1));
    val = (val*this.scale|0);
    
    if(evt.type == "mousedown" || (evt.type == "mousemove" && (evt.buttons&1))) {
        this.envelope.data[step] = val;
        this.envelope.effects[step] = this.selected_effect;
    }
    else {
        return;
    }
    
    this.draw();
}

EnvelopeView.prototype.setEnvelope = function(env) {
    this.envelope = env;
}

EnvelopeView.prototype.draw = function() {
    this.pcanvas.fill("#000");
    
    const bar_color = [palette[15], palette[14]];
    for(let step=0; step<32; ++step) {
        let val = (this.envelope.data[step]) / this.scale;
        if(val > 0) {
            for(let j=0; j<=this.rowzero; ++j) {
                let row = this.rowzero-j;
                if(row < val) {
                    this.pcanvas.pset(4*step+1, j, (row&1) ? "#000" : bar_color[(row/10)&1]);
                    this.pcanvas.pset(4*step+2, j, (row&1) ? "#000" : bar_color[(row/10)&1]);
                }
            }
        }
        else {
            for(let j=this.rowzero-1; j<this.pcanvas.h; ++j) {
                let row = this.rowzero-j;
                if(row > (val+1)) {
                    this.pcanvas.pset(4*step+1, j, (row&1) ? bar_color[((row-1)/10)&1] : "#000");
                    this.pcanvas.pset(4*step+2, j, (row&1) ? bar_color[((row-1)/10)&1] : "#000");
                }
            }
        }
        
        const idx = effect_types.findIndex(elem => elem == this.envelope.effects[step]);
        const color = this.colors[idx];
        
        let j = this.rowzero - val;
        this.pcanvas.pset(4*step+1, j, color);
        this.pcanvas.pset(4*step+2, j, color);
        this.pcanvas.pset(4*step+1, j-1, color);
        this.pcanvas.pset(4*step+2, j-1, color);
    }
    
    // Draw loop start and length markers
    for(let j=0; j<this.pcanvas.h; j+=2) {
        let step = this.envelope.loop_start;
        this.pcanvas.pset(4*this.envelope.loop_start, j, "#888");
        this.pcanvas.pset(4*this.envelope.length-1, j, "#888");
    }
}

// Takes logarithmic level (0 to 1.0) and returns gain
function levelToGain(level) {
    return level > 0 ? 2**(-(1.0-level)*3.32193*35/20) : 0; // 3.32193=log2(10); 35=scales level to -35...0 dB
}

function AWSynthSource() {
    AWSynthSource.SAMPLE_RATE = audio_ctx.sampleRate;
    AWSynthSource.CV_RATE = AWSynthSource.CV_RATE || (240/audio_ctx.sampleRate);
    AWSynthSource.ALGORITHMS = {
        square: AWSynthSource.sqr,
        pulse: AWSynthSource.pulse,
        sawtooth: AWSynthSource.saw,
        softsaw: AWSynthSource.softsaw,
        triangle: AWSynthSource.tri,
        sine: AWSynthSource.sin,
        noise: AWSynthSource.noise
    };
}

AWSynthSource.sqr = function(p) {
    return (p&128) ? -128 : 128;
}

AWSynthSource.pulse = function(p) {
    const w = 79;
    return (p&255) < w ? 128 : -128;
}

AWSynthSource.saw = function(p) {
    return ((p+128)&255) - 128;
}

AWSynthSource.softsaw = function(p) {
    const x_peak = 224;
    const rate_a = x_peak > 1 ? ((1<<20)/x_peak) : (1<<20);
    const rate_b = x_peak < 255 ? ((1<<20)/(256-x_peak)) : (1<<20);
    const o = (p+x_peak/2) & 255;
    return ((o<x_peak ? o*rate_a : (256-o)*rate_b) >> 12) - 128;
}

AWSynthSource.tri = function(p) {
    const o = (2*(p+64)) & 255;
    return (p+64)&128 ? 128-o : o-128;
}

AWSynthSource.sin = function(p) {
    const o = ((p&127) * (-p&127)) >> 5;
    return p&128 ? -o : o;
}

AWSynthSource.noise = function(p) {
    const z = (p>>7)^0x5DEECE66;
    return (((1664525*z + 1013904223)>>12) & 255) - 128;
}

AWSynthSource.prototype.release = function() {
    this.released = true;
}

AWSynthSource.prototype.play = function(midikey) {
    this.init(midikey);
    
    const buffer_size = 2048;
    this.processor = audio_ctx.createScriptProcessor(buffer_size, 0, 1);
    this.processor.onaudioprocess = (event) => {
        if(this.volume <= 0) {
            this.processor.disconnect();
            return;
        }
        
        let output = event.outputBuffer.getChannelData(0);
        
        for(let idx = 0; idx < output.length; ++idx) {
            output[idx] = this.tick();
        }
    };
    this.processor.connect(audio_ctx.destination);
}

AWSynthSource.prototype.init = function(midikey) {
    this.p = 0;
    
    this.cv_accu = 1;
    
    this.step_rate = AWSynthSource.CV_RATE / (2*awpatch.step) + 1e-15;
    this.step_div = 1 / (2*awpatch.step);
    this.step_accu = 0;
    
    this.volume = awpatch.volume / 100;
    this.release_rate = awpatch.release > 0 ? (this.volume / (awpatch.release*(2*awpatch.step))) : this.volume;
    
    this.levels_loop = awpatch.amplitudes.loop_start;
    this.levels_end = awpatch.amplitudes.length;
    this.levels_idx = 0;
    
    this.base_level = awpatch.amplitudes.data[0] / 32;
    this.delta_level = 0;
    
    switch(awpatch.amplitudes.effects[0]) {
        case "attack":
            this.delta_level = this.base_level;
            this.base_level = 0;
            break;
        
        case "decay":
            this.delta_level = -this.base_level;
            break;
        
        default:
            break;
    }
    
    const level = this.base_level + this.delta_level*this.step_div;
    this.target_gain = levelToGain(this.volume*level);
    this.delta_gain = this.target_gain - 0;
    
    this.semitones_loop = awpatch.semitones.loop_start;
    this.semitones_end = awpatch.semitones.length;
    this.semitones_idx = 0;
    
    this.base_pitchbend = awpatch.semitones.data[0] / 12;
    this.delta_pitchbend = 0;
    
    switch(awpatch.semitones.effects[0]) {
        case "attack":
            this.delta_pitchbend = this.base_pitchbend;
            this.base_pitchbend = 0;
            break;
        
        case "decay":
            this.delta_pitchbend = -this.base_pitchbend;
            break;
        
        default:
            break;
    }
    
    const pitchbend = this.base_pitchbend + this.delta_pitchbend*this.step_div/2;
    this.rate = 440 * 2**((midikey-69)/12 + pitchbend) / AWSynthSource.SAMPLE_RATE;
    this.phase = 0;
    
    this.glide_interval = 0;
    this.glide_rate = 0;
    this.glide_accu = 0;
    
    this.callback = AWSynthSource.ALGORITHMS[awpatch.waveform[0]];
    
    this.midikey = midikey;
    
    this.released = false;
}

AWSynthSource.prototype.tick = function() {
    const gain = this.target_gain - this.delta_gain*this.cv_accu;
    let val = this.callback(this.p+((this.phase*256)|0)) * gain;
    
    this.step_accu += this.step_rate;
    this.phase += this.rate;
    
    this.cv_accu -= AWSynthSource.CV_RATE;
    if(this.cv_accu <= 0) {
        this.cv_accu = 1;
        
        const idx = (this.levels_idx + this.step_accu) | 0;
        if(idx < 32) {
            this.callback = AWSynthSource.ALGORITHMS[awpatch.waveform[idx]];
        }
        
        this.update();
    }
    
    val = val > -128 ? (val < 127 ? val : 127) : -128;  // Clip to 8-bits
    return val / 128;
}

AWSynthSource.prototype.update = function() {
    this.p += 256*(this.phase | 0);
    this.phase -= this.phase | 0;
    
    if(this.released) {
        this.volume -= this.release_rate;
        if(this.volume <= 0) {
            this.volume = 0;
        }
    }
    
    let level = this.base_level;
    let pitchbend = this.base_pitchbend;
    
    if(this.step_accu + this.step_div >= 1) {
        this.step_accu = -this.step_div;
        
        level += this.delta_level;
        this.delta_level = 0;
        
        let len = (this.levels_end > 0 && this.levels_end < 32) ? this.levels_end : 32;
        if(this.levels_idx < len) {
            ++this.levels_idx;
            if(this.levels_idx >= len) {
                this.levels_idx = (this.levels_loop < this.levels_end) ? this.levels_loop : 32;
            }
            
            if(this.levels_idx < 32) {
                this.base_level = awpatch.amplitudes.data[this.levels_idx] / 32;
                
                switch(awpatch.amplitudes.effects[this.levels_idx]) {
                    case "attack":
                        this.delta_level = this.base_level;
                        this.base_level = 0;
                        break;
                    case "decay":
                        this.delta_level = -this.base_level;
                        break;
                   case "slide":
                        this.delta_level = this.base_level - level;
                        this.base_level = level;
                        break;
                    default:
                        break;
                }
                
                if(this.base_level < level) {
                    level = this.base_level;
                }
            }
            else {
                this.base_level = level;
                if(!this.released) {
                    this.release();
                    if(this.release_rate >= this.volume) {
                        this.volume = 0;
                    }
                }
            }
        }
        
        pitchbend += this.delta_pitchbend - this.delta_pitchbend*this.step_div/2;
        
        this.base_pitchbend += this.delta_pitchbend;
        this.delta_pitchbend = 0;
        
        len = (this.semitones_end > 0 && this.semitones_end < 32) ? this.semitones_end : 32;
        if(this.semitones_idx < len) {
            ++this.semitones_idx;
            if(this.semitones_idx >= len) {
                this.semitones_idx = this.semitones_loop < this.semitones_end ? this.semitones_loop : 32;
            }
            
            if(this.semitones_idx < 32) {
                const next_pitchbend = awpatch.semitones.data[this.semitones_idx] / 12;
                
                switch(awpatch.semitones.effects[this.semitones_idx]) {
                    case "step":
                        this.base_pitchbend = next_pitchbend;
                        break;
                    case "attack":
                        this.delta_pitchbend = next_pitchbend;
                        this.base_pitchbend = 0;
                        break;
                    case "decay":
                        this.delta_pitchbend = -next_pitchbend;
                        this.base_pitchbend = next_pitchbend;
                        break;
                    case "slide":
                        this.delta_pitchbend = next_pitchbend - this.base_pitchbend;
                        break;
                    default:
                        break;
                }
            }
        }
    }
    else {
        level += this.delta_level*(this.step_accu + this.step_div);
        pitchbend += this.delta_pitchbend*(this.step_accu + this.step_div/2);
    }
    
    const prev_gain = this.target_gain;
    this.target_gain = levelToGain(this.volume*level);
    this.delta_gain = this.target_gain - prev_gain;
    
    if(this.glide_accu > 0) {
        this.glide_accu -= this.glide_rate;
        if(this.glide_accu > 0) {
            pitchbend += this.glide_interval*this.glide_accu;
        }
        else {
            this.glide_accu = 0;
        }
    }
    
    this.rate = 440 * 2**((this.midikey-69)/12 + pitchbend) / AWSynthSource.SAMPLE_RATE;
}

function noteOn(midikey) {
    if(awpatch.glide > 0 && sources.length > 0 && sources[0].midikey > 0) {
        let source = sources[0];
        if(source.midikey != midikey) {
            // Calculate remaining glide interval in case the current patch hasn't finished it's pitch glide
            source.glide_interval = source.glide_interval*source.glide_accu;
            // Then add interval from this to previous midikey
            source.glide_interval += (source.midikey-midikey) / 12;
            
            source.glide_rate = 1 / (awpatch.glide*(2*awpatch.step));
            source.glide_accu = 1;    // Pitch starts from full glide interval and glides down to zero to current pitch
            
            source.midikey = midikey;
        }
    }
    else {
        for(let idx = 0; idx < sources.length; ++idx) {
            if(sources[idx].midikey == midikey) {
                return;
            }
        }
        
        let source = new AWSynthSource;
        sources.push(source);
        source.play(midikey);
    }
}

function noteOff(midikey) {
    let garbage = [];
    for(let idx = 0; idx < sources.length; ++idx) {
        if(sources[idx].midikey == midikey) {
            sources[idx].release();
            garbage.push(idx);
        }
    }
    for(let idx = 0; idx < garbage.length; ++idx) {
        sources.splice(garbage[idx], 1);
    }
}

function virtualPianoEventHandler(event) {
    if(event.type == "mousedown" && (event.buttons & 1)) {
        noteOn(event.target.dataset["midikey"]);
    }
    if(event.type == "mouseenter" && (event.buttons & 1)) {
        noteOn(event.target.dataset["midikey"]);
        // If entering from another key, turn the previous note off
        if(event.relatedTarget.classList.contains("pianokey")) {
            noteOff(event.relatedTarget.dataset["midikey"]);
        }
    }
    else if(event.type == "mouseup") {
        noteOff(event.target.dataset["midikey"]);
    }
    else if(event.type == "mouseleave" && (event.buttons & 1)) {
        // Don't turn note off when moving from one piano key to another, only when moving entirely outside the keyboard
        if(!event.relatedTarget.classList.contains("pianokey")) {
            noteOff(event.target.dataset["midikey"]);
        }
    }
}

document.addEventListener('keydown', function (event) {
    if(midikeyByKeyCode.has(event.keyCode)) {
        noteOn(midikeyByKeyCode.get(event.keyCode));
    }
}, false);

document.addEventListener('keyup', function (event) {
    if(midikeyByKeyCode.has(event.keyCode)) {
        noteOff(midikeyByKeyCode.get(event.keyCode));
    }
}, false);

window.onload = function (event) {
    setupVirtualPiano(virtualPianoEventHandler);    // Create the virtual piano keyboard
    window.scroll(0,0);                             // Scroll back to top, for some reason FemtoIDE always scrolls to bottom
    
    // Disable key event propagation for all input elements to prevent accidentally playing notes
    document.querySelectorAll("input").forEach((elem, idx) => {
        elem.addEventListener("keydown", function(event) { event.stopPropagation(); });
        elem.addEventListener("keyup", function(event) { event.stopPropagation(); });
    });
    
    document.querySelectorAll("#waveforms .item").forEach((elem, idx) => {
        elem.style.borderColor = waveform_colors[idx];
    });
    
    document.querySelectorAll("#pitch-effects .item").forEach((elem, idx) => {
        elem.style.borderColor = effect_colors[idx];
    });
    
    document.querySelectorAll("#amplitude-effects .item").forEach((elem, idx) => {
        elem.style.borderColor = effect_colors[idx];
    });
    
    waveformSelected(waveform_types[0]);
    pitchEffectSelected(effect_types[0]);
    amplitudeEffectSelected(effect_types[0]);
    
    document.querySelector('#fileinput-load').addEventListener('change', () => {
        let file = document.getElementById("fileinput-load").files[0];
        if(file.name.endsWith(".awpatch")) {
            let reader = new FileReader();
            reader.onload = () => {
                awpatch.parse(reader.result);
                refreshView();
            };
            reader.readAsText(file);
        }
        else {
            alert("Not .awpatch file");
        }
    });
    
    document.querySelector('#save-button').addEventListener('click', () => {
        const content = JSON.stringify(awpatch);

        const a = document.createElement('a');
        const file = new Blob([content], {type: 'application/json'});
        a.href= URL.createObjectURL(file);
        a.download = 'sfx.awpatch';
        a.click();
        
	    URL.revokeObjectURL(a.href);
    });
    
    // Load AWPatch from file
    if(window.readFile) {
        const filePath = window.location.search.substr(1);
        awpatch.parse(window.readFile(filePath, "utf-8"));
    }
    else {
        // Unhide load and save buttons if we are in web browser
        document.querySelector('#button-row').style.display = "";
    }
    saved_awpatch = awpatch.copy();

    refreshView();

    // Setup auto-save every 1 seconds
    window.setInterval(() => {
        if(!awpatch.match(saved_awpatch)) {
            if(window.saveFile) {
                let filePath = window.location.search.substr(1);
                window.saveFile(filePath, JSON.stringify(awpatch));
            }
            saved_awpatch = awpatch.copy();
        }
    }, 1000);
}

