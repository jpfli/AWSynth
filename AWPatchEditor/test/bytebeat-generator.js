class BytebeatGenerator extends AudioWorkletProcessor {
  constructor(options) {
    super(options);
    this.t = 0;
    this.rate = 8000/sampleRate;
  }
  
  process(inputs, outputs, parameters) {
    const output = outputs[0];
    const amplitude = parameters.amplitude;
    for (let channel = 0; channel < output.length; ++channel) {
      const outputChannel = output[channel];
      for (let i = 0; i < outputChannel.length; ++i) {
        const t = this.t | 0;
        this.t += this.rate;
        const o = ((t)>>4) | (t>>5) | t;
        outputChannel[i] = ((o&255) - 128) / 256;
      }
    }

    return true;
  }
}

registerProcessor('BytebeatGenerator', BytebeatGenerator);
