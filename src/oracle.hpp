// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::update(double eos_price) {
  #if !DEBUG
  require_auth(_self);
  #endif
  
  init();
  
  const auto previous_price = _stat.begin()->oracle_eos_price;
  
  _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
    r.oracle_timestamp = current_time_point();
    r.oracle_eos_price = eos_price;
  });
  
  if (eos_price < previous_price) {
    run_liquidation(50);
  }
  else {
    _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
      r.liquidation_timestamp = _stat.begin()->oracle_timestamp;
    });
    run_requests(5);
  }
}

double buck::get_eos_price() const {
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  return _stat.begin()->oracle_eos_price;
}