// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::process_taxes() {
  const auto& tax = *_tax.begin();
  
  // send part of collected insurance to Scruge
  const int64_t scruge_insurance_amount = tax.r_collected.amount * SP / 100;
  const int64_t insurance_amount = tax.r_collected.amount - scruge_insurance_amount;
  const auto scruge_insurance = asset(scruge_insurance_amount, REX);
  if (scruge_insurance_amount > 0) {
    add_funds(SCRUGE, scruge_insurance, _self);
  }
  
  // send part of collected savings to Scruge
  const int64_t scruge_savings_amount = tax.e_collected.amount * SP / 100;
  const int64_t savings_amount = tax.e_collected.amount - scruge_savings_amount;
  const auto scruge_savings = asset(scruge_savings_amount, BUCK);
  if (scruge_savings_amount > 0) {
    add_balance(SCRUGE, scruge_savings, _self, true);
  }

  const auto oracle_time = _stat.begin()->oracle_timestamp;
  static const uint32_t last = time_point_sec(oracle_time).utc_seconds;
  static const uint32_t now = time_point_sec(get_current_time_point()).utc_seconds;
  static const uint32_t delta_t = now - last;
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    
    r.insurance_pool += asset(insurance_amount, REX);
    r.savings_pool += asset(savings_amount, BUCK);
    
    r.r_aggregated += r.r_total * delta_t;
    r.r_collected = ZERO_REX;
    
    r.e_collected = ZERO_BUCK;
  });
  
  // sanity check
  check(tax.insurance_pool.amount >= 0, "programmer error, pools can't go below 0");
  check(tax.savings_pool.amount >= 0, "programmer error, pools can't go below 0");
  
  // PRINT("add to pool", insurance_amount)
}

void buck::collect_taxes(uint32_t max) {
  auto accrual_index = _cdp.get_index<"accrued"_n>();
  auto accrual_itr = accrual_index.begin();
  
  const time_point oracle_timestamp = _stat.begin()->oracle_timestamp;
  const uint32_t now = time_point_sec(oracle_timestamp).utc_seconds;
  
  int i = 0;
  while (i < max && accrual_itr != accrual_index.end()
          && now - accrual_itr->modified_round > ACCRUAL_PERIOD) {
  
    accrue_interest(_cdp.require_find(accrual_itr->id));
    accrual_itr = accrual_index.begin(); // take first element after index updated
    i++;
  }
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
  // PRINT("added", accrued_debt)
  
  update_supply(accrued_debt);

  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.collateral -= accrued_collateral;
    r.debt += accrued_debt;
    r.modified_round = now;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.e_collected += accrued_debt;
    r.r_collected += accrued_collateral;
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
  
  int64_t dividends_amount = 0;
  if (tax.r_aggregated > 0) {
    dividends_amount = uint128_t(agec) * tax.insurance_pool.amount / tax.r_aggregated;
  }
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.r_total -= excess;
    r.r_aggregated -= agec;
    r.insurance_pool -= asset(dividends_amount, REX);
  });
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.collateral += asset(dividends_amount, REX);
    r.modified_round = now;
  });
}

void buck::save(const name& account, const asset& value) {
  require_auth(account);
  const auto& tax = *_tax.begin();
  
  check(value.is_valid(), "invalid quantity");
  check(value.amount > 0, "can not use negative value");
  check(value.symbol == BUCK, "can not use asset with different symbol");
  check(value.amount > 1'0000, "not enough value to put in savings");
  
  accounts_i _accounts(_self, account.value);
  const auto account_itr = _accounts.find(BUCK.code().raw());
  check(account_itr != _accounts.end(), "balance object doesn't exist");
  
  uint64_t received_e = value.amount;
  if (tax.e_supply > 0) {
    received_e = (uint128_t) value.amount * tax.e_supply / tax.savings_pool.amount;
  }
  
  PRINT("supply", tax.e_supply)
  PRINT("pool", tax.savings_pool.amount)
  
  check(received_e > 0, "not enough value to receive minimum amount of savings");
  
  _accounts.modify(account_itr, account, [&](auto& r) {
    r.e_balance += received_e;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.e_supply += received_e;
    r.savings_pool += value;
  });
  
  sub_balance(account, value, false);
  run(3);
}

void buck::take(const name& account, const uint64_t value) {
  require_auth(account);
  const auto& tax = *_tax.begin();
  
  check(value > 0, "can not use negative value");
  
  accounts_i _accounts(_self, account.value);
  const auto account_itr = _accounts.find(BUCK.code().raw());
  check(account_itr != _accounts.end(), "balance object doesn't exist");
  check(account_itr->e_balance >= value, "overdrawn savings balance");
  
  const int64_t received_bucks_amount = uint128_t(value) * tax.savings_pool.amount / tax.e_supply;
  const asset received_bucks = asset(received_bucks_amount, BUCK);
  
  _accounts.modify(account_itr, account, [&](auto& r) {
    r.e_balance -= value;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.e_supply -= value;
    r.savings_pool -= asset(received_bucks_amount, BUCK);
  });
  
  add_balance(account, received_bucks, same_payer, false);
  run(3);
}