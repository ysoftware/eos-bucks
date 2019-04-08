// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::change(uint64_t cdp_id, asset change_debt, asset change_collateral) {
  auto position_item = _cdp.find(cdp_id);
  check(position_item != _cdp.end(), "debt position does not exist");
  
  check(change_debt.amount != 0 || change_collateral.amount != 0, 
    "can not create empty reparametrization request");
  
  check(position_item->debt.symbol == change_debt.symbol, "debt symbol mismatch");
  check(position_item->collateral.symbol == change_collateral.symbol, "debt symbol mismatch");
  
  auto account = position_item->account;
  require_auth(account);
  check(is_mature(cdp_id), "can not reparametrize this debt position yet");
  
  auto request_item = _reparamreq.find(cdp_id);
  if (request_item != _reparamreq.end()) {
    
    // give back debt if was negative change
    if (request_item->change_debt.amount < 0) {
      add_balance(account, -request_item->change_debt, account, true);
    }
    
    _reparamreq.erase(request_item); // remove existing request
  }
  
  // to-do validate arguments
  
  asset new_debt = position_item->debt + change_debt;
  asset new_collateral = position_item->collateral + change_collateral;
  
  check(new_debt > MIN_DEBT, "can not reparametrize debt below the limit");
  check(new_collateral > MIN_COLLATERAL, "can not reparametrize debt below the limit");
  
  // take away debt if negative change
  if (change_debt.amount < 0) {
    sub_balance(account, -change_debt, true);
  }
  
  _reparamreq.emplace(account, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.timestamp = current_time_point();
    r.change_collateral = change_collateral;
    r.change_debt = change_debt;
    r.isPaid = change_collateral.amount <= 0; // isPaid if not adding collateral
  });
  
  run_requests(2);
}

void buck::changeacr(uint64_t cdp_id, double acr) {
  check(acr >= CR || acr == 0, "acr value is too small");
  check(acr < 1000, "acr value is too high");
  
  auto position_item = _cdp.find(cdp_id);
  check(position_item != _cdp.end(), "debt position does not exist");
  check(position_item->acr != acr, "acr is already set to this value");
  
  require_auth(position_item->account);
  
  _cdp.modify(position_item, same_payer, [&](auto& r) {
    r.acr = acr;
  });
}

void buck::closecdp(uint64_t cdp_id) {
  auto position_item = _cdp.find(cdp_id);
  check(position_item != _cdp.end(), "debt position does not exist");
  
  close_req_i requests(_self, _self.value);
  auto requestItem = requests.find(cdp_id);
  check(requestItem == requests.end(), "request already exists");

  check(is_mature(cdp_id), "can not close this debt position yet");

  require_auth(position_item->account);
  
  sub_balance(position_item->account, position_item->debt, true);
  
  requests.emplace(position_item->account, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.timestamp = current_time_point();
  });
  
  sell_rex(position_item->id, position_item->collateral);
  
  run_requests(2);
}

void buck::redeem(name account, asset quantity) {
  require_auth(account);
  
  // validate
  check(quantity.symbol == BUCK, "symbol mismatch");
  
  // find previous request
  auto request_item = _redeemreq.find(account.value);
  
  if (request_item != _redeemreq.end()) {
    _redeemreq.modify(request_item, same_payer, [&](auto& r) {
      r.quantity += quantity;
      r.timestamp = current_time_point();
    });
  }
  else {
    _redeemreq.emplace(account, [&](auto& r) {
      r.account = account;
      r.quantity = quantity;
      r.timestamp = current_time_point();
    });
  }
  
  sub_balance(account, quantity, true);
  
  run(3);
}