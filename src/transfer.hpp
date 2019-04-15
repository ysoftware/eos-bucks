// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::transfer(const name& from, const name& to, const asset& quantity, const std::string& memo) {
  check(from != to, "cannot transfer to self");
  check(is_account(to), "to account does not exist");
  
  require_auth(from);
  require_recipient(from);
  require_recipient(to);
  
  check(quantity.is_valid(), "invalid quantity");
  check(quantity.amount > 0, "must transfer positive quantity");
  check(quantity.symbol == BUCK, "symbol precision mismatch");
  check(memo.size() <= 256, "memo has more than 256 bytes");
  
  // pay stability tax
  const uint64_t tax_amount = ceil((double) quantity.amount * TT);
  const asset tax = asset(tax_amount, BUCK);
  const asset receive_quantity = quantity - tax;
  pay_tax(tax);
  
  const auto payer = has_auth(to) ? to : from;
  sub_balance(from, quantity, false);
  add_balance(to, receive_quantity, payer, false);
  
  // send notification to receiver with actually received quantity
  inline_received(from, to, receive_quantity, memo);
	
  run(3);
}

void buck::received(const name& from, const name& to, const asset& quantity, const std::string& memo) {
  require_auth(_self);
  require_recipient(from);
  require_recipient(to);
}

void buck::notify_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo) {
  if (to != _self || from == _self) { return; }
  require_auth(from);
  
  check(quantity.symbol == EOS, "you have to transfer EOS");
  check(get_first_receiver() == "eosio.token"_n, "you have to transfer EOS");

  check(quantity.symbol.is_valid(), "invalid quantity");
	check(quantity.amount > 0, "must transfer positive quantity");
  
  if (memo == "") { // opening cdp 
    check(quantity > MIN_COLLATERAL, "you have to supply a larger amount");
    
    // find cdps
    auto index = _cdp.get_index<"byaccount"_n>();
    auto cdp_itr = index.find(from.value);
    while (cdp_itr->collateral.amount != 0 && cdp_itr != index.end()) {
      cdp_itr++;
    }
    
    check(cdp_itr != index.end(), "open a debt position first");
    
    const auto maturity_itr = _maturityreq.require_find(cdp_itr->id, "maturity has to exist for a new cdp");
    
    const double collateral_amount = (double) quantity.amount;
    const double ccr = maturity_itr->ccr;
    auto debt = ZERO_BUCK;
    
    if (ccr > 0) {
      
      // check if current debt amount is above the limit
      const auto price = get_eos_price();
      const auto debt_amount = (price * collateral_amount / ccr);
      debt = asset(floor(debt_amount), BUCK);
      check(debt >= MIN_DEBT, "not enough collateral to receive minimum debt");
    }
    else {
      
      // if ccr == 0, not issuing any debt
      index.modify(cdp_itr, same_payer, [&](auto& r) {
        r.debt = debt;
        r.timestamp = current_time_point();
        r.collateral = asset(collateral_amount, EOS);
      });
    }
    
    // setup maturity
    _maturityreq.modify(maturity_itr, same_payer, [&](auto& r) {
      r.maturity_timestamp = get_maturity();
      r.ccr = ccr;
      r.add_collateral = quantity;
    });
    
    // buy rex with user's collateral
    buy_rex(cdp_itr->id, quantity);
  }
  else if (memo == "r") { // reparametrizing cdp
    
    auto index = _cdp.get_index<"byaccount"_n>();
    auto cdp_itr = index.find(from.value);
    auto reparam_itr = _reparamreq.find(cdp_itr->id);
    
    while (cdp_itr != index.end()) {
      if (!reparam_itr->isPaid) { break; }
      cdp_itr++;
      reparam_itr = _reparamreq.find(cdp_itr->id);
    }
    check(reparam_itr != _reparamreq.end(), "could not find a reparametrization request");

    check(quantity == reparam_itr->change_collateral,
      "you must transfer the exact amount of collateral you wish to increase");
    
    _reparamreq.modify(reparam_itr, same_payer, [&](auto& r) {
      r.isPaid = true;
    });
  }
  
  run(3);
}

void buck::open(const name& account, double ccr, double acr) {
  require_auth(account);
  
  // check values
  check(ccr >= CR || ccr == 0, "ccr value is too small");
  check(acr >= CR || acr == 0, "acr value is too small");
  check(acr != 0 || ccr != 0, "acr and ccr can not be both 0");
  
  check(ccr < 1000, "ccr value is too high");
  check(acr < 1000, "acr value is too high");
  
  // to-do assert no other cdp without collateral opened
  auto account_index = _cdp.get_index<"byaccount"_n>();
  auto cdp_itr = account_index.begin();
  while (cdp_itr != account_index.end()) {
      check(cdp_itr->rex.amount > 0 || cdp_itr->collateral.amount > 0,
        "you already have created an unfinished debt position created");
      cdp_itr++;
  }
  
  // open cdp
  const auto id = _cdp.available_primary_key();
  _cdp.emplace(account, [&](auto& r) {
    r.id = id;
    r.account = account;
    r.acr = acr;
    r.rex_dividends = ZERO_EOS;
    r.collateral = ZERO_EOS;
    r.timestamp = current_time_point();
    r.rex = ZERO_REX;
    r.debt = ZERO_BUCK;
    r.modified_round = 0;
  });
  
  // open account if doesn't exist
  accounts_i accounts(_self, account.value);
  auto account_itr = accounts.find(BUCK.code().raw());
  if (account_itr == accounts.end()) {
    accounts.emplace(account, [&](auto& r) {
      r.balance = ZERO_BUCK;
    });
  }
  
  // open maturity request for collateral
  _maturityreq.emplace(account, [&](auto& r) {
    r.maturity_timestamp = get_maturity();
    r.add_collateral = ZERO_EOS;
    r.change_debt = ZERO_BUCK;
    r.cdp_id = id;
    r.ccr = ccr;
  });
  
  run(3);
}