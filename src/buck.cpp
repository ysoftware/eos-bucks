// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

#include <cmath>
#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>

using namespace eosio;

#include "constants.hpp"
#include "buck.hpp"
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
      _process(_self, _self.value),
      _fund(_self, _self.value),
      _tax(_self, _self.value)
   {}

bool buck::init() {
  if (_stat.begin() != _stat.end()) return false;
  
  static const uint32_t now = current_time_point_sec().utc_seconds;
  
  _stat.emplace(_self, [&](auto& r) {
    r.supply = ZERO_BUCK;
    r.max_supply = asset(1'000'000'000'000'0000, BUCK);
    r.issuer = _self;
    
    r.processing_status = 0;
    r.oracle_timestamp = time_point(microseconds(0));
    r.oracle_eos_price = 0;
  });
  
  _tax.emplace(_self, [&](auto& r) {
    
      r.r_supply = 0;
      r.r_price = PO;
      r.r_collected = 0;
      
      r.e_supply = 0;
      r.e_price = PO;
      r.e_collected = 0;
  });
  
  add_balance(SCRUGE, ZERO_BUCK, _self, false);
  add_funds(SCRUGE, ZERO_REX, _self);
  
  return true;
}