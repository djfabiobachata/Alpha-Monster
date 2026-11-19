#!/usr/bin/env python3
"""Generates synthetic test material for Alpha Monster (numpy + scipy).

reference "master" + its stems (ref_*.wav), an unmastered mix of a different
song (my_mix.wav) + its tracks (my_*.wav). Used by CI and by am_snapshot."""
import sys, os
import numpy as np, scipy.signal as ss, scipy.io.wavfile as wf
out=sys.argv[1] if len(sys.argv)>1 else '.'
os.makedirs(out,exist_ok=True)
sr=44100; T=30; n=sr*T; rng=np.random.default_rng(7); t=np.arange(n)/sr
def bp(x,lo,hi,o=2): return ss.lfilter(*ss.butter(o,[lo/(sr/2),hi/(sr/2)],'band'),x)
def lp(x,f,o=2): return ss.lfilter(*ss.butter(o,f/(sr/2)),x)
def hp(x,f,o=2): return ss.lfilter(*ss.butter(o,f/(sr/2),'high'),x)
def pan(m,p): th=(p+1)*np.pi/4; return np.vstack([m*np.cos(th),m*np.sin(th)])
def ir(rt):
    L=int(rt*sr); tt=np.arange(L)/sr
    return np.vstack([rng.standard_normal(L)*10**(-3*tt/rt) for _ in range(2)])*0.015
def rev(st,rt,wet):
    h=ir(rt); out=st.copy()
    for c in range(2): out[c]+=wet*ss.fftconvolve(st.mean(0),h[c])[:n]
    return out
bpm=120; beat=60/bpm
kick=np.zeros(n); snare=np.zeros(n); hat=np.zeros(n)
for k in np.arange(0,T,beat):
    i=int(k*sr); L=int(0.3*sr); tt=np.arange(L)/sr
    s=np.sin(2*np.pi*(50+120*np.exp(-tt/0.03))*tt)*np.exp(-tt/0.12)
    kick[i:i+L]+=s[:len(kick[i:i+L])]
for k in np.arange(beat,T,2*beat):
    i=int(k*sr); L=int(0.25*sr); tt=np.arange(L)/sr
    s=bp(rng.standard_normal(L),300,6000)*np.exp(-tt/0.07)*2
    snare[i:i+L]+=s[:len(snare[i:i+L])]
for k in np.arange(0,T,beat/2):
    i=int(k*sr); L=int(0.06*sr); tt=np.arange(L)/sr
    s=hp(rng.standard_normal(L),7000)*np.exp(-tt/0.015)
    hat[i:i+L]+=s[:len(hat[i:i+L])]
notes=[55,55,73.4,65.4]
bass=np.zeros(n)
for j,k in enumerate(np.arange(0,T,2*beat)):
    i=int(k*sr); L=int(2*beat*sr); tt=np.arange(L)/sr; f=notes[j%4]
    s=np.tanh(2*np.sin(2*np.pi*f*tt))*np.minimum(1,tt/0.01)*np.exp(-tt/1.5)
    bass[i:i+L]+=s[:len(bass[i:i+L])]
bass=lp(bass,400)
f0=220*(1+0.01*np.sin(2*np.pi*5*t)); ph=np.cumsum(2*np.pi*f0/sr)
voc=sum(np.sin(k*ph)/k**1.2 for k in range(1,16))*((t%4)<3)*(1+0.3*np.sin(2*np.pi*0.25*t))
voc=bp(voc,150,8000)
gl=bp(rng.standard_normal(n),200,3000); gr=bp(rng.standard_normal(n),200,3000)
chord=np.vstack([gl,gr])*0.25
drums=np.vstack([kick,kick])*0.9+pan(snare,0.05)*0.5+pan(hat,0.35)*0.25
def write(name,x):
    wf.write(os.path.join(out,name),sr,(x.T).astype(np.float32))
def lufs_norm(x,target):
    import math
    return x*10**((target-(10*np.log10(np.mean(x**2)*2)-0.691))/20)
# own mix (unmastered): darker, narrower, dynamic
own= drums*0.9 + pan(bass,0)*0.7 + rev(np.vstack([voc,voc])*0.5,1.2,0.25) + chord*0.5
own=np.vstack([lp(own[0],9000),lp(own[1],9000)])
own=own/np.max(np.abs(own))*0.5
write('my_mix.wav',own)
# stems of own mix for the mix app test
write('my_drums.wav',drums*0.9/np.max(np.abs(drums))*0.5)
write('my_bass.wav',pan(bass,0)*0.7/np.max(np.abs(bass))*0.5)
write('my_vocals.wav',np.vstack([voc,voc])/np.max(np.abs(voc))*0.5)
write('my_keys.wav',chord/np.max(np.abs(chord))*0.4)
# reference master: brighter, wider, compressed, loud
ref= drums*1.0 + pan(bass,0)*0.8 + rev(pan(voc,0)*0.7,2.0,0.45) + np.vstack([gl,gr])*0.35
ref=np.vstack([ref[0]+0.4*hp(ref[0],5000), ref[1]+0.4*hp(ref[1],5000)])
m=(ref[0]+ref[1])/2; s=(ref[0]-ref[1])/2*1.4; ref=np.vstack([m+s,m-s])
ref=ref/np.max(np.abs(ref))
ref=np.tanh(ref*4)/np.tanh(4)*0.89
write('ref_master.wav',ref)
# reference stems (stand-in for the AI separation in tests)
def proc(x):
    x=np.vstack([x[0]+0.4*hp(x[0],5000), x[1]+0.4*hp(x[1],5000)])
    m=(x[0]+x[1])/2; s=(x[0]-x[1])/2*1.4; return np.vstack([m+s,m-s])
comps={'drums':drums*1.0,'bass':pan(bass,0)*0.8,'vocals':rev(pan(voc,0)*0.7,2.0,0.45),'other':np.vstack([gl,gr])*0.35}
full=proc(sum(comps.values())); k=1/np.max(np.abs(full))
for name,x in comps.items(): write('ref_'+name+'.wav', proc(x)*k*0.89)
