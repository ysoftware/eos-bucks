// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

double buck::get_ccr(const asset& collateral, const asset& debt) const {
  const double price = get_eos_price();
  return (double) collateral.amount * price / (double) debt.amount;
}

void buck::sub_funds(const name& from, const asset& quantity) {
  auto fund_itr = _fund.require_find(from.value, "no fund balance found");
  check(fund_itr->balance >= quantity, "overdrawn fund balance");
  _fund.modify(fund_itr, from, [&](auto& r) {
    r.balance -= quantity;
  });
}

void buck::add_funds(const name& from, const asset& quantity) {
  auto fund_itr = _fund.find(from.value);
  if (fund_itr != _fund.end()) {
    _fund.modify(fund_itr, same_payer, [&](auto& r) {
      r.balance += quantity;
    });
  }
  else {
    _fund.emplace(_self, [&](auto& r) {
      r.balance = quantity;
      r.account = from;
    });
  }
}

void buck::add_balance(const name& owner, const asset& value, const name& ram_payer, bool change_supply) {
  eosio::print("+ ");
  eosio::print(value);
  eosio::print("@ ");
  eosio::print(owner);
  
  accounts_i accounts(_self, owner.value);
  auto account_itr = accounts.find(value.symbol.code().raw());
  
  if (account_itr == accounts.end()) {
    accounts.emplace(ram_payer, [&](auto& r) {
      r.balance = value;
    });
  }
  else {
    accounts.modify(account_itr, same_payer, [&](auto& r) {
      r.balance += value;
    });
  }
  
  if (change_supply) {
    _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
      r.supply += value;
    });
  }
}

void buck::sub_balance(const name& owner, const asset& value, bool change_supply) {
  eosio::print("- ");
  eosio::print(value);
  eosio::print("@ ");
  eosio::print(owner);
  
  accounts_i accounts(_self, owner.value);
  const auto account_itr = accounts.require_find(value.symbol.code().raw(), "no balance object found");
  check(account_itr->balance.amount >= value.amount, "overdrawn buck balance");

  // to-do ram payer should always be replaced by owner when possible
  accounts.modify(account_itr, same_payer, [&](auto& r) {
    r.balance -= value;
  });
  
  if (change_supply) {
    _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
      r.supply -= value;
    });
  }
}