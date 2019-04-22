// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

/// issue cdp dividends from the insurance pool
void buck::withdraw_insurance(const cdp_i::const_iterator& cdp_itr) {
  if (cdp_itr->debt.amount != 0) { return; }
  const auto& tax = *_tax.begin();
  
  const uint64_t delta_round = tax.current_round - cdp_itr->modified_round;
  const double round_weight = (double) delta_round / (double) BASE_ROUND_DURATION;
  const uint64_t user_aggregated_amount = round((double) cdp_itr->collateral.amount * round_weight);
  const double user_part = user_aggregated_amount / (double) tax.aggregated_excess.amount;
  const uint64_t dividends_amount = round((double) tax.insurance_pool.amount * user_part);
  
  // don't update modified_round if dividends calculated is 0
  if (dividends_amount == 0) { return; }
  
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

void buck::withdraw_savings(const name& account) {
  const auto& tax = *_tax.begin();
  
  accounts_i _accounts(_self, account.value);
  auto account_itr = _accounts.find(BUCK.code().raw());
  if (account_itr == _accounts.end()) { return; }
  
  const uint64_t delta_round = tax.current_round - account_itr->withdrawn_round;
  const double round_weight = (double) delta_round / (double) BASE_ROUND_DURATION;
  const uint64_t user_aggregated_amount = round((double) account_itr->balance.amount * round_weight);
  const double user_part = user_aggregated_amount / (double) tax.aggregated_bucks.amount;
  const uint64_t dividends_amount = round((double) tax.insurance_pool.amount * user_part);
  
  // don't update modified_round if dividends calculated is 0
  if (dividends_amount == 0) { return; }
  
  const auto user_aggregated = asset(user_aggregated_amount, BUCK);
  const auto dividends = asset(dividends_amount, BUCK);
  
  add_balance(account, dividends, same_payer, true);
  
  _accounts.modify(account_itr, same_payer, [&](auto& r) {
    r.withdrawn_round = tax.current_round;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.savings_pool -= dividends;
    r.aggregated_bucks -= user_aggregated;
  });
}

void buck::process_taxes() {
  const auto& tax = *_tax.begin();
  
  // send part of collected insurance to Scruge
  const uint64_t scruge_insurance_amount = round(tax.collected_insurance.amount * SP);
  const auto scruge_insurance = asset(scruge_insurance_amount, EOS);
  if (scruge_insurance_amount > 0) {
    add_funds(SCRUGE, scruge_insurance, _self);
  }
  
   // send part of collected savings to Scruge
  const uint64_t scruge_savings_amount = round(tax.collected_savings.amount * SP);
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

void buck::accrue_interest(const cdp_i::const_iterator& cdp_itr) {
  const auto& tax = *_tax.begin();
  const auto price = get_eos_price();
  
  const auto time_now = current_time_point();
  static const uint32_t now = time_point_sec(time_now).utc_seconds;
  const uint32_t last = time_point_sec(cdp_itr->accrued_timestamp).utc_seconds;
  
  const double years_held = (double) (now - last) / (double) YEAR;
  const double accrued_amount = (double) cdp_itr->debt.amount * (exp(AR * years_held) - 1);
  const uint64_t accrued_collateral_amount = round(accrued_amount * IR / price);
  const uint64_t accrued_debt_amount = round(accrued_amount * SR);
  
  const asset accrued_collateral = asset(accrued_collateral_amount, EOS);
  const asset accrued_debt = asset(accrued_debt_amount, BUCK);
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.collateral -= accrued_collateral;
    r.accrued_debt += accrued_debt;
    r.accrued_timestamp = time_now;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.collected_insurance += accrued_collateral;
    // r.collected_savings += accrued_debt; // goes to pool only after redemption/reparametrization
  });
  
  // to-do check ccr for liquidation
}

void buck::add_savings(const asset& value) {
  check(value.amount > 0, "added savings should be positive");
  _tax.modify(_tax.begin(), same_payer, [&](auto& r) {
    r.collected_savings += value;
  });
}

void buck::update_excess_collateral(const asset& value) {
  _tax.modify(_tax.begin(), same_payer, [&](auto& r) {
    r.changed_excess += value;
  });
}

void buck::update_bucks_supply(const asset& value) {
  _tax.modify(_tax.begin(), same_payer, [&](auto& r) {
    r.changed_bucks += value;
  });
}