// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::change(uint64_t cdp_id, asset change_debt, asset change_collateral) {
  cdp_i positions(_self, _self.value);
  auto positionItem = positions.find(cdp_id);
  check(positionItem != positions.end(), "debt position does not exist");
  
  require_auth(positionItem->account);
  
  reparam_req_i requests(_self, _self.value);
  auto request_item = requests.find(cdp_id);
  if (request_item != requests.end()) {
    requests.erase(request_item); // remove existing request
  }
  
  // to-do validate arguments
  
  check(change_debt.amount != 0 || change_collateral.amount != 0, 
    "can not create empty reparametrization request");
  
  check(positionItem->debt.symbol == change_debt.symbol, "debt symbol mismatch");
  check(positionItem->collateral.symbol == change_collateral.symbol, "debt symbol mismatch");
  
  asset new_debt = positionItem->debt + change_debt;
  asset new_collateral = positionItem->collateral + change_collateral;
  
  check(new_debt > MIN_DEBT, "can not reparametrize debt below the limit");
  check(new_collateral > MIN_COLLATERAL, "can not reparametrize debt below the limit");
  
  // take away debt if negative change
  if (change_debt.amount < 0) {
    sub_balance(positionItem->account, -change_debt, true);
  }
  
  requests.emplace(positionItem->account, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.timestamp = current_time_point();
    r.change_collateral = change_collateral;
    r.change_debt = change_debt;
    r.isPaid = change_collateral.amount <= 0; // isPaid if not increasing collateral
  });
  
  run_requests(2);
}

void buck::changeacr(uint64_t cdp_id, double acr) {
  check(acr >= CR || acr == 0, "acr value is too small");
  check(acr < 1000, "acr value is too high");
  
  cdp_i positions(_self, _self.value);
  auto positionItem = positions.find(cdp_id);
  check(positionItem != positions.end(), "debt position does not exist");
  check(positionItem->acr != acr, "acr is already set to this value");
  
  require_auth(positionItem->account);
  
  positions.modify(positionItem, same_payer, [&](auto& r) {
    r.acr = acr;
  });
}

void buck::closecdp(uint64_t cdp_id) {
  cdp_i positions(_self, _self.value);
  auto positionItem = positions.find(cdp_id);
  check(positionItem != positions.end(), "debt position does not exist");
  
  close_req_i requests(_self, _self.value);
  auto requestItem = requests.find(cdp_id);
  check(requestItem == requests.end(), "request already exists");

  require_auth(positionItem->account);
  
  sub_balance(positionItem->account, positionItem->debt, true);
  
  requests.emplace(positionItem->account, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.timestamp = current_time_point();
  });
  
  run_requests(2);
}


void buck::redeem(name account, asset quantity) {
  require_auth(account);
  
  // validate
  check(quantity.symbol == BUCK, "symbol mismatch");
  
  // find previous request
  redeem_req_i requests(_self, _self.value);
  auto request_item = requests.find(account.value);
  
  if (request_item != requests.end()) {
    requests.modify(request_item, same_payer, [&](auto& r) {
      
      r.account = account;
      r.quantity += quantity;
      r.timestamp = current_time_point();
    });
  }
  else {
    requests.emplace(account, [&](auto& r) {
      r.account = account;
      r.quantity = quantity;
      r.timestamp = current_time_point();
    });
  }
  
  sub_balance(account, quantity, true);
  
  run(3);
}