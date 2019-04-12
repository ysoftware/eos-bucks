// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::transfer(const name& from, const name& to, const asset& quantity, const std::string& memo) {
  check(from != to, "cannot transfer to self");
  require_auth(from);
  check(is_account(to), "to account does not exist");
  
  require_recipient(from);
  require_recipient(to);
  
  check(quantity.is_valid(), "invalid quantity");
  check(quantity.amount > 0, "must transfer positive quantity");
  check(quantity.symbol == BUCK, "symbol precision mismatch");
  check(memo.size() <= 256, "memo has more than 256 bytes");
  
  auto payer = has_auth(to) ? to : from;
  sub_balance(from, quantity, false);
  add_balance(to, quantity, payer, false);
  
  run(3);
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
    
    auto& maturity_item = _maturityreq.get(cdp_itr->id, "maturity has to exist for a new cdp");
    
    auto collateral_amount = (double) quantity.amount;
    auto debt = asset(0, BUCK);
    auto ccr = maturity_item.ccr;
    
    if (ccr > 0) {
      
      // check if current debt amount is above the limit
      auto price = get_eos_price();
      auto debt_amount = (price * collateral_amount / ccr);
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
    _maturityreq.modify(maturity_item, same_payer, [&](auto& r) {
      r.maturity_timestamp = get_maturity();
      r.ccr = ccr;
      r.add_collateral = quantity;
    });
    
    // buy rex with user's collateral
    buy_rex(cdp_itr->id, quantity);
  }
  else if (memo == "r") { // reparametrizing cdp
    
    auto index = _cdp.get_index<"byaccount"_n>();
    auto cdp_item = index.find(from.value);
    auto reparam_item = _reparamreq.find(cdp_item->id);
    
    while (cdp_item != index.end()) {
      if (!reparam_item->isPaid) { break; }
      cdp_item++;
      reparam_item = _reparamreq.find(cdp_item->id);
    }
    check(reparam_item != _reparamreq.end(), "could not find a reparametrization request");

    check(quantity == reparam_item->change_collateral,
      "you must transfer the exact amount of collateral you wish to increase");
    
    _reparamreq.modify(reparam_item, same_payer, [&](auto& r) {
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
  auto cdp_item = account_index.begin();
  while (cdp_item != account_index.end()) {
      check(cdp_item->rex.amount > 0 || cdp_item->collateral.amount > 0,
        "you already have created an unfinished debt position created");
        
      cdp_item++;
  }
  
  // open cdp
  auto id = _cdp.available_primary_key();
  _cdp.emplace(account, [&](auto& r) {
    r.id = id;
    r.account = account;
    r.acr = acr;
    r.rex_dividends = asset(0, EOS);
    r.collateral = asset(0, EOS);
    r.timestamp = current_time_point();
    r.rex = asset(0, REX);
    r.debt = asset(0, BUCK);
  });
  
  // open account if doesn't exist
  accounts_i accounts(_self, account.value);
  auto item = accounts.find(BUCK.code().raw());
  if (item == accounts.end()) {
    accounts.emplace(account, [&](auto& r) {
      r.balance = asset(0, BUCK);
    });
  }
  
  // open maturity request
  _maturityreq.emplace(account, [&](auto& r) {
    r.maturity_timestamp = get_maturity();
    r.add_collateral = asset(0, EOS);
    r.change_debt = asset(0, BUCK);
    r.cdp_id = id;
    r.ccr = ccr;
  });
  
  run(3);
}