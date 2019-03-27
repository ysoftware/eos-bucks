// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::transfer(name from, name to, asset quantity, std::string memo) {
  eosio_assert(from != to, "cannot transfer to self");
  require_auth(from);
  eosio_assert(is_account(to), "to account does not exist");
  
  require_recipient(from);
  require_recipient(to);
  
  eosio_assert(quantity.is_valid(), "invalid quantity");
  eosio_assert(quantity.amount > 0, "must transfer positive quantity");
  eosio_assert(quantity.symbol == BUCK, "symbol precision mismatch");
  eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");
  
  auto payer = has_auth(to) ? to : from;
  sub_balance(from, quantity);
  add_balance(to, quantity, payer);
}

void buck::receive_transfer(name from, name to, asset quantity, std::string memo) {
  if (to != _self || from == _self) { return; }
  require_auth(from);
  
  eosio_assert(quantity.symbol == EOS, "you have to use the system EOS token");
  eosio_assert(get_code() == "eosio.token"_n, "you have to use the system EOS token");
  
  eosio_assert(quantity.symbol.is_valid(), "Invalid quantity.");
	eosio_assert(quantity.amount > 0, "Only positive quantity allowed.");
  eosio_assert(quantity > MIN_COLLATERAL, "you have to supply a larger amount");
  
  // find cdp
  cdp_i positions(_self, _self.value);
  auto index = positions.get_index<"byaccount"_n>();
  auto item = index.find(from.value);
  while (item->collateral.amount != 0 && item != index.end()) {
    item++;
  }
  eosio_assert(item != index.end(), "open a debt position first");
  
  // calculate collateral after fee
  auto collateral_amount = (double) quantity.amount;
  auto debt = asset(0, BUCK);
  auto ccr = item->cr_sort;
  
  if (ccr > 0) {
    
    // take fee
    collateral_amount = collateral_amount * (CR-IF) / CR;
    
    // add debt
    auto priceEOS = get_eos_price();
    auto debt_amount = floor(priceEOS * collateral_amount / ccr);
    debt = asset(debt_amount, BUCK);
    add_balance(from, debt, from);
  }
  
  // update cdp
  index.modify(item, same_payer, [&](auto& r) {
    r.debt = debt;
    r.collateral = asset(collateral_amount, EOS);
    r.timestamp = time_ms();
  });
}

void buck::open(name account, double ccr, double acr) {
  require_auth(account);
  
  // check values
  eosio_assert(ccr >= CR || ccr == 0, "ccr value is too small");
  eosio_assert(acr >= CR || acr == 0, "acr value is too small");
  
  eosio_assert(ccr < 1000, "ccr value is too high");
  eosio_assert(acr < 1000, "acr value is too high");
  
  // open cdp
  cdp_i positions(_self, _self.value);
  positions.emplace(account, [&](auto& r) {
    r.id = positions.available_primary_key();
    r.account = account;
    r.cr_sort = 0;
    r.acr = acr;
    r.debt = asset(0, BUCK);
    r.collateral = asset(0, EOS);
    r.timestamp = 0;
  });
  
  // open account if doesn't exist
  accounts_i accounts(_self, account.value);
  auto item = accounts.find(BUCK.code().raw());
  if (item == accounts.end()) {
    accounts.emplace(account, [&](auto& r) {
      r.balance = asset(0, BUCK);
    });
  }
}