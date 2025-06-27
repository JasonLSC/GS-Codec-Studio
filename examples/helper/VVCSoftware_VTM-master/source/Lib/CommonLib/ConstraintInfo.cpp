/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2025, ITU/ISO/IEC
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

#include "CommonDef.h"
#include "ConstraintInfo.h"

bool operator == (const ConstraintInfo& op1, const ConstraintInfo& op2)
{
  if (op1.m_intraOnlyConstraintFlag != op2.m_intraOnlyConstraintFlag)
  {
    return false;
  }
  if (op1.m_maxBitDepthConstraintIdc != op2.m_maxBitDepthConstraintIdc)
  {
    return false;
  }
  if (op1.m_maxChromaFormatConstraintIdc != op2.m_maxChromaFormatConstraintIdc)
  {
    return false;
  }
  if (op1.m_onePictureOnlyConstraintFlag != op2.m_onePictureOnlyConstraintFlag)
  {
    return false;
  }
  if (op1.m_allRapPicturesFlag != op2.m_allRapPicturesFlag)
  {
    return false;
  }
  if (op1.m_allLayersIndependentConstraintFlag != op2.m_allLayersIndependentConstraintFlag)
  {
    return false;
  }
  if (op1.m_noMrlConstraintFlag != op2.m_noMrlConstraintFlag)
  {
    return false;
  }
  if (op1.m_noIspConstraintFlag != op2.m_noIspConstraintFlag)
  {
    return false;
  }
  if (op1.m_noMipConstraintFlag != op2.m_noMipConstraintFlag)
  {
    return false;
  }
  if (op1.m_noLfnstConstraintFlag != op2.m_noLfnstConstraintFlag)
  {
    return false;
  }
  if (op1.m_noMmvdConstraintFlag != op2.m_noMmvdConstraintFlag)
  {
    return false;
  }
  if (op1.m_noSmvdConstraintFlag != op2.m_noSmvdConstraintFlag)
  {
    return false;
  }
  if (op1.m_noProfConstraintFlag != op2.m_noProfConstraintFlag)
  {
    return false;
  }
  if (op1.m_noPaletteConstraintFlag != op2.m_noPaletteConstraintFlag)
  {
    return false;
  }
  if (op1.m_noActConstraintFlag != op2.m_noActConstraintFlag)
  {
    return false;
  }
  if (op1.m_noLmcsConstraintFlag != op2.m_noLmcsConstraintFlag)
  {
    return false;
  }
  if (op1.m_noExplicitScaleListConstraintFlag != op2.m_noExplicitScaleListConstraintFlag)
  {
    return false;
  }
  if (op1.m_noVirtualBoundaryConstraintFlag != op2.m_noVirtualBoundaryConstraintFlag)
  {
    return false;
  }
  if (op1.m_noChromaQpOffsetConstraintFlag != op2.m_noChromaQpOffsetConstraintFlag)
  {
    return false;
  }
  if (op1.m_noRprConstraintFlag != op2.m_noRprConstraintFlag)
  {
    return false;
  }
  if (op1.m_noResChangeInClvsConstraintFlag != op2.m_noResChangeInClvsConstraintFlag)
  {
    return false;
  }
  if (op1.m_noMttConstraintFlag != op2.m_noMttConstraintFlag)
  {
    return false;
  }
  if (op1.m_noQtbttDualTreeIntraConstraintFlag != op2.m_noQtbttDualTreeIntraConstraintFlag)
  {
    return false;
  }
  if (op1.m_noPartitionConstraintsOverrideConstraintFlag != op2.m_noPartitionConstraintsOverrideConstraintFlag)
  {
    return false;
  }
  if (op1.m_noSaoConstraintFlag != op2.m_noSaoConstraintFlag)
  {
    return false;
  }
  if (op1.m_noAlfConstraintFlag != op2.m_noAlfConstraintFlag)
  {
    return false;
  }
  if (op1.m_noCCAlfConstraintFlag != op2.m_noCCAlfConstraintFlag)
  {
    return false;
  }
  if (op1.m_noWeightedPredictionConstraintFlag != op2.m_noWeightedPredictionConstraintFlag)
  {
    return false;
  }
  if (op1.m_noRefWraparoundConstraintFlag != op2.m_noRefWraparoundConstraintFlag)
  {
    return false;
  }
  if (op1.m_noTemporalMvpConstraintFlag != op2.m_noTemporalMvpConstraintFlag)
  {
    return false;
  }
  if (op1.m_noSbtmvpConstraintFlag != op2.m_noSbtmvpConstraintFlag)
  {
    return false;
  }
  if (op1.m_noAmvrConstraintFlag != op2.m_noAmvrConstraintFlag)
  {
    return false;
  }
  if (op1.m_noBdofConstraintFlag != op2.m_noBdofConstraintFlag)
  {
    return false;
  }
  if (op1.m_noDmvrConstraintFlag != op2.m_noDmvrConstraintFlag)
  {
    return false;
  }
  if (op1.m_noCclmConstraintFlag != op2.m_noCclmConstraintFlag)
  {
    return false;
  }
  if (op1.m_noMtsConstraintFlag != op2.m_noMtsConstraintFlag)
  {
    return false;
  }
  if (op1.m_noSbtConstraintFlag != op2.m_noSbtConstraintFlag)
  {
    return false;
  }
  if (op1.m_noAffineMotionConstraintFlag != op2.m_noAffineMotionConstraintFlag)
  {
    return false;
  }
  if (op1.m_noBcwConstraintFlag != op2.m_noBcwConstraintFlag)
  {
    return false;
  }
  if (op1.m_noIbcConstraintFlag != op2.m_noIbcConstraintFlag)
  {
    return false;
  }
  if (op1.m_noCiipConstraintFlag != op2.m_noCiipConstraintFlag)
  {
    return false;
  }
  if (op1.m_noLadfConstraintFlag != op2.m_noLadfConstraintFlag)
  {
    return false;
  }
  if (op1.m_noTransformSkipConstraintFlag != op2.m_noTransformSkipConstraintFlag)
  {
    return false;
  }
  if (op1.m_noBDPCMConstraintFlag != op2.m_noBDPCMConstraintFlag)
  {
    return false;
  }
  if (op1.m_noJointCbCrConstraintFlag != op2.m_noJointCbCrConstraintFlag)
  {
    return false;
  }
  if (op1.m_noCuQpDeltaConstraintFlag != op2.m_noCuQpDeltaConstraintFlag)
  {
    return false;
  }
  if (op1.m_noDepQuantConstraintFlag != op2.m_noDepQuantConstraintFlag)
  {
    return false;
  }
  if (op1.m_noSignDataHidingConstraintFlag != op2.m_noSignDataHidingConstraintFlag)
  {
    return false;
  }
  if (op1.m_noTrailConstraintFlag != op2.m_noTrailConstraintFlag)
  {
    return false;
  }
  if (op1.m_noStsaConstraintFlag != op2.m_noStsaConstraintFlag)
  {
    return false;
  }
  if (op1.m_noRaslConstraintFlag != op2.m_noRaslConstraintFlag)
  {
    return false;
  }
  if (op1.m_noRadlConstraintFlag != op2.m_noRadlConstraintFlag)
  {
    return false;
  }
  if (op1.m_noIdrConstraintFlag != op2.m_noIdrConstraintFlag)
  {
    return false;
  }
  if (op1.m_noCraConstraintFlag != op2.m_noCraConstraintFlag)
  {
    return false;
  }
  if (op1.m_noGdrConstraintFlag != op2.m_noGdrConstraintFlag)
  {
    return false;
  }
  if (op1.m_noApsConstraintFlag != op2.m_noApsConstraintFlag)
  {
    return false;
  }
  return true;
}

bool operator != (const ConstraintInfo& op1, const ConstraintInfo& op2)
{
  return !(op1 == op2);
}
