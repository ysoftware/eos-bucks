// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::transfer(const name& from, const name& to, const asset& quantity, const std::string& memo) {
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  
  check(from != to, "cannot transfer to self");
  check(is_account(to), "to account does not exist");
  
  require_auth(from);
  require_recipient(from);
  require_recipient(to);
  
  check(quantity.is_valid(), "invalid quantity");
  check(quantity.amount > 0, "must transfer positive quantity");
  check(quantity.symbol == BUCK, "symbol precision mismatch");
  check(memo.size() <= 256, "memo has more than 256 bytes");

  const auto payer = has_auth(to) ? to : from;
  sub_balance(from, quantity, false);
  add_balance(to, quantity, payer, false);
	
  run(1);
}

void buck::withdraw(const name& from, const asset& quantity) {
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  
  require_auth(from);
  
  check(quantity.symbol == EOS, "you have to transfer EOS");
  check(get_first_receiver() == "eosio.token"_n, "you have to transfer EOS");

  check(quantity.symbol.is_valid(), "invalid quantity");
	check(quantity.amount > 0, "must transfer positive quantity");
  
  sub_funds(from, quantity);
  
  inline_transfer(from, quantity, "withdraw funds", EOSIO_TOKEN);
  
  run(3);
}

void buck::notify_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo) {
  if (to != _self || from == _self || memo != "deposit") { return; }
  
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");

  require_auth(from);
  
  check(quantity.symbol == EOS, "you have to transfer EOS");
  check(get_first_receiver() == "eosio.token"_n, "you have to transfer EOS");

  check(quantity.symbol.is_valid(), "invalid quantity");
	check(quantity.amount > 0, "must transfer positive quantity");
  
  // to-do to protect from spam, check if depositing more than 1 EOS
  
  add_funds(from, quantity, _self);

  run(3);
}

void buck::open(const name& account, const asset& quantity, double ccr, double acr) {
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  require_auth(account);
  
  // check values
  check(ccr >= CR || ccr == 0, "ccr value is too small");
  check(acr >= CR || acr == 0, "acr value is too small");
  check(acr != 0 || ccr != 0, "acr and ccr can not be both 0");
  
  check(ccr < 1000, "ccr value is too high");
  check(acr < 1000, "acr value is too high");
  
  sub_funds(account, quantity);
  
  auto debt = ZERO_BUCK;
  
  if (ccr > 0) {
    
    // check if debt amount is above the limit
    const auto price = get_eos_price();
    const auto debt_amount = price * (double) quantity.amount / ccr;
    debt = asset(floor(debt_amount), BUCK);
    check(debt >= MIN_DEBT, "not enough collateral to receive minimum debt");
  }
  
  // open cdp
  const auto id = _cdp.available_primary_key();
  _cdp.emplace(account, [&](auto& r) {
    r.id = id;
    r.account = account;
    r.acr = acr;
    r.collateral = ZERO_EOS;
    r.timestamp = current_time_point();
    r.rex = ZERO_REX;
    r.debt = ZERO_BUCK;
    r.accrued_debt = ZERO_BUCK;
    r.modified_round = 0;
    r.accrued_timestamp = current_time_point();
  });
  
  // open account if doesn't exist
  add_balance(account, ZERO_BUCK, account, false);
  add_funds(account, ZERO_EOS, account);
  
  // open maturity request for collateral
  _maturityreq.emplace(account, [&](auto& r) {
    r.maturity_timestamp = get_maturity();
    r.add_collateral = quantity;
    r.change_debt = ZERO_BUCK;
    r.cdp_id = id;
    r.ccr = ccr;
  });
  
  buy_rex(id, quantity);
  
  run(3);
}