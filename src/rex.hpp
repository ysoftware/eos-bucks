// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

name REX_ACCOUNT() {
  if (REX_TESTING) { return "rexrexrexrex"_n; }
  return EOSIO;
}

time_point_sec buck::get_maturity() const {
  time_point_sec cts{ current_time_point() };
  const uint32_t num_of_maturity_buckets = 5;
  static const uint32_t now = cts.utc_seconds;
  static const uint32_t r   = now % seconds_per_day;
  static const time_point_sec rms{ now - r + num_of_maturity_buckets * seconds_per_day };
  
  // maturity is 1 second
  if (REX_TESTING) { return time_point_sec{ now + 1 }; }
  
  return rms;
}

asset buck::get_rex_balance() const {
  rex_balance_i table(REX_ACCOUNT(), REX_ACCOUNT().value);
  auto item = table.find(_self.value);
  if (item == table.end()) {
    return asset(0, REX);
  }
  return item->rex_balance;
}

asset buck::get_eos_rex_balance() const {
  rex_fund_i table(REX_ACCOUNT(), REX_ACCOUNT().value);
  auto item = table.find(_self.value);
  if (item == table.end()) {
    return asset(0, EOS);
  }
  return item->balance;
}

bool buck::is_mature(uint64_t cdp_id) const {
  auto item = _maturityreq.find(cdp_id);
  return item == _maturityreq.end() || item->maturity_timestamp < current_time_point();
}

void buck::process(uint8_t kind) {
  check(_rexprocess.begin() != _rexprocess.end(), "this action is not to be ran manually");
  
  auto& item = *_rexprocess.begin();
  auto cdp_itr = _cdp.find(item.cdp_id);
  
  PRINT("running process", kind);
  
  if (kind == ProcessKind::bought_rex) {
    // bought rex, determine how much
    
    // get previous balance, subtract current balance
    auto previos_balance = item.current_balance;
    auto current_balance = get_rex_balance();
    auto diff = current_balance - previos_balance;
    _rexprocess.erase(item);
    
    // update maturity request
    auto& maturity_item = _maturityreq.get(item.cdp_id, "to-do: remove. did not find maturity");
    _maturityreq.modify(maturity_item, same_payer, [&](auto& r) {
      r.maturity_timestamp = get_maturity();
    });
    
    // update rex amount
    check(cdp_itr != _cdp.end(), "did not find cdp to buy rex for");
    _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
      r.rex += diff;
    });
  }
  else if (kind == ProcessKind::sold_rex) {
    // sold rex, determine for how much
    
    // to-do what if rex sold for 0 amount?
    auto previous_balance = item.current_balance;
    auto current_balance = get_eos_rex_balance();
    auto diff = current_balance - previous_balance;
    
    _rexprocess.modify(item, same_payer, [&](auto& r) {
      r.current_balance = diff;
    });
    
    action(permission_level{ _self, "active"_n },
      REX_ACCOUNT(), "withdraw"_n,
      std::make_tuple(_self, diff)
    ).send();
  }
  
  else if (kind == ProcessKind::reparam) {
    auto gained_collateral = item.current_balance;
    _rexprocess.erase(item);
    
    auto reparam_itr = _reparamreq.find(item.cdp_id);
    auto& request_item = *reparam_itr;
    
    asset new_collateral = cdp_itr->collateral + request_item.change_collateral;
    asset change_debt = request_item.change_debt;
    
    PRINT("cdp_itr->collateral", cdp_itr->collateral)
    PRINT("request_item.change_collateral", request_item.change_collateral)
    PRINT("new_collateral", new_collateral)
    PRINT("cdp_itr->debt", cdp_itr->debt)

    PRINT("gained_collateral", gained_collateral)
    
    // adding debt
    if (request_item.change_debt.amount > 0) {
      
      PRINT_("adding debt")
      // to-do check this
      
      double ccr = get_ccr(new_collateral, change_debt);
      double ccr_cr = ((ccr / CR) - 1) * (double) cdp_itr->debt.amount;
      double di = (double) request_item.change_debt.amount;
      uint64_t change_amount = ceil(fmin(ccr_cr, di));
      change_debt = asset(change_amount, BUCK);
      
      // take issuance fee
      uint64_t fee_amount = change_amount * IF;
      auto fee = asset(fee_amount, BUCK);
      add_fee(fee);
      
      PRINT("adding", change_debt - fee)
      PRINT("fee", fee)
      
      add_balance(cdp_itr->account, change_debt - fee, same_payer, true);
    }
    
    // removing debt
    else if (request_item.change_debt.amount < 0) {
      PRINT_("removing debt")
      change_debt = request_item.change_debt; // add negative value
    }
    
    _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
      r.collateral = new_collateral;
      r.debt += change_debt;
    });
    
    if (gained_collateral.amount > 0) {
      inline_transfer(cdp_itr->account, gained_collateral, "collateral return (+ rex dividends)", EOSIO_TOKEN);
    }
    
    _reparamreq.erase(request_item);
  }
  else if (kind == ProcessKind::redemption) {
    auto redeem_itr = _redeemreq.require_find(item.cdp_id, "to-do: remove. could not find the redemption request");
    auto gained_collateral = item.current_balance;
    _rexprocess.erase(item);
    
    // determine total collateral
    asset total_collateral = asset(0, EOS);
    asset total_rex = asset(0, REX);
    for (auto& process_item: _redprocess) {
      if (process_item.account == redeem_itr->account) {
        total_collateral += process_item.collateral;
        total_rex += process_item.rex;
      }
    }
    asset dividends = gained_collateral - total_collateral;
    
    PRINT("total_rex", total_rex)
    PRINT("gained_collateral", gained_collateral)
    PRINT("total_collateral", total_collateral)
    PRINT("dividends", dividends)
    
    // go through all redeem processing items and give dividends to cdps pro rata
    auto process_itr = _redprocess.begin();
    while (process_itr != _redprocess.end()) {
      if (process_itr->account != redeem_itr->account) {
        process_itr++;
        continue;
      }
      
      PRINT("id", process_itr->cdp_id)
      PRINT("rex", process_itr->rex)
      
      uint64_t cdp_dividends_amount = process_itr->rex.amount * dividends.amount / total_rex.amount;
      asset cdp_dividends = asset(cdp_dividends_amount, EOS);
      
      auto cdp_itr = _cdp.require_find(process_itr->cdp_id, "to-do: remove. could not find cdp (redemption");
      
      PRINT("account", process_itr->account)
      PRINT("cdp_dividends", cdp_dividends)
      
      if (cdp_dividends.amount > 0) {
        
        PRINT_("adding...")
        _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
          r.rex_dividends += cdp_dividends;
        });
      }
      
      process_itr = _redprocess.erase(process_itr);
    }
    
    _redeemreq.erase(redeem_itr);
  }
  else if (kind == ProcessKind::closing) {
    auto close_itr = _closereq.require_find(item.cdp_id, "to-do: remove. could not find cdp (closing)");
    _closereq.erase(close_itr);
    _cdp.erase(cdp_itr);
      
    // to-do use rex dividends?
    // auto gained_collateral = item.current_balance;
    // _rexprocess.erase(item);
  }
}

void buck::buy_rex(uint64_t cdp_id, const asset& quantity) {
  
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
	
	inline_process(ProcessKind::bought_rex);
}

// quantity in EOS for how much of collateral we're about to sell
void buck::sell_rex(uint64_t cdp_id, const asset& quantity, ProcessKind kind) {
  
  // store info current eos balance in rex pool for this cdp
  _rexprocess.emplace(_self, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.current_balance = get_eos_rex_balance();
  });
  
  asset sell_rex;
  if (kind == ProcessKind::redemption) {
    sell_rex = quantity;
  }
  else {
    auto& cdp_item = _cdp.get(cdp_id);
    auto sell_rex_amount = cdp_item.rex.amount * quantity.amount / cdp_item.collateral.amount;
    sell_rex = asset(sell_rex_amount, REX);
    
    _cdp.modify(cdp_item, same_payer, [&](auto& r) {
      r.rex -= sell_rex;
    });
  }

  // sell rex
  action(permission_level{ _self, "active"_n },
		REX_ACCOUNT(), "sellrex"_n,
		std::make_tuple(_self, sell_rex)
	).send();
  
  inline_process(ProcessKind::sold_rex);
	inline_process(kind);
}