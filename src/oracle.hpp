// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::update(uint32_t eos_price) {
  #if !DEBUG
  require_auth(_self);
  #endif
  
  init();
  process_taxes();
  
  const auto& stats = *_stat.begin();
  const uint32_t previous_price = stats.oracle_eos_price;
  
  _stat.modify(stats, same_payer, [&](auto& r) {
    r.oracle_timestamp = current_time_point();
    r.oracle_eos_price = eos_price;
  });
  
  set_processing_status(ProcessingStatus::processing_cdp_requests);
  
  if (eos_price < previous_price) {
    set_liquidation_status(LiquidationStatus::processing_liquidation);
    run_liquidation(25);
  }
  else {
    run_requests(25);
  }
}

inline uint32_t buck::get_eos_price() const {
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  return _stat.begin()->oracle_eos_price;
}