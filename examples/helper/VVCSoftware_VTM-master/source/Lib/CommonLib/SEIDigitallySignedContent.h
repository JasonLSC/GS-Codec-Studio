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

#pragma once


#include "CommonDef.h"
#include "SEI.h"


#if JVET_AJ0151_DSC_SEI

#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

struct PelStorage;

typedef enum : uint32_t
{
  DSC_Uninitalized,
  DSC_Initialized,
  DSC_Error,
  DSC_Untrusted,
  DSC_Verified,
  DSC_Invalid,
} DSCStatus;

#endif


class SEIDigitallySignedContentInitialization: public SEI
{
public:
  int8_t       dsciHashMethodType            = 0;
  std::string  dsciKeySourceUri;
  int8_t       dsciNumVerificationSubstreams = 0;
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
  std::vector<std::vector<bool>> dsciRefSubstreamFlag;
#endif
#if JVET_AL0117_DSC_VSS_IMPLICIT_ASSOCIATION
  bool         dsciVSSImplicitAssociationModeFlag = false;
#endif
  int8_t       dsciKeyRetrievalModeIdc       = 0;
  bool         dsciUseKeyRegisterIdxFlag     = false;
  int32_t      dsciKeyRegisterIdx            = 0;
  bool         dsciContentUuidPresentFlag    = false;
  std::array<uint8_t, 16> dsciContentUuid = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

public:
  SEIDigitallySignedContentInitialization()
  {};

  virtual ~SEIDigitallySignedContentInitialization()
  {};

  PayloadType payloadType() const { return PayloadType::DIGITALLY_SIGNED_CONTENT_INITIALIZATION; }
};

class SEIDigitallySignedContentSelection: public SEI
{
public:
  int32_t      dscsVerificationSubstreamId = 0;

public:
  SEIDigitallySignedContentSelection()
  {};

  virtual ~SEIDigitallySignedContentSelection()
  {};

  PayloadType payloadType() const { return PayloadType::DIGITALLY_SIGNED_CONTENT_SELECTION; }
};

class SEIDigitallySignedContentVerification: public SEI
{
public:
  int32_t              dscvVerificationSubstreamId = 0;
  int32_t              dscvSignatureLengthInOctets = 0;
  std::vector<uint8_t> dscvSignature;

public:
  SEIDigitallySignedContentVerification()
  {};

  virtual ~SEIDigitallySignedContentVerification()
  {};

  PayloadType payloadType() const { return PayloadType::DIGITALLY_SIGNED_CONTENT_VERIFICATION; }
};


#if JVET_AJ0151_DSC_SEI

class DscSignature
{
private:
  bool m_isInitialized  = false;
  int  m_hashMethodType = -1;
  EVP_PKEY *m_privKey   = nullptr;

public:
  ~DscSignature()
  {
    uninitDscSignature();
  };
  bool initDscSignature (const std::string &keyfile, int hashMethodType);
  bool signPacket (std::vector<uint8_t> &packet, std::vector<uint8_t> &signature);
  void uninitDscSignature ();
};

class DscVerificator
{
private:
  bool m_isInitialized  = false;
  int  m_hashMethodType = -1;
  EVP_PKEY *m_pubKey    = nullptr;
  DSCStatus m_certVerificationStatus = DSCStatus::DSC_Uninitalized;
public:
  ~DscVerificator()
  {
    uninitDscVerificator();
  };
  DSCStatus initDscVerificator (const std::string &pubKeyUri, const std::string &keyStoreDir, const std::string &trustStoreDir, int hashMethodType);
  DSCStatus verifyCert (const std::string &certFile, const std::string &trustStoreDir);
  DSCStatus verifyPacket (std::vector<uint8_t> &packet, std::vector<uint8_t> &signature);
  void      uninitDscVerificator ();

  bool isInitialized() { return m_isInitialized; };

protected:
  bool xLocateCertificate (const std::string &certificateURI, const std::string &keyStoreDir, std::string &targetPath);

};


class DscSubstream
{
private:
  DSCStatus   m_streamStatus  = DSC_Uninitalized;
  bool        m_dataAdded     = false;
  EVP_MD_CTX* m_ctx = nullptr;

  std::vector<uint8_t> m_lastDigest;
  std::vector<uint8_t> m_currentDigest;

public:
  void initSubstream(int hashMethod);
  bool addDatapacket(const char *data, size_t length);
  bool calculateHash();
  void getCurrentDigest (std::vector<uint8_t> &digest) { digest= m_currentDigest; };
  void getLastDigest    (std::vector<uint8_t> &digest) { digest= m_lastDigest; };
};

class DscSubstreamManager
{
private:
  DscSignature    m_dscSign;
  DscVerificator  m_dscVerify;

  std::vector<DscSubstream> m_substream;

  uint8_t m_hashMethodType = 0;
  bool    m_hasContentUuid = false;
  std::vector<uint8_t> m_contentUuid;

  std::string m_certUri;

  bool    m_isFirstSubstream = true;

  bool    m_isInitialized = false;

  bool    m_sigInitialized = false;

#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
  std::vector<std::vector<bool>> m_refSubstreamFlag;
#endif
#if JVET_AL0117_DSC_VSS_IMPLICIT_ASSOCIATION
  bool m_implicitAssociationModeFlag = false;
#endif

public:
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
#if JVET_AL0117_DSC_VSS_IMPLICIT_ASSOCIATION
  bool getDscAssociationModeFlag(){return m_implicitAssociationModeFlag;}
  void initDscSubstreamManager (int numSubstreams, int hashMethodType, const std::string &certUri, bool hasContentUuid, std::array<uint8_t,16> &contentUuid, const std::vector<std::vector<bool>> &refFlags, bool implicitAssociationFlag);
#else
  void initDscSubstreamManager (int numSubstreams, int hashMethodType, const std::string &certUri, bool hasContentUuid, std::array<uint8_t,16> &contentUuid, const std::vector<std::vector<bool>> &refFlags);
#endif
#else
  void initDscSubstreamManager (int numSubstreams, int hashMethodType, const std::string &certUri, bool hasContentUuid, std::array<uint8_t,16> &contentUuid);
#endif

  void initSignature   (const std::string &privKeyFile);
  bool initVerificator (const std::string &keyStoreDir, const std::string &trustStoreDir);

  void addToSubstream (int substreamId, const char *data, size_t lenght);
  void signSubstream (int substreamId, std::vector<uint8_t> &signature);
  bool verifySubstream (int substreamId, std::vector<uint8_t> &signature);
  void uninitDscSubstreamManager();

  bool isVerificationActive();
protected:
  void createDatapacket (int substreamId, std::vector<uint8_t> &dataPacket);

};

#endif
