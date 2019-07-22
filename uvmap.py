#!/usr/bin/env python

import struct
import sys
import math
import cmath
import ephem
import numpy as np
import astropy.io.fits as pyfits
import scipy.fftpack
from scipy.optimize import fmin
ch_beg=368;
ch_end=1680;
ch_max=2048
nchannels=ch_end-ch_beg


min_f=ch_beg/2048.*250e6
max_f=ch_end/2048.*250e6

freq_per_ch=250e6/2048

delay=0*1.47
#delay=0
max_bl=229-89;
bl=229-89
#bl=2740
c=2.99792458E8
max_uv=max_bl/(c/max_f)
img_size=2048

sizeof_float=4

if len (sys.argv)<5:
    print("Usage:{0} <time> <XY> <XX> <YY> [delay]".format(sys.argv[0]))
    sys.exit(0)

if len(sys.argv)>=6:
    delay=float(sys.argv[5])

mxr=np.zeros([img_size,img_size])
mxi=np.zeros([img_size,img_size])
wgt=np.zeros([img_size,img_size])
vis_xy=open(sys.argv[2],'rb')
vis_xx=open(sys.argv[3],'rb')
vis_yy=open(sys.argv[4],'rb')
#sid_file=open(sys.argv[1],'r')

ant=ephem.Observer()
ant.lon='86.422551'
ant.lat='42.552729'

for strtime in open(sys.argv[1]):
    print strtime
    ant.date=strtime
    xy = np.array(struct.unpack('<{0}f'.format(nchannels * 2 ), vis_xy.read(nchannels*2*sizeof_float)))
    xx = np.array(struct.unpack('<{0}f'.format(nchannels * 2 ), vis_xx.read(nchannels*2*sizeof_float)))
    yy = np.array(struct.unpack('<{0}f'.format(nchannels * 2 ), vis_yy.read(nchannels*2*sizeof_float)))
    xy=xy[::2]+1j*xy[1::2]
    xx=xx[::2]
    yy=yy[::2]
    cross_corr=xy/np.sqrt(xx*yy)
    sid_angle=ant.sidereal_time()
    for i in range(0,nchannels):
        ch=ch_beg+i
        f=ch*freq_per_ch
        lbd=c/f
        cc=cross_corr[i]*np.exp(1j*delay/lbd*2*np.pi)
        u=bl*math.cos(sid_angle)/lbd
        v=bl*math.sin(sid_angle)/lbd
        ui=int(u/max_uv*(img_size/2)+img_size/2)
        vi=int(v/max_uv*(img_size/2)+img_size/2)
        if ui>=0 and ui<img_size and vi>=0 and vi<img_size and not math.isnan(cross_corr[i].real) and not math.isnan(cross_corr[i].imag):
            mxr[vi,ui]+=cc.real
            mxi[vi,ui]+=cc.imag
            wgt[vi,ui]+=1
          
print "fill uvmap"
        
for i in range(0,img_size):
    for j in range(0,img_size):
        if wgt[i,j]>0:
            mxr[i,j]/=wgt[i,j]
            mxi[i,j]/=wgt[i,j]
        if np.sqrt(mxr[i,j]**2+mxi[i,j]**2)>.007:
            mxr[i,j]=0
            mxi[i,j]=0



pyfits.PrimaryHDU(mxr).writeto('uvr.fits',clobber=True)
pyfits.PrimaryHDU(mxi).writeto('uvi.fits',clobber=True)

mx=mxr+1j*mxi
mx+=np.conj(mx[::-1, ::-1])
img=scipy.fftpack.fftshift(scipy.fftpack.fft2(scipy.fftpack.fftshift(mx))).real
pyfits.PrimaryHDU(img).writeto('img.fits',clobber=True)
