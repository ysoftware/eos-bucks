// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::run(uint64_t max) {
  
  stats_i table(_self, _self.value);
  check(table.begin() != table.end(), "contract is not yet initiated");

  // check if liquidation complete for this round
  if (table.begin()->liquidation_timestamp == table.begin()->oracle_timestamp) {
    run_requests(max);
  }
  else {
    run_liquidation(max);
  }
}

void buck::run_requests(uint64_t max) {
  PRINT("running requests, max", max)
  uint64_t processed = 0;
  auto price = get_eos_price();
  
  stats_i table(_self, _self.value);
  check(table.begin() != table.end(), "contract is not yet initiated");
  auto oracle_timestamp = table.begin()->oracle_timestamp;
  
  cdp_i positions(_self, _self.value);
  close_req_i closereqs(_self, _self.value);
  reparam_req_i reparamreqs(_self, _self.value);
  redeem_req_i redeemreqs(_self, _self.value);
  auto debtor_index = positions.get_index<"debtor"_n>(); // to-do make sure index is correct here (liquidation debtor)!
  
  auto close_item = closereqs.begin();
  auto reparam_item = reparamreqs.begin();
  auto redeem_item = redeemreqs.begin();
  auto debtor_item = debtor_index.begin();
  
  // loop until any requests exist and not over limit
  while (processed < max) {
    processed++;
    
    // close request
    if (close_item != closereqs.end() && close_item->timestamp < oracle_timestamp) {
      
      // find cdp, should always exist
      auto& cdp_item = positions.get(close_item->cdp_id);
      
      // send eos
      inline_transfer(cdp_item.account, cdp_item.collateral, "closing debt position", EOSIO_TOKEN);
      
      // remove request and cdp
      close_item = closereqs.erase(close_item);
      positions.erase(cdp_item);
    }
    
    // reparam request
    if (reparam_item != reparamreqs.end() && reparam_item->timestamp < oracle_timestamp) {
      
      // remove old unpaid requests in ~2 rounds. no need to return buck cuz unpaid
      
      // look for a first paid request
      while (reparam_item != reparamreqs.end() && !reparam_item->isPaid) { reparam_item++; }
      if (reparam_item == reparamreqs.end() && !reparam_item->isPaid) { continue; }
      
      // find cdp
      auto cdp_item = positions.find(reparam_item->cdp_id);
      
      if (cdp_item != positions.end()) {
        
        asset new_debt = cdp_item->debt;
        asset new_collateral = cdp_item->collateral;
        auto ccr = get_ccr(cdp_item->collateral, cdp_item->debt);
  
        if (reparam_item->change_debt.amount > 0) { // 4
          
          auto ccr_cr = ((ccr / CR) - 1) * (double) cdp_item->debt.amount;
          auto di = (double) reparam_item->change_debt.amount;
          auto change = asset(ceil(fmin(ccr_cr, di)), BUCK);
          new_debt += change;
          
          add_balance(cdp_item->account, change, same_payer, true);
        }
        
        else if (reparam_item->change_debt.amount < 0) { // 1
          new_debt += reparam_item->change_debt; // add negative value
        }
        
        if (reparam_item->change_collateral.amount > 0) { // 2
          new_collateral += reparam_item->change_collateral;
        }
        
        else if (reparam_item->change_collateral.amount < 0) { // 3 
        
          auto cr_ccr = CR / ccr;
          auto cwe = (double) -reparam_item->change_collateral.amount / (double) cdp_item->collateral.amount;
          auto change = asset(ceil(fmin(cr_ccr, cwe) * cdp_item->collateral.amount), EOS);
          new_collateral -= change;
          
          inline_transfer(cdp_item->account, change, "collateral return", EOSIO_TOKEN);
        }
        
        // to-do check new ccr parameters
        
        // update cdp
        positions.modify(cdp_item, same_payer, [&](auto& r) {
          r.collateral = new_collateral;
          r.debt = new_debt;
        });
      }
      
      // remove request
      reparam_item = reparamreqs.erase(reparam_item);
    }
    
    // redeem request
    if (redeem_item != redeemreqs.end() && redeem_item->timestamp < oracle_timestamp) {
      
      auto redeem_quantity = redeem_item->quantity;
      asset collateral_return = asset(0, EOS);
      
      while (redeem_quantity.amount > 0 && debtor_item != debtor_index.end()) {
        
        auto using_debt_amount = std::min(redeem_quantity.amount, debtor_item->debt.amount);
        auto using_collateral_amount = floor((double) using_debt_amount / price);
        auto returning_collateral_amount = floor((double) using_debt_amount / (price + RF));
        
        asset using_debt = asset(using_debt_amount, BUCK);
        asset using_collateral = asset(using_collateral_amount, EOS);
        asset return_collateral = asset(returning_collateral_amount, EOS);
        
        redeem_quantity -= using_debt;
        collateral_return += return_collateral;
        
        // switch to next debtor if this one is out of debt
        if (using_debt >= debtor_item->debt) {
          debtor_item = debtor_index.erase(debtor_item);
        }
        else {
          debtor_index.modify(debtor_item, same_payer, [&](auto& r) {
            r.debt -= using_debt;
            r.collateral -= using_collateral;
          });
        }
      }
      
      // check if all bucks have been redeemed
      if (redeem_quantity.amount > 0) {
        add_balance(redeem_item->account, redeem_quantity, redeem_item->account, true);
      }
      
      inline_transfer(redeem_item->account, collateral_return, "redeem buck", EOSIO_TOKEN);
      
      // remove request
      redeem_item = redeemreqs.erase(redeem_item);
    }
  }
}

void buck::run_liquidation(uint64_t max) {
  PRINT("running liquidation, max", max)
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
      check(table.begin() != table.end(), "contract is not yet initiated");
      
      table.modify(table.begin(), same_payer, [&](auto& r) {
        r.liquidation_timestamp = table.begin()->oracle_timestamp;
      });
      
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
      if (liquidator_acr > 0 && liquidator_ccr <= liquidator_acr || liquidator_item == liquidator_index.end()) {
        
        // no more liquidators
        return;
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
}