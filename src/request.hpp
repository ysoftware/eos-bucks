// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::changedebt(name account, uint64_t cdp_id, asset debt) {
  require_auth(account);
  
  cdp_i positions(_self, _self.value);
  auto positionItem = positions.find(cdp_i);
  eosio_assert(positionItem != positions.end(), "cdp does not exist");
  
  reparam_req requests(_self, _self.value);
  auto item = requests.find(cdp_id);
  eosio_assert(item == requests.end(), "request already exists");
  
  // to-do validate arguments
  
  requests.emplace(account, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.timestamp = time_ms();
    r.collateral = asset(0, EOS);
    r.debt = debt;
    r.isPaid = true;
  });
  
  // to-do do other things?
  
  run_requests(2);
}

void buck::changecollat(name account, uint64_t cdp_id, asset collateral) {
  
}

void buck::changeacr(name account, double acr) {
  require_auth(account);
  
  eosio_assert(acr >= CR || acr == 0, "acr value is too small");
  eosio_assert(acr < 1000, "acr value is too high");
  
  cdp_i positions(_self, _self.value);
  auto positionItem = positions.find(cdp_i);
  eosio_assert(positionItem != positions.end(), "cdp does not exist");
  eosio_assert(positionItem->acr != acr, "acr is already set to this value");
  
  positions.modify(positionItem, same_payer, [&](auto& r) {
    r.acr = acr;
  });
}

void buck::closecdp(name account, uint64_t cdp_id) {
  require_auth(account);
  
  cdp_i positions(_self, _self.value);
  auto positionItem = positions.find(cdp_i);
  eosio_assert(positionItem != positions.end(), "cdp does not exist");
  
  close_req_i requests(_self, _self.value);
  auto requestItem = requests.find(cdp_id);
  eosio_assert(requestItem == requests.end(), "request already exists");
  
  requests.emplace(account, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.timestamp = time_ms();
  });
  
  run_requests(2);
}