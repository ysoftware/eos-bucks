// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

const bool REX_TESTING = true;

time_point_sec buck::get_maturity() {
  time_point_sec cts{ current_time_point() };
  const uint32_t num_of_maturity_buckets = 5;
  static const uint32_t now = cts.utc_seconds;
  static const uint32_t r   = now % seconds_per_day;
  static const time_point_sec rms{ now - r + num_of_maturity_buckets * seconds_per_day };
  
  // maturity is 5 seconds
  if (REX_TESTING) { return time_point_sec{ now + 5 }; }
  
  return rms;
}

asset buck::get_rex_balance() {
  rex_balance_i table(EOSIO, EOSIO.value);
  auto item = table.find(_self.value);
  if (item == table.end()) {
    return asset(0, REX);
  }
  return item->rex_balance;
}

asset buck::get_eos_rex_balance() {
  rex_fund_i table(EOSIO, EOSIO.value);
  auto item = table.find(_self.value);
  if (item == table.end()) {
    return asset(0, EOS);
  }
  return item->balance;
}

bool buck::is_mature(uint64_t cdp_id) {
  auto item = _maturityreq.find(cdp_id);
  return item == _maturityreq.end() || item->maturity_timestamp < current_time_point();
}

void buck::process() {
  check(_rexprocess.begin() != _rexprocess.end(), "this action is not to be ran manually");
  
  auto& item = *_rexprocess.begin();
  auto& cdp_item = _cdp.get(item.cdp_id);
  
  if (item.current_balance.symbol == REX) {
    // bought rex, determine how much
    
    // get previous balance, subtract current balance
    auto previos_balance = item.current_balance;
    auto current_balance = get_rex_balance();
    
    auto diff = current_balance - previos_balance;
    if (diff.amount != 0) {
      
      // update maturity request
      auto maturity_itr = _maturityreq.find(item.cdp_id);
      _maturityreq.modify(maturity_itr, same_payer, [&](auto& r) {
        r.maturity_timestamp = get_maturity();
      });
      
      // update rex amount  
      _cdp.modify(cdp_item, same_payer, [&](auto& r) {
        r.rex += diff;
      });
      
      _rexprocess.erase(item);
    }
  }
  else if (item.current_balance.symbol == EOS) {
    
    if (item.current_balance.amount > 0) {
      // sold rex, determine for how much
      
      // get previous balance, subtract from current balance
      auto previous_balance = item.current_balance;
      auto current_balance = get_eos_rex_balance();
      
      auto diff = current_balance - previous_balance;
      if (diff.amount != 0) {
      
        _rexprocess.modify(item, same_payer, [&](auto& r) {
          r.current_balance = -diff;
        });
        
        // withdraw
        action(permission_level{ _self, "active"_n },
          EOSIO, "withdraw"_n,
          std::make_tuple(_self, diff)
        ).send();
      }
      
      _rexprocess.modify(item, same_payer, [&](auto& r) {
        r.current_balance = asset(0, EOS);
      });
      
      // run processing again after withdraw
      inline_process();
    }
    else {
      // withdrew money from rex, sending to user
      
      inline_transfer(cdp_item.account, -item.current_balance, "", EOSIO_TOKEN);
      
      auto& request_itr = _reparamreq.get(item.cdp_id);
      if (request_itr.change_debt.amount != 0) {
        // if changing debt, verify ccr
        
      }
      
      _rexprocess.erase(item);
    }
  }
}

void buck::buy_rex(uint64_t cdp_id, asset quantity) {
  
  // store info current rex balance and this cdp
  _rexprocess.emplace(_self, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.current_balance = get_rex_balance();
  });
  
  if (REX_TESTING) { return; }
  
  // deposit
  action(permission_level{ _self, "active"_n },
		EOSIO, "deposit"_n,
		std::make_tuple(_self, quantity)
	).send();
	
  // buy rex
  action(permission_level{ _self, "active"_n },
		EOSIO, "buyrex"_n,
		std::make_tuple(_self, quantity)
	).send();
	
	inline_process();
}

// quantity in EOS for how much of collateral we're about to sell
void buck::sell_rex(uint64_t cdp_id, asset quantity) {
  
  // store info current eos balance in rex pool for this cdp
  _rexprocess.emplace(_self, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.current_balance = get_eos_rex_balance();
  });
 
  auto& cdp_item = _cdp.get(cdp_id);
  
  if (REX_TESTING) { return; }
  
  auto sell_rex_amount = cdp_item.collateral.amount * cdp_item.rex.amount / quantity.amount;
  auto sell_rex = asset(sell_rex_amount, REX);
  
  // sell rex
  action(permission_level{ _self, "active"_n },
		EOSIO, "sellrex"_n,
		std::make_tuple(_self, sell_rex)
	).send();
	
	inline_process();
}