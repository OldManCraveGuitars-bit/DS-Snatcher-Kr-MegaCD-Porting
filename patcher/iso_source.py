"""Read-only user-supplied CUE/BIN, ISO or extracted ISO directory."""
from pathlib import Path
import re, hashlib, struct

def digest_file(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda:f.read(8*1024*1024),b''):h.update(block)
    return h.hexdigest()

class SourceDisc:
    def __init__(self,path):
        self.path=Path(path).resolve(strict=True);self.used={};self.records={}
        self.stream=None;self.sector_size=2048;self.offset=0;self.data_start=0
        self.tracks=[]
        if self.path.is_dir():
            self.directory=True;return
        self.directory=False;data_path=self.path
        if self.path.suffix.lower()=='.cue':
            current=None;track=None
            for line in self.path.read_text(encoding='utf-8-sig').splitlines():
                m=re.match(r'\s*FILE\s+"([^"]+)"\s+BINARY\s*$',line,re.I)
                if m:current=(self.path.parent/m[1]).resolve(strict=True)
                m=re.match(r'\s*TRACK\s+(\d+)\s+(\S+)\s*$',line,re.I)
                if m:
                    if current is None:raise ValueError('CUE track has no FILE')
                    track=dict(number=int(m[1]),mode=m[2].upper(),path=current,indexes={})
                    self.tracks.append(track)
                m=re.match(r'\s*INDEX\s+(\d+)\s+(\d+):(\d+):(\d+)\s*$',line,re.I)
                if m:
                    if track is None:raise ValueError('CUE index has no TRACK')
                    minute,second,frame=map(int,m.groups()[1:])
                    if second>=60 or frame>=75:raise ValueError('Invalid CUE index')
                    track['indexes'][int(m[1])]=(minute*60+second)*75+frame
            data=[t for t in self.tracks if t['mode'].startswith('MODE1/')]
            if len(data)!=1:raise ValueError('Expected one MODE1 data track')
            track=data[0];data_path=track['path'];self.data_start=track['indexes'][1]
            self.sector_size=int(track['mode'].split('/')[1])
        elif self.path.suffix.lower()=='.bin':self.sector_size=2352
        elif self.path.suffix.lower()!='.iso':raise ValueError('Use CUE, BIN, ISO or extracted directory')
        if self.sector_size not in (2048,2352):raise ValueError('Unsupported sector format')
        self.offset=16 if self.sector_size==2352 else 0
        self.data_path=data_path;self.stream=data_path.open('rb')
        self.original_stat=(data_path.stat().st_size,data_path.stat().st_mtime_ns)
        pvd=self.read_sectors(16,1)
        if pvd[:7]!=b'\x01CD001\x01':raise ValueError('ISO9660 volume descriptor not found')
        root=self._record(pvd,156)
        self._directory(root['lba'],root['size'],'',set())

    def read_sectors(self,lba,count):
        if not self.stream or lba<0 or count<0:raise ValueError('Invalid sector read')
        output=bytearray()
        for sector in range(lba,lba+count):
            at=(self.data_start+sector)*self.sector_size
            self.stream.seek(at)
            raw=self.stream.read(self.sector_size)
            if len(raw)!=self.sector_size:raise ValueError('Truncated source image')
            if self.sector_size==2352 and (raw[:12]!=b'\0'+b'\xff'*10+b'\0' or raw[15]!=1):
                raise ValueError('Expected Mode 1 sector')
            output.extend(raw[self.offset:self.offset+2048])
        return bytes(output)

    def _record(self,b,at):
        size=b[at]
        if size<34 or at+size>len(b):raise ValueError('Invalid directory record')
        lba,length=struct.unpack_from('<I',b,at+2)[0],struct.unpack_from('<I',b,at+10)[0]
        if lba!=struct.unpack_from('>I',b,at+6)[0] or length!=struct.unpack_from('>I',b,at+14)[0]:
            raise ValueError('Directory endian copies disagree')
        name=b[at+33:at+33+b[at+32]]
        return dict(length=size,lba=lba,size=length,flags=b[at+25],name=name)

    def _directory(self,lba,size,prefix,visited):
        if lba in visited:raise ValueError('Cyclic ISO directory')
        visited.add(lba);raw=self.read_sectors(lba,(size+2047)//2048)[:size];p=0
        while p<size:
            if not raw[p]:p=(p//2048+1)*2048;continue
            row=self._record(raw,p);p+=row['length']
            if row['name'] in (b'\0',b'\1'):continue
            name=row['name'].decode('ascii').split(';')[0]
            if '/' in name or '\\' in name or name in ('.','..'):raise ValueError('Unsafe ISO name')
            name=prefix+name
            if row['flags']&2:self._directory(row['lba'],row['size'],name+'/',visited)
            else:self.records[name.upper()]=row

    def read_file(self,name):
        if not re.fullmatch(r'[A-Z0-9_]+\.BIN',name):raise ValueError('Unsupported resource name')
        if self.directory:data=(self.path/name).read_bytes()
        else:
            row=self.records[name];data=self.read_sectors(row['lba'],(row['size']+2047)//2048)[:row['size']]
        self.used[name]=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
        return data

    def close(self):
        if self.stream:self.stream.close();self.stream=None
        if not self.directory:
            now=(self.data_path.stat().st_size,self.data_path.stat().st_mtime_ns)
            if now!=self.original_stat:raise ValueError('Input changed during conversion')
    def __enter__(self):return self
    def __exit__(self,*args):self.close()
