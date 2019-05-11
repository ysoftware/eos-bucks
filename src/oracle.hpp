// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::update(uint32_t eos_price) {
  require_auth(permission_level(_self, "oracle"_n));
  
  init();
  process_taxes();
  
  const auto& stats = *_stat.begin();
  const uint32_t previous_price = stats.oracle_eos_price;
  
  // to-do shave off price change if don't have a special permission
  
  _stat.modify(stats, same_payer, [&](auto& r) {
    r.oracle_timestamp = get_current_time_point();
    r.oracle_eos_price = eos_price;
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