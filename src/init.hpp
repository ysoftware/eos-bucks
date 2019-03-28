// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::init() {
  stats_i table(_self, _self.value);
  eosio_assert(table.begin() == table.end(), "contract is already initiated");
  
  table.emplace(_self, [&](auto& r) {
    r.supply = asset(0, BUCK);
    r.max_supply = asset(10000000000000000, BUCK);
    r.issuer = _self;
    
    r.oracle_timestamp = 0;
    r.oracle_eos_price = 0;
    r.oracle_buck_price = 0;
  });
}