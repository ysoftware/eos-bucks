// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::update(double eos_price) {
  require_auth(_self);
  
  // update prices
  stats_i table(_self, _self.value);
  check(table.begin() != table.end(), "contract is not yet initiated");
  
  auto previous_price = table.begin()->oracle_eos_price;
  
  table.modify(table.begin(), same_payer, [&](auto& r) {
    r.oracle_timestamp = current_time_point();
    r.oracle_eos_price = eos_price;
  });
  
  if (eos_price < previous_price) {
    run_liquidation(50);
  }
  else {
    table.modify(table.begin(), same_payer, [&](auto& r) {
      r.liquidation_timestamp = table.begin()->oracle_timestamp;
    });
    run_requests(5);
  }
}

double buck::get_eos_price() {
  stats_i table(_self, _self.value);
  check(table.begin() != table.end(), "contract is not yet initiated");
  double price = table.begin()->oracle_eos_price;
  check(price != 0, "oracle prices are not yet set");
  return table.begin()->oracle_eos_price;
}
