// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::distribute_tax(const cdp_i::const_iterator& cdp_itr) {
  const auto& stats = *_stat.begin();
  
  const auto tax_pool_amount = stats.tax_pool.amount;
  const auto collateral_amount = cdp_itr->collateral.amount;
  const auto delta_round = cdp_itr->modified_round - stats.current_round;
  const auto dividends_amount = tax_pool_amount * collateral_amount * delta_round / stats.aggregated_collateral.amount;
  const auto dividends = asset(dividends_amount, BUCK);
  
  add_balance(cdp_itr->account, dividends, same_payer, true);
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.modified_round = stats.current_round;
  });
  
  inline_received(_self, cdp_itr->account, dividends, "dividends");
}

void buck::process_taxes() {
  const auto& stats = *_stat.begin();
  
  // send part of collected taxes to Scruge
  const auto scruge_amount = stats.collected_collateral.amount * SP;
  const auto scruge_taxes_part = asset(scruge_amount, BUCK);
  add_balance(SCRUGE, scruge_taxes_part, _self, true);
  
  time_point_sec cts{ current_time_point() };
  static const uint32_t now = cts.utc_seconds;
  
  // update values
  _stat.modify(stats, same_payer, [&](auto& r) {
    r.current_round = now;
    r.aggregated_collateral += stats.collected_collateral - scruge_taxes_part;
    r.collected_collateral = ZERO_EOS;
  });
}

void buck::update_collateral(const asset& value) {
  _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
    r.collected_collateral += value;
  });
}

void buck::pay_tax(const asset& value) {
  _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
    r.tax_pool += value;
  });
}