// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

double buck::get_ccr(asset collateral, asset debt) {
  double price = get_eos_price();
  return (double) collateral.amount * price / (double) debt.amount;
}

void buck::add_balance(name owner, asset value, name ram_payer, bool change_supply) {
  PRINT("+ balance", value)
  
  accounts_i accounts(_self, owner.value);
  auto item = accounts.find(value.symbol.code().raw());
  
  if (item == accounts.end()) {
    accounts.emplace(ram_payer, [&](auto& r) {
      r.balance = value;
    });
  }
  else {
    accounts.modify(item, same_payer, [&](auto& r) {
      r.balance += value;
    });
  }
  
  if (change_supply) {
    check(_stat.begin() != _stat.end(), "contract is not yet initiated");
    
    _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
      r.supply += value;
    });
  }
}

void buck::sub_balance(name owner, asset value, bool change_supply) {
  PRINT("- balance", value)
  
  accounts_i accounts(_self, owner.value);

  const auto& item = accounts.get(value.symbol.code().raw(), "no balance object found");
  check(item.balance.amount >= value.amount, "overdrawn buck balance");

  accounts.modify(item, owner, [&](auto& r) {
    r.balance -= value;
  });
  
  if (change_supply) {
    check(_stat.begin() != _stat.end(), "contract is not yet initiated");
    
    _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
      r.supply -= value;
    });
  }
}