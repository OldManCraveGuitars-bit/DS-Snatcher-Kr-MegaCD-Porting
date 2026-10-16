"""Exact original CDDA sectors to PCM WAV; no emulator recording or mixing."""
from pathlib import Path
import struct,hashlib

def build(source,numbers=None):
    outputs={};records=[]
    if not source.tracks:return outputs,records
    for index,track in enumerate(source.tracks):
        if track['mode']!='AUDIO' or (numbers is not None and track['number'] not in numbers):continue
        path=Path(track['path']);before=(path.stat().st_size,path.stat().st_mtime_ns)
        if before[0]%2352:raise ValueError('Audio BIN length is not a whole CD sector')
        start=track['indexes'][1];end=before[0]//2352
        for following in source.tracks[index+1:]:
            if following['path']==path:
                end=following['indexes'].get(0,following['indexes'][1]);break
        if not 0<=start<end<=before[0]//2352:raise ValueError('Invalid original CDDA extent')
        size=(end-start)*2352
        with path.open('rb') as stream:
            stream.seek(start*2352);pcm=stream.read(size)
        if len(pcm)!=size:raise ValueError('Truncated original CDDA')
        if (path.stat().st_size,path.stat().st_mtime_ns)!=before:raise ValueError('Audio source changed during conversion')
        # CUE/BIN CDDA is signed16-bit little-endian LR interleaved,44100Hz.
        # Each2352-byte sector contains588 stereo sample frames.
        header=struct.pack('<4sI4s4sIHHIIHH4sI',b'RIFF',36+size,b'WAVE',b'fmt ',16,
                           1,2,44100,44100*4,4,16,b'data',size)
        name=f'original-cdda-{track["number"]:02d}.wav';outputs[name]=header+pcm
        row=dict(track=track['number'],start_lba=start,end_lba=end,sectors=end-start,
                 sample_frames=size//4,channels=2,sample_bits=16,sample_rate=44100,
                 pcm_sha256=hashlib.sha256(pcm).hexdigest(),bytes=size+44,
                 wav_sha256=hashlib.sha256(outputs[name]).hexdigest(),source_writes=0,
                 emulator_audio_input=False,nds_stream_conversion_pending=True)
        records.append(row);source.used[f'CDDA{track["number"]:02d}']=dict(bytes=size,sha256=row['pcm_sha256'])
    return outputs,records
