"""Read original DATA_D1/D0 executable callback tables, not video timestamps."""
import struct
from native_resources import BASE,sha

def callbacks(data):
    function=struct.unpack_from('>I',data,8)[0]-BASE
    assert 0<=function<=len(data)-50
    assert data[function:function+4]==bytes.fromhex('30387896')
    assert data[function+8:function+16]==bytes.fromhex('d0403200d040d041')
    assert data[function+16:function+20]==bytes.fromhex('223b0022')
    assert data[function+42:function+50]==bytes.fromhex('303b00064efb0002')
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
