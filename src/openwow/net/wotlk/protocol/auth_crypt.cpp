#include "openwow/net/wotlk/protocol/auth_crypt.h"

#include <algorithm>
#include <cstring>

namespace openwow::net::wotlk {

void AuthCrypt::Init(const std::uint8_t session_key[40]) {
  std::memcpy(key_, session_key, sizeof(key_));
  send_i_ = recv_i_ = 0;
  send_j_ = recv_j_ = 0;
  initialized_ = true;
}

void AuthCrypt::Init(
    const std::uint8_t session_key[40],
    const std::span<const std::uint8_t, 32> redirect_challenge) {
  (void)redirect_challenge;
  Init(session_key);
}

void AuthCrypt::EncryptSend(std::uint8_t* data, std::size_t len) {
  if (!initialized_ || data == nullptr) return;
  // Client headers are six bytes in Classic: uint16 size + uint32 opcode.
  const std::size_t count = std::min<std::size_t>(len, 6);
  for (std::size_t index = 0; index < count; ++index) {
    const std::uint8_t value =
        static_cast<std::uint8_t>((data[index] ^ key_[send_i_ % sizeof(key_)]) +
                                   send_j_);
    ++send_i_;
    send_j_ = value;
    data[index] = value;
  }
}

void AuthCrypt::DecryptRecv(std::uint8_t* data, std::size_t len) {
  if (!initialized_ || data == nullptr) return;
  const std::size_t count = std::min<std::size_t>(len, 4);
  for (std::size_t index = 0; index < count; ++index) {
    const std::uint8_t encrypted = data[index];
    const std::uint8_t value = static_cast<std::uint8_t>(
        (encrypted - recv_j_) ^ key_[recv_i_ % sizeof(key_)]);
    ++recv_i_;
    recv_j_ = encrypted;
    data[index] = value;
  }
}

}
