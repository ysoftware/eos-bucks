// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::exchange(const name& from, const asset value) {
  require_auth(from);

  check(value.is_valid(), "invalid quantity");
  check(value.amount > 0, "must transfer positive quantity");
  check(value.symbol == BUCK || value.symbol == EOS, "symbol mismatch");
  
  // take tokens
  if (value.symbol == BUCK) {
    sub_balance(from, value, false);
  }
  else {
    sub_exchange_funds(from, value);
  }
  
  const auto ex_itr = _exchange.find(from.value);
  if (ex_itr == _exchange.end()) {
    
    // new order
    _exchange.emplace(from, [&](auto& r) {
      r.account = from;
      r.quantity = value;
      r.timestamp = get_current_time_point();
    });
  }
  else {
    
    if (ex_itr->quantity.symbol != value.symbol) {
      
      // return previous order
      if (ex_itr->quantity.symbol == BUCK) {
        add_balance(from, ex_itr->quantity, from, false);
      }
      else {
        add_exchange_funds(from, ex_itr->quantity, from);
      }
      
      // setup new one
      _exchange.modify(ex_itr, from, [&](auto& r) {
        r.quantity = value;
        r.timestamp = get_current_time_point();
      });
    }
    else {
      
      // update existing
      _exchange.modify(ex_itr, from, [&](auto& r) {
        r.quantity += value;
        r.timestamp = get_current_time_point();
      });
    }
  }
  run(10);
}