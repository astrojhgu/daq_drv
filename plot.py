#!/usr/bin/env python

import matplotlib
matplotlib.use("AGG")
import numpy as np
import matplotlib.pylab as plt

import sys

if len(sys.argv)<2:
    sys.exit(-1)

corr=np.loadtxt(sys.argv[1]);

if len(sys.argv)==2:
    auto1=1.0
    auto2=1.0
elif len(sys.argv)==4:
    auto1=np.loadtxt(sys.argv[2])[:,1]
    auto2=np.loadtxt(sys.argv[3])[:,1]

for i in range(1, corr.shape[1]):
    corr[:,i]/=np.sqrt(auto1*auto2)

if corr.shape[1]==3:
    plt.plot(corr[:,0], corr[:,1])
    plt.plot(corr[:,0], corr[:,2])
elif corr.shape[1]==2:
    plt.plot(corr[:,0], np.log10(corr[:,1])*10)

plt.tight_layout()
plt.savefig('corr.png')
plt.show()
