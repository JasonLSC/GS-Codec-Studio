/* The copyright in this software is being made available under the BSD
* License, included below. This software may be subject to other third party
* and contributor rights, including patent rights, and no such rights are
* granted under this license.
*
* Copyright (c) 2010-2022, ITU/ISO/IEC
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*  * Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
*    be used to endorse or promote products derived from this software without
*    specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

/** \file     SEIFilmGrainSynthesizer.cpp
    \brief    SMPTE RDD5 based film grain synthesis functionality from SEI messages
*/

#include "SEIFilmGrainSynthesizer.h"

#include <stdio.h>
#include <cmath>

#if JVET_X0048_X0103_FILM_GRAIN

/* static look up table definitions */
static const int8_t gaussianLUT[2048] =
{
-11, 12, 103, -11, 42, -35, 12, 59, 77, 98, -87, 3, 65, -78, 45, 56,
-51, 21, 13, -11, -20, -19, 33, -127, 17, -6, -105, 18, 19, 71, 48, -10,
-38, 42, -2, 75, -67, 52, -90, 33, -47, 21, -3, -56, 49, 1, -57, -42,
-1, 120, -127, -108, -49, 9, 14, 127, 122, 109, 52, 127, 2, 7, 114, 19,
30, 12, 77, 112, 82, -61, -127, 111, -52, -29, 2, -49, -24, 58, -29, -73,
12, 112, 67, 79, -3, -114, -87, -6, -5, 40, 58, -81, 49, -27, -31, -34,
-105, 50, 16, -24, -35, -14, -15, -127, -55, -22, -55, -127, -112, 5, -26, -72,
127, 127, -2, 41, 87, -65, -16, 55, 19, 91, -81, -65, -64, 35, -7, -54,
99, -7, 88, 125, -26, 91, 0, 63, 60, -14, -23, 113, -33, 116, 14, 26,
51, -16, 107, -8, 53, 38, -34, 17, -7, 4, -91, 6, 63, 63, -15, 39,
-36, 19, 55, 17, -51, 40, 33, -37, 126, -39, -118, 17, -30, 0, 19, 98,
60, 101, -12, -73, -17, -52, 98, 3, 3, 60, 33, -3, -2, 10, -42, -106,
-38, 14, 127, 16, -127, -31, -86, -39, -56, 46, -41, 75, 23, -19, -22, -70,
74, -54, -2, 32, -45, 17, -92, 59, -64, -67, 56, -102, -29, -87, -34, -92,
68, 5, -74, -61, 93, -43, 14, -26, -38, -126, -17, 16, -127, 64, 34, 31,
93, 17, -51, -59, 71, 77, 81, 127, 127, 61, 33, -106, -93, 0, 0, 75,
-69, 71, 127, -19, -111, 30, 23, 15, 2, 39, 92, 5, 42, 2, -6, 38,
15, 114, -30, -37, 50, 44, 106, 27, 119, 7, -80, 25, -68, -21, 92, -11,
-1, 18, 41, -50, 79, -127, -43, 127, 18, 11, -21, 32, -52, 27, -88, -90,
-39, -19, -10, 24, -118, 72, -24, -44, 2, 12, 86, -107, 39, -33, -127, 47,
51, -24, -22, 46, 0, 15, -35, -69, -2, -74, 24, -6, 0, 29, -3, 45,
32, -32, 117, -45, 79, -24, -17, -109, -10, -70, 88, -48, 24, -91, 120, -37,
50, -127, 58, 32, -82, -10, -17, -7, 46, -127, -15, 89, 127, 17, 98, -39,
-33, 37, 42, -40, -32, -21, 105, -19, 19, 19, -59, -9, 30, 0, -127, 34,
127, -84, 75, 24, -40, -49, -127, -107, -14, 45, -75, 1, 30, -20, 41, -68,
-40, 12, 127, -3, 5, 20, -73, -59, -127, -3, -3, -53, -6, -119, 93, 120,
-80, -50, 0, 20, -46, 67, 78, -12, -22, -127, 36, -41, 56, 119, -5, -116,
-22, 68, -14, -90, 24, -82, -44, -127, 107, -25, -37, 40, -7, -7, -82, 5,
-87, 44, -34, 9, -127, 39, 70, 49, -63, 74, -49, 109, -27, -89, -47, -39,
44, 49, -4, 60, -42, 80, 9, -127, -9, -56, -49, 125, -66, 47, 36, 117,
15, -11, -96, 109, 94, -17, -56, 70, 8, -14, -5, 50, 37, -45, 120, -30,
-76, 40, -46, 6, 3, 69, 17, -78, 1, -79, 6, 127, 43, 26, 127, -127,
28, -55, -26, 55, 112, 48, 107, -1, -77, -1, 53, -9, -22, -43, 123, 108,
127, 102, 68, 46, 5, 1, 123, -13, -55, -34, -49, 89, 65, -105, -5, 94,
-53, 62, 45, 30, 46, 18, -35, 15, 41, 47, -98, -24, 94, -75, 127, -114,
127, -68, 1, -17, 51, -95, 47, 12, 34, -45, -75, 89, -107, -9, -58, -29,
-109, -24, 127, -61, -13, 77, -45, 17, 19, 83, -24, 9, 127, -66, 54, 4,
26, 13, 111, 43, -113, -22, 10, -24, 83, 67, -14, 75, -123, 59, 127, -12,
99, -19, 64, -38, 54, 9, 7, 61, -56, 3, -57, 113, -104, -59, 3, -9,
-47, 74, 85, -55, -34, 12, 118, 28, 93, -72, 13, -99, -72, -20, 30, 72,
-94, 19, -54, 64, -12, -63, -25, 65, 72, -10, 127, 0, -127, 103, -20, -73,
-112, -103, -6, 28, -42, -21, -59, -29, -26, 19, -4, -51, 94, -58, -95, -37,
35, 20, -69, 127, -19, -127, -22, -120, -53, 37, 74, -127, -1, -12, -119, -53,
-28, 38, 69, 17, 16, -114, 89, 62, 24, 37, -23, 49, -101, -32, -9, -95,
-53, 5, 93, -23, -49, -8, 51, 3, -75, -90, -10, -39, 127, -86, -22, 20,
20, 113, 75, 52, -31, 92, -63, 7, -12, 46, 36, 101, -43, -17, -53, -7,
-38, -76, -31, -21, 62, 31, 62, 20, -127, 31, 64, 36, 102, -85, -10, 77,
80, 58, -79, -8, 35, 8, 80, -24, -9, 3, -17, 72, 127, 83, -87, 55,
18, -119, -123, 36, 10, 127, 56, -55, 113, 13, 26, 32, -13, -48, 22, -13,
5, 58, 27, 24, 26, -11, -36, 37, -92, 78, 81, 9, 51, 14, 67, -13,
0, 32, 45, -76, 32, -39, -22, -49, -127, -27, 31, -9, 36, 14, 71, 13,
57, 12, -53, -86, 53, -44, -35, 2, 127, 12, -66, -44, 46, -115, 3, 10,
56, -35, 119, -19, -61, 52, -59, -127, -49, -23, 4, -5, 17, -82, -6, 127,
25, 79, 67, 64, -25, 14, -64, -37, -127, -28, 21, -63, 66, -53, -41, 109,
-62, 15, -22, 13, 29, -63, 20, 27, 95, -44, -59, -116, -10, 79, -49, 22,
-43, -16, 46, -47, -120, -36, -29, -52, -44, 29, 127, -13, 49, -9, -127, 75,
-28, -23, 88, 59, 11, -95, 81, -59, 58, 60, -26, 40, -92, -3, -22, -58,
-45, -59, -22, -53, 71, -29, 66, -32, -23, 14, -17, -66, -24, -28, -62, 47,
38, 17, 16, -37, -24, -11, 8, -27, -19, 59, 45, -49, -47, -4, -22, -81,
30, -67, -127, 74, 102, 5, -18, 98, 34, -66, 42, -52, 7, -59, 24, -58,
-19, -24, -118, -73, 91, 15, -16, 79, -32, -79, -127, -36, 41, 77, -83, 2,
56, 22, -75, 127, -16, -21, 12, 31, 56, -113, -127, 90, 55, 61, 12, 55,
-14, -113, -14, 32, 49, -67, -17, 91, -10, 1, 21, 69, -70, 99, -19, -112,
66, -90, -10, -9, -71, 127, 50, -81, -49, 24, 61, -61, -111, 7, -41, 127,
88, -66, 108, -127, -6, 36, -14, 41, -50, 14, 14, 73, -101, -28, 77, 127,
-8, -100, 88, 38, 121, 88, -125, -60, 13, -94, -115, 20, -67, -87, -94, -119,
44, -28, -30, 18, 5, -53, -61, 20, -43, 11, -77, -60, 13, 29, 3, 6,
-72, 38, -60, -11, 108, -53, 41, 66, -12, -127, -127, -49, 24, 29, 46, 36,
91, 34, -33, 116, -51, -34, -52, 91, 7, -83, 73, -26, -103, 24, -10, 76,
84, 5, 68, -80, -13, -17, -32, -48, 20, 50, 26, 10, 63, -104, -14, 37,
127, 114, 97, 35, 1, -33, -55, 127, -124, -33, 61, -7, 119, -32, -127, -53,
-42, 63, 3, -5, -26, 70, -58, -33, -44, -43, 34, -56, -127, 127, 25, -35,
-11, 16, -81, 29, -58, 40, -127, -127, 20, -47, -11, -36, -63, -52, -32, -82,
78, -76, -73, 8, 27, -72, -9, -74, -85, -86, -57, 25, 78, -10, -97, 35,
-65, 8, -59, 14, 1, -42, 32, -88, -44, 17, -3, -9, 59, 40, 12, -108,
-40, 24, 34, 18, -28, 2, 51, -110, -4, 100, 1, 65, 22, 0, 127, 61,
45, 25, -31, 6, 9, -7, -48, 99, 16, 44, -2, -40, 32, -39, -52, 10,
-110, -19, 56, -127, 69, 26, 51, 92, 40, 61, -52, 45, -38, 13, 85, 122,
27, 66, 45, -111, -83, -3, 31, 37, 19, -36, 58, 71, 39, -78, -47, 58,
-78, 8, -62, -36, -14, 61, 42, -127, 71, -4, 24, -54, 52, -127, 67, -4,
-42, 30, -63, 59, -3, -1, -18, -46, -92, -81, -96, -14, -53, -10, -11, -77,
13, 1, 8, -67, -127, 127, -28, 26, -14, 18, -13, -26, 2, 10, -46, -32,
-15, 27, -31, -59, 59, 77, -121, 28, 40, -54, -62, -31, -21, -37, -32, -6,
-127, -25, -60, 70, -127, 112, -127, 127, 88, -7, 116, 110, 53, 87, -127, 3,
16, 23, 74, -106, -51, 3, 74, -82, -112, -74, 65, 81, 25, 53, 127, -45,
-50, -103, -41, -65, -29, 79, -67, 64, -33, -30, -8, 127, 0, -13, -51, 67,
-14, 5, -92, 29, -35, -8, -90, -57, -3, 36, 43, 44, -31, -69, -7, 36,
39, -51, 43, -81, 58, 6, 127, 12, 57, 66, 46, 59, -43, -42, 41, -15,
-120, 24, 3, -11, 19, -13, 51, 28, 3, 55, -48, -12, -1, 2, 97, -19,
29, 42, 13, 43, 78, -44, 56, -108, -43, -19, 127, 15, -11, -18, -81, 83,
-37, 77, -109, 15, 65, -50, 43, 12, 13, 27, 28, 61, 57, 30, 26, 106,
-18, 56, 13, 97, 4, -8, -62, -103, 94, 108, -44, 52, 27, -47, -9, 105,
-53, 46, 89, 103, -33, 38, -34, 55, 51, 70, -94, -35, -87, -107, -19, -31,
9, -19, 79, -14, 77, 5, -19, -107, 85, 21, -45, -39, -42, 9, -29, 74,
47, -75, 60, -127, 120, -112, -57, -32, 41, 7, 79, 76, 66, 57, 41, -25,
31, 37, -47, -36, 43, -73, -37, 63, 127, -69, -52, 90, -33, -61, 60, -55,
44, 15, 4, -67, 13, -92, 64, 29, -39, -3, 83, -2, -38, -85, -86, 58,
35, -69, -61, 29, -37, -95, -78, 4, 30, -4, -32, -80, -22, -9, -77, 46,
7, -93, -71, 65, 9, -50, 127, -70, 26, -12, -39, -114, 63, -127, -100, 4,
-32, 111, 22, -60, 65, -101, 26, -42, 21, -59, -27, -74, 2, -94, 6, 126,
5, 76, -88, -9, -43, -101, 127, 1, 125, 92, -63, 52, 56, 4, 81, -127,
127, 80, 127, -29, 30, 116, -74, -17, -57, 105, 48, 45, 25, -72, 48, -38,
-108, 31, -34, 4, -11, 41, -127, 52, -104, -43, -37, 52, 2, 47, 87, -9,
77, 27, -41, -25, 90, 86, -56, 75, 10, 33, 78, 58, 127, 127, -7, -73,
49, -33, -106, -35, 38, 57, 53, -17, -4, 83, 52, -108, 54, -125, 28, 23,
56, -43, -88, -17, -6, 47, 23, -9, 0, -13, 111, 75, 27, -52, -38, -34,
39, 30, 66, 39, 38, -64, 38, 3, 21, -32, -51, -28, 54, -38, -87, 20,
52, 115, 18, -81, -70, 0, -14, -46, -46, -3, 125, 16, -14, 23, -82, -84,
-69, -20, -65, -127, 9, 81, -49, 61, 7, -36, -45, -42, 57, -26, 47, 20,
-85, 46, -13, 41, -37, -75, -60, 86, -78, -127, 12, 50, 2, -3, 13, 47,
5, 19, -78, -55, -27, 65, -71, 12, -108, 20, -16, 11, -31, 63, -55, 37,
75, -17, 127, -73, -33, -28, -120, 105, 68, 106, -103, -106, 71, 61, 2, 23,
-3, 33, -5, -15, -67, -15, -23, -54, 15, -63, 76, 58, -110, 1, 83, -27,
22, 75, -39, -17, -11, 64, -17, -127, -54, -66, 31, 96, 116, 3, -114, -7,
-108, -63, 97, 9, 50, 8, 75, -28, 72, 112, -36, -112, 95, -50, 23, -13,
-19, 55, 21, 23, 92, 91, 22, -49, 16, -75, 23, 9, -49, -97, -37, 49,
-36, 36, -127, -86, 43, 127, -24, -24, 84, 83, -35, -34, -12, 109, 102, -38,
51, -68, 34, 19, -22, 49, -32, 127, 40, 24, -93, -4, -3, 105, 3, -58,
-18, 8, 127, -18, 125, 68, 69, -62, 30, -36, 54, -57, -24, 17, 43, -36,
-27, -57, -67, -21, -10, -49, 68, 12, 65, 4, 48, 55, 127, -75, 44, 89,
-66, -13, -78, -82, -91, 22, 30, 33, -40, -87, -34, 96, -91, 39, 10, -64,
-3, -12, 127, -50, -37, -56, 23, -35, -36, -54, 90, -91, 2, 50, 77, -6,
-127, 16, 46, -5, -73, 0, -56, -18, -72, 28, 93, 60, 49, 20, 18, 111,
-111, 32, -83, 47, 47, -10, 35, -88, 43, 57, -98, 127, -17, 0, 1, -39,
-127, -2, 0, 63, 93, 0, 36, -66, -61, -19, 39, -127, 58, 50, -17, 127,
88, -43, -108, -51, -16, 7, -36, 68, 46, -14, 107, 40, 57, 7, 19, 8,
3, 88, -90, -92, -18, -21, -24, 13, 7, -4, -78, -91, -4, 8, -35, -5,
19, 2, -111, 4, -66, -81, 122, -20, -34, -37, -84, 127, 68, 46, 17, 47
};

static const uint32_t seedLUT[256] = {
747538460, 1088979410, 1744950180, 1767011913, 1403382928, 521866116, 1060417601, 2110622736,
1557184770, 105289385, 585624216, 1827676546, 1191843873, 1018104344, 1123590530, 663361569,
2023850500, 76561770, 1226763489, 80325252, 1992581442, 502705249, 740409860, 516219202,
557974537, 1883843076, 720112066, 1640137737, 1820967556, 40667586, 155354121, 1820967557,
1115949072, 1631803309, 98284748, 287433856, 2119719977, 988742797, 1827432592, 579378475,
1017745956, 1309377032, 1316535465, 2074315269, 1923385360, 209722667, 1546228260, 168102420,
135274561, 355958469, 248291472, 2127839491, 146920100, 585982612, 1611702337, 696506029,
1386498192, 1258072451, 1212240548, 1043171860, 1217404993, 1090770605, 1386498193, 169093201,
541098240, 1468005469, 456510673, 1578687785, 1838217424, 2010752065, 2089828354, 1362717428,
970073673, 854129835, 714793201, 1266069081, 1047060864, 1991471829, 1098097741, 913883585,
1669598224, 1337918685, 1219264706, 1799741108, 1834116681, 683417731, 1120274457, 1073098457,
1648396544, 176642749, 31171789, 718317889, 1266977808, 1400892508, 549749008, 1808010512,
67112961, 1005669825, 903663673, 1771104465, 1277749632, 1229754427, 950632997, 1979371465,
2074373264, 305357524, 1049387408, 1171033360, 1686114305, 2147468765, 1941195985, 117709841,
809550080, 991480851, 1816248997, 1561503561, 329575568, 780651196, 1659144592, 1910793616,
604016641, 1665084765, 1530186961, 1870928913, 809550081, 2079346113, 71307521, 876663040,
1073807360, 832356664, 1573927377, 204073344, 2026918147, 1702476788, 2043881033, 57949587,
2001393952, 1197426649, 1186508931, 332056865, 950043140, 890043474, 349099312, 148914948,
236204097, 2022643605, 1441981517, 498130129, 1443421481, 924216797, 1817491777, 1913146664,
1411989632, 929068432, 495735097, 1684636033, 1284520017, 432816184, 1344884865, 210843729,
676364544, 234449232, 12112337, 1350619139, 1753272996, 2037118872, 1408560528, 533334916,
1043640385, 357326099, 201376421, 110375493, 541106497, 416159637, 242512193, 777294080,
1614872576, 1535546636, 870600145, 910810409, 1821440209, 1605432464, 1145147393, 951695441,
1758494976, 1506656568, 1557150160, 608221521, 1073840384, 217672017, 684818688, 1750138880,
16777217, 677990609, 953274371, 1770050213, 1359128393, 1797602707, 1984616737, 1865815816,
2120835200, 2051677060, 1772234061, 1579794881, 1652821009, 1742099468, 1887260865, 46468113,
1011925248, 1134107920, 881643832, 1354774993, 472508800, 1892499769, 1752793472, 1962502272,
687898625, 883538000, 1354355153, 1761673473, 944820481, 2020102353, 22020353, 961597696,
1342242816, 964808962, 1355809701, 17016649, 1386540177, 647682692, 1849012289, 751668241,
1557184768, 127374604, 1927564752, 1045744913, 1614921984, 43588881, 1016185088, 1544617984,
1090519041, 136122424, 215038417, 1563027841, 2026918145, 1688778833, 701530369, 1372639488,
1342242817, 2036945104, 953274369, 1750192384, 16842753, 964808960, 1359020032, 1358954497
};

static const uint32_t deblockFactor[13] =
{ 64, 71, 77, 84, 90, 96, 103, 109, 116, 122, 128, 128, 128 };


SEIFilmGrainSynthesizer::SEIFilmGrainSynthesizer()
  : m_width           (0)
  , m_height          (0)
  , m_chromaFormat    (NUM_CHROMA_FORMAT)
  , m_bitDepth        (0)
  , m_idrPicId        (0)
  , m_grainSynt       (NULL)
  , m_fgsBlkSize      (8)
  , m_poc             (0)
  , m_errorCode       (0)
  , m_fgcParameters   (NULL)
{

}

void SEIFilmGrainSynthesizer::create(uint32_t width, uint32_t height, ChromaFormat fmt, uint8_t bitDepth, uint32_t idrPicId)
{
  m_width             = width;
  m_height            = height;
  m_chromaFormat      = fmt;
  m_bitDepth          = bitDepth;
  m_idrPicId          = idrPicId;
  m_fgsBlkSize        = 8;
  m_errorCode         = 0;

  if (!m_grainSynt)
    m_grainSynt       = new GrainSynthesisStruct;
  if (!m_fgcParameters)
    m_fgcParameters   = new SEIFilmGrainCharacteristics;
}

SEIFilmGrainSynthesizer::~SEIFilmGrainSynthesizer()
{
  destroy();
}

void SEIFilmGrainSynthesizer::fgsInit()
{
  deriveFGSBlkSize();
  dataBaseGen();
}

void SEIFilmGrainSynthesizer::destroy()
{
  if (m_fgcParameters)
  {
    delete m_fgcParameters;
    m_fgcParameters = NULL;
  }
  if (m_grainSynt)
  {
    delete m_grainSynt;
    m_grainSynt = NULL;
  }
}

void SEIFilmGrainSynthesizer::grainSynthesizeAndBlend(TComPicYuv* pGrainBuf, Bool isIdrPic)
{
  uint8_t     numComp = MAX_NUM_COMPONENT, compCtr; /* number of color components */
  uint8_t     color_offset[MAX_NUM_COMPONENT];
  uint32_t    widthComp[MAX_NUM_COMPONENT], heightComp[MAX_NUM_COMPONENT], strideComp[MAX_NUM_COMPONENT];
  uint32_t *  offsetsArr[MAX_NUM_COMPONENT];
  Pel *       decComp[MAX_NUM_COMPONENT];
  uint32_t    pseudoRandValEc;
  uint32_t    picOffset;

  /* from SMPTE RDD5 */
  color_offset[0] = COLOUR_OFFSET_LUMA;
  color_offset[1] = COLOUR_OFFSET_CR;
  color_offset[2] = COLOUR_OFFSET_CB;

  if (0 != m_fgcParameters->m_filmGrainCharacteristicsCancelFlag)
  {
    return;
  }

  widthComp[0]  = m_width;
  heightComp[0] = m_height;

  if (CHROMA_420 == m_chromaFormat)
  {
    widthComp[1]  = (m_width >> 1);
    widthComp[2]  = (m_width >> 1);
    heightComp[1] = (m_height >> 1);
    heightComp[2] = (m_height >> 1);
  }
  else if (CHROMA_422 == m_chromaFormat)
  {
    widthComp[1]  = (m_width >> 1);
    widthComp[2]  = (m_width >> 1);
    heightComp[1] = m_height;
    heightComp[2] = m_height;
  }
  else if (CHROMA_400 == m_chromaFormat)
  {
    numComp = 1;
  }

  /*Allocate memory for offsets assuming 16x16 block size,
  32x32 will need lesser than this*/
  uint32_t maxNumBlocks = ((m_width >> 4) + 1) * ((m_height >> 4) + 1);

  for (compCtr = 0; compCtr < numComp; compCtr++)
  {
    offsetsArr[compCtr] = new uint32_t[maxNumBlocks];
  }

  decComp[0] = pGrainBuf->getAddr(COMPONENT_Y);
  decComp[1] = pGrainBuf->getAddr(COMPONENT_Cb);
  decComp[2] = pGrainBuf->getAddr(COMPONENT_Cr);

  /* component strides */
  strideComp[0] = pGrainBuf->getStride(COMPONENT_Y);
  strideComp[1] = 0;
  strideComp[2] = 0;

  if (CHROMA_400 != m_chromaFormat)
  {
    strideComp[1] = pGrainBuf->getStride(COMPONENT_Cb);
    strideComp[2] = pGrainBuf->getStride(COMPONENT_Cr);
  }

  int32_t numBlks_x[MAX_NUM_COMPONENT];
  int32_t numBlks_y[MAX_NUM_COMPONENT];

  picOffset = m_poc;
  for (compCtr = 0; compCtr < numComp; compCtr++)
  {
    if (FG_BLK_32 == m_fgsBlkSize)
    {
      numBlks_x[compCtr]         = (widthComp[compCtr] >> 5) + ((widthComp[compCtr] & 0x1F) ? 1 : 0);
      numBlks_y[compCtr]         = (heightComp[compCtr] >> 5) + ((heightComp[compCtr] & 0x1F) ? 1 : 0);
    }
    else
    {
      numBlks_x[compCtr]         = (widthComp[compCtr] >> 4) + ((widthComp[compCtr] & 0xF) ? 1 : 0);
      numBlks_y[compCtr]         = (heightComp[compCtr] >> 4) + ((heightComp[compCtr] & 0xF) ? 1 : 0);
    }
  }

  for (compCtr = 0; compCtr < numComp; compCtr++)
  {
    if (1 == m_fgcParameters->m_compModel[compCtr].bPresentFlag)
    {
      uint32_t *tmp = offsetsArr[compCtr];
      int       i, j;

      /* Seed initialization for current picture*/
      pseudoRandValEc = seedLUT[((picOffset + color_offset[compCtr]) & 0xFF)];

      for (i = 0; i < numBlks_y[compCtr]; i++)
      {
        for (j = 0; j < numBlks_x[compCtr]; j++)
        {
          *tmp            = pseudoRandValEc;
          pseudoRandValEc = prng(pseudoRandValEc);
          tmp++;
        }
      }
    }
  }

  m_fgsArgs.numComp = numComp;
  for (compCtr = 0; compCtr < numComp; compCtr++)
  {
    if (1 == m_fgcParameters->m_compModel[compCtr].bPresentFlag)
    {
      m_fgsArgs.decComp[compCtr]    = decComp[compCtr];
      m_fgsArgs.widthComp[compCtr]  = widthComp[compCtr];
      m_fgsArgs.strideComp[compCtr] = strideComp[compCtr];
      m_fgsArgs.fgsOffsets[compCtr] = offsetsArr[compCtr];

      if (FG_BLK_32 == m_fgsBlkSize)
      {
        m_fgsArgs.heightComp[compCtr] = numBlks_y[compCtr] * FG_BLK_32;
      }
      else
      {
        m_fgsArgs.heightComp[compCtr] = numBlks_y[compCtr] * FG_BLK_16;
      }
    }
  }
  m_fgsArgs.pFgcParameters = m_fgcParameters;
  m_fgsArgs.blkSize = m_fgsBlkSize;
  m_fgsArgs.bitDepth = m_bitDepth;
  m_fgsArgs.pGrainSynt = m_grainSynt;

  fgsProcess(m_fgsArgs);

  for (compCtr = 0; compCtr < numComp; compCtr++)
  {
    delete offsetsArr[compCtr];
  }
  return;
}

/* Function validates film grain parameters and returns 0 for valid parameters of SMPTE-RDD5 else 1*/
/* Also down converts the chroma model values for 4:2:0 and 4:2:2 chroma_formats */
uint8_t SEIFilmGrainSynthesizer::grainValidateParams()
{
  uint8_t   numComp = MAX_NUM_COMPONENT; /* number of color components */
  uint8_t   compCtr, intensityCtr, multiGrainCheck[MAX_NUM_COMPONENT][FG_MAX_NUM_INTENSITIES] = { 0 };
  uint16_t  multiGrainCtr;
  uint8_t   limitCompModelVal1[10] = { 0 }, limitCompModelVal2[10] = { 0 };
  uint8_t   num_comp_model_pairs = 0, limitCompModelCtr, compPairMatch;

  memset(m_grainSynt->intensityInterval, INTENSITY_INTERVAL_MATCH_FAIL, sizeof(m_grainSynt->intensityInterval));

  if ((m_width < FG_MIN_WIDTH) || (m_width > FG_MAX_WIDTH) || (m_width % 4))
  {
    return FGS_INVALID_WIDTH; /* Width not supported */
  }
  if ((m_height < FG_MIN_HEIGHT) || (m_height > FG_MAX_HEIGHT) || (m_height % 4))
  {
    return FGS_INVALID_HEIGHT; /* Height not  supported */
  }
  if ((m_chromaFormat < MIN_CHROMA_FORMAT_IDC) || (m_chromaFormat > MAX_CHROMA_FORMAT_IDC))
  {
    return FGS_INVALID_CHROMA_FORMAT; /* Chroma format not supported */
  }
  if (m_chromaFormat == MIN_CHROMA_FORMAT_IDC) /* Mono Chrome */
  {
    numComp = 1;
  }

  if ((m_bitDepth < MIN_BIT_DEPTH) || (m_bitDepth > MAX_BIT_DEPTH))
  {
    return FGS_INVALID_BIT_DEPTH; /* Bit depth not supported */
  }

  if ((0 != m_fgcParameters->m_filmGrainCharacteristicsCancelFlag) &&
      (1 != m_fgcParameters->m_filmGrainCharacteristicsCancelFlag))
  {
    return FGS_INVALID_FGC_CANCEL_FLAG; /* Film grain synthesis disabled */
  }

  if (FILM_GRAIN_MODEL_ID_VALUE != m_fgcParameters->m_filmGrainModelId)
  {
    return FGS_INVALID_GRAIN_MODEL_ID; /* Not supported */
  }

  if (0 != m_fgcParameters->m_separateColourDescriptionPresentFlag)
  {
    return FGS_INVALID_SEP_COL_DES_FLAG; /* Not supported */
  }

  if (BLENDING_MODE_VALUE != m_fgcParameters->m_blendingModeId)
  {
    return FGS_INVALID_BLEND_MODE; /* Not supported */
  }

  if (m_fgcParameters->m_compModel[0].bPresentFlag || m_fgcParameters->m_compModel[1].bPresentFlag || m_fgcParameters->m_compModel[2].bPresentFlag) {
    if ((m_fgcParameters->m_log2ScaleFactor < MIN_LOG2SCALE_VALUE) ||
      (m_fgcParameters->m_log2ScaleFactor > MAX_LOG2SCALE_VALUE))
    {
      return FGS_INVALID_LOG2_SCALE_FACTOR; /* Not supported  */
    }
  }

  /* validation of component model present flag */
  for (compCtr = 0; compCtr < numComp; compCtr++)
  {
    if ((m_fgcParameters->m_compModel[compCtr].bPresentFlag != true) &&
        (m_fgcParameters->m_compModel[compCtr].bPresentFlag != false))
    {
      return FGS_INVALID_COMP_MODEL_PRESENT_FLAG; /* Not supported  */
    }
    if (m_fgcParameters->m_compModel[compCtr].bPresentFlag &&
       (m_fgcParameters->m_compModel[compCtr].numModelValues > FG_MAX_ALLOWED_MODEL_VALUES))
    {
      return FGS_INVALID_NUM_MODEL_VALUES; /* Not supported  */
    }
  }

  /* validation of intensity intervals and  */
  for (compCtr = 0; compCtr < numComp; compCtr++)
  {
    if (m_fgcParameters->m_compModel[compCtr].bPresentFlag)
    {
      for (intensityCtr = 0; intensityCtr < m_fgcParameters->m_compModel[compCtr].intensityValues.size(); intensityCtr++)
      {

        if (m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].intensityIntervalLowerBound >
            m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].intensityIntervalUpperBound)
        {
          return FGS_INVALID_INTENSITY_BOUNDARY_VALUES; /* Not supported  */
        }

        for (multiGrainCtr = m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].intensityIntervalLowerBound;
             multiGrainCtr <= m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].intensityIntervalUpperBound; multiGrainCtr++)
        {
          m_grainSynt->intensityInterval[compCtr][multiGrainCtr] = intensityCtr;
          if (multiGrainCheck[compCtr][multiGrainCtr]) /* Non over lap */
          {
            return FGS_INVALID_INTENSITY_BOUNDARY_VALUES; /* Not supported  */
          }
          else
          {
            multiGrainCheck[compCtr][multiGrainCtr] = 1;
          }
        }

        m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue.resize(FG_MAX_NUM_MODEL_VALUES);
        /* default initialization for cut off frequencies */
        if (1 == m_fgcParameters->m_compModel[compCtr].numModelValues)
        {
          m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[1] = DEFAULT_HORZ_CUT_OFF_FREQUENCY;
          m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[2] = m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[1];
        }
        else if (2 == m_fgcParameters->m_compModel[compCtr].numModelValues)
        {
          m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[2] = m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[1];
        }

        /* Error check on model component value */
        if (m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[0] > (FG_MAX_STANDARD_DEVIATION << (m_bitDepth - FG_BIT_DEPTH_8)))
        {
          return FGS_INVALID_STANDARD_DEVIATION; /* Not supported  */
        }
        else if ((m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[1] < MIN_CUT_OFF_FREQUENCY) ||
                 (m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[1] > MAX_CUT_OFF_FREQUENCY))
        {
          return FGS_INVALID_CUT_OFF_FREQUENCIES; /* Not supported  */
        }
        else if ((m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[2] < MIN_CUT_OFF_FREQUENCY) ||
                 (m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[2] > MAX_CUT_OFF_FREQUENCY))
        {
          return FGS_INVALID_CUT_OFF_FREQUENCIES; /* Not supported  */
        }

        /* conversion of component model values for 4:2:0 and 4:4:4 */
        if (CHROMA_444 != m_chromaFormat && (compCtr > 0))
        {
          if (CHROMA_420 == m_chromaFormat) /* 4:2:0 */
          {
            m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[0] >>= 1;
            m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[1] =
              CLIP3(MIN_CUT_OFF_FREQUENCY, MAX_CUT_OFF_FREQUENCY,
              (m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[1] << 1));
            m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[2] =
              CLIP3(MIN_CUT_OFF_FREQUENCY, MAX_CUT_OFF_FREQUENCY,
              (m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[2] << 1));

          }
          else if (CHROMA_422 == m_chromaFormat)/* 4:2:2 */
          {
            m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[0] =
              (m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[0] * SCALE_DOWN_422) >> Q_FORMAT_SCALING;

            m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[1] =
              CLIP3(MIN_CUT_OFF_FREQUENCY, MAX_CUT_OFF_FREQUENCY,
              (m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[1] << 1));
          }
        }

        compPairMatch = 0;
        for (limitCompModelCtr = 0; limitCompModelCtr <= num_comp_model_pairs; limitCompModelCtr++)
        {
          if ((limitCompModelVal1[limitCompModelCtr] ==
            m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[1]) &&
            (limitCompModelVal2[limitCompModelCtr] ==
              m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[2]))
          {
            compPairMatch = 1;
          }
        }

        if (0 == compPairMatch)
        {
          num_comp_model_pairs++;
          /* max allowed pairs are 10 as per SMPTE -RDD5*/
          if (num_comp_model_pairs > FG_MAX_ALLOWED_COMP_MODEL_PAIRS)
          {
            return FGS_INVALID_NUM_CUT_OFF_FREQ_PAIRS; /* Not supported  */
          }
          limitCompModelVal1[num_comp_model_pairs - 1] =
            m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[1];
          limitCompModelVal2[num_comp_model_pairs - 1] =
            m_fgcParameters->m_compModel[compCtr].intensityValues[intensityCtr].compModelValue[2];
        }
      }
    }
  }
  return FGS_SUCCESS; /* Success */
}

void SEIFilmGrainSynthesizer::deriveFGSBlkSize()
{
  uint32_t picSizeInLumaSamples = m_height * m_width;
  if (picSizeInLumaSamples <= (1920 * 1080))
  {
    m_fgsBlkSize = FG_BLK_8;
  }
  else if (picSizeInLumaSamples <= (3840 * 2160))
  {
    m_fgsBlkSize = FG_BLK_16;
  }
  else
  {
    m_fgsBlkSize = FG_BLK_32;
  }
}
void SEIFilmGrainSynthesizer::dataBaseGen()
{
  uint32_t      pseudoRandValEhv;
  uint8_t       h, v; /* Horizaontal and vertical cut off frequencies (+2)*/
  uint32_t      ScaleCutOffFh, ScaleCutOffFv, l, r, i, j, k;
  int32_t       B[FG_DATA_BASE_SIZE][FG_DATA_BASE_SIZE], IDCT[FG_DATA_BASE_SIZE][FG_DATA_BASE_SIZE];
  int32_t       Grain[FG_DATA_BASE_SIZE][FG_DATA_BASE_SIZE];

  const TMatrixCoeff *Tmp = g_aiT64[TRANSFORM_FORWARD][0];
  const int transform_scale = 9;                      // upscaling of original transform as specified in VVC (for 64x64 block)
  const int add_1st = 1 << (transform_scale - 1);

  TMatrixCoeff T[FG_DATA_BASE_SIZE][FG_DATA_BASE_SIZE];     // Original
  TMatrixCoeff TT[FG_DATA_BASE_SIZE][FG_DATA_BASE_SIZE];    // Transpose
  for (int x = 0; x < FG_DATA_BASE_SIZE; x++)
  {
    for (int y = 0; y < FG_DATA_BASE_SIZE; y++)
    {
      T[x][y] = Tmp[x * 64 + y]; /* Matrix Original */
      TT[y][x] = Tmp[x * 64 + y]; /* Matrix Transpose */
    }
  }

  for (h = 0; h < NUM_CUT_OFF_FREQ; h++)
  {
    for (v = 0; v < NUM_CUT_OFF_FREQ; v++)
    {
      memset(&B, 0, FG_DATA_BASE_SIZE*FG_DATA_BASE_SIZE * sizeof(int32_t));
      memset(&IDCT, 0, FG_DATA_BASE_SIZE*FG_DATA_BASE_SIZE * sizeof(int32_t));
      memset(&Grain, 0, FG_DATA_BASE_SIZE*FG_DATA_BASE_SIZE * sizeof(int32_t));
      ScaleCutOffFh = ((h + 3) << 2) - 1;
      ScaleCutOffFv = ((v + 3) << 2) - 1;

      /* ehv : seed to be used for the psudo random generator for a given h and v */
      pseudoRandValEhv = seedLUT[h + v * 13];

      for (l = 0, r = 0; l <= ScaleCutOffFv; l++)
      {
        for (k = 0; k <= ScaleCutOffFh; k += 4)
        {
          B[k][l] = gaussianLUT[pseudoRandValEhv % 2048];
          B[k + 1][l] = gaussianLUT[(pseudoRandValEhv + 1) % 2048];
          B[k + 2][l] = gaussianLUT[(pseudoRandValEhv + 2) % 2048];
          B[k + 3][l] = gaussianLUT[(pseudoRandValEhv + 3) % 2048];
          r++;
          pseudoRandValEhv = prng(pseudoRandValEhv);
        }
      }
      B[0][0] = 0;

      for (i = 0; i < FG_DATA_BASE_SIZE; i++)
      {
        for (j = 0; j < FG_DATA_BASE_SIZE; j++)
        {
          for (k = 0; k < FG_DATA_BASE_SIZE; k++)
          {
            IDCT[i][j] += TT[i][k] * B[k][j];
          }
          IDCT[i][j] += add_1st;
          IDCT[i][j] = IDCT[i][j] >> transform_scale;
        }
      }

      for (i = 0; i < FG_DATA_BASE_SIZE; i++)
      {
        for (j = 0; j < FG_DATA_BASE_SIZE; j++)
        {
          for (k = 0; k < FG_DATA_BASE_SIZE; k++)
          {
            Grain[i][j] += IDCT[i][k] * T[k][j];
          }
          Grain[i][j] += add_1st;
          Grain[i][j] = Grain[i][j] >> transform_scale;
          m_grainSynt->dataBase[h][v][j][i] = CLIP3(-127, 127, Grain[i][j]);
        }
      }

      /* De-blocking at horizontal block edges */
      for (l = 0; l < FG_DATA_BASE_SIZE; l += m_fgsBlkSize)
      {
        for (k = 0; k < FG_DATA_BASE_SIZE; k++)
        {
          m_grainSynt->dataBase[h][v][l][k] = ((m_grainSynt->dataBase[h][v][l][k]) * deblockFactor[v]) >> 7;
          m_grainSynt->dataBase[h][v][l + m_fgsBlkSize - 1][k] = ((m_grainSynt->dataBase[h][v][l + m_fgsBlkSize - 1][k]) * deblockFactor[v]) >> 7;
        }
      }
    }
  }
  return;
}

uint32_t SEIFilmGrainSynthesizer::prng(uint32_t x_r)
{
  uint32_t addVal;
  addVal  = (1 + ((x_r & (POS_2)) > 0) + ((x_r & (POS_30)) > 0)) % 2;
  x_r     = (x_r << 1) + addVal;
  return x_r;
}

uint32_t SEIFilmGrainSynthesizer::fgsProcess(fgsProcessArgs &inArgs)
{
  uint32_t errorCode;
  uint8_t  blkSize = inArgs.blkSize;

  if (blkSize == 8)
    errorCode = fgsSimulationBlending_8x8(&inArgs);
  else if (blkSize == 16)
    errorCode = fgsSimulationBlending_16x16(&inArgs);
  else if (blkSize == 32)
    errorCode = fgsSimulationBlending_32x32(&inArgs);
  else
    errorCode = FGS_FAIL;

  return errorCode;
}

void SEIFilmGrainSynthesizer::deblockGrainStripe(Pel *grainStripe, uint32_t widthComp, uint32_t heightComp,
  uint32_t strideComp, uint32_t blkSize)
{
  int32_t  left1, left0, right0, right1;
  uint32_t pos, vertCtr;

  uint32_t widthCropped = (widthComp - blkSize);
  
  for (vertCtr = 0; vertCtr < heightComp; vertCtr++)
  {
    for (pos = 0; pos < widthCropped; pos += blkSize)
    {
      left1 = *(grainStripe + blkSize - 2);
      left0 = *(grainStripe + blkSize - 1);
      right0 = *(grainStripe + blkSize + 0);
      right1 = *(grainStripe + blkSize + 1);
      *(grainStripe + blkSize + 0) = (left0 + (right0 << 1) + right1) >> 2;
      *(grainStripe + blkSize - 1) = (left1 + (left0 << 1) + right0) >> 2;
      grainStripe += blkSize;
    }
    grainStripe = grainStripe + (strideComp - pos);
  }
  return;
}

void SEIFilmGrainSynthesizer::blendStripe(Pel *decSampleHbdOffsetY, Pel *grainStripe, uint32_t widthComp,
  uint32_t strideSrc, uint32_t strideGrain, uint32_t blockHeight, uint8_t bitDepth)
{
  uint32_t k, l;
  uint16_t maxRange;
  maxRange = (1 << bitDepth) - 1;

  int32_t  grainSample;
  uint16_t decodeSampleHbd;
  uint8_t bitDepthShift = (bitDepth - FG_BIT_DEPTH_8);
  uint32_t bufInc = (strideSrc - widthComp);
  uint32_t grainBufInc = (strideGrain - widthComp);

  for (l = 0; l < blockHeight; l++) /* y direction */
  {
    for (k = 0; k < widthComp; k++) /* x direction */
    {
      decodeSampleHbd = *decSampleHbdOffsetY;
      grainSample = *grainStripe;
      grainSample <<= bitDepthShift;
      grainSample = CLIP3(0, maxRange, grainSample + decodeSampleHbd);
      *decSampleHbdOffsetY = (Pel)grainSample;
      decSampleHbdOffsetY++;
      grainStripe++;
    }
    decSampleHbdOffsetY += bufInc;
    grainStripe += grainBufInc;
  }
  return;
}

void SEIFilmGrainSynthesizer::blendStripe_32x32(Pel *decSampleHbdOffsetY, Pel *grainStripe, uint32_t widthComp,
  uint32_t strideSrc, uint32_t strideGrain, uint32_t blockHeight, uint8_t bitDepth)
{
  uint32_t k, l;
  uint16_t maxRange;
  maxRange = (1 << bitDepth) - 1;

  int32_t  grainSample;
  uint16_t decodeSampleHbd;
  uint8_t bitDepthShift = (bitDepth - FG_BIT_DEPTH_8);
  uint32_t bufInc = (strideSrc - widthComp);
  uint32_t grainBufInc = (strideGrain - widthComp);

  for (l = 0; l < blockHeight; l++) /* y direction */
  {
    for (k = 0; k < widthComp; k++) /* x direction */
    {
      decodeSampleHbd = *decSampleHbdOffsetY;
      grainSample = *grainStripe;
      grainSample <<= bitDepthShift;
      grainSample = CLIP3(0, maxRange, grainSample + decodeSampleHbd);
      *decSampleHbdOffsetY = (Pel)grainSample;
      decSampleHbdOffsetY++;
      grainStripe++;
    }
    decSampleHbdOffsetY += bufInc;
    grainStripe += grainBufInc;
  }
  return;
}

Pel SEIFilmGrainSynthesizer::blockAverage_8x8(Pel *decSampleBlk8, uint32_t widthComp, uint16_t *pNumSamples,
  uint8_t ySize, uint8_t xSize, uint8_t bitDepth)
{
  uint32_t blockAvg = 0;
  uint8_t  k;
  uint8_t l;
  for (k = 0; k < ySize; k++)
  {
    for (l = 0; l < xSize; l++)
    {
      blockAvg += *decSampleBlk8;
      decSampleBlk8++;
    }
    decSampleBlk8 += widthComp - xSize;
  }

  blockAvg = blockAvg >> (FG_BLK_8_shift + (bitDepth - FG_BIT_DEPTH_8));
  *pNumSamples = FG_BLK_AREA_8x8;

  return blockAvg;
}

uint32_t SEIFilmGrainSynthesizer::blockAverage_16x16(Pel *decSampleBlk8, uint32_t widthComp, uint16_t *pNumSamples,
  uint8_t ySize, uint8_t xSize, uint8_t bitDepth)
{
  uint32_t blockAvg = 0;
  uint8_t  k;
  uint8_t l;
  for (k = 0; k < ySize; k++)
  {
    for (l = 0; l < xSize; l++)
    {
      blockAvg += *decSampleBlk8;
      decSampleBlk8++;
    }
    decSampleBlk8 += widthComp - xSize;
  }

  // blockAvg = blockAvg >> (FG_BLK_16_shift + (bitDepth - FG_BIT_DEPTH_8));
  // If FG_BLK_16 is not used or changed FG_BLK_AREA_16x16 has to be changed
  *pNumSamples = FG_BLK_AREA_16x16;
  return blockAvg;
}

uint32_t SEIFilmGrainSynthesizer::blockAverage_32x32(Pel *decSampleBlk32, uint32_t strideComp, uint8_t bitDepth)
{
  uint32_t blockAvg = 0;
  uint8_t  k;
  uint8_t l;
  uint32_t bufInc = strideComp - FG_BLK_32;
  for (k = 0; k < FG_BLK_32; k++)
  {
    for (l = 0; l < FG_BLK_32; l++)
    {
      blockAvg += *decSampleBlk32++;
    }
    decSampleBlk32 += bufInc;
  }
  blockAvg = blockAvg >> (FG_BLK_32_shift + (bitDepth - FG_BIT_DEPTH_8));
  return blockAvg;
}

void SEIFilmGrainSynthesizer::simulateGrainBlk8x8(Pel *grainStripe, uint32_t grainStripeOffsetBlk8,
  GrainSynthesisStruct *grain_synt, uint32_t width,
  uint8_t log2ScaleFactor, int16_t scaleFactor, uint32_t kOffset,
  uint32_t lOffset, uint8_t h, uint8_t v, uint32_t xSize)
{
  uint32_t l;
  int8_t * database_h_v = &grain_synt->dataBase[h][v][lOffset][kOffset];
  grainStripe += grainStripeOffsetBlk8;
  uint32_t k;
  for (l = 0; l < FG_BLK_8; l++) /* y direction */
  {
    for (k = 0; k < xSize; k++) /* x direction */
    {
      *grainStripe = ((scaleFactor * (*database_h_v)) >> (log2ScaleFactor + GRAIN_SCALE));
      grainStripe++;
      database_h_v++;
    }
    grainStripe += width - xSize;
    database_h_v += FG_DATA_BASE_SIZE - xSize;
  }
  return;
}

void SEIFilmGrainSynthesizer::simulateGrainBlk16x16(Pel *grainStripe, uint32_t grainStripeOffsetBlk8,
  GrainSynthesisStruct *grain_synt, uint32_t width,
  uint8_t log2ScaleFactor, int16_t scaleFactor, uint32_t kOffset,
  uint32_t lOffset, uint8_t h, uint8_t v, uint32_t xSize)
{
  uint32_t l;
  int8_t * database_h_v = &grain_synt->dataBase[h][v][lOffset][kOffset];
  grainStripe += grainStripeOffsetBlk8;
  uint32_t k;
  for (l = 0; l < FG_BLK_16; l++) /* y direction */
  {
    for (k = 0; k < xSize; k++) /* x direction */
    {
      *grainStripe = (int16_t)(((int32_t)scaleFactor * (*database_h_v)) >> (log2ScaleFactor + GRAIN_SCALE));
      grainStripe++;
      database_h_v++;
    }
    grainStripe += width - xSize;
    database_h_v += FG_DATA_BASE_SIZE - xSize;
  }
  return;
}

void SEIFilmGrainSynthesizer::simulateGrainBlk32x32(Pel *grainStripe, uint32_t grainStripeOffsetBlk32,
  GrainSynthesisStruct *grain_synt, uint32_t width,
  uint8_t log2ScaleFactor, int16_t scaleFactor, uint32_t kOffset,
  uint32_t lOffset, uint8_t h, uint8_t v)
{
  uint32_t l;
  int8_t * database_h_v = &grain_synt->dataBase[h][v][lOffset][kOffset];
  grainStripe += grainStripeOffsetBlk32;
  uint32_t k;
  uint8_t shiftVal = log2ScaleFactor + GRAIN_SCALE;
  uint32_t grainbufInc = width - FG_BLK_32;

  for (l = 0; l < FG_BLK_32; l++) /* y direction */
  {
    for (k = 0; k < FG_BLK_32; k++) /* x direction */
    {
      *grainStripe = ((scaleFactor * (*database_h_v)) >> shiftVal);
      grainStripe++;
      database_h_v++;
    }
    grainStripe += grainbufInc;
    database_h_v += FG_DATA_BASE_SIZE - FG_BLK_32;
  }
  return;
}

uint32_t SEIFilmGrainSynthesizer::fgsSimulationBlending_8x8(fgsProcessArgs *inArgs)
{
  uint8_t  numComp, compCtr, blkId; /* number of color components */
  uint8_t  log2ScaleFactor, h, v;
  uint8_t  bitDepth; /*grain bit depth and decoded bit depth are assumed to be same */
  uint32_t widthComp[MAX_NUM_COMPONENT], heightComp[MAX_NUM_COMPONENT], strideComp[MAX_NUM_COMPONENT];
  Pel *    decSampleHbdBlk16, *decSampleHbdBlk8, *decSampleHbdOffsetY;
  Pel *    decHbdComp[MAX_NUM_COMPONENT];
  uint16_t numSamples;
  int16_t  scaleFactor;
  uint32_t kOffset, lOffset, grainStripeOffset, grainStripeOffsetBlk8, offsetBlk8x8;
  uint32_t kOffset_const, lOffset_const;
  int16_t  scaleFactor_const;
  Pel *    grainStripe; /* worth a row of 16x16 : Max size : 16xw;*/
  int32_t  yOffset8x8, xOffset8x8;
  uint32_t x, y;
  uint32_t blockAvg, intensityInt; /* ec : seed to be used for the psudo random generator for a given color component */
  uint32_t grainStripeWidth;
  uint32_t wdPadded;

  bitDepth        = inArgs->bitDepth;
  numComp         = inArgs->numComp;
  log2ScaleFactor = inArgs->pFgcParameters->m_log2ScaleFactor;

  for (compCtr = 0; compCtr < numComp; compCtr++)
  {
    decHbdComp[compCtr] = inArgs->decComp[compCtr];
    strideComp[compCtr] = inArgs->strideComp[compCtr];
    widthComp[compCtr]  = inArgs->widthComp[compCtr];
    heightComp[compCtr] = inArgs->heightComp[compCtr];
  }

  wdPadded = ((inArgs->widthComp[0] - 1) | 0xF) + 1;
  grainStripe = new Pel[wdPadded * FG_BLK_16];

  if (0 == inArgs->pFgcParameters->m_filmGrainCharacteristicsCancelFlag)
  {
    for (compCtr = 0; compCtr < numComp; compCtr++)
    {
      if (1 == inArgs->pFgcParameters->m_compModel[compCtr].bPresentFlag)
      {
        decSampleHbdOffsetY  = decHbdComp[compCtr];
        uint32_t *offset_tmp = inArgs->fgsOffsets[compCtr];
        grainStripeWidth = ((widthComp[compCtr] - 1) | 0xF) + 1;   // Make next muliptle of 16

        /* Loop of 16x16 blocks */
        for (y = 0; y < heightComp[compCtr]; y += FG_BLK_16)
        {
          /* Initialization of grain stripe of 16xwidth size */
          memset(grainStripe, 0, (grainStripeWidth * FG_BLK_16 * sizeof(Pel)));
          for (x = 0; x < widthComp[compCtr]; x += FG_BLK_16)
          {
            /* start position offset of decoded sample in x direction */
            grainStripeOffset = x;

            decSampleHbdBlk16 = decSampleHbdOffsetY + x;

            kOffset_const = (MSB16(*offset_tmp) % 52);
            kOffset_const &= 0xFFFC;

            lOffset_const = (LSB16(*offset_tmp) % 56);
            lOffset_const &= 0xFFF8;
            scaleFactor_const = 1 - 2 * BIT0(*offset_tmp);
            for (blkId = 0; blkId < NUM_8x8_BLKS_16x16; blkId++)
            {
              yOffset8x8   = (blkId >> 1) * FG_BLK_8;
              xOffset8x8   = (blkId & 0x1) * FG_BLK_8;
              offsetBlk8x8 = xOffset8x8 + (yOffset8x8 * strideComp[compCtr]);

              grainStripeOffsetBlk8 = grainStripeOffset + (xOffset8x8 + (yOffset8x8 * grainStripeWidth));

              decSampleHbdBlk8 = decSampleHbdBlk16 + offsetBlk8x8;
              blockAvg = blockAverage_8x8(decSampleHbdBlk8, strideComp[compCtr], &numSamples, FG_BLK_8, FG_BLK_8, bitDepth);

              /* Selection of the component model */
              intensityInt = inArgs->pGrainSynt->intensityInterval[compCtr][blockAvg];

              if (INTENSITY_INTERVAL_MATCH_FAIL != intensityInt)
              {
                /* 8x8 grain block offset using co-ordinates of decoded 8x8 block in the frame */
                // kOffset = kOffset_const;
                kOffset = kOffset_const + xOffset8x8;

                lOffset = lOffset_const + yOffset8x8;

                scaleFactor =
                  scaleFactor_const
                  * inArgs->pFgcParameters->m_compModel[compCtr].intensityValues[intensityInt].compModelValue[0];
                h = inArgs->pFgcParameters->m_compModel[compCtr].intensityValues[intensityInt].compModelValue[1] - 2;
                v = inArgs->pFgcParameters->m_compModel[compCtr].intensityValues[intensityInt].compModelValue[2] - 2;

                /* 8x8 block grain simulation */
                simulateGrainBlk8x8(grainStripe, grainStripeOffsetBlk8, inArgs->pGrainSynt, grainStripeWidth,
                                    log2ScaleFactor, scaleFactor, kOffset, lOffset, h, v, FG_BLK_8);
              } /* only if average falls in any interval */
              //  }/* includes corner case handling */
            } /* 8x8 level block processing */

            /* uppdate the PRNG once per 16x16 block of samples */
            offset_tmp++;
          } /* End of 16xwidth grain simulation */

          /* deblocking at the vertical edges of 8x8 at 16xwidth*/
          deblockGrainStripe(grainStripe, widthComp[compCtr], FG_BLK_16, grainStripeWidth, FG_BLK_8);

          /* Blending of size 16xwidth*/

          blendStripe(decSampleHbdOffsetY, grainStripe, widthComp[compCtr], strideComp[compCtr], grainStripeWidth,
                      FG_BLK_16, bitDepth);
          decSampleHbdOffsetY += FG_BLK_16 * strideComp[compCtr];

        } /* end of component loop */
      }
    }
  }

  delete grainStripe;
  return FGS_SUCCESS;
}

uint32_t SEIFilmGrainSynthesizer::fgsSimulationBlending_16x16(fgsProcessArgs *inArgs)
{
  uint8_t  numComp, compCtr; /* number of color components */
  uint8_t  log2ScaleFactor, h, v;
  uint8_t  bitDepth; /*grain bit depth and decoded bit depth are assumed to be same */
  uint32_t widthComp[MAX_NUM_COMPONENT], heightComp[MAX_NUM_COMPONENT], strideComp[MAX_NUM_COMPONENT];
  Pel *    decSampleHbdBlk16, *decSampleHbdOffsetY;
  Pel *    decHbdComp[MAX_NUM_COMPONENT];
  uint16_t numSamples;
  int16_t  scaleFactor;
  uint32_t kOffset, lOffset, grainStripeOffset;
  Pel *    grainStripe; /* worth a row of 16x16 : Max size : 16xw;*/
  uint32_t x, y;
  uint32_t blockAvg, intensityInt; /* ec : seed to be used for the psudo random generator for a given color component */
  uint32_t grainStripeWidth;
  uint32_t wdPadded;

  bitDepth        = inArgs->bitDepth;
  numComp         = inArgs->numComp;
  log2ScaleFactor = inArgs->pFgcParameters->m_log2ScaleFactor;

  for (compCtr = 0; compCtr < numComp; compCtr++)
  {
    decHbdComp[compCtr] = inArgs->decComp[compCtr];
    strideComp[compCtr] = inArgs->strideComp[compCtr];
    widthComp[compCtr]  = inArgs->widthComp[compCtr];
    heightComp[compCtr] = inArgs->heightComp[compCtr];
  }

  wdPadded = ((inArgs->widthComp[0] - 1) | 0xF) + 1;
  grainStripe = new Pel[wdPadded * FG_BLK_16];

  if (0 == inArgs->pFgcParameters->m_filmGrainCharacteristicsCancelFlag)
  {
    for (compCtr = 0; compCtr < numComp; compCtr++)
    {
      if (1 == inArgs->pFgcParameters->m_compModel[compCtr].bPresentFlag)
      {
        decSampleHbdOffsetY  = decHbdComp[compCtr];
        uint32_t *offset_tmp = inArgs->fgsOffsets[compCtr];
        grainStripeWidth = ((widthComp[compCtr] - 1) | 0xF) + 1;   // Make next muliptle of 16

        /* Loop of 16x16 blocks */
        for (y = 0; y < heightComp[compCtr]; y += FG_BLK_16)
        {
          /* Initialization of grain stripe of 16xwidth size */
          memset(grainStripe, 0, (grainStripeWidth * FG_BLK_16 * sizeof(Pel)));
          for (x = 0; x < widthComp[compCtr]; x += FG_BLK_16)
          {
            /* start position offset of decoded sample in x direction */
            grainStripeOffset = x;

            decSampleHbdBlk16 = decSampleHbdOffsetY + x;

            blockAvg =
              blockAverage_16x16(decSampleHbdBlk16, strideComp[compCtr], &numSamples, FG_BLK_16, FG_BLK_16, bitDepth);
            blockAvg = blockAvg >> (FG_BLK_16_shift + (bitDepth - FG_BIT_DEPTH_8));
            /* Selection of the component model */
            intensityInt = inArgs->pGrainSynt->intensityInterval[compCtr][blockAvg];

            if (INTENSITY_INTERVAL_MATCH_FAIL != intensityInt)
            {
              kOffset = (MSB16(*offset_tmp) % 52);
              kOffset &= 0xFFFC;

              lOffset = (LSB16(*offset_tmp) % 56);
              lOffset &= 0xFFF8;
              scaleFactor = 1 - 2 * BIT0(*offset_tmp);

              scaleFactor *=
                inArgs->pFgcParameters->m_compModel[compCtr].intensityValues[intensityInt].compModelValue[0];
              h = inArgs->pFgcParameters->m_compModel[compCtr].intensityValues[intensityInt].compModelValue[1] - 2;
              v = inArgs->pFgcParameters->m_compModel[compCtr].intensityValues[intensityInt].compModelValue[2] - 2;

              /* 16x16 block grain simulation */
              simulateGrainBlk16x16(grainStripe, grainStripeOffset, inArgs->pGrainSynt, grainStripeWidth,
                                    log2ScaleFactor, scaleFactor, kOffset, lOffset, h, v, FG_BLK_16);

            } /* only if average falls in any interval */
            //  }/* includes corner case handling */
            /* uppdate the PRNG once per 16x16 block of samples */
            offset_tmp++;
          } /* End of 16xwidth grain simulation */
          /* deblocking at the vertical edges of 16x16 at 16xwidth*/
          deblockGrainStripe(grainStripe, widthComp[compCtr], FG_BLK_16, grainStripeWidth, FG_BLK_16);

          /* Blending of size 16xwidth*/
          blendStripe(decSampleHbdOffsetY, grainStripe, widthComp[compCtr], strideComp[compCtr], grainStripeWidth,
                      FG_BLK_16, bitDepth);
          decSampleHbdOffsetY += FG_BLK_16 * strideComp[compCtr];

        } /* end of component loop */
      }
    }
  }

  delete grainStripe;
  return FGS_SUCCESS;
}

uint32_t SEIFilmGrainSynthesizer::fgsSimulationBlending_32x32(fgsProcessArgs *inArgs)
{
  uint8_t  numComp, compCtr; /* number of color components */
  uint8_t  log2ScaleFactor, h, v;
  uint8_t  bitDepth; /*grain bit depth and decoded bit depth are assumed to be same */
  uint32_t widthComp[MAX_NUM_COMPONENT], heightComp[MAX_NUM_COMPONENT], strideComp[MAX_NUM_COMPONENT];
  Pel *    decSampleBlk32, *decSampleOffsetY;
  Pel *    decComp[MAX_NUM_COMPONENT];
  int16_t  scaleFactor;
  uint32_t kOffset, lOffset, grainStripeOffset;
  Pel *    grainStripe;
  uint32_t x, y;
  uint32_t blockAvg, intensityInt; /* ec : seed to be used for the psudo random generator for a given color component */
  uint32_t grainStripeWidth;
  uint32_t wdPadded;

  bitDepth = inArgs->bitDepth;
  numComp  = inArgs->numComp;

  log2ScaleFactor = inArgs->pFgcParameters->m_log2ScaleFactor;

  for (compCtr = 0; compCtr < numComp; compCtr++)
  {
    decComp[compCtr]    = inArgs->decComp[compCtr];
    strideComp[compCtr] = inArgs->strideComp[compCtr];
    heightComp[compCtr] = inArgs->heightComp[compCtr];
    widthComp[compCtr]  = inArgs->widthComp[compCtr];
  }

  wdPadded = ((inArgs->widthComp[0] - 1) | 0x1F) + 1;
  grainStripe = new Pel[wdPadded * FG_BLK_32];

  if (0 == inArgs->pFgcParameters->m_filmGrainCharacteristicsCancelFlag)
  {
    for (compCtr = 0; compCtr < numComp; compCtr++)
    {
      if (1 == inArgs->pFgcParameters->m_compModel[compCtr].bPresentFlag)
      {
        uint32_t *offset_tmp = inArgs->fgsOffsets[compCtr];
        decSampleOffsetY     = decComp[compCtr];
        grainStripeWidth = ((widthComp[compCtr] - 1) | 0x1F) + 1;   // Make next muliptle of 32

        /* Loop of 32x32 blocks */
        for (y = 0; y < heightComp[compCtr]; y += FG_BLK_32)
        {
          /* Initialization of grain stripe of 32xwidth size */
          memset(grainStripe, 0, (grainStripeWidth * FG_BLK_32 * sizeof(Pel)));
          for (x = 0; x < widthComp[compCtr]; x += FG_BLK_32)
          {
            /* start position offset of decoded sample in x direction */
            grainStripeOffset = x;
            decSampleBlk32    = decSampleOffsetY + x;
            blockAvg = blockAverage_32x32(decSampleBlk32, strideComp[compCtr], bitDepth);

            /* Selection of the component model */
            intensityInt = inArgs->pGrainSynt->intensityInterval[compCtr][blockAvg];

            if (INTENSITY_INTERVAL_MATCH_FAIL != intensityInt)
            {
              kOffset = (MSB16(*offset_tmp) % 36);
              kOffset &= 0xFFFC;

              lOffset = (LSB16(*offset_tmp) % 40);
              lOffset &= 0xFFF8;
              scaleFactor = 1 - 2 * BIT0(*offset_tmp);

              scaleFactor *= inArgs->pFgcParameters->m_compModel[compCtr].intensityValues[intensityInt].compModelValue[0];
              h = inArgs->pFgcParameters->m_compModel[compCtr].intensityValues[intensityInt].compModelValue[1] - 2;
              v = inArgs->pFgcParameters->m_compModel[compCtr].intensityValues[intensityInt].compModelValue[2] - 2;

              /* 32x32 block grain simulation */
              simulateGrainBlk32x32(grainStripe, grainStripeOffset, inArgs->pGrainSynt, grainStripeWidth,
                                    log2ScaleFactor, scaleFactor, kOffset, lOffset, h, v);

            } /* only if average falls in any interval */

            /* uppdate the PRNG once per 16x16 block of samples */
            offset_tmp++;
          } /* End of 32xwidth grain simulation */

          /* deblocking at the vertical edges of 8x8 at 16xwidth*/
          deblockGrainStripe(grainStripe, widthComp[compCtr], FG_BLK_32, grainStripeWidth, FG_BLK_32);

          blendStripe_32x32(decSampleOffsetY, grainStripe, widthComp[compCtr], strideComp[compCtr], grainStripeWidth, FG_BLK_32, bitDepth);
          decSampleOffsetY += FG_BLK_32 * strideComp[compCtr];
        } /* end of component loop */
      }
    }
  }

  delete grainStripe;
  return FGS_SUCCESS;
}

#endif
