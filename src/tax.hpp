// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::process_taxes() {
  const auto& tax = *_tax.begin();
  
  // send part of collected insurance to Scruge
  const uint64_t scruge_insurance_amount = tax.r_collected * SP / 100;
  const uint64_t insurance_amount = tax.r_collected - scruge_insurance_amount;
  const auto scruge_insurance = asset(scruge_insurance_amount, REX);
  if (scruge_insurance_amount > 0) {
    add_funds(SCRUGE, scruge_insurance, _self);
  }
  
  // send part of collected savings to Scruge
  const uint64_t scruge_savings_amount = tax.e_collected * SP / 100;
  const uint64_t savings_amount = tax.e_collected - scruge_savings_amount;
  const auto scruge_savings = asset(scruge_savings_amount, BUCK);
  if (scruge_savings_amount > 0) {
    add_balance(SCRUGE, scruge_savings, _self, true);
  }

  const auto oracle_time = _stat.begin()->oracle_timestamp;
  static const uint32_t last = time_point_sec(oracle_time).utc_seconds;
  static const uint32_t now = time_point_sec(get_current_time_point()).utc_seconds;
  static const uint32_t delta_t = now - last;
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    
    r.insurance_pool += insurance_amount;
    r.savings_pool += savings_amount;
    
    r.r_aggregated += r.r_total * delta_t;
    r.r_collected = 0;
    
    r.e_collected = 0;
  });
  
  // PRINT("add to pool", insurance_amount)
}

// collect interest to insurance pool from this cdp
void buck::accrue_interest(const cdp_i::const_iterator& cdp_itr) {
  const auto& tax = *_tax.begin();
  
  const auto oracle_time = _stat.begin()->oracle_timestamp;
  static const uint32_t now = time_point_sec(oracle_time).utc_seconds;
  const uint32_t last = cdp_itr->modified_round;
  
  if (now == last) return;
  if (cdp_itr->debt.amount == 0) return;
  
  static const uint128_t DM = 1000000000000;
  const uint128_t v = (exp(AR * double(now - last) / double(YEAR)) - 1) * DM;
  const int64_t accrued_amount = cdp_itr->debt.amount * v / DM;
  
  const int64_t accrued_debt_amount = accrued_amount * SR / 100;
  const int64_t accrued_collateral_amount = to_rex(accrued_amount * IR, 0);
  
  const asset accrued_debt = asset(accrued_debt_amount, BUCK);
  const asset accrued_collateral = asset(accrued_collateral_amount, REX);
  
  // PRINT("add tax", cdp_itr->id)
  update_supply(accrued_debt);

  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.collateral -= accrued_collateral;
    r.debt += accrued_debt;
    r.modified_round = now;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.e_collected += accrued_debt_amount;
    r.r_collected += accrued_collateral_amount;
  });
  
  // to-do check ccr for liquidation
}

void buck::buy_r(const cdp_i::const_iterator& cdp_itr) {
  const auto& tax = *_tax.begin();
  
  if (cdp_itr->debt.amount > 0 || cdp_itr->acr == 0) return;
  
  const int64_t excess = cdp_itr->collateral.amount * 100 / cdp_itr->acr;
  const auto oracle_time = _stat.begin()->oracle_timestamp;
  static const uint32_t now = time_point_sec(oracle_time).utc_seconds;
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.modified_round = now;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.r_total += excess;
  });
}

void buck::sell_r(const cdp_i::const_iterator& cdp_itr) {
  const auto& tax = *_tax.begin();
  
  if (cdp_itr->acr == 0 || cdp_itr->debt.amount > 0) return;
  
  const auto oracle_time = _stat.begin()->oracle_timestamp;
  static const uint32_t now = time_point_sec(oracle_time).utc_seconds;
  const uint32_t delta_t = now - cdp_itr->modified_round;
  
  const uint64_t excess = cdp_itr->collateral.amount * 100 / cdp_itr->acr;
  const uint64_t agec = excess * delta_t;
  
  uint64_t dividends_amount = 0;
  if (tax.r_aggregated > 0) {
    dividends_amount = uint128_t(agec) * tax.insurance_pool / tax.r_aggregated;
  }
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.r_total -= excess;
    r.r_aggregated -= agec;
    r.insurance_pool -= dividends_amount;
  });
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.collateral += asset(dividends_amount, REX);
    r.modified_round = now;
  });
}

void buck::save(const name& account, const asset& value) {
  require_auth(account);
  const auto& tax = *_tax.begin();
  
  // to-do validate all
  
  check(value.amount > 0, "can not use negative value");
  check(value.symbol == REX, "can not use asset with different symbol");
  
  accounts_i _accounts(_self, account.value);
  const auto account_itr = _accounts.find(BUCK.code().raw());
  
  sub_balance(account, value, false);
  
  // buy E
  const uint64_t received_e = ((uint128_t) value.amount * tax.savings_pool / tax.e_supply;
  check(received_e > 0, "to-do remove. this is probably wrong (save)");
  
  _accounts.modify(account_itr, account, [&](auto& r) {
    r.e_balance += received_e;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.e_supply += received_e;
  });
  
  run(3);
}

void buck::take(const name& account, const asset& value) {
  require_auth(account);
  const auto& tax = *_tax.begin();
  
  // to-do validate all
  
  check(value.amount > 0, "can not use negative value");
  check(value.symbol == REX, "can not use asset with different symbol");
  
  accounts_i _accounts(_self, account.value);
  const auto account_itr = _accounts.find(BUCK.code().raw());

  // sell E
  const uint64_t selling_e = (uint128_t(value.amount) * tax.savings_pool / tax.e_supply;
  check(account_itr->e_balance >= selling_e, "overdrawn savings balance");
  
  // to-do check supply not 0
  
  const int64_t pool_part = uint128_t(selling_e) * tax.savings_pool / tax.e_supply;
  const int64_t received_bucks_amount = pool_part * tax.savings_pool / tax.e_supply;
  const asset received_bucks = asset(received_bucks_amount, BUCK);
  
  _accounts.modify(account_itr, account, [&](auto& r) {
    r.e_balance -= selling_e;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.e_supply -= selling_e;
    r.savings_pool -= received_bucks_amount;
  });
  
  add_balance(account, received_bucks, same_payer, false);
  run(3);
}