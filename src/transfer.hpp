// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::transfer(const name& from, const name& to, const asset& quantity, const std::string& memo) {
  require_auth(from);
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  
  check(from != to, "cannot transfer to self");
  check(is_account(to), "to account does not exist");
  
  require_recipient(from);
  require_recipient(to);
  
  check(quantity.is_valid(), "invalid quantity");
  check(quantity.amount > 0, "must transfer positive quantity");
  check(quantity.symbol == BUCK, "symbol precision mismatch");
  check(memo.size() <= 256, "memo has more than 256 bytes");

  const auto payer = has_auth(to) ? to : from;
  sub_balance(from, quantity, false);
  add_balance(to, quantity, payer, false);
	
  run(3);
}

void buck::withdraw(const name& from, const asset& quantity) {
  require_auth(from);
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  
  check(quantity.symbol.is_valid(), "invalid quantity");
	check(quantity.amount > 0, "must transfer positive quantity");
  
  if (quantity.symbol == REX) {
    time_point_sec maturity_time = get_amount_maturity(from, quantity);
    check(current_time_point_sec() > maturity_time, "insufficient matured rex");
    
    sub_funds(from, quantity);
    sell_rex(from, quantity);
  }
  else if (quantity.symbol == EOS) {
    sub_exchange_funds(from, quantity);
    inline_transfer(from, quantity, "buck: withdraw from exchange funds", EOSIO_TOKEN);
  }
  else {
    check(false, "symbol mismatch");
  }
  run(10);
}

void buck::notify_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo) {
  require_auth(from);
  if (to != _self || from == _self) { return; }
  
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  
  check(quantity.symbol == EOS, "you have to transfer EOS");
  check(get_first_receiver() == "eosio.token"_n, "you have to transfer EOS");

  check(quantity.symbol.is_valid(), "invalid quantity");
	check(quantity.amount > 0, "must transfer positive quantity");
  
  if (memo == "deposit") {
    buy_rex(from, quantity);
  }
  else if (memo == "exchange") {
    add_exchange_funds(from, quantity, _self);
  }
  else {
    check(false, "do not send tokens in the contract without correct memo");
  }
  run(3);
}

void buck::open(const name& account, const asset& quantity, uint16_t ccr, uint16_t acr) {
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  require_auth(account);
  
  // to-do validate
  
  check(quantity.is_valid(), "invalid quantity");
  check(quantity.amount > 0, "can not use negative value");
  check(quantity.symbol == REX, "can not use asset with different symbol");
  
  check(ccr >= CR || ccr == 0, "ccr value is too small");
  check(acr >= CR || acr == 0, "acr value is too small");
  check(acr != 0 || ccr != 0, "acr and ccr can not be both 0");
  
  check(ccr < 1'000'00, "ccr value is too high");
  check(acr < 1'000'00, "acr value is too high");
  
  sub_funds(account, quantity);
  
  const time_point oracle_timestamp = _stat.begin()->oracle_timestamp;
  const uint32_t now = time_point_sec(oracle_timestamp).utc_seconds;
  auto issue_debt = ZERO_BUCK;
  
  if (ccr > 0) {
    
    // check if debt amount is above the limit (actual amount is calculated at maturity)
    const auto debt_amount = to_buck(quantity.amount) / ccr;
    issue_debt = asset(debt_amount, BUCK);
    check(issue_debt >= MIN_DEBT, "not enough collateral to receive minimum debt");
  }
  
  // open account if doesn't exist
  add_balance(account, ZERO_BUCK, account, false);
  add_funds(account, ZERO_REX, account);
  
  const auto id = _cdp.available_primary_key();
  _cdp.emplace(account, [&](auto& r) {
    r.id = id;
    r.account = account;
    r.acr = acr;
    r.collateral = quantity;
    r.debt = issue_debt;
    r.modified_round = now;
    r.maturity = get_amount_maturity(account, quantity);
  });
  
  if (issue_debt.amount == 0) {
    buy_r(_cdp.require_find(id));
  }
  else {
    add_balance(account, issue_debt, account, true);
  }
  
  run(5);
}