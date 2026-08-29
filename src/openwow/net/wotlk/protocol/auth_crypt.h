#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace openwow::net::wotlk {

class AuthCrypt {
 public:

  void Init(const std::uint8_t session_key[40]);

  void Init(const std::uint8_t session_key[40],
            std::span<const std::uint8_t, 32> redirect_challenge);

  void EncryptSend(std::uint8_t* data, std::size_t len);

  void DecryptRecv(std::uint8_t* data, std::size_t len);

 [[nodiscard]] bool IsInitialized() const { return initialized_; }

 private:
  std::uint8_t key_[40]{};
  std::size_t send_i_{0};
  std::uint8_t send_j_{0};
  std::size_t recv_i_{0};
  std::uint8_t recv_j_{0};
  bool initialized_{false};
};

}
