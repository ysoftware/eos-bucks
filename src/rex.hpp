// Copyright © Scruge 2019.
// This file is part of BUCK Protocol.
// Created by Yaroslav Erohin and Dmitry Morozov.

void buck::process_maturities(const fund_i::const_iterator& fund_itr) {
  const time_point_sec now = current_time_point_sec();
  _fund.modify(fund_itr, same_payer, [&](auto& r) {
    while (!r.rex_maturities.empty() && r.rex_maturities.front().first <= now) {
      r.matured_rex += r.rex_maturities.front().second;
      r.rex_maturities.pop_front();
    }
  });
}


time_point_sec buck::get_maturity() const {
  const uint32_t num_of_maturity_buckets = 5;
  static const uint32_t now = current_time_point_sec().utc_seconds;
  static const uint32_t r   = now % seconds_per_day;
  static const time_point_sec rms{ now - r + num_of_maturity_buckets * seconds_per_day };
  return rms;
}

/// rex -> eos -> buck
int64_t buck::to_buck(int64_t quantity) const {
  static const int64_t EU = get_eos_usd_price();
  return convert(uint128_t(quantity) * EU, false);
}

/// buck -> eos -> rex
int64_t buck::to_rex(int64_t quantity, int64_t tax) const {
  static const int64_t EU = get_eos_usd_price();
  return convert(quantity, true) / (EU * (100 + tax) / 100);
}

/// eos -> rex (true)
/// rex -> eos (false)
int64_t buck::convert(uint128_t quantity, bool to_rex) const {
  rex_pool_i _pool(REX_ACCOUNT, REX_ACCOUNT.value);
  const auto pool_itr = _pool.begin();
  if (pool_itr == _pool.end()) return quantity; // test net case (1 rex = 1 eos)
  
  static const int64_t R0 = pool_itr->total_rex.amount;
  static const int64_t S0 = pool_itr->total_lendable.amount;
  
  if (to_rex) return quantity * R0 / S0;
  else return quantity * S0 / R0;
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
  if (balance_itr == _balance.end()) return ZERO_EOS;
  return balance_itr->balance;
}

void buck::processrex() {
  require_auth(_self);
  check(_process.begin() != _process.end(), "this action is not to be executed by a user");
  const auto rexprocess_itr = _process.begin();
  
  // user bought rex. send it to the funds
  if (rexprocess_itr->current_balance.symbol == REX) {
    const auto previos_balance = rexprocess_itr->current_balance;
    const auto current_balance = get_rex_balance();
    const auto diff = current_balance - previos_balance; // REX
    add_funds(rexprocess_itr->account, diff, same_payer, get_maturity());
  }
  
  // user sold rex. transfer eos
  else {
    const auto previos_balance = rexprocess_itr->current_balance;
    const auto current_balance = get_eos_rex_balance();
    const auto diff = current_balance - previos_balance; // EOS
      
    // withdraw
    action(permission_level{ _self, "active"_n },
    	REX_ACCOUNT, "withdraw"_n,
    	std::make_tuple(_self, current_balance)
    ).send();
	
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