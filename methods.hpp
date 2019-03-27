// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

uint64_t time_ms() {
	return current_time() / 1'000;
}

void buck::add_debt(name owner, asset value, name ram_payer) {
  accounts_i accounts(_self, owner.value);
  
  auto item = accounts.find(BUCK.code().raw());
  if (item == accounts.end()) {
    accounts.emplace(owner, [&](auto& r) {
      r.balance = value;
    });
  }
  else {
    accounts.modify(item, same_payer, [&](auto& r) {
      r.balance += value;
    });
  }
}

void buck::sub_debt(name owner, asset value) {
  accounts_i accounts(_self, owner.value);

  const auto& item = accounts.get(_self.value, "no balance object found");
  eosio_assert(item.balance.amount >= value.amount, "overdrawn balance");

  accounts.modify(item, same_payer, [&](auto& r) {
    r.balance -= value;
  });
}