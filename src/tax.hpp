// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::process_taxes() {
  const auto& tax = *_tax.begin();
  
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
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.insurance_pool += asset(r.r_collected - scruge_insurance_amount, REX);
    r.savings_pool += asset(r.e_collected - scruge_savings_amount, BUCK);
    
    r.r_price += (r.r_collected - scruge_insurance_amount) / r.r_supply;
    r.e_price += (r.e_collected - scruge_savings_amount) / r.e_supply;
    
    r.e_collected = 0;
    r.r_collected = 0;
  });
}

// collect interest to insurance pool from this cdp
void buck::accrue_interest(const cdp_i::const_iterator& cdp_itr) {
  const auto& tax = *_tax.begin();
  
  // to-do handle the case when debt is 0
  
  const auto time_now = _stat.begin()->oracle_timestamp;
  static const uint32_t now = time_point_sec(time_now).utc_seconds;
  const uint32_t last = cdp_itr->modified_round;
  
  if (now == last) return;
  
  static const uint128_t DM = 1000000000000;
  const uint128_t v = (exp(AR * double(now - last) / double(YEAR)) - 1) * DM;
  const int64_t accrued_amount = cdp_itr->debt.amount * v / DM;
  
  const int64_t accrued_debt_amount = accrued_amount * SR / 100;
  const int64_t accrued_collateral_amount = convert_to_usd_rex(accrued_amount * IR, 0);
  
  const asset accrued_debt = asset(accrued_debt_amount, BUCK);
  const asset accrued_collateral = asset(accrued_collateral_amount, REX);
  
  update_supply(accrued_debt);
  
  // if (accrued_debt.amount > 0) {
  //   PRINT("tax", cdp_itr->id)
  //   PRINT("dt", now - last)
  //   PRINT("interest", accrued_amount)
  //   PRINT("d", accrued_debt)
  //   PRINT("c", accrued_collateral)
  //   PRINT_("---")
  // }
  
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
  
  check(received_r > 0, "to-do remove. this is probably wrong (buy_r)");
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.r_balance += received_r;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.r_supply += received_r;
  });
}

void buck::sell_r(const cdp_i::const_iterator& cdp_itr) {
  const auto& tax = *_tax.begin();
  
  const int64_t received_rex_amount = cdp_itr->r_balance * tax.r_price;
  const asset received_rex = asset(received_rex_amount, REX);
  
  if (received_rex_amount == 0) return;
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.r_balance = 0;
    r.collateral += received_rex;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.r_supply -= cdp_itr->r_balance;
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
  const uint64_t received_e = ((uint128_t) value.amount * PO) / tax.e_price;
  
  check(received_e > 0, "to-do remove. this is probably wrong (save)");
  
  _accounts.modify(account_itr, account, [&](auto& r) {
    r.savings += value;
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
  
  check(account_itr->savings >= value, "overdrawn savings balance");

  // sell E
  const uint64_t selling_e = ((uint128_t) value.amount * account_itr->e_balance) / account_itr->savings.amount;
  const int64_t received_bucks_amount = selling_e * tax.e_price;
  const asset received_bucks = asset(received_bucks_amount, BUCK);
  
  _accounts.modify(account_itr, account, [&](auto& r) {
    r.savings -= value;
    r.e_balance -= selling_e;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.e_supply -= selling_e;
  });
  
  add_balance(account, received_bucks, same_payer, false);
  run(3);
}