// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

const bool REX_TESTING = false;

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
  cdp_maturity_req_i requests(_self, _self.value);
  auto item = requests.find(cdp_id);
  return item == requests.end() || item->maturity_timestamp < current_time_point();
}

void buck::process_rex() {
  rex_processing_i info(_self, _self.value);
  if (info.begin() != info.end()) {
    auto& item = *info.begin();
    
    cdp_i positions(_self, _self.value);
    auto& cdp_item = positions.get(item.cdp_id);
    
    if (item.current_balance.symbol == BUCK) {
      // bought rex, determine how much
      
      // get previous balance, subtract current balance
      auto previos_balance = item.current_balance;
      auto current_balance = get_rex_balance();
      auto diff = current_balance - previos_balance;
      if (diff.amount != 0) {
        
        positions.modify(cdp_item, same_payer, [&](auto& r) {
          r.rex += diff;
        });
      }
    }
    else if (item.current_balance.symbol == EOS) {
      // sold rex, determine for how much
      
      // get previous balance, subtract from current balance
      auto previos_balance = item.current_balance;
      auto current_balance = get_eos_rex_balance();
      auto diff = current_balance - previous_balance;
      if (diff.amount != 0) {
      
        positions.modify(cdp_item, same_payer, [&](auto& r) {
          r.rex += current_balance - previos_balance;
        });
        
        // withdraw
        action(permission_level{ _self, "active"_n },
          EOSIO, "withdraw"_n,
          std::make_tuple(_self, diff)
        ).send();
        
        // to-do what do we do here????
        
        // send eos to user
        inline_transfer(cdp_item.account, diff, "collateral return", EOSIO_TOKEN);
      }
    }
    
    info.erase(info.begin());
  }
}

void buck::buy_rex(uint64_t cdp_id, asset quantity) {
  
  // update previous rex process
  process_rex();
  
  // store info current rex balance and this cdp
  rex_processing_i info(_self, _self.value);
  info.emplace(_self, [&](auto& r) {
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
}

// quantity in EOS for how much of collateral we're about to sell
void buck::sell_rex(uint64_t cdp_id, asset quantity) {
  
  // update previous rex process
  process_rex();

  // store info current eos balance in rex pool for this cdp
  rex_processing_i info(_self, _self.value);
  info.emplace(_self, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.current_rex_balance = get_eos_rex_balance();
  });
 
  cdp_i positions(_self, _self.value);
  auto& cdp_item = positions.get(cdp_id);
  
  if (REX_TESTING) { return; }
  
  auto sell_rex_amount = cdp_item.collateral.amount * cdp_item.rex.amount / quantity.amount;
  auto sell_rex = asset(sell_rex_amount, REX);
  
  // sell rex
  action(permission_level{ _self, "active"_n },
		EOSIO, "sellrex"_n,
		std::make_tuple(_self, sell_rex)
	).send();
}