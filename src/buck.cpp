// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

#include "buck.hpp"
#include "constants.hpp"
#include "debug.hpp"
#include "actions.hpp"
#include "init.hpp"
#include "methods.hpp"
#include "oracle.hpp"
#include "run.hpp"
#include "transfer.hpp"
#include "request.hpp"
#include "tax.hpp"

extern "C" {
  
  void apply(uint64_t receiver, uint64_t code, uint64_t action) {
    
    if (code == receiver) {
  	  switch (action) {
        EOSIO_DISPATCH_HELPER(buck, 
            (update)(init)(run)
            (transfer)(open)(closecdp)(change)(changeacr)(redeem)
            (zdestroy))
  	  }
    }
  	else if (action == "transfer"_n.value && code != receiver) {
  	  execute_action(name(receiver), name(code), &buck::notify_transfer);
  	}
  }
};