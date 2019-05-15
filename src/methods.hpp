// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::add_exchange_funds(const name& from, const asset& quantity, const name& ram_payer) {
  #if DEBUG
  if (quantity.amount != 0) { eosio::print("+"); eosio::print(quantity); eosio::print(" @ "); eosio::print(from); eosio::print("\n"); }
  #endif
  
  auto fund_itr = _fund.find(from.value);
  if (fund_itr != _fund.end()) {
    process_maturities(fund_itr);
    _fund.modify(fund_itr, ram_payer, [&](auto& r) {
      r.exchange_balance += quantity;
    });
  }
  else {
    _fund.emplace(ram_payer, [&](auto& r) {
      r.balance = ZERO_REX;
      r.account = from;
      r.exchange_balance = quantity;
      r.matured_rex = 0;
    });
  }
}

void buck::sub_exchange_funds(const name& from, const asset& quantity) {
  auto fund_itr = _fund.require_find(from.value, "no fund balance found");
  process_maturities(fund_itr);
  
  if (quantity.amount == 0) return;
  
  #if DEBUG
  eosio::print("-"); eosio::print(quantity); eosio::print(" @ "); eosio::print(from); eosio::print("\n");
  #endif
  
  check(fund_itr->exchange_balance >= quantity, "overdrawn exchange fund balance");
  
  _fund.modify(fund_itr, from, [&](auto& r) {
    r.exchange_balance -= quantity;
  });
}

// to-do add maturity
void buck::add_funds(const name& from, const asset& quantity, const name& ram_payer) {
  #if DEBUG
  if (quantity.amount != 0) { eosio::print("+"); eosio::print(quantity); eosio::print(" @ "); eosio::print(from); eosio::print("\n"); }
  #endif
  
  auto fund_itr = _fund.find(from.value);
  if (fund_itr != _fund.end()) {
    process_maturities(fund_itr);
    
    _fund.modify(fund_itr, ram_payer, [&](auto& r) {
      r.balance += quantity;
    });
  }
  else {
    _fund.emplace(ram_payer, [&](auto& r) {
      r.balance = quantity;
      r.account = from;
      r.exchange_balance = ZERO_EOS;
      r.matured_rex = 0;
    });
  }
}

void buck::sub_funds(const name& from, const asset& quantity) {
  auto fund_itr = _fund.require_find(from.value, "no fund balance found");
  process_maturities(fund_itr);
  
  if (quantity.amount == 0) return;
  
  #if DEBUG
  eosio::print("-"); eosio::print(quantity); eosio::print(" @ "); eosio::print(from); eosio::print("\n");
  #endif
  
  check(fund_itr->balance >= quantity, "overdrawn fund balance");
  
  _fund.modify(fund_itr, from, [&](auto& r) {
    r.balance -= quantity;
    r.matured_rex -= quantity.amount;
  });
}

void buck::add_balance(const name& owner, const asset& value, const name& ram_payer, bool change_supply) {
  #if DEBUG
  if (value.amount != 0) { eosio::print("+"); eosio::print(value); eosio::print(" @ "); eosio::print(owner); eosio::print("\n"); }
  #endif
  
  accounts_i accounts(_self, owner.value);
  auto account_itr = accounts.find(value.symbol.code().raw());
  
  if (account_itr == accounts.end()) {
    accounts.emplace(ram_payer, [&](auto& r) {
      r.balance = value;
    });
  }
  else {
    const auto payer = has_auth(owner) ? owner : ram_payer;
    accounts.modify(account_itr, payer, [&](auto& r) {
      r.balance += value;
    });
  }
  
  if (change_supply) {
    update_supply(value);
  }
}

void buck::sub_balance(const name& owner, const asset& value, bool change_supply) {
  if (value.amount == 0) return;
  
  #if DEBUG
  eosio::print("-"); eosio::print(value); eosio::print(" @ "); eosio::print(owner); eosio::print("\n");
  #endif
  
  accounts_i accounts(_self, owner.value);
  const auto account_itr = accounts.require_find(value.symbol.code().raw(), "no balance object found");
  check(account_itr->balance.amount >= value.amount, "overdrawn buck balance");

  const auto payer = has_auth(owner) ? owner : same_payer;
  
  accounts.modify(account_itr, payer, [&](auto& r) {
    r.balance -= value;
  });
  
  if (change_supply) {
    update_supply(-value);
  }
}

void buck::update_supply(const asset& quantity) {
  _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
    r.supply += quantity;
  });
}

void buck::set_liquidation_status(LiquidationStatus status) {
  _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
    r.processing_status = (r.processing_status & 0b1100) | (uint8_t) status; // bits 1100
  });
}

void buck::set_processing_status(ProcessingStatus status) {
  _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
    r.processing_status = (r.processing_status & 0b0011) | ((uint8_t) status << 2); // bits 0011
  });
}

buck::ProcessingStatus buck::get_processing_status() const {
  return static_cast<ProcessingStatus>(_stat.begin()->processing_status >> 2);
}

buck::LiquidationStatus buck::get_liquidation_status() const {
  return static_cast<LiquidationStatus>(_stat.begin()->processing_status & (uint8_t) 0b11);
}

time_point_sec buck::current_time_point_sec() const {
  const static time_point_sec cts{ get_current_time_point() };
  return cts;
}

inline time_point buck::get_current_time_point() const {
  #if TEST_TIME
  time_test_i _time(_self, _self.value);
  if (_time.begin() != _time.end()) return _time.begin()->now;
  #endif
  return current_time_point();
}