//!MENU-ENTRY: Convert AW Patches

log("AW Patch convertion started");

Promise.all(
    dirRec(path.dirname("."))
    .filter( fileName => /\.awpatch$/i.test(fileName) )
    .map( patchName => exportPatch(patchName.replace(/\.awpatch/i, ".h"), JSON.parse(read(patchName))) )
).then(_=>{
    log("AW Patch convertion finished");
    if(hookTrigger == "pre-build") hookArgs[1]();
}).catch(_=>{
    if(hookTrigger == "pre-build") hookArgs[1]("Conversion error");
});

function dirRec(name){
    let out = [];
    dir(name).forEach(child=>{
        const fullChild = path.join(name, child);
        out = out.concat(stat(fullChild).isDirectory() ? dirRec(fullChild) : [fullChild]);
    });
    return out;
}

function exportPatch(filename, patch) {
    const wave_functions = {
        square:   "Audio::AWSynthSource::sqr",
        pulse:    "[](std::uint32_t p){return Audio::AWSynthSource::sqr(p,79);}",
        sawtooth: "Audio::AWSynthSource::saw",
        softsaw:  "Audio::AWSynthSource::saw<32>",
        triangle: "Audio::AWSynthSource::tri",
        sine:     "Audio::AWSynthSource::sin",
        noise:    "Audio::AWSynthSource::noise"
    };
    
    let name = filename.substr(0, filename.lastIndexOf("."));   // Remove filename extension
    name = name.replace( /\W/g , '_');                          // Replace every non alphanumeric character with '_'
    if(/^[0-9]./.test(name)) name = "_"+name;                   // Insert '_' if name starts with number
    
    let out = "#pragma once\n\n";
    out += "#include \"AWSynthSource.h\"\n\n";
    out += "constexpr auto "+path.basename(name)+" = Audio::AWPatch([](std::uint32_t t, std::uint32_t p)->std::int32_t {";
    
    if(patch.waveform.constructor !== Array) {
        patch.waveform = Array(32).fill(patch.waveform);
    }
    
    let num_wavetypes = 1;
    let type = patch.waveform[0];
    for(let idx=1; idx<patch.waveform.length; ++idx) {
        if(patch.waveform[idx] != type) {
            type = patch.waveform[idx];
            ++num_wavetypes;
        }
    }
    
    if(num_wavetypes > 1) {
        out += "\n";
        out += "        static constexpr std::int32_t (*wave_functions[])(std::uint32_t) = {"+wave_functions[patch.waveform[0]];
        for(let idx=1; idx<patch.waveform.length; ++idx) {
            out += ", "+wave_functions[patch.waveform[idx]];
        }
        out += "};\n";
        out += "        std::uint32_t step = t>>8;\n";
        //out += "        if(step >= "+patch.amplitudes.length+") step = "
        out += "        return wave_functions[";
        if(patch.amplitudes.loop_start < patch.amplitudes.length) {
            const loop = patch.amplitudes.loop_start;
            const len = patch.amplitudes.length;
            //out += patch.amplitudes.loop_start+" + step%("+(patch.amplitudes.length-patch.amplitudes.loop_start)+");\n";
            out += "(step<"+len+")?step:("+loop+"+step%"+(len-loop)+")](p);\n";
        }
        else {
            const len = patch.amplitudes.length;
            //out += (patch.amplitudes.length-1)+";\n";
            out += "(step<"+len+")?step:"+(len-1)+"](p);\n";
        }
        //out += "        return wave_functions[step](p);\n";
        out += "    })\n";
    }
    else {
        out += " return "+wave_functions[patch.waveform[0]]+"(p); })\n";
    }
    
    out += "    .volume("+patch.volume+").step("+patch.step+").release("+patch.release+").glide("+patch.glide+")\n";
    out += "    .semitones(Audio::AWPatch::Envelope(";
    out += patch.semitones.data.join();
    if('smooth' in patch.semitones) {
        out += ").smooth("+patch.semitones.smooth;
    }
    else if('effects' in patch.semitones) {
        out += ").effects("+patch.semitones.effects.map(item => { return ["step","attack","decay","slide"].indexOf(item); }).join();
    }
    out += ").loop("+patch.semitones.loop_start+","+patch.semitones.length+"))\n";
    out += "    .amplitudes(Audio::AWPatch::Envelope(";
    
    out += (patch.amplitudes.data.findIndex(item => item > 32) >= 0) ?
        patch.amplitudes.data.map(item => (item*31/100)|0).join() :
        patch.amplitudes.data.join();
    // out += patch.amplitudes.data.join();
    
    if('smooth' in patch.amplitudes) {
        out += ").smooth("+patch.amplitudes.smooth;
    }
    else if('effects' in patch.amplitudes) {
        out += ").effects("+patch.amplitudes.effects.map(item => { return ["step","attack","decay","slide"].indexOf(item); }).join();
    }
    out += ").loop("+patch.amplitudes.loop_start+","+patch.amplitudes.length+"));\n";
    
    write(filename, out, undefined);
    return true;
}

