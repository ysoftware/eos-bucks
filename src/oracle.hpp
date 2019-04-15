// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::update(double eos_price) {
  #if !DEBUG
  require_auth(_self);
  #endif
  
  if (!init()) {
    process_taxes();
  }
  
  const auto& stats = *_stat.begin();
  const auto previous_price = stats.oracle_eos_price;
  _stat.modify(stats, same_payer, [&](auto& r) {
    r.oracle_timestamp = current_time_point();
    r.oracle_eos_price = eos_price;
  });
  
  if (eos_price < previous_price) {
    run_liquidation(50);
  }
  else {
    _stat.modify(stats, same_payer, [&](auto& r) {
      r.liquidation_timestamp = stats.oracle_timestamp;
    });
    run_requests(5);
  }
}

inline double buck::get_eos_price() const {
  return _stat.begin()->oracle_eos_price;
}