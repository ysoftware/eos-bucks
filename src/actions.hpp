// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::inline_transfer(name account, asset quantity, std::string memo, name contract) {
	action(permission_level{ _self, "active"_n },
		contract, "transfer"_n,
		std::make_tuple(_self, account, quantity, memo)
	).send();
}

asset buck::get_rex_balance() {
  rex_balance_i table(EOSIO, EOSIO.value);
  auto item = table.find(_self.value);
  if (item == table.end()) {
    return asset(0, REX);
  }
  return item->rex_balance;
}

void buck::buy_rex(uint64_t cdp_id, asset quantity) {
  
  // update previous rex process
  process_rex();
  
  // store info current rex balance and this cdp
  rex_processing_i info(_self, _self.value);
  info.emplace(_self, [&](auto& r) {
    r.cdp_id = cdp_id;
    r.current_rex_balance = get_rex_balance();
  });
  
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

void buck::sell_rex(uint64_t cdp_id, asset quantity) {
  
}