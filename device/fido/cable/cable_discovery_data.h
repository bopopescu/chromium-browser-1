// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_CABLE_DISCOVERY_DATA_H_
#define DEVICE_FIDO_CABLE_CABLE_DISCOVERY_DATA_H_

#include <stdint.h>
#include <array>

#include "base/component_export.h"
#include "base/containers/span.h"

namespace device {

constexpr size_t kCableEphemeralIdSize = 16;
constexpr size_t kCableSessionPreKeySize = 32;
constexpr size_t kCableQRSecretSize = 16;
constexpr size_t kCableNonceSize = 8;
constexpr size_t kCableIdentityKeySeedSize = 32;
constexpr size_t kCableCompressedPublicKeySize =
    /* type byte */ 1 + /* field element */ (256 / 8);
constexpr size_t kCableQRDataSize =
    kCableCompressedPublicKeySize + kCableQRSecretSize;

using CableEidArray = std::array<uint8_t, kCableEphemeralIdSize>;
using CableSessionPreKeyArray = std::array<uint8_t, kCableSessionPreKeySize>;
// QRGeneratorKey is a random, AES-256 key that is used by
// |CableDiscoveryData::DeriveQRKeyMaterial| to encrypt a coarse timestamp and
// generate QR secrets, EIDs, etc.
using QRGeneratorKey = std::array<uint8_t, 32>;
// CableNonce is a nonce used in BLE handshaking.
using CableNonce = std::array<uint8_t, 8>;
// CableEidGeneratorKey is an AES-256 key that is used to encrypt a 64-bit nonce
// and 64-bits of zeros, resulting in a BLE-advertised EID.
using CableEidGeneratorKey = std::array<uint8_t, 32>;
// CablePskGeneratorKey is HKDF input keying material that is used to
// generate a Noise PSK given the nonce decrypted from an EID.
using CablePskGeneratorKey = std::array<uint8_t, 32>;
// CableAuthenticatorIdentityKey is a P-256 public value used to authenticate a
// paired phone.
using CableAuthenticatorIdentityKey = std::array<uint8_t, 65>;
using CableIdentityKeySeed = std::array<uint8_t, kCableIdentityKeySeedSize>;
using CableQRData = std::array<uint8_t, kCableQRDataSize>;

// Encapsulates information required to discover Cable device per single
// credential. When multiple credentials are enrolled to a single account
// (i.e. more than one phone has been enrolled to an user account as a
// security key), then FidoCableDiscovery must advertise for all of the client
// EID received from the relying party.
// TODO(hongjunchoi): Add discovery data required for MakeCredential request.
// See: https://crbug.com/837088
struct COMPONENT_EXPORT(DEVICE_FIDO) CableDiscoveryData {
  enum class Version {
    INVALID,
    V1,
    V2,
  };

  CableDiscoveryData(Version version,
                     const CableEidArray& client_eid,
                     const CableEidArray& authenticator_eid,
                     const CableSessionPreKeyArray& session_pre_key);
  // Creates discovery data given a specific QR secret and identity key seed.
  // This will be used on the QR-displaying-side of a QR handshake. See
  // |DeriveQRSecret| and |DeriveIdentityKeySeed| for how to generate such
  // secrets.
  CableDiscoveryData(
      base::span<const uint8_t, kCableQRSecretSize> qr_secret,
      base::span<const uint8_t, kCableIdentityKeySeedSize> identity_key_seed);
  CableDiscoveryData();
  CableDiscoveryData(const CableDiscoveryData& data);
  ~CableDiscoveryData();

  // Creates discovery data given QR data, which contains a compressed public
  // key and the QR secret. This will be used by the QR-scanning-side of a QR
  // handshake. Returns |nullopt| if the embedded elliptic-curve point is
  // invalid.
  static base::Optional<CableDiscoveryData> FromQRData(
      base::span<const uint8_t,
                 kCableCompressedPublicKeySize + kCableQRSecretSize> qr_data);

  CableDiscoveryData& operator=(const CableDiscoveryData& other);
  bool operator==(const CableDiscoveryData& other) const;

  // Match attempts to recognise the given EID. If it matches this discovery
  // data, the nonce is returned.
  base::Optional<CableNonce> Match(const CableEidArray& candidate_eid) const;

  // NewQRKey returns a random key for QR generation.
  static QRGeneratorKey NewQRKey();

  // CurrentTimeTick returns the current time as used by QR generation. The size
  // of these ticks is a purely local matter for Chromium.
  static int64_t CurrentTimeTick();

  // DeriveQRKeyMaterial returns a QR-secret given a generating key and a
  // timestamp.
  static std::array<uint8_t, kCableQRSecretSize> DeriveQRSecret(
      base::span<const uint8_t, 32> qr_generator_key,
      const int64_t tick);

  // DeriveIdentityKeySeed returns a seed that can be used to create a P-256
  // identity key for a handshake using |EC_KEY_derive_from_secret|.
  static CableIdentityKeySeed DeriveIdentityKeySeed(
      base::span<const uint8_t, 32> qr_generator_key,
      const int64_t tick);

  // DeriveQRData returns the QR data, a combination of QR secret and public
  // identity key. This is base64url-encoded and placed in a caBLE v2 QR code
  // with a prefix prepended.
  static CableQRData DeriveQRData(
      base::span<const uint8_t, 32> qr_generator_key,
      const int64_t tick);

  // version indicates whether v1 or v2 data is contained in this object.
  // |INVALID| is not a valid version but is set as the default to catch any
  // cases where the version hasn't been set explicitly.
  Version version = Version::INVALID;

  struct V1Data {
    CableEidArray client_eid;
    CableEidArray authenticator_eid;
    CableSessionPreKeyArray session_pre_key;
  };
  base::Optional<V1Data> v1;

  struct COMPONENT_EXPORT(DEVICE_FIDO) V2Data {
    V2Data();
    V2Data(const V2Data&);
    ~V2Data();

    CableEidGeneratorKey eid_gen_key;
    CablePskGeneratorKey psk_gen_key;
    base::Optional<CableAuthenticatorIdentityKey> peer_identity;
    base::Optional<CableIdentityKeySeed> local_identity_seed;
    // peer_name is an authenticator-controlled, UTF8-valid string containing
    // the self-reported, human-friendly name of a v2 authenticator. This need
    // not be filled in when handshaking but an authenticator may provide it
    // when offering long-term pairing data.
    base::Optional<std::string> peer_name;
  };
  base::Optional<V2Data> v2;

 private:
  void InitFromQRSecret(
      base::span<const uint8_t, kCableQRSecretSize> qr_secret);
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_CABLE_DISCOVERY_DATA_H_
