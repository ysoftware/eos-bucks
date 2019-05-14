// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::change(uint64_t cdp_id, const asset& change_debt, const asset& change_collateral) {
  
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  
  // to-do validation
  
  const auto cdp_itr = _cdp.find(cdp_id);
  check(cdp_itr != _cdp.end(), "debt position does not exist");
  
  check(change_debt.amount != 0 || change_collateral.amount != 0, 
    "can not create empty reparametrization request");
      
  check(cdp_itr->debt.symbol == change_debt.symbol, "debt symbol mismatch");
  check(cdp_itr->collateral.symbol == change_collateral.symbol, "debt symbol mismatch");
    
  const auto account = cdp_itr->account;
  require_auth(account);
  check(is_mature(cdp_id), "can not reparametrize this debt position yet");
  
  // revert previous request to replace it with the new one
  
  const auto reparam_itr = _reparamreq.find(cdp_id);
  if (reparam_itr != _reparamreq.end()) {
    
    // give back debt if was negative change
    if (reparam_itr->change_debt.amount < 0) {
      add_balance(account, -reparam_itr->change_debt, account, true);
    }
    _reparamreq.erase(reparam_itr); // remove existing request
  }
  
  const auto maturity_itr = _maturityreq.find(cdp_id);
  if (maturity_itr != _maturityreq.end()) {
    
    // to-do
    // fail? request has been paid for already (from fund)
  }
  
  // to-do validate arguments
  
  const asset new_debt = cdp_itr->debt + change_debt;
  const asset new_collateral = cdp_itr->collateral + change_collateral;
  
  // to-do what if wants to become insurer (0 debt)?
  
  if (new_debt.amount > 0) {
    const auto ccr = to_buck(new_collateral.amount) / new_debt.amount;
    check(ccr >= CR, "can not reparametrize below 150% CCR");
  }
  
  
  check(new_debt >= MIN_DEBT || new_debt.amount == 0, "can not reparametrize debt below the limit");
  // to-do check min collateral in EOS
  
  // take away debt if negative change
  if (change_debt.amount < 0) {
    sub_balance(account, -change_debt, false);
  }
  
  if (change_collateral.amount > 0) {
    sub_funds(cdp_itr->account, change_collateral);
  }
  
  // if rex is not matured, create maturity request
  if (get_amount_maturity(cdp_itr->account, change_collateral) > get_current_time_point()) {
    
    _maturityreq.emplace(account, [&](auto& r) {
      r.maturity_timestamp = get_amount_maturity(cdp_itr->account, change_collateral);
      r.add_collateral = change_collateral;
      r.change_debt = change_debt;
      r.cdp_id = cdp_id;
      r.ccr = 0;
    });
  }
  else {
  
    _reparamreq.emplace(account, [&](auto& r) {
      r.cdp_id = cdp_id;
      r.timestamp = get_current_time_point();
      r.change_collateral = change_collateral;
      r.change_debt = change_debt;
    });
  }
  
  run_requests(10);
}

void buck::changeacr(uint64_t cdp_id, uint16_t acr) {
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  
  // to-do validation
  
  const auto maturity_itr = _maturityreq.find(cdp_id);
  check(maturity_itr == _maturityreq.end(), "cdp is being updated right now");
  
  check(acr >= CR || acr == 0, "acr value is too small");
  check(acr < 1000'00, "acr value is too high");
  
  const auto cdp_itr = _cdp.find(cdp_id);
  check(cdp_itr != _cdp.end(), "debt position does not exist");
  check(cdp_itr->acr != acr, "acr is already set to this value");
  
  require_auth(cdp_itr->account);
  
  accrue_interest(cdp_itr);
  sell_r(cdp_itr);
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.acr = acr;
  });
  
  buy_r(cdp_itr);
  run_requests(10);
}

void buck::close(uint64_t cdp_id) {
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  
  // to-do validation
  
  const auto maturity_itr = _maturityreq.find(cdp_id);
  check(maturity_itr == _maturityreq.end(), "cdp is being updated right now");
  
  const auto cdp_itr = _cdp.find(cdp_id);
  check(cdp_itr != _cdp.end(), "debt position does not exist");
  
  const auto close_itr = _closereq.find(cdp_id);
  check(close_itr == _closereq.end(), "request already exists");

  check(is_mature(cdp_id), "can not close this debt position yet");

  require_auth(cdp_itr->account);
  
  accrue_interest(cdp_itr);
  
  
  // check oracle??
  
  sub_balance(cdp_itr->account, cdp_itr->debt, true);
  
  _closereq.emplace(cdp_itr->account, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.timestamp = get_current_time_point();
  });
  
  run_requests(10);
}

void buck::redeem(const name& account, const asset& quantity) {
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  require_auth(account);
  
  // validate
  check(quantity.symbol == BUCK, "symbol mismatch");
  
  // open account if doesn't exist
  add_funds(account, ZERO_REX, account);
  
  // find previous request
  const auto redeem_itr = _redeemreq.find(account.value);
  
  if (redeem_itr != _redeemreq.end()) {
    
    // return previous request
    add_balance(account, redeem_itr->quantity, account, false);
    
    _redeemreq.modify(redeem_itr, same_payer, [&](auto& r) {
      r.quantity = quantity;
      r.timestamp = get_current_time_point();
    });
  }
  else {
    _redeemreq.emplace(account, [&](auto& r) {
      r.account = account;
      r.quantity = quantity;
      r.timestamp = get_current_time_point();
    });
  }
  
  sub_balance(account, quantity, false);
  run(10);
}