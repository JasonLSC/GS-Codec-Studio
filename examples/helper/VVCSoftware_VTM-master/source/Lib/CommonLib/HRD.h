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


#ifndef __HRD__
#define __HRD__

#include "Common.h"
#include "SEI.h"
class OlsHrdParams
{
private:
  bool     m_fixedPicRateGeneralFlag   = false;
  bool     m_fixedPicRateWithinCvsFlag = false;
  bool     m_lowDelayHrdFlag           = false;
  uint32_t m_elementDurationInTc       = 1;

  uint32_t m_bitRateValueMinus1[MAX_CPB_CNT][2];
  uint32_t m_cpbSizeValueMinus1[MAX_CPB_CNT][2];
  uint32_t m_ducpbSizeValueMinus1[MAX_CPB_CNT][2];
  uint32_t m_duBitRateValueMinus1[MAX_CPB_CNT][2];
  bool     m_cbrFlag[MAX_CPB_CNT][2];

public:
  OlsHrdParams();
  virtual ~OlsHrdParams();

  void      setFixedPicRateGeneralFlag(bool flag) { m_fixedPicRateGeneralFlag = flag; }
  bool      getFixedPicRateGeneralFlag() const { return m_fixedPicRateGeneralFlag; }
  void      setFixedPicRateWithinCvsFlag(bool flag) { m_fixedPicRateWithinCvsFlag = flag; }
  bool      getFixedPicRateWithinCvsFlag() const { return m_fixedPicRateWithinCvsFlag; }
  void      setElementDurationInTc(uint32_t value) { m_elementDurationInTc = value; }
  uint32_t  getElementDurationInTc() const { return m_elementDurationInTc; }
  void      setLowDelayHrdFlag(bool flag) { m_lowDelayHrdFlag = flag; }
  bool      getLowDelayHrdFlag() const { return m_lowDelayHrdFlag; }
  void      setBitRateValueMinus1(int cpbcnt, int nalOrVcl, uint32_t value) { m_bitRateValueMinus1[cpbcnt][nalOrVcl] = value; }
  uint32_t  getBitRateValueMinus1(int cpbcnt, int nalOrVcl) const { return m_bitRateValueMinus1[cpbcnt][nalOrVcl]; }

  void      setCpbSizeValueMinus1(int cpbcnt, int nalOrVcl, uint32_t value) { m_cpbSizeValueMinus1[cpbcnt][nalOrVcl] = value; }
  uint32_t  getCpbSizeValueMinus1(int cpbcnt, int nalOrVcl) const { return m_cpbSizeValueMinus1[cpbcnt][nalOrVcl]; }
  void      setDuCpbSizeValueMinus1(int cpbcnt, int nalOrVcl, uint32_t value) { m_ducpbSizeValueMinus1[cpbcnt][nalOrVcl] = value; }
  uint32_t  getDuCpbSizeValueMinus1(int cpbcnt, int nalOrVcl) const { return m_ducpbSizeValueMinus1[cpbcnt][nalOrVcl]; }
  void      setDuBitRateValueMinus1(int cpbcnt, int nalOrVcl, uint32_t value) { m_duBitRateValueMinus1[cpbcnt][nalOrVcl] = value; }
  uint32_t  getDuBitRateValueMinus1(int cpbcnt, int nalOrVcl) const { return m_duBitRateValueMinus1[cpbcnt][nalOrVcl]; }
  void      setCbrFlag(int cpbcnt, int nalOrVcl, bool value) { m_cbrFlag[cpbcnt][nalOrVcl] = value; }
  bool      getCbrFlag(int cpbcnt, int nalOrVcl) const { return m_cbrFlag[cpbcnt][nalOrVcl]; }
};

class GeneralHrdParams
{
private:
  uint32_t m_numUnitsInTick;
  uint32_t m_timeScale;
  bool     m_generalNalHrdParamsPresentFlag;
  bool     m_generalVclHrdParamsPresentFlag;
  bool     m_generalSamePicTimingInAllOlsFlag;
  uint32_t m_tickDivisorMinus2;
  bool     m_generalDecodingUnitHrdParamsPresentFlag;
  uint32_t m_bitRateScale;
  uint32_t m_cpbSizeScale;
  uint32_t m_cpbSizeDuScale;
  uint32_t m_hrdCpbCntMinus1;

public:
  GeneralHrdParams()
    :m_generalNalHrdParamsPresentFlag(false)
    ,m_generalVclHrdParamsPresentFlag(false)
    ,m_generalSamePicTimingInAllOlsFlag(true)
    ,m_tickDivisorMinus2                 (0)
    ,m_generalDecodingUnitHrdParamsPresentFlag  (false)
    ,m_bitRateScale                      (0)
    ,m_cpbSizeScale                      (0)
    ,m_cpbSizeDuScale                    (0)
    ,m_hrdCpbCntMinus1(0)
  {}
  bool operator==(const GeneralHrdParams& other) const
  {
    return (m_numUnitsInTick == other.m_numUnitsInTick
      && m_timeScale == other.m_timeScale
      && m_generalNalHrdParamsPresentFlag == other.m_generalNalHrdParamsPresentFlag
      && m_generalVclHrdParamsPresentFlag == other.m_generalVclHrdParamsPresentFlag
      && m_generalSamePicTimingInAllOlsFlag == other.m_generalSamePicTimingInAllOlsFlag
      && m_generalDecodingUnitHrdParamsPresentFlag == other.m_generalDecodingUnitHrdParamsPresentFlag
      && (m_generalDecodingUnitHrdParamsPresentFlag ? (m_tickDivisorMinus2 == other.m_tickDivisorMinus2): 1)
      && m_bitRateScale == other.m_bitRateScale
      && m_cpbSizeScale == other.m_cpbSizeScale
      && (m_generalDecodingUnitHrdParamsPresentFlag ? (m_cpbSizeDuScale == other.m_cpbSizeDuScale) : 1)
      && m_hrdCpbCntMinus1 == other.m_hrdCpbCntMinus1
      );
  }

  GeneralHrdParams& operator=(const GeneralHrdParams& input)
  {
    m_numUnitsInTick = input.m_numUnitsInTick;
    m_timeScale = input.m_timeScale;
    m_generalNalHrdParamsPresentFlag = input.m_generalNalHrdParamsPresentFlag;
    m_generalVclHrdParamsPresentFlag = input.m_generalVclHrdParamsPresentFlag;
    m_generalSamePicTimingInAllOlsFlag = input.m_generalSamePicTimingInAllOlsFlag;
    m_generalDecodingUnitHrdParamsPresentFlag = input.m_generalDecodingUnitHrdParamsPresentFlag;
    if (input.m_generalDecodingUnitHrdParamsPresentFlag)
    {
      m_tickDivisorMinus2 = input.m_tickDivisorMinus2;
    }
    m_bitRateScale = input.m_bitRateScale;
    m_cpbSizeScale = input.m_cpbSizeScale;
    if (input.m_generalDecodingUnitHrdParamsPresentFlag)
    {
      m_cpbSizeDuScale = input.m_cpbSizeDuScale;
    }
    m_hrdCpbCntMinus1 = input.m_hrdCpbCntMinus1;
    return *this;
  }
  virtual ~GeneralHrdParams() {}
  void      setNumUnitsInTick(uint32_t value) { m_numUnitsInTick = value; }
  uint32_t  getNumUnitsInTick() const { return m_numUnitsInTick; }

  void      setTimeScale(uint32_t value) { m_timeScale = value; }
  uint32_t  getTimeScale() const { return m_timeScale; }

  void      setGeneralNalHrdParametersPresentFlag(bool flag) { m_generalNalHrdParamsPresentFlag = flag; }
  bool      getGeneralNalHrdParametersPresentFlag() const { return m_generalNalHrdParamsPresentFlag; }

  void      setGeneralVclHrdParametersPresentFlag(bool flag) { m_generalVclHrdParamsPresentFlag = flag; }
  bool      getGeneralVclHrdParametersPresentFlag() const { return m_generalVclHrdParamsPresentFlag; }

  void      setGeneralSamePicTimingInAllOlsFlag(bool flag) { m_generalSamePicTimingInAllOlsFlag = flag; }
  bool      getGeneralSamePicTimingInAllOlsFlag() const { return m_generalSamePicTimingInAllOlsFlag; }


  void      setTickDivisorMinus2( uint32_t value )                                     { m_tickDivisorMinus2 = value;                               }
  uint32_t  getTickDivisorMinus2( ) const                                              { return m_tickDivisorMinus2;                                }


  void      setGeneralDecodingUnitHrdParamsPresentFlag( bool flag)                     { m_generalDecodingUnitHrdParamsPresentFlag = flag;                 }
  bool      getGeneralDecodingUnitHrdParamsPresentFlag( ) const                        { return m_generalDecodingUnitHrdParamsPresentFlag;                 }

  void      setBitRateScale( uint32_t value )                                          { m_bitRateScale = value;                                    }
  uint32_t  getBitRateScale( ) const                                                   { return m_bitRateScale;                                     }

  void      setCpbSizeScale( uint32_t value )                                          { m_cpbSizeScale = value;                                    }
  uint32_t  getCpbSizeScale( ) const                                                   { return m_cpbSizeScale;                                     }
  void      setCpbSizeDuScale( uint32_t value )                                        { m_cpbSizeDuScale = value;                                  }
  uint32_t  getCpbSizeDuScale( ) const                                                 { return m_cpbSizeDuScale;                                   }

  void      setHrdCpbCntMinus1(uint32_t value) { m_hrdCpbCntMinus1 = value; }
  uint32_t  getHrdCpbCntMinus1() const { return m_hrdCpbCntMinus1; }

};

inline void checkBPSyntaxElementLength(const SEIBufferingPeriod* bp1, const SEIBufferingPeriod* bp2)
{
  CHECK(bp1->cpbInitialRemovalDelayLength != bp2->cpbInitialRemovalDelayLength
          || bp1->cpbRemovalDelayLength != bp2->cpbRemovalDelayLength
          || bp1->dpbOutputDelayLength != bp2->dpbOutputDelayLength
          || bp1->duCpbRemovalDelayIncrementLength != bp2->duCpbRemovalDelayIncrementLength
          || bp1->dpbOutputDelayDuLength != bp2->dpbOutputDelayDuLength,
        "All scalable-nested and non-scalable nested BP SEI messages in a CVS shall have the same value for "
        "each of the syntax elements bp_cpb_initial_removal_delay_length_minus1, bp_cpb_removal_delay_length_minus1, "
        "bp_dpb_output_delay_length_minus1, bp_du_cpb_removal_delay_increment_length_minus1, "
        "and bp_dpb_output_delay_du_length_minus1");
}

class HRD
{
public:
  HRD() : m_bufferingPeriodInitialized(false), m_pictureTimingAvailable(false){};

  virtual ~HRD()
  {};
  void                 setGeneralHrdParameters(const GeneralHrdParams &generalHrdParam) { m_generalHrdParams = generalHrdParam; }
  GeneralHrdParams        getGeneralHrdParameters() const { return m_generalHrdParams; }
  const GeneralHrdParams& getGeneralHrdParameters() { return m_generalHrdParams; }

  void                 setOlsHrdParameters(int tLayter, OlsHrdParams &olsHrdParam) { m_olsHrdParams[tLayter] = olsHrdParam; }
  OlsHrdParams           getOlsHrdParameters(int idx) { return m_olsHrdParams[idx]; }
  OlsHrdParams*          getOlsHrdParametersAddr() { return m_olsHrdParams; }
  const OlsHrdParams&    getOlsHrdParameters(int idx) const { return m_olsHrdParams[idx]; }

  void setBufferingPeriodSEI(const SEIBufferingPeriod* bp)
  {
    if (m_bufferingPeriodInitialized)
    {
      checkBPSyntaxElementLength(bp, &m_bufferingPeriodSEI);
    }
    bp->copyTo(m_bufferingPeriodSEI);
    m_bufferingPeriodInitialized = true;
  }

  const SEIBufferingPeriod* getBufferingPeriodSEI() const
  {
    return m_bufferingPeriodInitialized ? &m_bufferingPeriodSEI : nullptr;
  }

  void setPictureTimingSEI(const SEIPictureTiming* pt)
  {
    m_pictureTimingSEI = *pt;
    m_pictureTimingAvailable = true;
  }
  const SEIPictureTiming* getPictureTimingSEI() const
  {
    return m_pictureTimingAvailable ? &m_pictureTimingSEI : nullptr;
  }

protected:
  GeneralHrdParams      m_generalHrdParams;
  OlsHrdParams          m_olsHrdParams[MAX_TLAYER];
  bool               m_bufferingPeriodInitialized;
  SEIBufferingPeriod m_bufferingPeriodSEI;
  bool               m_pictureTimingAvailable;
  SEIPictureTiming   m_pictureTimingSEI;
};

#endif //__HRD__
