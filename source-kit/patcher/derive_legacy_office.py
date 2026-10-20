"""Recreate the historical save-compatible office subtree from user SP06 bytes.

The current full-story graph is independent. Older legitimate port saves still
use this small interpreter, so its source commands are regenerated as well.
"""
import hashlib,struct
from source_story_text import build as text_index


def build(source):
    data=source.read_file('SP06.BIN');sub=source.read_file('SUBCODE.BIN')
    if hashlib.sha256(data).hexdigest()!='47f2c8a5d6f5bff67b8246fd2ce1b7a143a06ace6547191992945efbd9c40714':
        raise ValueError('Unsupported original office script')
    if hashlib.sha256(sub).hexdigest()!='37d2bb27883acfedd6b05585fa5189c832bc6c94d37b3f5cf7c9a241142c88ac':
        raise ValueError('Unsupported original office interpreter')
    descriptors={0x0c:'05701e0c',0x19:'01481a6e',0x1b:'01401a52',0x25:'04401a12',0x26:'04801a12',
        0x27:'04701a12',0x2d:'00001c2c',0x30:'04001a08',0x34:'03401c18',0x38:'02401b94',
        0x3e:'01401b4c',0x3f:'01401af0',0x43:'04001ec0'}
    if any(sub[0x98dc+op*4:0x98e0+op*4].hex()!=value for op,value in descriptors.items()):
        raise ValueError('Original office expression descriptor mismatch')
    offsets={int(e['text_offset'],16) for e in text_index(source,('SP06.BIN',))['files'][0]['entries']}
    nodes=[];memo={};active=set();texts=set()
    def word(pc):return int.from_bytes(data[pc:pc+2],'big')
    def text(at):
        if at not in offsets:raise ValueError('Invalid original office text offset')
        texts.add(at);return at
    def parse(pc):
        if not (0x7eb<=pc<0x872 or 0xc0a<=pc<0xc59):raise ValueError('Office expression outside supported legacy subtree')
        if pc in memo:return memo[pc]
        if pc in active:raise ValueError('Cyclic office expression')
        active.add(pc);op=data[pc]
        if op not in descriptors:raise ValueError('Unknown office expression')
        node=dict(pc=pc,op=op,arg=0,left=-1,right=-1,speaker=0);index=len(nodes);nodes.append(node)
        if op==0x43:
            if data[pc+1:pc+3]!=b'\x20\0':raise ValueError('Unsupported office speaker tuple')
            node.update(arg=text(word(pc+4)),speaker=data[pc+3]);end=pc+6
        elif op==0x0c:
            if word(pc+1)!=0xc0a:raise ValueError('Unknown office subroutine')
            node['left']=parse(0xc0a)[0];end=pc+3
        elif op==0x2d:end=pc+1
        elif op in (0x27,0x30):node['left'],end=parse(pc+1)
        elif op==0x25:
            node['left'],at=parse(pc+1);node['right'],end=parse(at)
        elif op==0x26:
            node['left'],end=parse(pc+3)
            if end!=word(pc+1):raise ValueError('Office linked-sequence boundary')
            node['right'],end=parse(end)
        elif op==0x19:
            node['arg']=word(pc+3);node['left'],end=parse(pc+5)
            if end!=word(pc+1):raise ValueError('Office conditional boundary')
            node['right'],end=parse(end)
        else:
            node['arg']=word(pc+1)
            if op==0x38:text(node['arg'])
            node['left'],end=parse(pc+3)
        memo[pc]=(index,end);active.remove(pc);return index,end
    root,end=parse(0x7eb)
    if root!=0 or end!=0x872:raise ValueError('Original office subtree root')
    labels=[]
    def collect(i):
        n=nodes[i]
        if n['op'] in (0x25,0x26):collect(n['left']);collect(n['right'])
        elif n['op']==0x38:labels.append((n['arg'],n['left']))
        else:raise ValueError('Unsupported office menu tree')
    collect(root);text_ids={at:i for i,at in enumerate(sorted(texts))}
    for n in nodes:
        if n['op'] in (0x38,0x43):n['arg']=text_ids[n['arg']]
    if len(nodes)!=64 or len(texts)!=23 or len(labels)!=5:raise ValueError('Unexpected office compatibility subtree shape')
    arrays=dict(office_nodes=b''.join(struct.pack('<3H2hH',*(n[k] for k in ('pc','op','arg','left','right','speaker'))) for n in nodes),
        office_menu_text=struct.pack('<5I',*(text_ids[t] for t,_ in labels)),office_menu_actions=struct.pack('<5I',*(i for _,i in labels)))
    header=['#pragma once','#include <stdint.h>',
        'typedef struct { uint16_t pc,op,arg; int16_t left,right; uint16_t speaker; } OfficeNode;',
        'static const OfficeNode office_nodes[] = {']
    header+=['{'+','.join(str(n[k]) for k in ('pc','op','arg','left','right','speaker'))+'},' for n in nodes]
    header+=['};','static const unsigned office_menu_text[] = {'+','.join(str(text_ids[t]) for t,_ in labels)+'};',
        'static const unsigned office_menu_actions[] = {'+','.join(str(i) for _,i in labels)+'};',
        '#define OFFICE_NODE_COUNT 64','#define OFFICE_TEXT_COUNT 23','#define OFFICE_MENU_COUNT 5']
    return arrays,'\n'.join(header)
