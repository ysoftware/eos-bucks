// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

#if DEBUG

#define PRINT(x, y) eosio::print(x); eosio::print(": "); eosio::print(y); eosio::print("\n");
#define PRINT_(x) eosio::print(x); eosio::print("\n");
#define RMS(x, scope) { x table(_self, scope); auto item = table.begin(); while (item != table.end()) item = table.erase(item); }
#define RM(x) { x table(_self, _self.value); auto item = table.begin(); while (item != table.end()) item = table.erase(item); }

void buck::zdestroy() {
  RM(stats_i)
  RM(cdp_maturity_req_i)
  RM(rex_processing_i)
  RM(red_processing_i)
  RM(redeem_req_i)
  RM(close_req_i)
  RM(reparam_req_i)
  
  cdp_i table(_self, _self.value);
  auto item = table.begin();
  while (item != table.end()) {
    RMS(accounts_i, item->account.value)
    item = table.erase(item);
  }
}

#else
#define PRINT(x, y) 
#define PRINT_(x)
#endif