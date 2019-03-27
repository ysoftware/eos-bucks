// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::zdestroy() {
	{
    cdp_i table(_self, _self.value);
    auto item = table.begin();
    while (item != table.end()) {
      
      accounts_i accounts(_self, item->account.value);
      auto account_item = accounts.find(BUCK.code().raw());
      if (account_item != accounts.end()) {
        accounts.modify(account_item, same_payer, [&](auto& r) {
          r.balance = asset(0, BUCK);
        });
      }
      
      item = table.erase(item);
    }
	}
}