// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

#include "buck.hpp"
#include "constants.hpp"

void buck::transfer(name from, name to, asset quantity, std::string memo) {
  eosio_assert(false, "nope");
}

void buck::receive_transfer(name from, name to, asset quantity, std::string memo) {
  require_auth(from);
  if (to != _self || from == _self) { return; }
  
	eosio_assert(quantity.symbol.is_valid(), "Invalid quantity.");
	eosio_assert(quantity.amount > 0, "Only positive quantity allowed.");
  
  eosio_assert(quantity.symbol == EOS_SYMBOL, "you have to use the system EOS token");
  eosio_assert(get_code() == "eosio.token"_n, "you have to use the system EOS token");
  
  eosio_assert(false, "yep");
}

extern "C" {
  
  void apply(uint64_t receiver, uint64_t code, uint64_t action) {
     
    if (code == receiver) {
  	  switch (action) {
        EOSIO_DISPATCH_HELPER(buck, 
            (transfer))
  	  }
    }
  	else if (action == "transfer"_n.value && code != receiver) {
  	  execute_action(name(receiver), name(code), &buck::receive_transfer);
  	}
  }
};