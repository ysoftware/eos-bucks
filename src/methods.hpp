// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::sub_funds(const name& from, const asset& quantity) {
  eosio::print("- "); eosio::print(quantity); eosio::print("@ "); eosio::print(from); eosio::print("\n");
  auto fund_itr = _fund.require_find(from.value, "no fund balance found");
  check(fund_itr->balance >= quantity, "overdrawn fund balance");
  _fund.modify(fund_itr, from, [&](auto& r) {
    r.balance -= quantity;
  });
}

void buck::add_funds(const name& from, const asset& quantity, const name& ram_payer) {
  eosio::print("+ "); eosio::print(quantity); eosio::print("@ "); eosio::print(from); eosio::print("\n");
  auto fund_itr = _fund.find(from.value);
  if (fund_itr != _fund.end()) {
    _fund.modify(fund_itr, ram_payer, [&](auto& r) {
      r.balance += quantity;
    });
  }
  else {
    _fund.emplace(ram_payer, [&](auto& r) {
      r.balance = quantity;
      r.account = from;
    });
  }
}

void buck::add_balance(const name& owner, const asset& value, const name& ram_payer, bool change_supply) {
  eosio::print("+ "); eosio::print(value); eosio::print("@ "); eosio::print(owner); eosio::print("\n");
  
  accounts_i accounts(_self, owner.value);
  auto account_itr = accounts.find(value.symbol.code().raw());
  
  if (account_itr == accounts.end()) {
    accounts.emplace(ram_payer, [&](auto& r) {
      r.balance = value;
      r.savings = ZERO_BUCK;
      r.withdrawn_round = _tax.begin()->current_round;
    });
  }
  else {
    accounts.modify(account_itr, same_payer, [&](auto& r) {
      r.balance += value;
    });
  }
  
  if (change_supply) {
    
    update_supply(value);
  }
}

void buck::sub_balance(const name& owner, const asset& value, bool change_supply) {
  eosio::print("- "); eosio::print(value); eosio::print("@ "); eosio::print(owner); eosio::print("\n");
  
  accounts_i accounts(_self, owner.value);
  const auto account_itr = accounts.require_find(value.symbol.code().raw(), "no balance object found");
  check(account_itr->balance.amount >= value.amount, "overdrawn buck balance");

  // to-do ram payer should always be replaced by owner when possible
  accounts.modify(account_itr, same_payer, [&](auto& r) {
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