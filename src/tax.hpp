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
    r.current_round = now;
    
    r.insurance_pool += tax.collected_insurance - scruge_insurance;
    r.savings_pool += tax.collected_savings - scruge_savings;
    
    r.r_price += (r.r_collected - scruge_insurance_amount) / r.r_supply;
    r.e_price += (r.e_collected - scruge_savings_amount) / r.e_supply;
    
    r.e_collected = 0;
    r.r_collected = 0;
  });
}

// collect interest to insurance pool from this cdp
void buck::accrue_interest(const cdp_i::const_iterator& cdp_itr) {
  const auto& tax = *_tax.begin();
  
  if (cdp_itr->debt.amount == 0) {
    withdraw_insurance_dividends(cdp_itr);
    return;
  }
  
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
    r.collected_savings += accrued_debt;
    r.collected_insurance += accrued_collateral;
  });
  
  // to-do check ccr for liquidation
}

void buck::update_excess_collateral(const asset& value) {
  
  // buy R
  if (value.amount > 0) {
    _tax.modify(_tax.begin(), same_payer, [&](auto& r) {
      r.changed_excess += value;
    });
  }
  
  // sell R
  else {
    
  }
}

/// issue cdp dividends from the insurance pool
void buck::withdraw_insurance_dividends(const cdp_i::const_iterator& cdp_itr) {
  if (cdp_itr->debt.amount != 0) return;
  
  const auto& tax = *_tax.begin();
  
  const int64_t ca = cdp_itr->collateral.amount;
  const int64_t ipa = tax.insurance_pool.amount;
  const int64_t aea = tax.aggregated_excess.amount;
  
  const auto time_now = _stat.begin()->oracle_timestamp;
  static const uint32_t now = time_point_sec(time_now).utc_seconds;
   
  if (now == cdp_itr->modified_round) return;
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.modified_round = now;
  });
  
  if (aea == 0) return;
  
  const int64_t delta_round = tax.current_round - cdp_itr->modified_round;
  const int64_t user_aggregated_amount = (uint128_t) ca * delta_round / BASE_ROUND_DURATION;
  const int64_t dividends_amount = (uint128_t) ipa * user_aggregated_amount / aea;
  
  const auto user_aggregated = asset(user_aggregated_amount, REX);
  const auto dividends = asset(dividends_amount, REX);
  
  add_funds(cdp_itr->account, dividends, same_payer);
  
  PRINT("dividends for", cdp_itr->id)
  PRINT("amount", dividends)
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.insurance_pool -= dividends;
    r.aggregated_excess -= user_aggregated;
  });
}

void buck::save(const name& account, const asset& value) {
  require_auth(account);
  
  accounts_i _accounts(_self, account.value);
  auto account_itr = _accounts.find(BUCK.code().raw());
  
  // buy E
  const auto received_e = value.amount * PO / _tax.begin()->e_price;
  
  _accounts.modify(account_itr, account, [&](auto& r) {
    r.savings += value;
    r.e_balance += received_e;
  });
  
  _tax.modify(_tax.begin(), same_payer, [&](auto& r) {
    r.e_supply += received_e;
  });
  
  sub_balance(account, value, false);
  run(3);
}

void buck::take(const name& account, const asset& value) {
  require_auth(account);
  
  accounts_i _accounts(_self, account.value);
  auto account_itr = _accounts.find(BUCK.code().raw());
  
  check(account_itr->savings >= value, "overdrawn savings balance");

  // sell E
  const auto selling_e = value.amount * account_itr->e_balance / account_itr->savings.amount;
  const auto received_bucks = selling_e * _tax.begin()->e_price;
  
  _accounts.modify(account_itr, account, [&](auto& r) {
    r.savings -= value;
    r.e_balance -= selling_e;
  });
  
  _tax.modify(_tax.begin(), same_payer, [&](auto& r) {
    r.e_supply -= selling_e;
  });
  
  add_balance(account, received_bucks, same_payer, false);
  run(3);
}