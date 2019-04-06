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

void buck::init() {
  require_auth(_self);
  
  stats_i stats(_self, _self.value);
  check(stats.begin() == stats.end(), "contract is already initiated");
  
  stats.emplace(_self, [&](auto& r) {
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