fout = 960.0005e6
fref = 50e6;
R=1;

fpfd = 50e6
assert(fref/R == fpfd)

Adiv = 4
fvco = Adiv*fout;
fvco_fpfd = fvco/fpfd

ndiv = floor(fvco_fpfd)
fract = fvco_fpfd-ndiv
[f,m]=rat(fract,1e-9)
fout_synth = (ndiv+f/m)*fpfd/Adiv
ferror = fout - fout_synth