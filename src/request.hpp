// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::change(uint64_t cdp_id, const asset& change_debt, const asset& change_collateral) {
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  
  check(change_debt.symbol.is_valid(), "invalid debt quantity");
  check(change_collateral.symbol.is_valid(), "invalid collateral quantity");
  check(change_debt.amount != 0 || change_collateral.amount != 0, "empty request does not make sense");
  
  const auto cdp_itr = _cdp.require_find(cdp_id, "debt position does not exist");
  check(cdp_itr->debt.symbol == change_debt.symbol, "debt symbol mismatch");
  check(cdp_itr->collateral.symbol == change_collateral.symbol, "debt symbol mismatch");
  
  const auto account = cdp_itr->account;
  require_auth(account);
  
  // to-do maturity
  
  // to-do cancel and return previous request

  // to-do remove close request
  
  const auto reparam_itr = _reparamreq.find(cdp_id);
  if (reparam_itr != _reparamreq.end()) {
    
    // give back debt if was negative change
    if (reparam_itr->change_debt.amount < 0) {
      add_balance(account, -reparam_itr->change_debt, account, true);
    }
    
    // to-do what about collateral?
    
    _reparamreq.erase(reparam_itr); // remove existing request
  }
  
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
  
  const auto now = get_current_time_point();
  check(cdp_itr->maturity <= now || change_collateral.amount > 0, "can not remove immature cdp collateral");
  
  _reparamreq.emplace(account, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.timestamp = get_current_time_point();
    r.change_collateral = change_collateral;
    r.change_debt = change_debt;
    
    const auto maturity = get_amount_maturity(cdp_itr->account, change_collateral);
    
    // if adding collateral and amount is not matured yet
    if (change_collateral.amount > 0 && maturity > now) {
      r.maturity = maturity;
    }
    else {
      r.maturity = time_point(microseconds(0));
    }
  });
  
  run_requests(10);
}

void buck::changeacr(uint64_t cdp_id, uint16_t acr) {
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  
  check(acr >= CR || acr == 0, "acr value is too small");
  check(acr < 1000'00, "acr value is too high");
  
  const auto cdp_itr = _cdp.require_find(cdp_id, "debt position does not exist");
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
  
  // to-do collision (undo reparam request)
  
  const auto cdp_itr = _cdp.require_find(cdp_id, "debt position does not exist");
  check(cdp_itr->maturity <= get_current_time_point(), "can not close immature cdp");
  
  const auto close_itr = _closereq.find(cdp_id);
  check(close_itr == _closereq.end(), "close request already exists");

  require_auth(cdp_itr->account);
  
  accrue_interest(cdp_itr);
  
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