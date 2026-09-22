"""Bounded planform smoothing and fold-safe native river width metadata."""
import numpy as np
from scipy.ndimage import gaussian_filter1d
from scipy.interpolate import CubicHermiteSpline

def planform(xy):
    distance=np.r_[0,np.cumsum(np.linalg.norm(np.diff(xy,axis=0),axis=1))]
    keys=np.linspace(0,distance[-1],max(2,int(np.ceil(distance[-1]/12))+1))
    sampled=np.column_stack([np.interp(keys,distance,xy[:,axis]) for axis in (0,1)])
    if len(keys)>2:
        delta=gaussian_filter1d(sampled,12/(keys[1]-keys[0]),axis=0,mode='nearest')-sampled
        magnitude=np.linalg.norm(delta,axis=1)
        delta*=np.minimum(1,12/np.maximum(magnitude,1e-9))[:,None]
        delta*=np.minimum(1,np.minimum(keys,keys[-1]-keys)/24)[:,None]
        sampled+=delta
    tangent=np.gradient(sampled,axis=0)
    segments=np.linalg.norm(np.diff(sampled,axis=0),axis=1)
    limit=np.minimum(np.r_[segments[0],segments],np.r_[segments,segments[-1]])
    tangent*=np.minimum(1,limit/np.maximum(np.linalg.norm(tangent,axis=1),1e-9))[:,None]
    return sampled,tangent,keys,distance

def safe_widths(xy,tangent,nominal):
    spline=CubicHermiteSpline(np.arange(len(xy)),xy,tangent)
    widths=nominal.copy()
    for i in range(len(xy)-1):
        t=np.linspace(i,i+1,65)
        velocity,acceleration=spline(t,1),spline(t,2)
        curvature=np.abs(velocity[:,0]*acceleration[:,1]-velocity[:,1]*acceleration[:,0])/np.maximum(np.linalg.norm(velocity,axis=1)**3,1e-12)
        # Half-width <= 0.6 radius avoids an inverted inner bank, with sampling margin.
        widths[i:i+2]=np.minimum(widths[i:i+2],1.2/max(curvature.max(),1e-10))
    widths=np.minimum(widths,gaussian_filter1d(widths,1,mode='nearest'))
    # Limit widening to one metre per four metres of travel, in both directions.
    lengths=np.linalg.norm(np.diff(xy,axis=0),axis=1)
    for i in range(1,len(widths)):widths[i]=min(widths[i],widths[i-1]+lengths[i-1]*.25)
    for i in range(len(widths)-2,-1,-1):widths[i]=min(widths[i],widths[i+1]+lengths[i]*.25)
    if widths.min()<.05:raise ValueError('Degenerate river bend needs explicit source review')
    return widths
