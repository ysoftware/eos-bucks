// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

#if DEBUG
#define PRINT(x, y) eosio::print(x); eosio::print(": "); eosio::print(y); eosio::print("\n");
#define PRINT_(x) eosio::print(x); eosio::print("\n");
#define RMS(x, scope) { x table(_self, scope); auto item = table.begin(); while (item != table.end()) item = table.erase(item); }
#define RM(x) { x table(_self, _self.value); auto item = table.begin(); while (item != table.end()) item = table.erase(item); }

void buck::zdestroy() {
  #if !DEBUG
  require_auth(_self);
  #else
  
  RM(stats_i)
  RM(cdp_maturity_req_i)
  RM(processing_i)
  RM(red_processing_i)
  RM(redeem_req_i)
  RM(close_req_i)
  RM(reparam_req_i)
  RM(cdp_i)
  RM(fund_i)
  RM(taxation_i)
  
  RMS(accounts_i, "yaroslaveroh"_n.value)
  RMS(accounts_i, "buckbuckbuck"_n.value)
  RMS(accounts_i, "testaccountp"_n.value)
  RMS(accounts_i, "scrugescruge"_n.value)
}
#else

// disable all other test flags if DEBUG is disabled
#undef REX_TESTING
#define REX_TESTING false
#undef TEST_TIME
#define TEST_TIME false

// print does nothing
#define PRINT(x, y) 
#define PRINT_(x)
#endif

#if TEST_TIME
void buck::zmaketime(uint64_t seconds) {
  #if !DEBUG
  require_auth(_self);
  #else
  
  time_test_i _time(_self, _self.value);
  if (_time.begin() != _time.end()) {
    _time.modify(_time.begin(), same_payer, [&](auto& r) {
      r.now = time_point_sec(seconds);
    });
  }
  else {
    _time.emplace(_self, [&](auto& r) {
      r.now = time_point_sec(seconds);
    });
  }
}
#endif