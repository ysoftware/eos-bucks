// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::change(uint64_t cdp_id, asset change_debt, asset change_collateral) {
  cdp_i positions(_self, _self.value);
  auto positionItem = positions.find(cdp_id);
  eosio_assert(positionItem != positions.end(), "cdp does not exist");
  
  require_auth(positionItem->account);
  
  // to-do modify instead of failing
  reparam_req_i requests(_self, _self.value);
  auto requestItem = requests.find(cdp_id);
  eosio_assert(requestItem == requests.end(), "request already exists");
  
  // to-do validate arguments
  
  eosio_assert(change_debt.amount != 0 || change_collateral.amount != 0, 
    "can not create empty reparametrization request");
  
  eosio_assert(positionItem->debt.symbol == change_debt.symbol, "debt symbol mismatch");
  eosio_assert(positionItem->collateral.symbol == change_collateral.symbol, "debt symbol mismatch");
  
  asset new_debt = positionItem->debt + change_debt;
  asset new_collateral = positionItem->collateral + change_collateral;
  
  eosio_assert(new_debt > MIN_DEBT, "can not reparametrize debt below the limit");
  eosio_assert(new_collateral > MIN_COLLATERAL, "can not reparametrize debt below the limit");
  
  // take away debt if negative change
  if (change_debt.amount < 0) {
    sub_balance(positionItem->account, -change_debt, true);
  }
  
  requests.emplace(positionItem->account, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.timestamp = time_ms();
    r.change_collateral = change_collateral;
    r.change_debt = change_debt;
    r.isPaid = change_collateral.amount <= 0; // isPaid if not increasing collateral
  });
  
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


void buck::redeem(name account, asset quantity) {
  require_auth(account);
  
  // validate
  eosio_assert(quantity.symbol == BUCK, "symbol mismatch");
  
  // find previous request
  redeem_req_i requests(_self, _self.value);
  auto request_item = requests.find(account.value);
  
  if (request_item != requests.end()) {
    requests.modify(request_item, same_payer, [&](auto& r) {
      
      r.account = account;
      r.quantity += quantity;
      r.timestamp = time_ms();
    });
  }
  else {
    requests.emplace(account, [&](auto& r) {
      r.account = account;
      r.quantity = quantity;
      r.timestamp = time_ms();
    });
  }
  
  sub_balance(account, quantity, true);
  
  run(3);
}