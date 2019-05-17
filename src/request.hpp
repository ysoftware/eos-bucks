// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::cancel_previous_requests(const cdp_i::const_iterator& cdp_itr) {
  
  // remove close request
  const auto close_itr = _closereq.find(cdp_itr->id);
  if (close_itr != _closereq.end()) {
    
    // give back bucks
    add_balance(cdp_itr->account, cdp_itr->debt, same_payer);
    
    _closereq.erase(close_itr);
  }
  
  // remove reparam request
  const auto reparam_itr = _reparamreq.find(cdp_itr->id);
  if (reparam_itr != _reparamreq.end()) {
    
    // give back bucks if was negative change
    if (reparam_itr->change_debt.amount < 0) {
      add_balance(cdp_itr->account, -reparam_itr->change_debt, same_payer);
    }
    
    // give back rex if was positive change
    if (reparam_itr->change_collateral.amount > 0) {
      add_funds(cdp_itr->account, reparam_itr->change_collateral, same_payer, reparam_itr->maturity);
    }
    
    _reparamreq.erase(reparam_itr);
  }
}

void buck::change(uint64_t cdp_id, const asset& change_debt, const asset& change_collateral) {
  check(check_operation_status(0), "cdp operations have been temporarily frozen");
  
  check(change_debt.is_valid(), "invalid debt quantity");
  check(change_collateral.is_valid(), "invalid collateral quantity");
  check(change_debt.amount != 0 || change_collateral.amount != 0, "empty request does not make sense");
  
  const auto cdp_itr = _cdp.require_find(cdp_id, "debt position does not exist");
  const auto account = cdp_itr->account;
  require_auth(account);
  
  check(cdp_itr->debt.symbol == change_debt.symbol, "debt symbol mismatch");
  check(cdp_itr->collateral.symbol == change_collateral.symbol, "collateral symbol mismatch");
  check(cdp_itr->maturity <= get_current_time_point(), "can not close immature cdp");

  cancel_previous_requests(cdp_itr);

  // start with new request
  accrue_interest(cdp_itr, true);
  
  const asset new_debt = cdp_itr->debt + change_debt;
  const asset new_collateral = cdp_itr->collateral + change_collateral;
  
  if (new_debt.amount > 0) {
    const auto ccr = to_buck(new_collateral.amount) / new_debt.amount;
    check(ccr >= CR, "can not reparametrize CCR below CR");
  }
  
  check(new_debt >= MIN_DEBT || new_debt.amount == 0, "can not reparametrize debt below the limit");
  
  const auto min_collateral = convert(MIN_COLLATERAL.amount, true);
  check(new_collateral.amount >= min_collateral, "can not reparametrize collateral below the limit");
  
  // take away debt if negative change
  if (change_debt.amount < 0) {
    sub_balance(account, -change_debt);
  }

  auto amount_maturity = FAR_PAST;
  if (change_collateral.amount > 0) {
    amount_maturity = sub_funds(cdp_itr->account, change_collateral);
  }
  
  const auto now = get_current_time_point();
  check(cdp_itr->maturity <= now || change_collateral.amount > 0, "can not remove immature cdp collateral");
  
  _reparamreq.emplace(account, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.timestamp = get_current_time_point();
    r.change_collateral = change_collateral;
    r.change_debt = change_debt;
    r.maturity = amount_maturity;
  });
  
  run_requests(10);
}

void buck::changeacr(uint64_t cdp_id, uint16_t acr) {
  check(check_operation_status(0), "cdp operations have been temporarily frozen");
  
  check(acr >= CR || acr == 0, "acr value is too small");
  check(acr < 1000, "acr value is too high");
  
  const auto cdp_itr = _cdp.require_find(cdp_id, "debt position does not exist");
  require_auth(cdp_itr->account);
  
  check(cdp_itr->acr != acr, "acr is already set to this value");
  check(cdp_itr->debt.amount != 0 || acr != 0, "can not set 0 acr for cdp with 0 debt");
  
  accrue_interest(cdp_itr, true);
  remove_excess_collateral(cdp_itr);
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.acr = acr;
  });
  
  set_excess_collateral(cdp_itr);
  run_requests(10);
}

void buck::close(uint64_t cdp_id) {
  check(check_operation_status(0), "cdp operations have been temporarily frozen");
  
  const auto cdp_itr = _cdp.require_find(cdp_id, "debt position does not exist");
  require_auth(cdp_itr->account);
  
  cancel_previous_requests(cdp_itr);
  
  check(cdp_itr->maturity <= get_current_time_point(), "can not close immature cdp");
  const auto close_itr = _closereq.find(cdp_id);
  check(close_itr == _closereq.end(), "close request already exists");
  
  accrue_interest(cdp_itr, true);
  sub_balance(cdp_itr->account, cdp_itr->debt);
  
  _closereq.emplace(cdp_itr->account, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.timestamp = get_current_time_point();
  });
  
  run_requests(10);
}

void buck::redeem(const name& account, const asset& quantity) {
  require_auth(account);
  check(check_operation_status(0), "cdp operations have been temporarily frozen");
  
  check(quantity.is_valid(), "invalid quantity");
  check(quantity.amount > 0, "must transfer positive quantity");
  check(quantity.symbol == BUCK, "symbol mismatch");
  check(quantity >= MIN_REDEMPTION, "not enough quantity to redeem");
  
  // open account if doesn't exist
  add_funds(account, ZERO_REX, account, FAR_PAST);
  
  // find previous request
  const auto redeem_itr = _redeemreq.find(account.value);
  
  if (redeem_itr != _redeemreq.end()) {
    
    // return previous request
    add_balance(account, redeem_itr->quantity, account);
    
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
  
  sub_balance(account, quantity);
  run(10);
}