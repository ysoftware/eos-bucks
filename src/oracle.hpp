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
    run_liquidation(25);
  }
}

double buck::get_eos_price() {
  stats_i table(_self, _self.value);
  eosio_assert(table.begin() != table.end(), "contract is not yet initiated");
  double price = table.begin()->oracle_eos_price;
  eosio_assert(price != 0, "oracle prices are not yet set");
  return table.begin()->oracle_eos_price;
}

double buck::get_buck_price() {
  stats_i table(_self, _self.value);
  eosio_assert(table.begin() != table.end(), "contract is not yet initiated");
  double price = table.begin()->oracle_buck_price;
  eosio_assert(price != 0, "oracle prices are not yet set");
  return table.begin()->oracle_buck_price;
}