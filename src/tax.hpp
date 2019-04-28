// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::process_taxes() {
  const auto& tax = *_tax.begin();
  
  // send part of collected insurance to Scruge
  const uint64_t scruge_insurance_amount = tax.collected_insurance.amount * SP / 100;
  const auto scruge_insurance = asset(scruge_insurance_amount, EOS);
  if (scruge_insurance_amount > 0) {
    add_funds(SCRUGE, scruge_insurance, _self);
  }
  
   // send part of collected savings to Scruge
  const uint64_t scruge_savings_amount = tax.collected_savings.amount * SP / 100;
  const auto scruge_savings = asset(scruge_savings_amount, BUCK);
  if (scruge_savings_amount > 0) {
    add_balance(SCRUGE, scruge_savings, _self, true);
  }
  
  time_point_sec cts{ current_time_point() };
  static const uint32_t now = cts.utc_seconds;
  const auto delta_round = now - tax.current_round;
  
  // update excess collateral
  const auto new_total_excess = tax.total_excess + tax.changed_excess;
  const uint64_t aggregate_excess_amount = new_total_excess.amount * delta_round / BASE_ROUND_DURATION;
  const auto aggregate_excess = asset(aggregate_excess_amount, EOS);
  
  // update savings
  const auto new_total_bucks = tax.total_bucks + tax.changed_bucks;
  const uint64_t aggregate_bucks_amount = new_total_bucks.amount * delta_round / BASE_ROUND_DURATION;
  const auto aggregate_bucks = asset(aggregate_bucks_amount, BUCK);
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.current_round = now;
    
    r.insurance_pool += tax.collected_insurance - scruge_insurance;
    r.savings_pool += tax.collected_savings - scruge_savings;
    
    r.collected_insurance = ZERO_EOS;
    r.collected_savings = ZERO_BUCK;
    
    r.aggregated_excess += aggregate_excess;
    r.aggregated_bucks += aggregate_bucks;
    
    r.total_excess = new_total_excess;
    r.total_bucks = new_total_bucks;
    
    r.changed_excess = ZERO_EOS;
    r.changed_bucks = ZERO_BUCK;
  });
}

// add interest to savings pool
void buck::add_savings_pool(const asset& value) {
  if (value.amount <= 0) { return; }
  _tax.modify(_tax.begin(), same_payer, [&](auto& r) {
    r.collected_savings += value;
  });
}

// collect interest to insurance pool from this cdp
void buck::accrue_interest(const cdp_i::const_iterator& cdp_itr) {
  PRINT_("accr")
  const auto& tax = *_tax.begin();
  const auto price = get_eos_price();
  
  if (price == 0) return;
  
  const auto time_now = current_time_point();
  static const uint32_t now = time_point_sec(time_now).utc_seconds;
  const uint32_t last = time_point_sec(cdp_itr->accrued_timestamp).utc_seconds;
  
  static const uint128_t DM = 1000000000000;
  const uint128_t v = (exp(double(AR) / 100 * double(now - last) / double(YEAR)) - 1) * DM;
  const int64_t accrued_amount = cdp_itr->debt.amount * v / DM;
  
  const int64_t accrued_collateral_amount = accrued_amount * IR / price;
  const int64_t accrued_debt_amount = accrued_amount * SR / 100;
  
  const asset accrued_collateral = asset(accrued_collateral_amount, EOS);
  const asset accrued_debt = asset(accrued_debt_amount, BUCK);
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.collateral -= accrued_collateral;
    r.accrued_debt += accrued_debt;
    r.accrued_timestamp = time_now;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.collected_insurance += accrued_collateral;
  });
  
  // to-do check ccr for liquidation
}

/// issue cdp dividends from the insurance pool
void buck::withdraw_insurance_dividends(const cdp_i::const_iterator& cdp_itr) {
  PRINT_("with")
  if (cdp_itr->debt.amount != 0) return;
  
  const auto& tax = *_tax.begin();
  
  const int64_t ca = cdp_itr->collateral.amount;
  const int64_t ipa = tax.insurance_pool.amount;
  const int64_t aea = tax.aggregated_excess.amount;
  
  if (aea == 0) {
    _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
      r.modified_round = tax.current_round;
    });
    return;
  }
  
  accrue_interest(cdp_itr);
  
  const int64_t delta_round = tax.current_round - cdp_itr->modified_round;
  const int64_t user_aggregated_amount = (uint128_t) ca * delta_round / BASE_ROUND_DURATION;
  const int64_t dividends_amount = (uint128_t) ipa * user_aggregated_amount / aea;
  
  // don't update modified_round if dividends calculated is 0
  if (dividends_amount == 0) return;
  
  const auto user_aggregated = asset(user_aggregated_amount, EOS);
  const auto dividends = asset(dividends_amount, EOS);
  
  add_funds(cdp_itr->account, dividends, same_payer);
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.modified_round = tax.current_round;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.insurance_pool -= dividends;
    r.aggregated_excess -= user_aggregated;
  });
}

asset buck::withdraw_savings_dividends(const name& account) {
  const auto& tax = *_tax.begin();
  
  accounts_i _accounts(_self, account.value);
  auto account_itr = _accounts.find(BUCK.code().raw());
  check (account_itr != _accounts.end(), "no balance object found");
  
  const int64_t sa =  account_itr->savings.amount;
  const int64_t spa = tax.savings_pool.amount;
  const int64_t aba = tax.aggregated_bucks.amount;
  
  if (aba == 0) {
    _accounts.modify(account_itr, same_payer, [&](auto& r) {
      r.withdrawn_round = tax.current_round;
    });
    return ZERO_BUCK;
  }
  
  const int64_t delta_round = tax.current_round - account_itr->withdrawn_round;
  const int64_t user_aggregated_amount = (uint128_t) sa * delta_round / BASE_ROUND_DURATION;
  const int64_t dividends_amount = (uint128_t) spa * user_aggregated_amount / aba;
  
  const auto user_aggregated = asset(user_aggregated_amount, BUCK);
  const auto dividends = asset(dividends_amount, BUCK);
  
  _accounts.modify(account_itr, same_payer, [&](auto& r) {
    r.withdrawn_round = tax.current_round;
  });

  _tax.modify(tax, same_payer, [&](auto& r) {
    r.savings_pool -= dividends;
    r.aggregated_bucks -= user_aggregated;
  });
  
  return dividends;
}

void buck::save(const name& account, const asset& value) {
  require_auth(account);
  
  const auto dividends = withdraw_savings_dividends(account);
  accounts_i _accounts(_self, account.value);
  auto account_itr = _accounts.find(BUCK.code().raw());
  
  _accounts.modify(account_itr, account, [&](auto& r) {
    r.savings += value;
  });
  
  _tax.modify(_tax.begin(), same_payer, [&](auto& r) {
    r.changed_bucks += value;
  });
  
  sub_balance(account, value - dividends, false);
  run(3);
}

void buck::take(const name& account, const asset& value) {
  require_auth(account);
  
  const auto dividends = withdraw_savings_dividends(account);
  accounts_i _accounts(_self, account.value);
  auto account_itr = _accounts.find(BUCK.code().raw());
  
  check(account_itr->savings >= value, "overdrawn savings balance");

  _accounts.modify(account_itr, account, [&](auto& r) {
    r.savings -= value;
  });
  
  _tax.modify(_tax.begin(), same_payer, [&](auto& r) {
    r.changed_bucks -= value;
  });
  
  add_balance(account, value + dividends, same_payer, false);
  run(3);
}

void buck::update_excess_collateral(const asset& value) {
  _tax.modify(_tax.begin(), same_payer, [&](auto& r) {
    r.changed_excess += value;
  });
}