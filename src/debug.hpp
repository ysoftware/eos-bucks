// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

#define PRINT(x, y) eosio::print(x); eosio::print(": "); eosio::print(y); eosio::print("\n");
#define PRINT_(x) eosio::print(x); eosio::print("\n");

#define RM(x, scope) { x table(_self, scope); auto item = table.begin(); while (item != table.end()) { item = table.erase(item); } }

void buck::zdestroy() {
  
  RM(stats_i, _self.value)
  
  RM(cdp_maturity_req_i, _self.value)
  
  RM(redeem_req_i, _self.value)
  
  RM(close_req_i, _self.value)
  
  RM(reparam_req_i, _self.value)
  
	{
    cdp_i table(_self, _self.value);
    auto item = table.begin();
    while (item != table.end()) {
      
      RM(accounts_i, item->account.value)
      
      item = table.erase(item);
    }
	}
}