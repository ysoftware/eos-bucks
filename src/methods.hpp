// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

double buck::get_ccr(const asset& collateral, const asset& debt) const {
  double price = get_eos_price();
  return (double) collateral.amount * price / (double) debt.amount;
}

void buck::add_balance(const name& owner, const asset& value, const name& ram_payer, bool change_supply) {
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
    _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
      r.supply += value;
    });
  }
}

void buck::sub_balance(const name& owner, const asset& value, bool change_supply) {
  PRINT("- balance", value)
  
  accounts_i accounts(_self, owner.value);

  const auto& item = accounts.get(value.symbol.code().raw(), "no balance object found");
  check(item.balance.amount >= value.amount, "overdrawn buck balance");

  // to-do ram payer should always be replaced by owner when possible
  accounts.modify(item, same_payer, [&](auto& r) {
    r.balance -= value;
  });
  
  if (change_supply) {
    _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
      r.supply -= value;
    });
  }
}