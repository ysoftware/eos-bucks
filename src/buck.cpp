// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

#include "buck.hpp"
#include "constants.hpp"
#include "debug.hpp"
#include "actions.hpp"
#include "methods.hpp"
#include "oracle.hpp"
#include "run.hpp"
#include "rex.hpp"
#include "transfer.hpp"
#include "request.hpp"
#include "tax.hpp"

buck::buck(eosio::name receiver, eosio::name code, datastream<const char*> ds)
    :contract(receiver, code, ds),
      _cdp(_self, _self.value),
      _stat(_self, _self.value),
      _reparamreq(_self, _self.value),
      _closereq(_self, _self.value),
      _redeemreq(_self, _self.value),
      _maturityreq(_self, _self.value),
      _rexprocess(_self, _self.value),
      _redprocess(_self, _self.value)
   {}

void buck::init() {
  if (_stat.begin() == _stat.end()) {
  
    _stat.emplace(_self, [&](auto& r) {
      r.supply = asset(0, BUCK);
      r.max_supply = asset(1'000'000'000'000'0000, BUCK);
      r.issuer = _self;
      
      r.oracle_timestamp = time_point(microseconds(0));
      r.oracle_eos_price = 0;
      r.liquidation_timestamp = time_point(microseconds(0));
      
      r.aggregated_collateral = asset(0, EOS);
      r.total_collateral = asset(0, EOS);
      r.gathered_fees = asset(0, BUCK);
    });
  }
}