// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::update(double eos_price, double buck_price) {
  
  // update prices
  stats_i table(_self, _self.value);
  eosio_assert(table.begin() != table.end(), "contract is not yet initiated");
  
  auto previous_price = table.begin()->oracle_eos_price;
  
  table.modify(table.begin(), same_payer, [&](auto& r) {
    r.oracle_timestamp = time_ms();
    r.oracle_eos_price = eos_price;
    r.oracle_buck_price = buck_price;
  });
  
  if (eos_price < previous_price) {
    run_liquidation();
  }
  else {
    // to-do mark liquidation done for this round
  }
}

double buck::get_eos_price() {
  
  return 3.6545;
}

double buck::get_buck_price() {
  
  return 1.05;
}