// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::change(uint64_t cdp_id, asset debt, asset collateral) {
  cdp_i positions(_self, _self.value);
  auto positionItem = positions.find(cdp_id);
  eosio_assert(positionItem != positions.end(), "cdp does not exist");
  
  require_auth(positionItem->account);
  
  reparam_req_i requests(_self, _self.value);
  auto requestItem = requests.find(cdp_id);
  eosio_assert(requestItem == requests.end(), "request already exists");
  
  // to-do validate arguments
  
  requests.emplace(positionItem->account, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.timestamp = time_ms();
    r.change_collateral = asset(0, EOS);
    r.change_debt = debt;
    r.isPaid = true;
  });
  
  // to-do do other things?
  
  run_requests(2);
}

void buck::changeacr(uint64_t cdp_id, double acr) {
  eosio_assert(acr >= CR || acr == 0, "acr value is too small");
  eosio_assert(acr < 1000, "acr value is too high");
  
  cdp_i positions(_self, _self.value);
  auto positionItem = positions.find(cdp_id);
  eosio_assert(positionItem != positions.end(), "cdp does not exist");
  eosio_assert(positionItem->acr != acr, "acr is already set to this value");
  
  require_auth(positionItem->account);
  
  positions.modify(positionItem, same_payer, [&](auto& r) {
    r.acr = acr;
  });
}

void buck::closecdp(uint64_t cdp_id) {
  cdp_i positions(_self, _self.value);
  auto positionItem = positions.find(cdp_id);
  eosio_assert(positionItem != positions.end(), "cdp does not exist");
  
  close_req_i requests(_self, _self.value);
  auto requestItem = requests.find(cdp_id);
  eosio_assert(requestItem == requests.end(), "request already exists");

  require_auth(positionItem->account);
  
  sub_balance(positionItem->account, positionItem->debt, true);
  
  requests.emplace(positionItem->account, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.timestamp = time_ms();
  });
  
  run_requests(2);
}