// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::process_maturities(const fund_i::const_iterator& fund_itr) {
  const time_point_sec now = current_time_point_sec();
  _fund.modify(fund_itr, same_payer, [&](auto& r) {
    while (!r.rex_maturities.empty() && r.rex_maturities.front().first <= now) {
      r.matured_rex += r.rex_maturities.front().second;
      r.rex_maturities.pop_front();
    }
  });
}

time_point_sec buck::get_amount_maturity(const name& account, const asset& quantity) const {
  const time_point_sec now = current_time_point_sec();
  auto fund_itr = _fund.require_find(account.value, "to-do should not happen? get_amount_maturity");
  int64_t i = 0;
  for (auto maturity: fund_itr->rex_maturities) {
    i += maturity.second;
    if (i >= quantity.amount) {
      return maturity.first;
    }
  }
  return time_point_sec(0);
}

time_point_sec buck::get_maturity() const {
  const uint32_t num_of_maturity_buckets = 5;
  static const uint32_t now = current_time_point_sec().utc_seconds;
  static const uint32_t r   = now % seconds_per_day;
  static const time_point_sec rms{ now - r + num_of_maturity_buckets * seconds_per_day };
  return rms;
}

int64_t buck::convert_to_rex_usd(int64_t quantity) const {
  static const int64_t EU = get_eos_usd_price();
  
  rex_pool_i _pool(REX_ACCOUNT, REX_ACCOUNT.value);
  const auto pool_itr = _pool.begin();
  if (pool_itr == _pool.end()) { return quantity * EU; } // test net case (1 rex = 1 eos)
  
  static const int64_t S0 = pool_itr->total_lendable.amount;
  static const int64_t R0 = pool_itr->total_rex.amount;
  const int64_t p  = (uint128_t(quantity) * S0) / R0 / EU;
  return p;
}

int64_t buck::convert_to_usd_rex(int64_t quantity, int64_t tax) const {
  static const int64_t EU = get_eos_usd_price() + tax;
  
  rex_pool_i _pool(REX_ACCOUNT, REX_ACCOUNT.value);
  const auto pool_itr = _pool.begin();
  if (pool_itr == _pool.end()) { return quantity / EU; } // test net case (1 rex = 1 eos)
  
  static const int64_t S0 = pool_itr->total_lendable.amount;
  static const int64_t R0 = pool_itr->total_rex.amount;
  const int64_t p  = (uint128_t(quantity) * R0 * EU) / S0;
  return p;
}

asset buck::get_rex_balance() const {
  rex_balance_i _balance(REX_ACCOUNT, REX_ACCOUNT.value);
  const auto balance_itr = _balance.find(_self.value);
  if (balance_itr == _balance.end()) return ZERO_REX;
  return balance_itr->rex_balance;
}

asset buck::get_eos_rex_balance() const {
  rex_fund_i _balance(REX_ACCOUNT, REX_ACCOUNT.value);
  const auto balance_itr = _balance.find(_self.value);
  if (balance_itr == _balance.end()) return asset(0, EOS);
  return balance_itr->balance;
}

bool buck::is_mature(uint64_t cdp_id) const {
  const auto maturity_itr = _maturityreq.find(cdp_id);
  return maturity_itr == _maturityreq.end() || maturity_itr->maturity_timestamp < get_current_time_point();
}

void buck::processrex() {
  check(_process.begin() != _process.end(), "this action is not to be executed by a user");
  const auto rexprocess_itr = _process.begin();
  
  // user bought rex. send it to the funds
  if (rexprocess_itr->current_balance.symbol == REX) {
    const auto previos_balance = rexprocess_itr->current_balance;
    const auto current_balance = get_rex_balance();
    const auto diff = current_balance - previos_balance; // REX
    add_funds(rexprocess_itr->account, diff, _self);
    
    const time_point_sec maturity = get_maturity();
    auto fund_itr = _fund.require_find(rexprocess_itr->account.value, "to-do should not happen? processrex");
    _fund.modify(fund_itr, same_payer, [&](auto& r) {
        if (!r.rex_maturities.empty() && r.rex_maturities.back().first == maturity) {
          r.rex_maturities.back().second += diff.amount;
        } 
        else {
          r.rex_maturities.emplace_back(maturity, diff.amount);
        }
    });
    
    process_maturities(fund_itr);
  }
  
  // user sold rex. transfer eos
  else {
    const auto previos_balance = rexprocess_itr->current_balance;
    const auto current_balance = get_eos_rex_balance();
    const auto diff = current_balance - previos_balance; // EOS
    inline_transfer(rexprocess_itr->account, diff, "buck: withdraw eos (+ rex dividends)", EOSIO_TOKEN);
  }
  
  _process.erase(_process.begin());
}

void buck::buy_rex(const name& account, const asset& quantity) {
  
  // store info current rex balance and this cdp
  _process.emplace(_self, [&](auto& r) {
    r.current_balance = get_rex_balance();
    r.account = account;
  });
  
  // deposit
  action(permission_level{ _self, "active"_n },
		REX_ACCOUNT, "deposit"_n,
		std::make_tuple(_self, quantity)
	).send();
	
  // buy rex
  action(permission_level{ _self, "active"_n },
		REX_ACCOUNT, "buyrex"_n,
		std::make_tuple(_self, quantity)
	).send();
	
	action(permission_level{ _self, "active"_n }, 
  	_self, "processrex"_n, 
  	std::make_tuple()
	).send();
}

void buck::sell_rex(const name& account, const asset& quantity) {
  
  // store info current eos balance in rex pool for this cdp
  _process.emplace(_self, [&](auto& r) {
    r.current_balance = get_eos_rex_balance();
    r.account = account;
  });
  
  // sell rex
  action(permission_level{ _self, "active"_n },
		REX_ACCOUNT, "sellrex"_n,
		std::make_tuple(_self, quantity)
  ).send();
	
  action(permission_level{ _self, "active"_n }, 
    _self, "processrex"_n, 
    std::make_tuple()
  ).send();
}