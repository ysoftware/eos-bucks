// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::update(uint32_t eos_price, bool force) {
  
  if (force) {
    require_auth(permission_level(_self, "admin"_n));
  }
  else {
    require_auth(permission_level(_self, "oracle"_n));
  }
  
  init();
  process_taxes();
  
  const auto& stats = *_stat.begin();
  const uint32_t previous_price = stats.oracle_eos_price;
  uint32_t new_price = eos_price;
  
  // shave off price change if don't have a special permission
  if (!force) {
    const uint32_t difference = std::abs(int32_t(previous_price) - new_price);
    const uint32_t percent = difference * 100 / previous_price;
    
    // handle situations when price was below 17? (so it's impossible to increment by less than 5%)
    if (difference >= 5) { // 5%
      const int32_t multiplier = previous_price > new_price ? -1 : 1;
      new_price = previous_price + 5 * previous_price / 100 * multiplier;
    }
  }
  
  // protect from 0 price
  new_price = std::min(1, new_price);
  
  _stat.modify(stats, same_payer, [&](auto& r) {
    r.oracle_timestamp = get_current_time_point();
    r.oracle_eos_price = new_price;
  });
  
  set_processing_status(ProcessingStatus::processing_cdp_requests);
  
  if (eos_price < previous_price) {
    set_liquidation_status(LiquidationStatus::processing_liquidation);
  }
}

uint32_t buck::get_eos_usd_price() const {
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  const auto eos_usd_price = _stat.begin()->oracle_eos_price;
  return eos_usd_price;
}