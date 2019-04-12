// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::update(double eos_price) {
  // require_auth(_self); // to-do disabled while debug
  init();
  
  auto previous_price = _stat.begin()->oracle_eos_price;
  
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

double buck::get_eos_price() {
  double price = _stat.begin()->oracle_eos_price;
  check(price != 0, "oracle prices are not yet set");
  return _stat.begin()->oracle_eos_price;
}
