// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::run(uint64_t max) {
  
  stats_i table(_self, _self.value);
  eosio_assert(table.begin() != table.end(), "contract is not yet initiated");

  // check if liquidation complete for this round
  if (table.begin()->liquidation_timestamp == table.begin()->oracle_timestamp) {
    run_requests(max);
  }
  else {
    run_liquidation(max);
  }
}

void buck::run_requests(uint64_t max) {
  uint64_t processed = 0;
  
  cdp_i positions(_self, _self.value);
  close_req_i closereqs(_self, _self.value);
  auto close_item = closereqs.begin();
  reparam_req_i reparamreqs(_self, _self.value);
  auto reparam_item = reparamreqs.begin();
  
  stats_i table(_self, _self.value);
  eosio_assert(table.begin() != table.end(), "contract is not yet initiated");
  auto oracle_timestamp = table.begin()->oracle_timestamp;
  
  // loop until any requests exist and not over limit
  while (processed < max) {
    processed++;
    
    // close request
    
    // check request time
    if (close_item != closereqs.end() && close_item->timestamp < oracle_timestamp) {
      
      // find cdp
      auto& cdp_item = positions.get(close_item->cdp_id);
      
      // send eos
      inline_transfer(cdp_item.account, cdp_item.collateral, "closing cdp", EOSIO_TOKEN);
      
      // remove request
      close_item = closereqs.erase(close_item);
      
      // remove cdp
      positions.erase(cdp_item);
      
      PRINT("closed cdp", close_item->cdp_id)
    }
    
    // reparam request 
    
    // check request time
    if (reparam_item != reparamreqs.end() && reparam_item->timestamp < oracle_timestamp) {
      
    }
  }
  
  PRINT("done requests", processed)
}

void buck::run_liquidation(uint64_t max) {
  uint64_t processed = 0;
  auto eos_price = get_eos_price();
  
  cdp_i positions(_self, _self.value);
  auto debtor_index = positions.get_index<"debtor"_n>();
  auto debtor_item = debtor_index.begin();
  auto liquidator_index = positions.get_index<"liquidator"_n>();
  auto liquidator_item = liquidator_index.begin();
  
  // loop through debtors
  while (debtor_item != debtor_index.end() && processed < max) {
    
    double debt = (double) debtor_item->debt.amount;
    double debtor_ccr = (double) debtor_item->collateral.amount * eos_price / debt;
    
    // this and all further debtors don't have any bad debt
    if (debtor_ccr >= CR) {
      
      stats_i table(_self, _self.value);
      eosio_assert(table.begin() != table.end(), "contract is not yet initiated");
      
      table.modify(table.begin(), same_payer, [&](auto& r) {
        r.liquidation_timestamp = table.begin()->oracle_timestamp;
      });
      
      // to-do mark liquidation done for this round
      PRINT("liquidation complete for", processed)
      
      break;
    }
    
    double bad_debt = (CR - debtor_ccr) * debt;
    
    PRINT("debtor", debtor_item->id)
    PRINT("ccr", debtor_ccr)
    
    // loop through liquidators
    while (bad_debt > 0) {
    
      PRINT("bad debt", asset(ceil(bad_debt), BUCK))
      PRINT_("")
    
      // to-do check debt not 0
      double liquidator_collateral = (double) liquidator_item->collateral.amount;
      double liquidator_debt = (double) liquidator_item->debt.amount;
      double liquidator_acr = liquidator_item->acr;
      double liquidator_ccr = liquidator_collateral * eos_price / liquidator_debt;
      
      PRINT("checking liquidator", liquidator_item->id)
      PRINT("ccr", liquidator_ccr)
      PRINT("acr", liquidator_acr)
      PRINT_("")
      
      // this and all further liquidators can not bail out anymore bad debt 
      if (liquidator_ccr <= liquidator_acr || liquidator_item == liquidator_index.end()) {
        
        // to-do send all remaining bad debt to bailout pool
        PRINT_("sending to bailout pool...")
        
        break;
      }
      
      double bailable;
      if (liquidator_debt == 0) {
        bailable = liquidator_collateral / liquidator_acr * eos_price;
      }
      else {
        bailable = liquidator_collateral / ((liquidator_ccr / liquidator_acr) - 1) * eos_price;
      }
      
      double used_debt_amount = fmin(bad_debt, bailable);
      double used_collateral_amount = used_debt_amount / (eos_price * (1 - LF));
      
      asset used_debt = asset(ceil(used_debt_amount), BUCK);
      asset used_collateral = asset(ceil(used_collateral_amount), EOS);
      
      PRINT("sending debt", used_debt)
      PRINT("sending collateral", used_collateral)
      PRINT_("")
      
      debtor_index.modify(debtor_item, same_payer, [&](auto& r) {
        r.collateral -= used_collateral;
        r.debt -= used_debt;
      });
      
      liquidator_index.modify(liquidator_item, same_payer, [&](auto& r) {
        r.collateral += used_collateral;
        r.debt += used_debt;
      });
      
      // if liquidator did not bail out all of bad debt, continue with the next one  
      if (bad_debt > bailable) { 
        liquidator_item++;
      }
      
      // update values
      bad_debt -= used_debt_amount;
      PRINT_("---\n")
    }
    
    PRINT_("--------------\n")
    
    // continue to the next debtor
    processed++;
    debtor_item++;
  }
  
  // deferred_run(25);
}