
parcat - Concatenation tool for parallel simulations
====================================================

This tool allows to cat segments from parallel simulation according to JVET-B0036 into bitstream bitexact with corresponding output of sequential simulation.

What it does
------------

This tool

- removes any duplicated and unnesessary information from segments like SPS, PPS, VPS, IDR-frames, SEI from all segmetns but first.
- adjust POC value to provide continious numbering and correct referencing (actual POC modification occurs only for second and folowing segments)
- cat filtered segments into single file

Output of this tool is decodable JEM bitstream.

Usage
-----

```
parcat <segment1> [<segment2> ... <segmentN>] <outfile>
```

where `<segment_i>` is result of parallel simulation according to JVET-B0036.

Building
--------

The tool is quite simple and is stored in one C++ file. You can build it using any decent C++98 compiler using command line.

Alternatevily cmake build system scripts are provided to maintain cross platform experience and simplify generation of IDE-s projects like Visual Studio.

To use it

- install cmake from [cmake.org](http://cmake.org)
- create build folder inside project root folder (where CMakeLists.txt is located)
- cd to this folder
- configure project using following command line `cmake ..`
- build project using your platform's default build system, say Visual Studio on Windows and make on UNIX

Restrictions
------------

parcat currently supports only JEM-2.0 and JEM-3.0 bitstreams. Support for HM 16.9 bitstreams is scheduled.



