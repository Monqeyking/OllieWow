#pragma once

#include "openwow/data/formats/dbc/dbc_entries_extended.h"
#include "openwow/data/formats/dbc/dbc_store.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace openwow::ui::glue {

class RandomNameDictionary {
public:
  void Rebuild(const openwow::data::dbc::DbcStore<openwow::data::dbc::NameGenEntry> &name_gen,
               const std::uint32_t race_id, const std::uint32_t sex) {
    names_.clear();
    if (names_.capacity() < kNameStorageReserve) {
      names_.reserve(kNameStorageReserve);
    }

    for (const auto &entry : name_gen.entries()) {
      if (entry.race_id != race_id || entry.sex != sex) {
        continue;
      }

      if (entry.name.empty()) {
        continue;
      }

      names_.push_back(entry.name);
    }
  }

  [[nodiscard]] bool empty() const noexcept {
    return names_.empty();
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return names_.size();
  }

  [[nodiscard]] std::string Generate(
      const std::function<std::uint32_t(std::uint32_t)> &select_weight_ordinal,
      const std::size_t = 14u) const {
    if (names_.empty()) {
      return {};
    }

    std::uint32_t ordinal = select_weight_ordinal(static_cast<std::uint32_t>(names_.size()));
    if (ordinal >= names_.size()) {
      ordinal = static_cast<std::uint32_t>(names_.size() - 1u);
    }
    return std::string(names_[ordinal]);
  }

private:
  static constexpr std::size_t kNameStorageReserve = 256u;

  std::vector<std::string_view> names_;
};

}
