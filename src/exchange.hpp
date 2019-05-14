// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::exchange(const name& from, const asset value) {
  require_auth(from);

  check(value.is_valid(), "invalid quantity");
  check(value.amount > 0, "must transfer positive quantity");
  check(value.symbol == BUCK || value.symbol == EOS, "symbol mismatch");
  
  // create accounts if not yet
  add_balance(from, ZERO_BUCK, from, false);
  add_exchange_funds(from, ZERO_EOS, from);
  
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

void buck::run_exchange(uint8_t max) {
  const uint32_t price = get_eos_usd_price();
  const time_point oracle_timestamp = _stat.begin()->oracle_timestamp;
  
  uint8_t processed = 0;
  auto index = _exchange.get_index<"type"_n>();
  
  while (index.begin() != index.end() && max > processed) {
    processed++;
    
    // get elements
    const auto buy_itr = index.begin();
    const auto sell_itr = --index.end(); // take first from end
    
    // ran out of sellers or buyers
    if (buy_itr->quantity.symbol == BUCK || sell_itr->quantity.symbol == EOS) return;
    
    // no sell or buy orders made since the last oracle update
    if (buy_itr->timestamp > oracle_timestamp || sell_itr->timestamp > oracle_timestamp) return;
    
    // proceed
    const int64_t buck_amount = std::min(sell_itr->quantity.amount, buy_itr->quantity.amount * price);
    const int64_t eos_amount = buck_amount / price;
    
    const asset buck = asset(buck_amount, BUCK);
    const asset eos = asset(eos_amount, EOS);
    
    PRINT("seller", sell_itr->account)
    PRINT("buyer", buy_itr->account)
    
    // update balances
    add_balance(sell_itr->account, buck, same_payer, false);
    add_exchange_funds(buy_itr->account, eos, same_payer);
    
    // update orders
    if (sell_itr->quantity.amount == buck_amount) {
      index.erase(sell_itr);
    }
    else {
      index.modify(sell_itr, same_payer, [&](auto& r) {
        r.quantity -= buck;  
      });
    }
    
    if (buy_itr->quantity.amount == eos_amount) {
      index.erase(buy_itr);
    }
    else {
      index.modify(buy_itr, same_payer, [&](auto& r) {
        r.quantity -= eos;
      });
    }
  }
}