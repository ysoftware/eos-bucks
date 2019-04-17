// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::distribute_tax(const cdp_i::const_iterator& cdp_itr) {
  const auto& stats = *_stat.begin();
  
  const uint64_t delta_round = stats.current_round - cdp_itr->modified_round;
  const double round_weight = (double) delta_round / (double) ROUND_DURATION;
  const uint64_t user_aggregated_amount = floor((double) cdp_itr->collateral.amount * round_weight);
  const double user_part = user_aggregated_amount / (double) stats.aggregated_collateral.amount;
  const uint64_t dividends_amount = floor((double) stats.tax_pool.amount * user_part);
  
  const auto user_aggregated = asset(user_aggregated_amount, EOS);
  const auto dividends = asset(dividends_amount, BUCK);
  
  PRINT("tax_pool", stats.tax_pool)
  PRINT("collateral", cdp_itr->collateral)
  PRINT("delta_round", delta_round)
  PRINT("aggregated_collateral", stats.aggregated_collateral)
  PRINT("part", user_part)
  PRINT("dividends", dividends)
  
  add_balance(cdp_itr->account, dividends, same_payer, true);
  
  _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
    r.modified_round = stats.current_round;
  });
  
  _stat.modify(stats, same_payer, [&](auto& r) {
    r.tax_pool -= dividends;
    r.aggregated_collateral -= user_aggregated;
  });
  
  // inline_received(_self, cdp_itr->account, dividends, "dividends");
}

void buck::process_taxes() {
  const auto& stats = *_stat.begin();
  
  // send part of collected taxes to Scruge
  const uint64_t scruge_amount = stats.collected_taxes.amount * SP;
  const auto scruge_taxes_part = asset(scruge_amount, BUCK);
  if (scruge_amount > 0) {
    add_balance(SCRUGE, scruge_taxes_part, _self, true);
  }
  
  time_point_sec cts{ current_time_point() };
  static const uint32_t now = cts.utc_seconds;
  
  const uint64_t aggregate_amount = stats.total_collateral.amount * (now - stats.current_round) / ROUND_DURATION;
  const auto add_aggregated = asset(aggregate_amount, EOS);
  
  PRINT("time diff", now - stats.current_round)
  PRINT("round diff", (now - stats.current_round)/ROUND_DURATION)
  PRINT("total_collateral", stats.total_collateral)
  PRINT("add_aggregated", add_aggregated)
  
  // update values
  _stat.modify(stats, same_payer, [&](auto& r) {
    r.current_round = now;
    r.aggregated_collateral += add_aggregated;
    r.tax_pool += stats.collected_taxes - scruge_taxes_part;
    r.collected_taxes = ZERO_BUCK;
  });
}

void buck::update_collateral(const asset& value) {
  _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
    r.total_collateral += value;
  });
}

void buck::pay_tax(const asset& value) {
  _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
    r.collected_taxes += value;
  });
}