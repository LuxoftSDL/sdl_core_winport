/*
 * Copyright (c) 2014, Ford Motor Company
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the name of the Ford Motor Company nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SRC_COMPONENTS_SECURITY_MANAGER_INCLUDE_SECURITY_MANAGER_CRYPTO_MANAGER_IMPL_H_
#define SRC_COMPONENTS_SECURITY_MANAGER_INCLUDE_SECURITY_MANAGER_CRYPTO_MANAGER_IMPL_H_

#if defined(OS_WINDOWS)
#include "utils/winhdr.h"
#endif

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdint.h>
#include <map>
#include <string>

#include "security_manager/crypto_manager.h"
#include "security_manager/ssl_context.h"
#include "utils/lock.h"
#include "utils/macro.h"

#ifdef OS_WINDOWS
#ifdef X509_NAME
#undef X509_NAME
#endif
#endif

namespace security_manager {
class CryptoManagerImpl : public CryptoManager {
 private:
  class SSLContextImpl : public SSLContext {
   public:
    SSLContextImpl(SSL* conn, Mode mode);
    virtual HandshakeResult StartHandshake(const uint8_t** const out_data,
                                           size_t* out_data_size);
    virtual HandshakeResult DoHandshakeStep(const uint8_t* const in_data,
                                            size_t in_data_size,
                                            const uint8_t** const out_data,
                                            size_t* out_data_size) OVERRIDE;
    bool Encrypt(const uint8_t* const in_data,
                 size_t in_data_size,
                 const uint8_t** const out_data,
                 size_t* out_data_size) OVERRIDE;
    bool Decrypt(const uint8_t* const in_data,
                 size_t in_data_size,
                 const uint8_t** const out_data,
                 size_t* out_data_size) OVERRIDE;
    bool IsInitCompleted() const OVERRIDE;
    bool IsHandshakePending() const OVERRIDE;
    virtual bool GetCertifcateDueDate(struct tm& due_date) const OVERRIDE;
    size_t get_max_block_size(size_t mtu) const OVERRIDE;
    std::string LastError() const OVERRIDE;
    void ResetConnection() OVERRIDE;
    void SetHandshakeContext(const HandshakeContext& hsh_ctx) OVERRIDE;
    ~SSLContextImpl();

    void PrintCertData(X509* cert, const std::string& cert_owner);

   private:
    X509* GetCertificate() const;
    void PrintCertInfo();
    HandshakeResult CheckCertContext();
    bool ReadHandshakeData(const uint8_t** const out_data,
                           size_t* out_data_size);
    bool WriteHandshakeData(const uint8_t* const in_data, size_t in_data_size);
    HandshakeResult PerformHandshake();
    HandshakeResult ProcessSuccessHandshake();
    HandshakeResult ProcessHandshakeError(const int handshake_error);
    bool CheckInitFinished();
    typedef size_t (*BlockSizeGetter)(size_t);
    void EnsureBufferSizeEnough(size_t size);
    void SetHandshakeError(const int error);
    HandshakeResult openssl_error_convert_to_internal(const long error);

    std::string GetTextBy(X509_NAME* name, int object) const;

    int pull_number_from_buf(char* buf, int* idx) const;
    void asn1_time_to_tm(ASN1_TIME* time, struct tm& cert_time) const;

    SSL* connection_;
    BIO* bioIn_;
    BIO* bioOut_;
    BIO* bioFilter_;
    mutable sync_primitives::Lock ssl_locker_;
    size_t buffer_size_;
    uint8_t* buffer_;
    bool is_handshake_pending_;
    Mode mode_;
    mutable std::string last_error_;
    BlockSizeGetter max_block_size_;
    static std::map<std::string, BlockSizeGetter> max_block_sizes;
    static std::map<std::string, BlockSizeGetter> create_max_block_sizes();
    HandshakeContext hsh_context_;
    DISALLOW_COPY_AND_ASSIGN(SSLContextImpl);
  };

 public:
  CryptoManagerImpl();
  ~CryptoManagerImpl();

  bool Init(Mode mode,
            Protocol protocol,
            const std::string& cert_data,
            const std::string& ciphers_list,
            const bool verify_peer,
            const std::string& ca_certificate_file,
            const size_t hours_before_update) OVERRIDE;
  bool OnCertificateUpdated(const std::string& data) OVERRIDE;
  SSLContext* CreateSSLContext() OVERRIDE;
  void ReleaseSSLContext(SSLContext* context) OVERRIDE;
  std::string LastError() const OVERRIDE;
  virtual bool IsCertificateUpdateRequired(struct tm cert_time) const OVERRIDE;

 private:
  bool set_certificate(const std::string& cert_data);

  SSL_CTX* context_;
  Mode mode_;
  static uint32_t instance_count_;
  static sync_primitives::Lock instance_lock_;
  std::string certificate_data_;
  bool verify_peer_;
  size_t hours_before_update_;
  DISALLOW_COPY_AND_ASSIGN(CryptoManagerImpl);
};
}  // namespace security_manager
#endif  // SRC_COMPONENTS_SECURITY_MANAGER_INCLUDE_SECURITY_MANAGER_CRYPTO_MANAGER_IMPL_H_
