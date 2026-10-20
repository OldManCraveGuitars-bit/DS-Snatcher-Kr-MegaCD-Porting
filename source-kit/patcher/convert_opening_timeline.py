"""Read original DATA_D1/D0 executable callback tables, not video timestamps."""
from source_validation import matches_fingerprint, find_fingerprint, unique_fingerprint
import struct
from native_resources import BASE,sha

def callbacks(data):
    function=struct.unpack_from('>I',data,8)[0]-BASE
    assert 0<=function<=len(data)-50
    assert matches_fingerprint(data[function:function + 4],4,'9e5d843874202ed60fe613886d09f0296189e09995ccaebffc29524413803123')
    assert matches_fingerprint(data[function + 8:function + 16],8,'a0a903fd46d63243cf84f1ec510a7a822fca7d8fe508bec1937b367fc239bab9')
    assert matches_fingerprint(data[function + 16:function + 20],4,'dcaaea7e2e979b7cfc33cd17b01155c10c03a9c88f07e33a713c2694e3a07b7c')
    assert matches_fingerprint(data[function + 42:function + 50],8,'8f4bf35389f17a8d9f0c56dfb1b620b8236723c42dbc83db263dff23bb76b541')
    assert data[function+4:function+6]==b'\x6b\x00'
    table=function+50
    # The signed-index BMI target is a separate routine immediately after the
    # table (D0 uses it to start CD5 and await readiness). Minimum handler
    # offsets overcount the table because that routine occupies the gap.
    stop=function+6+struct.unpack_from('>h',data,function+6)[0]
    assert table<stop<len(data) and (stop-table)%6==0
    count=(stop-table)//6
    records=[]
    for index in range(count):
        relative,time=struct.unpack_from('>HI',data,table+index*6)
        handler=table+relative
        assert handler<len(data)
        digits=[time>>24&255,time>>16&255,time>>8&255]
        assert time&255==0
        decoded=[]
        for value in digits:
            assert value&15<=9 and value>>4<=9
            decoded.append((value>>4)*10+(value&15))
        minute,second,frame=decoded
        assert second<60 and frame<75
        records.append(dict(index=index,clock_bcd=time,cd_frame=(minute*60+second)*75+frame,
                            handler=BASE+handler))
    # D0 contains deliberately immediate (zero-time) callbacks between timed
    # events. Preserve native order; sorting would change the opening.
    return function,records

def build(source):
    tracks=[]
    for track,file_id,name in ((3,3,'DATA_D1.BIN'),(4,2,'DATA_D0.BIN')):
        data=source.read_file(name) if hasattr(source,'read_file') else (source/name).read_bytes()
        function,records=callbacks(data)
        tracks.append(dict(track=track,file_id=file_id,file=name,function=BASE+function,
                           resource_sha256=sha(data),records=records))
    output=bytearray(16+len(tracks)*16)
    for i,track in enumerate(tracks):
        offset=len(output)
        struct.pack_into('<HHIII',output,16+i*16,track['track'],track['file_id'],
                         len(track['records']),offset,track['function'])
        for record in track['records']:output+=struct.pack('<II',record['clock_bcd'],record['handler'])
    struct.pack_into('<4sHHII',output,0,b'NSTM',1,len(tracks),16,0)
    report=dict(kind='original_opening_callback_tables',tracks=tracks,bytes=len(output),sha256=sha(output),
                observed_video_timestamps=False,source_writes=0,
                limits=['Native handler execution/rendering is still being connected',
                        'Original CD clock is BCD minute/second/frame,75 frames per second'])
    return bytes(output),report
