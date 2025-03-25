### trace-gen

Help:

```
Allowed options:
  -h [ --help ]                   Produce this message
  -m [ --addresses ] arg          Footprint size (number of unique addresses)
  -n [ --length ] arg             Length of trace (in addresses)
  -p [ --p_irm ] arg              Probability of the trace that is IRM (float
                                  between 0 and 1)
  -s [ --seed ] arg (=42)         RNG seed
  -b [ --blocksize ] arg (=4096)  Size of a block in bytes
  -f [ --ird ] arg (=b)           IRD distribution. Can be one of the
                                  pre-specified distributions (b to f) or
                                  inputs to fgen (k # of classes, non-spike
                                  heights, and indices of spikes) separated by
                                  columns. Example: -f b or -f
                                  fgen:10000:0.00001:3,5,10,20
  -g [ --irm ] arg (=zipf:1.2,20) IRM distribution. Can be: zipf:alpha,n,
                                  pareto:xm,a,n, uniform:max,
                                  normal:mean,stddev).
  -r [ --rwratio ] arg (=1)       Fraction of addresses that are reads (vs
                                  writes)
  -z [ --sizedist ] arg (=1:1)    Distribution of request sizes in
                                  blocks.Specified as a list of weights
                                  (floats) followed by a list of sizes in
                                  blocks (ints).Ex: 1,1,1:1,3,4 means equal
                                  chance of 1, 3, or 4-block requests
```

Examples:

```
# 10k address space, 100 address trace, 50% IRM, type 'c' IRD, 4k block size, 
# 50% reads, 50% writes, sizes are evenly distributed between 1 and 2 blocks
./trace-gen -m 10000 -n 100 -p 0.5 -f c -r 0.5 -z 1,1:1,2

# seed rng with 42
./trace-gen -m 10000 -n 100 -p 0.5 -f c -r 0.5 -z 1,1:1,2 -s 42

# make the trace all reads instead (default behaviour)
./trace-gen -m 10000 -n 100 -p 0.5 -f c -z 1,1:1,2 -s 42

# have a 25/25/50 split of 1, 3, and 4 block requests
./trace-gen -m 10000 -n 100 -p 0.5 -f c -z 1,1,2:1,3,4 -s 42

# set blocksize to one (so generated addresses are adjacent)
./trace-gen -m 10000 -n 100 -p 0.5 -f c -z 1,1,2:1,3,4 -s 42 -b 1
```

### Update
#### Gen from 2D
```bash
build/2d-tracegen \
  --addresses 100 \
  --length 1000 \
  --p_irm 0.3 \
  --ird b \
  --irm zipf:1.2,20 \
  --blocksize 4096 \
  --rwratio 1 \
  --sizedist 1,1:1,4
```
or  
```bash
build/2d-tracegen \
  --addresses 100 \
  --length 1000 \
  --p_irm 0.3 \
  --ird b \
  --irm "2,8" \
  --blocksize 4096 \
  --rwratio 1 \
  --sizedist "1,1:1,4"
```

#### Gen from KD
```bash
build/kd-tracegen \
  --addresses 10 \
  --length 100 \
  --groups 2 \
  --ird "fgen:100:0.00001:3,5,20;fgen:100:0.005:3,5,10,20" \
  --irm "7,3" \
  --blocksize 4096 \
  --rwratio 1 \
  --sizedist 1,1:1,4
```
or   
```bash
build/kd-tracegen \
  --addresses 10 \
  --length 100 \
  --groups 2 \
  --ird "fgen:100:0.00001:3,5,20;fgen:100:0.005:3,5,10,20" \
  --irm "zipf:1.2,2" \
  --blocksize 4096 \
  --rwratio 1 \
  --sizedist "1,1:1,4"
```