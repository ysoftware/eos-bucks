// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

time_point_sec buck::get_maturity() {
  time_point_sec cts{ current_time_point() };
  const uint32_t num_of_maturity_buckets = 5;
  static const uint32_t now = cts.utc_seconds;
  static const uint32_t r   = now % seconds_per_day;
  static const time_point_sec rms{ now - r + num_of_maturity_buckets * seconds_per_day };
  return rms;
}

bool buck::is_mature(uint64_t cdp_id) {
  cdp_maturity_req_i requests(_self, _self.value);
  auto item = requests.find(cdp_id);
  return item == requests.end() || item->maturity_timestamp < current_time_point();
}

double buck::get_ccr(asset collateral, asset debt) {
  double price = get_eos_price();
  return (double) collateral.amount * price / (double) debt.amount;
}

void buck::process_rex() {
  rex_processing_i info(_self, _self.value);
  if (info.begin() != info.end()) {
    auto& item = *info.begin();
    
    // process
    auto previos_balance = item.current_rex_balance;
    auto current_balance = get_rex_balance();
    auto diff = current_balance - previos_balance;
    if (diff.amount == 0) { return; }
    
    cdp_i positions(_self, _self.value);
    auto& cdp = positions.get(item.cdp_id);
    positions.modify(cdp, same_payer, [&](auto& r) {
      r.rex += current_balance - previos_balance; // to-do what if negative
    });
    
    info.erase(info.begin());
  }
}

void buck::add_balance(name owner, asset value, name ram_payer, bool change_supply) {
  accounts_i accounts(_self, owner.value);
  auto item = accounts.find(value.symbol.code().raw());
  
  if (item == accounts.end()) {
    accounts.emplace(ram_payer, [&](auto& r) {
      r.balance = value;
    });
  }
  else {
    accounts.modify(item, same_payer, [&](auto& r) {
      r.balance += value;
    });
  }
  
  if (change_supply) {
    stats_i stats(_self, _self.value);
    check(stats.begin() != stats.end(), "contract is not yet initiated");
    
    stats.modify(stats.begin(), same_payer, [&](auto& r) {
      r.supply += value;
    });
  }
}

void buck::sub_balance(name owner, asset value, bool change_supply) {
  accounts_i accounts(_self, owner.value);

  const auto& item = accounts.get(value.symbol.code().raw(), "no balance object found");
  check(item.balance.amount >= value.amount, "overdrawn balance");

  accounts.modify(item, owner, [&](auto& r) {
    r.balance -= value;
  });
  
  if (change_supply) {
    stats_i stats(_self, _self.value);
    check(stats.begin() != stats.end(), "contract is not yet initiated");
    
    stats.modify(stats.begin(), same_payer, [&](auto& r) {
      r.supply -= value;
    });
  }
}