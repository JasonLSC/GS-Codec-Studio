# Alternative config add-ons

The configuration files in this directory can be used as add-ons to the CTC configuration files.

## Adaptive resolution for random access

`random_access_adaptive_resolution.cfg` may be used as an add-on to a random access CTC configuration file.
It enables adaptive resolution switching to achieve better visual quality.

## Reduced encoder run time

One of `reduced_runtime1.cfg`, `reduced_runtime2.cfg`, and `reduced_runtime3.cfg` may be used as an add-on to
an intra, random access, or low-delay CTC configuration file.
Each configuration file represent a difference trade-off between runtime reduction and compression performance.
`reduced_runtime3.cfg` yields the largest runtime reduction and `reduced_runtime1.cfg` the smallest.
The runtime reduction is achieved mainly by constraining the partitioning tree.



