// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

const bool REX_TESTING = true;

name REX_ACCOUNT() {
  if (REX_TESTING) { return "rexrexrexrex"_n; }
  return EOSIO;
}

time_point_sec buck::get_maturity() {
  time_point_sec cts{ current_time_point() };
  const uint32_t num_of_maturity_buckets = 5;
  static const uint32_t now = cts.utc_seconds;
  static const uint32_t r   = now % seconds_per_day;
  static const time_point_sec rms{ now - r + num_of_maturity_buckets * seconds_per_day };
  
  // maturity is 1 second
  if (REX_TESTING) { return time_point_sec{ now + 1 }; }
  
  return rms;
}

asset buck::get_rex_balance() {
  rex_balance_i table(REX_ACCOUNT(), REX_ACCOUNT().value);
  auto item = table.find(_self.value);
  if (item == table.end()) {
    return asset(0, REX);
  }
  return item->rex_balance;
}

asset buck::get_eos_rex_balance() {
  rex_fund_i table(REX_ACCOUNT(), REX_ACCOUNT().value);
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
    
    // update maturity request
    auto& maturity_item = _maturityreq.get(item.cdp_id, "to-do: remove. did not find maturity");
    _maturityreq.modify(maturity_item, same_payer, [&](auto& r) {
      r.maturity_timestamp = get_maturity();
    });
    
    // update rex amount
    _cdp.modify(cdp_item, same_payer, [&](auto& r) {
      r.rex += diff;
    });
    
    _rexprocess.erase(item);
  }
  else if (item.current_balance.symbol == EOS) {
    
    if (item.current_balance.amount > 0) {
      // sold rex, determine for how much
      
      // get previous balance, subtract from current balance
      auto previous_balance = item.current_balance;
      auto current_balance = get_eos_rex_balance();
      auto diff = current_balance - previous_balance;
      
      PRINT("sold rex for ", -diff)
      _rexprocess.modify(item, same_payer, [&](auto& r) {
        r.current_balance = -diff;
      });
      
      action(permission_level{ _self, "active"_n },
        REX_ACCOUNT(), "withdraw"_n,
        std::make_tuple(_self, diff)
      ).send();
      
      // run processing again after withdraw
      inline_process();
    }
    else {
      // withdrew money from rex, sending to user
      
      if (item.current_balance.amount != 0) {
        inline_transfer(cdp_item.account, -item.current_balance, "", EOSIO_TOKEN);
      }
  
      auto& request_item = _reparamreq.get(item.cdp_id, "to-do: remove. no request for this rex process item");
      asset new_collateral = cdp_item.collateral + request_item.change_collateral;
      asset change_debt = asset(0, BUCK);
      asset new_debt = cdp_item.debt + change_debt;
      
      if (request_item.change_debt.amount > 0) {
        
        // to-do check this
        
        // to-do use updated collateral value or the old one?
        double ccr = get_ccr(new_collateral, new_debt);
        double ccr_cr = ((ccr / CR) - 1) * (double) cdp_item.debt.amount;
        double di = (double) request_item.change_debt.amount;
        uint64_t change_amount = ceil(fmin(ccr_cr, di));
        
        // take fee
        auto fee_amount = change_amount * IF;
        auto fee = asset(fee_amount, BUCK);
        add_fee(fee);
        
        auto change = asset(change_amount - fee_amount, BUCK);
        auto change_debt = change;
        
        add_balance(cdp_item.account, change, same_payer, true);
      }
      
      // removing debt
      else if (request_item.change_debt.amount < 0) {
        change_debt = request_item.change_debt; // add negative value
      }
      
      _cdp.modify(cdp_item, same_payer, [&](auto& r) {
        r.collateral = new_collateral;
        r.debt += change_debt;
      });
  
      _reparamreq.erase(request_item);
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
  
  // deposit
  action(permission_level{ _self, "active"_n },
		REX_ACCOUNT(), "deposit"_n,
		std::make_tuple(_self, quantity)
	).send();
	
  // buy rex
  action(permission_level{ _self, "active"_n },
		REX_ACCOUNT(), "buyrex"_n,
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
  auto sell_rex_amount = cdp_item.collateral.amount * cdp_item.rex.amount / quantity.amount;
  auto sell_rex = asset(sell_rex_amount, REX);
  
  _cdp.modify(cdp_item, same_payer, [&](auto& r) {
    r.rex =- sell_rex;
  });

  // sell rex
  action(permission_level{ _self, "active"_n },
		REX_ACCOUNT(), "sellrex"_n,
		std::make_tuple(_self, sell_rex)
	).send();
  
	inline_process();
}