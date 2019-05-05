// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::process_taxes() {
  const auto& tax = *_tax.begin();
  
  const auto collected_to_do_remove_this = tax.r_collected;
  
  // send part of collected insurance to Scruge
  const uint64_t scruge_insurance_amount = tax.r_collected * SP / 100;
  const auto scruge_insurance = asset(scruge_insurance_amount, REX);
  if (scruge_insurance_amount > 0) {
    add_funds(SCRUGE, scruge_insurance, _self);
  }
  
  // send part of collected savings to Scruge
  const uint64_t scruge_savings_amount = tax.e_collected * SP / 100;
  const auto scruge_savings = asset(scruge_savings_amount, BUCK);
  if (scruge_savings_amount > 0) {
    add_balance(SCRUGE, scruge_savings, _self, true);
  }

  const auto oracle_time = _stat.begin()->oracle_timestamp;
  static const uint32_t now = time_point_sec(oracle_time).utc_seconds;
  PRINT("now", now)
  PRINT("processing taxes, price", tax.r_price)
  PRINT("collected", collected_to_do_remove_this)
  PRINT("add to pool", collected_to_do_remove_this - scruge_insurance_amount)

  _tax.modify(tax, same_payer, [&](auto& r) {
    r.insurance_pool += asset(r.r_collected - scruge_insurance_amount, REX);
    r.savings_pool += asset(r.e_collected - scruge_savings_amount, BUCK);
    
    r.r_supply += r.r_collected - scruge_insurance_amount;
    r.e_supply += r.e_collected - scruge_savings_amount;
    
    if (r.r_supply > 0) {
      r.r_price += (r.r_collected - scruge_insurance_amount) * PO / r.r_supply;
      r.r_collected = 0;
    }
    
    if (r.e_supply > 0) {
      r.e_price += (r.e_collected - scruge_savings_amount) * PO / r.e_supply;
      r.e_collected = 0;
    }
  });
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
  
  update_supply(accrued_debt);
  
  cdp_itr->p();
  PRINT("tax", cdp_itr->id)
  PRINT("dt", now - last)
  PRINT("d", accrued_debt)
  PRINT("c", accrued_collateral)
  PRINT("new collected", tax.r_collected + accrued_collateral_amount)
  PRINT_("---")

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

void buck::buy_r(const cdp_i::const_iterator& cdp_itr, const asset& added_collateral) {
  const auto& tax = *_tax.begin();
  
  if (cdp_itr->debt.amount > 0 || cdp_itr->acr == 0) return;
  if (added_collateral.amount <= 0) return;
  
  const int64_t new_excess = added_collateral.amount / cdp_itr->acr;
  const uint64_t received_r = ((uint128_t) new_excess * PO) / tax.r_price;
  
  
  check(added_collateral.symbol == REX, "to-do incorrect symbol");
  check(received_r > 0, "to-do remove. this is probably wrong (buy_r)");
  
  const auto oracle_time = _stat.begin()->oracle_timestamp;
  static const uint32_t now = time_point_sec(oracle_time).utc_seconds;
  
  PRINT_("buy r")
  PRINT("excess collateral", new_excess)
  PRINT("received r", received_r)
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.r_balance += received_r;
    r.modified_round = now;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.r_supply += received_r;
  });
}

void buck::sell_r(const cdp_i::const_iterator& cdp_itr) {
  const auto& tax = *_tax.begin();
  
  // to-do check supply not 0
  
  PRINT_("sell r")
   
  const int64_t pool_part = cdp_itr->r_balance * tax.r_price / PO;
  const int64_t received_rex_amount = pool_part * tax.insurance_pool.amount / tax.r_supply;
  const asset received_rex = asset(received_rex_amount, REX);
  
  const auto oracle_time = _stat.begin()->oracle_timestamp;
  static const uint32_t now = time_point_sec(oracle_time).utc_seconds;
  
  if (received_rex_amount > 0) {
    cdp_itr->p();
    PRINT("insurer, added col", received_rex)
    PRINT("r price", tax.r_price)
    PRINT("r supply", tax.r_supply)
    PRINT("pool", tax.insurance_pool)
    PRINT("time", now)
  }
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.r_supply -= cdp_itr->r_balance;
    r.insurance_pool -= received_rex;
  });
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.r_balance = 0;
    r.collateral += received_rex;
    r.modified_round = now;
  });
  
  if (received_rex_amount > 0) {
    cdp_itr->p();
    PRINT_("---\n")
  }
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
  const uint64_t received_e = ((uint128_t) value.amount * PO) / tax.e_price;
  
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
  const uint64_t selling_e = ((uint128_t) value.amount * PO) / tax.e_price;
  check(account_itr->e_balance >= selling_e, "overdrawn savings balance");
  
  // to-do check supply not 0
  
  const int64_t pool_part = selling_e * tax.e_price / PO;
  const int64_t received_bucks_amount = pool_part * tax.savings_pool.amount / tax.e_supply;
  const asset received_bucks = asset(received_bucks_amount, BUCK);
  
  _accounts.modify(account_itr, account, [&](auto& r) {
    r.e_balance -= selling_e;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.e_supply -= selling_e;
    r.savings_pool -= received_bucks;
  });
  
  add_balance(account, received_bucks, same_payer, false);
  run(3);
}