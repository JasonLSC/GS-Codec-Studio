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

#include "SEI.h"
#include "SEIDigitallySignedContent.h"

#include <cctype>

#if JVET_AJ0151_DSC_SEI

#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
#if _WIN32
#include <openssl/applink.c>
#endif
#endif

const EVP_MD* getHashFunction(int hashMethod)
{
  switch (hashMethod)
  {
    case 0:
      return EVP_sha1();
      break;
    case 1:
      return EVP_sha224();
      break;
    case 2:
      return EVP_sha256();
      break;
    case 3:
      return EVP_sha384();
      break;
    case 4:
      return EVP_sha512();
      break;
    case 5:
      return EVP_sha512_224();
      break;
    case 6:
      return EVP_sha512_256();
      break;
    default:
      THROW ("unknown hash function");
  }
}

void DscSubstream::initSubstream(int hashMethod)
{
  if (m_currentDigest.size())
  {
    m_lastDigest=m_currentDigest;
    m_currentDigest.clear();
  }

  if (m_ctx != nullptr)
  {
    printf ("DSC warning: initializing substream that was not signed or verified\n");
    EVP_MD_CTX_free(m_ctx);
  }

  m_ctx = EVP_MD_CTX_new();
  CHECK (m_ctx == nullptr, "Failed to create EVP_MD_CTX");
  
  if (EVP_DigestInit_ex(m_ctx, getHashFunction(hashMethod), nullptr) != 1)
  {
    THROW( "EVP_DigestInit_ex failed" );
    EVP_MD_CTX_free(m_ctx);
    exit(1);
  }
  m_streamStatus = DSC_Initialized;
  m_dataAdded = false;
}
bool DscSubstream::addDatapacket(const char *data, size_t length)
{
  CHECK(m_streamStatus == DSC_Uninitalized, "substream is not initialized");

  if (m_streamStatus == DSC_Verified)
  {
    printf ("Stream already verified, cannot add data.\n");
    return false;
  }

  if (!EVP_DigestUpdate(m_ctx, data, length))
  {
    THROW("Message digest update failed.");
    EVP_MD_CTX_free(m_ctx);
    exit(1);
  }
  m_dataAdded = true;

  return true;
}

bool DscSubstream::calculateHash()
{
  CHECK(m_streamStatus == DSC_Uninitalized, "substream is not initialized");

  if (m_streamStatus == DSC_Verified)
  {
    printf ("Stream already verified, cannot calculate hash again\n");
    return false;
  }

  if (m_dataAdded == false)
  {
    return false;
  }

  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int lengthOfHash = 0;

  if (EVP_DigestFinal_ex(m_ctx, hash, &lengthOfHash) != 1)
  {
    THROW ("EVP_DigestFinal_ex failed");
    EVP_MD_CTX_free(m_ctx);
    exit(1);
  }

  EVP_MD_CTX_free(m_ctx);
  m_ctx = nullptr;

  m_currentDigest.resize(lengthOfHash);
  std::memcpy(m_currentDigest.data(), hash, lengthOfHash);

  return true;
}


#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
#if JVET_AL0117_DSC_VSS_IMPLICIT_ASSOCIATION
void DscSubstreamManager::initDscSubstreamManager (int numSubstreams, int hashMethodType, const std::string &certUri, bool hasContentUuid, std::array<uint8_t,16> &contentUuid, const std::vector<std::vector<bool>> &refFlags, bool implicitAssociationFlag)
#else
void DscSubstreamManager::initDscSubstreamManager (int numSubstreams, int hashMethodType, const std::string &certUri, bool hasContentUuid, std::array<uint8_t,16> &contentUuid, const std::vector<std::vector<bool>> &refFlags)
#endif
#else
void DscSubstreamManager::initDscSubstreamManager (int numSubstreams, int hashMethodType, const std::string &certUri, bool hasContentUuid, std::array<uint8_t,16> &contentUuid)
#endif
{
  if (!m_isInitialized)
  {
    m_substream.resize(numSubstreams);
    for (auto &substream: m_substream)
    {
      substream.initSubstream(hashMethodType);
    }
    m_hashMethodType = hashMethodType;
    m_certUri        = certUri;
    m_hasContentUuid = hasContentUuid;
    if (hasContentUuid)
    {
      // always 16 bytes (128 bit)
      m_contentUuid.resize(16);
      std::memcpy(m_contentUuid.data(), contentUuid.data(), 16);
    }

    m_isInitialized = true;
    m_isFirstSubstream = true;

#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
    m_refSubstreamFlag = refFlags;
#if JVET_AL0117_DSC_VSS_IMPLICIT_ASSOCIATION
    m_implicitAssociationModeFlag = implicitAssociationFlag;
#endif
#endif

    printf ("DSC: initializing %d substreams\n", numSubstreams);
  }
  else
  {
    printf ("DSC: re-initializing %d substreams\n", numSubstreams);
    if (numSubstreams  != m_substream.size())
    {
      printf ("DSC Warning: re-initializing with different number of substream, starting a new signed segment\n");
      uninitDscSubstreamManager();
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
#if JVET_AL0117_DSC_VSS_IMPLICIT_ASSOCIATION
      initDscSubstreamManager(numSubstreams, hashMethodType, certUri, hasContentUuid, contentUuid, refFlags, implicitAssociationFlag);
#else
      initDscSubstreamManager(numSubstreams, hashMethodType, certUri, hasContentUuid, contentUuid, refFlags);
#endif
#else
      initDscSubstreamManager(numSubstreams, hashMethodType, certUri, hasContentUuid, contentUuid);
#endif
      return;
    }
    if (hashMethodType  != m_hashMethodType)
    {
      printf ("DSC Warning: re-initializing with different hash method type, starting a new signed segment\n");
      uninitDscSubstreamManager();
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
#if JVET_AL0117_DSC_VSS_IMPLICIT_ASSOCIATION
      initDscSubstreamManager(numSubstreams, hashMethodType, certUri, hasContentUuid, contentUuid, refFlags, implicitAssociationFlag);
#else
      initDscSubstreamManager(numSubstreams, hashMethodType, certUri, hasContentUuid, contentUuid, refFlags);
#endif
#else
      initDscSubstreamManager(numSubstreams, hashMethodType, certUri, hasContentUuid, contentUuid);
#endif
      return;
    }
    if (certUri  != m_certUri)
    {
      printf ("DSC Warning: re-initializing with different certificate URI, starting a new signed segment\n");
      uninitDscSubstreamManager();
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
#if JVET_AL0117_DSC_VSS_IMPLICIT_ASSOCIATION
      initDscSubstreamManager(numSubstreams, hashMethodType, certUri, hasContentUuid, contentUuid, refFlags, implicitAssociationFlag);
#else
      initDscSubstreamManager(numSubstreams, hashMethodType, certUri, hasContentUuid, contentUuid, refFlags);
#endif
#else
      initDscSubstreamManager(numSubstreams, hashMethodType, certUri, hasContentUuid, contentUuid);
#endif
      return;
    }
    if (hasContentUuid  != m_hasContentUuid)
    {
      printf ("DSC Warning: re-initializing with different presence of content UUID, starting a new signed segment\n");
      uninitDscSubstreamManager();
#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
#if JVET_AL0117_DSC_VSS_IMPLICIT_ASSOCIATION
      initDscSubstreamManager(numSubstreams, hashMethodType, certUri, hasContentUuid, contentUuid, refFlags, implicitAssociationFlag);
#else
      initDscSubstreamManager(numSubstreams, hashMethodType, certUri, hasContentUuid, contentUuid, refFlags);
#endif
#else
      initDscSubstreamManager(numSubstreams, hashMethodType, certUri, hasContentUuid, contentUuid);
#endif
      return;
    }
    for (auto &substream: m_substream)
    {
      substream.initSubstream(hashMethodType);
    }
    // update content UUID
    if (hasContentUuid)
    {
      // always 16 bytes (128 bit)
      m_contentUuid.resize(16);
      std::memcpy(m_contentUuid.data(), contentUuid.data(), 16);
    }
    m_isFirstSubstream = false;
  }
}

void DscSubstreamManager::initSignature(const std::string &privKeyFile)
{
  if (!m_sigInitialized)
  {
    m_dscSign.initDscSignature(privKeyFile, m_hashMethodType);
    m_sigInitialized = true;
  }
}

bool DscSubstreamManager::initVerificator (const std::string &keyStoreDir, const std::string &trustStoreDir)
{
  if (!m_dscVerify.initDscVerificator (m_certUri, keyStoreDir, trustStoreDir, m_hashMethodType))
  {
    return false;
  }

  return true;
}

bool DscSubstreamManager::isVerificationActive()
{
  return m_isInitialized && m_dscVerify.isInitialized();
}


void DscSubstreamManager::addToSubstream (int substreamId, const char *data, size_t length)
{
  CHECK(substreamId >= m_substream.size(), "Invalid substream");
  if (!m_substream[substreamId].addDatapacket(data, length))
  {
    printf ("Trying to add NAL unit to substream %d, which was already verified.\n", substreamId);
  }
}


void DscSubstreamManager::createDatapacket (int substreamId, std::vector<uint8_t> &dataPacket)
{
  std::vector<uint8_t> refDigest;
  std::vector<uint8_t> curDigest;

  m_substream[substreamId].getCurrentDigest(curDigest);

#if JVET_AK0287_DSCI_SEI_REF_SUBSTREAM_FLAG
  int numRefs = 0;
  std::vector<int> refSubstreamId;
  for (int i = 0; i < substreamId; i++)
  {
    if (m_refSubstreamFlag[substreamId][i])
    {
      numRefs++;
      refSubstreamId.push_back(i);
    }
  }
  if (m_isFirstSubstream)
  {
    refDigest.resize(curDigest.size());
    std::memset(refDigest.data(), 0xFF, refDigest.size());
  }
  else
  {
    m_substream[numRefs > 0 ? refSubstreamId[0] : substreamId].getLastDigest(refDigest);
  }
  dataPacket.insert(dataPacket.end(), refDigest.begin(), refDigest.end());
  for (int i = 0; i < numRefs; i++)
  {
    m_substream[refSubstreamId[i]].getCurrentDigest(refDigest);
    dataPacket.insert(dataPacket.end(), refDigest.begin(), refDigest.end());
  }
#else
  if (substreamId == 0)
  {
    if (m_isFirstSubstream)
    {
      refDigest.resize(curDigest.size());
      std::memset(refDigest.data(), 0xFF, refDigest.size());
    }
    else
    {
      m_substream[substreamId].getLastDigest(refDigest);
    }
  }
  else
  {
    m_substream[substreamId - 1].getCurrentDigest(refDigest);
  }
  dataPacket.insert(dataPacket.end(), refDigest.begin(), refDigest.end());
#endif
  dataPacket.insert(dataPacket.end(), curDigest.begin(), curDigest.end());
  dataPacket.insert(dataPacket.end(), m_hashMethodType);
  if (m_hasContentUuid)
  {
    dataPacket.insert(dataPacket.end(), m_contentUuid.begin(), m_contentUuid.end());
  }

};

void DscSubstreamManager::signSubstream (int substreamId, std::vector<uint8_t> &signature)
{
  printf ("DSC: signing substream %d\n", substreamId);
  CHECK(substreamId >= m_substream.size(), "Invalid substream");

  if (! m_substream[substreamId].calculateHash())
  {
    THROW("Cannot create signature, because no data was added into substream");
  }

  std::vector<uint8_t> dataPacket;
  createDatapacket (substreamId, dataPacket);

  m_dscSign.signPacket(dataPacket, signature);
}

bool DscSubstreamManager::verifySubstream (int substreamId, std::vector<uint8_t> &signature)
{
  if (!m_isInitialized || !m_dscVerify.isInitialized() )
  {
    std::cerr << "Cannot verify signature for substream " << substreamId << ". Not initialized." << std::endl;
    return false;
  }
  printf ("DSC: verifying substream %d\n", substreamId);
  CHECK(substreamId >= m_substream.size(), "Invalid substream");
  if (! m_substream[substreamId].calculateHash())
  {
    std::cerr << "Cannot verify signature for substream " << substreamId << " because no NAL units are present in that substream." << std::endl;
    return false;
  }
  std::vector<uint8_t> dataPacket;
  createDatapacket (substreamId, dataPacket);

  return m_dscVerify.verifyPacket(dataPacket, signature);
}


void DscSubstreamManager::uninitDscSubstreamManager()
{
  CHECK(m_isInitialized == false, "substream manager is not initialized");
  m_substream.clear();
  m_isInitialized = false;
}

bool DscSignature::initDscSignature (const std::string &keyfile, int hashMethod)
{
  CHECK(m_isInitialized == true, "Signature is already initialized");
  // Read private key
  FILE *pkeyFile = fopen(keyfile.c_str(), "r");
  if (!pkeyFile)
  {
    std::cerr << "Error opening private key file: " << keyfile << std::endl;
    return false;
  }
  m_privKey = PEM_read_PrivateKey(pkeyFile, nullptr, nullptr, nullptr);
  fclose(pkeyFile);
  if (!m_privKey)
  {
    std::cerr << "Error reading private key: " << keyfile << std::endl;
    return false;
  }

  m_hashMethodType    = hashMethod;
  m_isInitialized = true;
  return true;
}

bool DscSignature::signPacket (std::vector<uint8_t> &packet, std::vector<uint8_t> &signature)
{
  CHECK(m_isInitialized == false, "Signature is not initialized");


  // Create signature
  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  EVP_SignInit(mdctx, getHashFunction(m_hashMethodType));
  EVP_SignUpdate(mdctx, packet.data(), packet.size());
  unsigned int sig_len;
  // resize the maximum lenght of the signature
  signature.resize(EVP_PKEY_size(m_privKey));
  EVP_SignFinal(mdctx, signature.data(), &sig_len, m_privKey);
  // resize to the actual length of the signature
  signature.resize(sig_len);
  EVP_MD_CTX_free(mdctx);
  return true;
}

void DscSignature::uninitDscSignature ()
{
  if (m_isInitialized)
  {
    if (m_privKey)
    {
      EVP_PKEY_free(m_privKey);
      m_privKey = nullptr;
    }
    m_isInitialized = false;
  }
}


// Verify a certificate against trusted certificate storage
// Directory needs to contain hash links as created by 'openssl rehash <dir>'
// returns
//   DSCStatus::DSCError - in case of error
//   DSCStatus::DSC_Untrusted - if the verification fails
//   DSCStatus::DSC_Verified  - if the verificatoion is successfull
DSCStatus DscVerificator::verifyCert (const std::string &certFile, const std::string &trustStoreDir)
{
  BIO *certbio = NULL;
  X509 *cert = NULL;

  // Create a new BIO object for reading the certificate
  certbio = BIO_new(BIO_s_file());

  // Load the certificate from file
  if (BIO_read_filename(certbio, certFile.c_str()) <= 0)
  {
    printf("Error loading cert PEM file\n");
    return DSCStatus::DSC_Error;
  }
  cert = PEM_read_bio_X509(certbio, NULL, 0, NULL);
  if (!cert)
  {
    printf("Error loading cert into memory\n");
    return DSCStatus::DSC_Error;
  }

  // Print the certificate's subject name
  char *subject = X509_NAME_oneline(X509_get_subject_name(cert), NULL, 0);
  printf("Certificate Subject: %s\n", subject);

  // The trusted store is the set of CA certificates that are trusted
  X509_STORE *store = X509_STORE_new();

  // Load all CA certificates from the specified directory
  if (!X509_STORE_load_locations(store, NULL, trustStoreDir.c_str()))
  {
    printf("Error loading CA certs from directory\n");
    return DSCStatus::DSC_Error;
  }

  // The context holds the validation parameters and results
  X509_STORE_CTX *ctx = X509_STORE_CTX_new();

  // Initialize the context for the validation operation
  if (!X509_STORE_CTX_init(ctx, store, cert, NULL))
  {
    printf("Error initializing validation context\n");
    return DSCStatus::DSC_Error;
  }

  // Perform the validation
  int result = X509_verify_cert(ctx);
  if (result != 1)
  {
    printf("\033[1;31mCertificate validation failed\033[0m\n");
    m_certVerificationStatus = DSCStatus::DSC_Untrusted;
    return DSCStatus::DSC_Untrusted;
  }
  else
  {
    // Print the certificate's issuer name
    char *issuer = X509_NAME_oneline(X509_get_issuer_name(cert), NULL, 0);
    printf("Certificate Issuer (CA): %s\n", issuer);
    printf("\033[1;32mCertificate validation passed.\033[0m\n");
    m_certVerificationStatus = DSCStatus::DSC_Verified;
  }

  // Clean up
  X509_STORE_CTX_free(ctx);
  X509_STORE_free(store);
  X509_free(cert);
  BIO_free_all(certbio);

  return DSCStatus::DSC_Verified;
}

// Convert a URI to a local storage location
// Works only for file:// URIs at the moment, but could easily be extended to other sources
// Paths in the URI are ignored
bool DscVerificator::xLocateCertificate (const std::string &certificateURI, const std::string &keyStoreDir, std::string &targetPath)
{
  // we currently support only "FILE://" URIs
  std::string uri = certificateURI;
  std::string lowercaseURI = uri;
  std::transform(lowercaseURI.begin(), lowercaseURI.end(), lowercaseURI.begin(), [](unsigned char c){ return std::tolower(c); });
  if (lowercaseURI.rfind("file://", 0) == std::string::npos)
  {
    printf ("This software currently only supports FILE:// URIs\n");
    return false;
  }

  std::size_t found = uri.rfind("/");

  if ((found + 1) == uri.size())
  {
    // "/" character found at the end of URI -> remove and find again
    uri.pop_back();
    found = uri.rfind("/");
  }

  CHECK (found==std::string::npos, "no / character found in URI. This should not happen after previous prefix check.")

  targetPath = keyStoreDir + "/" + uri.substr(found + 1);

  std::cout << "Using certificate file: " << targetPath << std::endl;

  return true;
}

DSCStatus DscVerificator::initDscVerificator (const std::string &certificateURI, const std::string &keyStoreDir, const std::string &trustStoreDir, int hashMethod)
{
  m_certVerificationStatus = DSCStatus::DSC_Uninitalized;

  // sanitize certificate URI and redirect to key store
  std::string certificatePath;

  if (!xLocateCertificate(certificateURI, keyStoreDir, certificatePath))
  {
    return DSCStatus::DSC_Error;
  }

  // verify certificate
  m_certVerificationStatus = verifyCert(certificatePath, trustStoreDir);
  if (m_certVerificationStatus == DSCStatus::DSC_Error )
  {
    return m_certVerificationStatus;
  }

  // Read certificate
  FILE *certFile = fopen(certificatePath.c_str(), "r");
  if (!certFile)
  {
    std::cerr << "Error opening certificate file\n";
    return DSCStatus::DSC_Error;
  }

  X509 *cert = PEM_read_X509(certFile, nullptr, nullptr, nullptr);
  fclose(certFile);

  if (!cert)
  {
    std::cerr << "Error reading certificate\n";
    return DSCStatus::DSC_Error;
  }

  // Get public key from certificate
  m_pubKey = X509_get_pubkey(cert);
  if (!m_pubKey)
  {
    std::cerr << "Error getting public key from certificate\n";
    return DSCStatus::DSC_Error;
  }

  m_hashMethodType = hashMethod;
  m_isInitialized = true;

  return DSCStatus::DSC_Initialized;
}

DSCStatus DscVerificator::verifyPacket (std::vector<uint8_t> &packet, std::vector<uint8_t> &signature)
{
  if (!m_isInitialized)
  {
    return DSCStatus::DSC_Uninitalized;
  }
  // Verify signature
  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  EVP_VerifyInit(mdctx, getHashFunction(m_hashMethodType));
  EVP_VerifyUpdate(mdctx, packet.data(), packet.size());
  int verify_result = EVP_VerifyFinal(mdctx, signature.data(), (unsigned int) signature.size(), m_pubKey);
  EVP_MD_CTX_free(mdctx);

  if (verify_result == 1)
  {
    if ( m_certVerificationStatus != DSCStatus::DSC_Verified )
    {
      std::cout << "\033[1;31mSignature is valid, but CA is untrusted.\033[0m\n";
      return DSCStatus::DSC_Untrusted;
    }
    std::cout << "\033[1;32mSignature is valid.\033[0m\n";
    return DSCStatus::DSC_Verified;
  } else if (verify_result == 0)
  {
    std::cout << "\033[1;31mSignature is invalid.\033[0m\n";
    return DSCStatus::DSC_Invalid;
  } else
  {
    std::cout << "Error occurred during verification.\n";
  }

  if (verify_result == -1)
  {
    unsigned long err = ERR_get_error();
    char err_msg[120];
    ERR_error_string_n(err, err_msg, sizeof(err_msg));
    std::cerr << "Verification error: " << err_msg << "\n";
  }
  return DSCStatus::DSC_Error;
}

void DscVerificator::uninitDscVerificator ()
{
  if (m_isInitialized)
  {
    if (m_pubKey)
    {
      EVP_PKEY_free(m_pubKey);
      m_pubKey = nullptr;
    }
    m_isInitialized = false;
  }
}

#endif
