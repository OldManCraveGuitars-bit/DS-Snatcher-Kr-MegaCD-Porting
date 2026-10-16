"""Original stereo CDDA to a16-bit32kHz DS stream, without gameplay recordings.

The DS Maxmod stream API accepts at most32768Hz. A64-tap windowed-sinc lowpass
preserves both channels and16-bit depth. NumPy is an optional acceleration;
the standard-library path implements the same kernel and rounding.
"""
import struct,hashlib,math,sys
from array import array
RATE=32000;SOURCE_RATE=44100;TAPS=64;HALF=TAPS//2
def kernels():
    result=[];cutoff=RATE/SOURCE_RATE*.95
    for phase in range(320):
        row=[];fraction=phase/320
        for offset in range(-HALF+1,HALF+1):
            distance=offset-fraction;x=math.pi*cutoff*distance
            value=cutoff*(math.sin(x)/x if x else 1)
            window=.5+.5*math.cos(math.pi*distance/HALF) if abs(distance)<HALF else 0
            row.append(value*window)
        total=sum(row);result.append(tuple(v/total for v in row))
    return result
def resample(pcm,accelerate=True):
    if len(pcm)%4:raise ValueError('Truncated original stereo CDDA')
    frames=len(pcm)//4;count=(frames*320+440)//441;weights=kernels()
    try:
        if not accelerate:raise ImportError
        import numpy as np
    except ImportError:
        source=array('h');source.frombytes(pcm)
        if sys.byteorder!='little':source.byteswap()
        output=array('h')
        for n in range(count):
            whole,phase=divmod(n*441,320);coefficients=weights[phase]
            values=[0.,0.]
            for j,weight in enumerate(coefficients):
                at=whole+j-HALF+1
                if 0<=at<frames:
                    values[0]+=source[at*2]*weight;values[1]+=source[at*2+1]*weight
            output.extend(max(-32768,min(32767,round(value))) for value in values)
        if sys.byteorder!='little':output.byteswap()
        return output.tobytes()
    source=np.frombuffer(pcm,dtype='<i2').reshape(-1,2);output=np.empty((count,2),dtype='<i2')
    table=np.array(weights,dtype=np.float64)
    for first in range(0,count,32768):
        last=min(first+32768,count);positions=np.arange(first,last,dtype=np.int64)*441
        whole,phase=positions//320,positions%320
        values=np.zeros((last-first,2),dtype=np.float64)
        # Fixed summation order equals the portable path; no parallel reduction.
        for j in range(TAPS):
            at=whole+j-HALF+1;valid=(at>=0)&(at<frames)
            values[valid]+=source[at[valid]]*table[phase[valid],j,None]
        output[first:last]=np.clip(np.rint(values),-32768,32767).astype('<i2')
    return output.tobytes()
def build(wavs,tracks=(3,4,5)):
    outputs={};records=[]
    for track in tracks:
        name=f'original-cdda-{track:02d}.wav'
        if name not in wavs:continue
        wav=wavs[name]
        expected=struct.pack('<4sI4s4sIHHIIHH4sI',b'RIFF',len(wav)-8,b'WAVE',b'fmt ',16,
            1,2,SOURCE_RATE,SOURCE_RATE*4,4,16,b'data',len(wav)-44)
        if wav[:44]!=expected:raise ValueError('Expected original uncompressed CDDA WAV')
        audio=resample(wav[44:]);frames=len(audio)//4
        output=struct.pack('<4sHHIIII',b'NST1',1,2,RATE,frames,24,16)+audio
        key=f'native-cdda-{track:02d}-r22.nsta';outputs[key]=output
        records.append(dict(track=track,bytes=len(output),channels=2,sample_bits=16,
            sample_rate=RATE,sample_frames=frames,filter_taps=TAPS,
            source_pcm_sha256=hashlib.sha256(wav[44:]).hexdigest(),
            pcm_sha256=hashlib.sha256(audio).hexdigest(),sha256=hashlib.sha256(output).hexdigest(),
            emulator_audio_input=False,source_writes=0,
            limits=['Resampled from44100Hz for the DS32768Hz stream limit; not byte-identical PCM']))
    return outputs,records
