// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

/// issue cdp dividends from the insurance pool
void buck::withdraw_insurance(const cdp_i::const_iterator& cdp_itr) {
  const auto& tax = *_tax.begin();
  
  if (cdp_itr->debt.amount != 0) { return; }
  
  const uint64_t delta_round = tax.current_round - cdp_itr->modified_round;
  const double round_weight = (double) delta_round / (double) BASE_ROUND_DURATION;
  const uint64_t user_aggregated_amount = floor((double) cdp_itr->collateral.amount * round_weight);
  const double user_part = user_aggregated_amount / (double) tax.aggregated_excess.amount;
  const uint64_t dividends_amount = floor((double) tax.insurance_pool.amount * user_part);
  
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
  const uint64_t user_aggregated_amount = floor((double) account_itr->balance.amount * round_weight);
  const double user_part = user_aggregated_amount / (double) tax.aggregated_excess.amount;
  const uint64_t dividends_amount = floor((double) tax.insurance_pool.amount * user_part);
  
  const auto user_aggregated = asset(user_aggregated_amount, EOS);
  const auto dividends = asset(dividends_amount, BUCK);
  
  add_balance(account, dividends, same_payer, true);
  
  _accounts.modify(account_itr, same_payer, [&](auto& r) {
    r.withdrawn_round = tax.current_round;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.savings_pool -= dividends;
    r.aggregated_savings -= user_aggregated;
  });
}

void buck::process_taxes() {
  const auto& tax = *_tax.begin();
  
  // send part of collected insurance (EOS) to Scruge
  const uint64_t scruge_insurance_amount = round(tax.collected_excess.amount * SP);
  const auto scruge_insurance = asset(scruge_insurance_amount, EOS);
  if (scruge_insurance_amount > 0) {
    add_balance(SCRUGE, scruge_insurance, _self, true);
  }
  
   // send part of collected savings (BUCK) to Scruge
  const uint64_t scruge_savings_amount = round(tax.collected_savings.amount * SP);
  const auto scruge_savings = asset(scruge_savings_amount, BUCK);
  if (scruge_savings_amount > 0) {
    add_funds(SCRUGE, scruge_savings, same_payer);
  }
  
  time_point_sec cts{ current_time_point() };
  static const uint32_t now = cts.utc_seconds;
  
  // update excess collateral
  const uint64_t add_excess_amount = tax.total_excess.amount * (now - tax.current_round) / BASE_ROUND_DURATION;
  const auto add_excess = asset(add_excess_amount, EOS);
  
  // update savings
  const uint64_t add_savings_amount = tax.total_savings.amount * (now - tax.current_round) / BASE_ROUND_DURATION;
  const auto add_savings = asset(add_savings_amount, BUCK);
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.current_round = now;
    
    r.insurance_pool = tax.collected_excess - scruge_insurance;
    r.collected_excess = ZERO_EOS;
    r.aggregated_excess += add_excess;
    
    r.savings_pool = tax.collected_savings - scruge_savings;
    r.collected_savings = ZERO_BUCK;
    r.aggregated_savings += add_savings;
  });
}

void buck::accrue_interest(const cdp_i::const_iterator& cdp_itr) {
  const auto& tax = *_tax.begin();
  const auto price = get_eos_price();
  
  static const uint32_t now = time_point_sec(current_time_point()).utc_seconds;
  const uint32_t last = time_point_sec(cdp_itr->accrued_timestamp).utc_seconds;
  
  // to-do do rounding
  
  const uint32_t time_delta = now - last;
  const double years_held = (double) time_delta / (double) YEAR;
  const double accrued_amount = (double) cdp_itr->debt.amount * exp(AR * time_delta);
  const uint64_t accrued_collateral_amount = ceil(accrued_amount * IR / price);
  const uint64_t accrued_debt_amount = ceil(accrued_amount * SR);
  
  const asset accrued_collateral = asset(accrued_collateral_amount, BUCK);
  const asset accrued_debt = asset(accrued_debt_amount, BUCK);
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.collateral -= accrued_collateral;
    r.accrued_debt += accrued_debt;
  });
  
  _tax.modify(tax, same_payer, [&](auto& r) {
    r.collected_savings -= accrued_collateral;
    r.collected_excess += accrued_debt;
  });
  
  // to-do check ccr for liquidation
}

void buck::update_excess_collateral(const asset& value) {
  _tax.modify(_tax.begin(), same_payer, [&](auto& r) {
    r.collected_excess += value;
  });
}

void buck::pay_tax(const asset& value) {
  _tax.modify(_tax.begin(), same_payer, [&](auto& r) {
    
  });
}