// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::run(uint8_t max) {
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  
  const uint8_t value = std::min(max, (uint8_t) 50);
  if (get_liquidation_status() == LiquidationStatus::processing_liquidation) {
    run_liquidation(value);
  }
  else {
    run_requests(value);
  }
}

void buck::run_requests(uint8_t max) {
  const time_point now = get_current_time_point();
  const uint32_t price = get_eos_price();
  const time_point oracle_timestamp = _stat.begin()->oracle_timestamp;
  uint8_t status = get_processing_status();
  
  auto maturity_index = _maturityreq.get_index<"bytimestamp"_n>();
  auto debtor_index = _cdp.get_index<"debtor"_n>(); // to-do make sure index is correct here (liquidation debtor)!
  
  auto close_itr = _closereq.begin();
  auto reparam_itr = _reparamreq.begin();
  auto redeem_itr = _redeemreq.begin(); // to-do redeem sorting
  auto debtor_itr = debtor_index.begin();
  auto maturity_itr = maturity_index.begin();
  
  // loop until any requests exist and not over limit
  for (int i = 0; i < max; i++) {
    
    if (status == ProcessingStatus::processing_cdp_requests) {
      bool did_work = false;

      // close request
      if (close_itr != _closereq.end() && close_itr->timestamp < oracle_timestamp) {
        
        const auto cdp_itr = _cdp.require_find(close_itr->cdp_id, "to-do: remove. no cdp for this close request");
        
        if (cdp_itr->debt.amount == 0) {
          withdraw_insurance_dividends(cdp_itr);
          update_excess_collateral(-cdp_itr->collateral);
        }
        
        _cdp.erase(cdp_itr);
        add_funds(cdp_itr->account, cdp_itr->collateral, same_payer);
      
        close_itr = _closereq.erase(close_itr);
        did_work = true;
    }
      
      // reparam request
      if (reparam_itr != _reparamreq.end() && reparam_itr->timestamp < oracle_timestamp) {
      
        // find cdp
        const auto cdp_itr = _cdp.require_find(reparam_itr->cdp_id);
        bool shouldRemove = true;
        
        asset change_debt = ZERO_BUCK;
        asset new_collateral = cdp_itr->collateral;
        const int64_t total_debt_amount = cdp_itr->debt.amount + cdp_itr->accrued_debt.amount;
        
        const uint64_t ccr = CR; // used in to calculate relation with CR
        if (total_debt_amount > 0) {
          cdp_itr->collateral.amount * price / total_debt_amount;
        }
  
        // adding debt
        if (reparam_itr->change_debt.amount > 0) {
          
          const int64_t max_debt = ((ccr * 100 / CR) - 100) * total_debt_amount / 100;
          const int64_t change_amount = std::min(max_debt, reparam_itr->change_debt.amount);

          change_debt = asset(change_amount, BUCK);
        }
        
        // removing debt
        else if (reparam_itr->change_debt.amount < 0) {
          change_debt = reparam_itr->change_debt; // add negative value
        }
        
        // adding collateral
        if (reparam_itr->change_collateral.amount > 0) {
          new_collateral += reparam_itr->change_collateral;
          
          // get maturity request
          const auto maturity_itr = _maturityreq.require_find(cdp_itr->id, "to-do: remove. could not find maturity (run)");
          _maturityreq.modify(maturity_itr, same_payer, [&](auto& r) {
            r.maturity_timestamp = get_maturity();
            r.add_collateral = reparam_itr->change_collateral;
            r.cdp_id = cdp_itr->id;
            r.change_debt = change_debt;
            r.ccr = 0;
          });
        }
        
        // removing collateral
        else if (reparam_itr->change_collateral.amount < 0) {
        
          const int64_t can_withdraw = (CR - 100) * cdp_itr->collateral.amount / ccr;
          const int64_t change_amount = std::min(can_withdraw, -reparam_itr->change_collateral.amount);
      
          const asset change = asset(change_amount, EOS);
          new_collateral -= change;
          
          shouldRemove = false;
        }
        
        // to-do check new ccr parameters
        // don't give debt if ccr < CR
  
        // not removing collateral here, update immediately
        if (reparam_itr->change_collateral.amount == 0) {
          asset change_accrued_debt = ZERO_BUCK;
          
          if (change_debt.amount > 0) {
            add_balance(cdp_itr->account, change_debt, same_payer, true);
          }
          else {
            const uint64_t change_accrued_debt_amount = std::max(change_debt.amount, -cdp_itr->accrued_debt.amount);
            change_accrued_debt = asset(-change_accrued_debt_amount, BUCK); // positive
            change_debt += change_accrued_debt; // add to negative
            
            add_savings_pool(change_accrued_debt);
            update_supply(change_debt);
          }
          
          _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
            r.collateral = new_collateral;
            r.accrued_debt += change_accrued_debt;
            r.debt += change_debt;
          });
        }
        
        // check if request should be removed here or is handled in process method
        if (shouldRemove) {
          reparam_itr = _reparamreq.erase(reparam_itr);
        }
        else {
          reparam_itr++;
        }
        did_work = true;
      }
      
      // maturity requests (issue bucks, add/remove cdp debt, add collateral)
      if (maturity_itr != maturity_index.end()) {
        
        // look for a first valid request
        while (maturity_itr != maturity_index.end() && !(maturity_itr->maturity_timestamp < now && time_point_sec(maturity_itr->maturity_timestamp).utc_seconds != 0)) { maturity_itr++; }
        if (maturity_itr != maturity_index.end() && maturity_itr->maturity_timestamp < now && time_point_sec(maturity_itr->maturity_timestamp).utc_seconds != 0) {
          
          // to-do remove cdp if all collateral is 0 (and cdp was just created) ???
          const auto cdp_itr = _cdp.require_find(maturity_itr->cdp_id, "to-do: remove. no cdp for this maturity");
          
          // calculate new debt and collateral
          asset change_debt = maturity_itr->change_debt; // changing debt explicitly (or 0)
          const asset add_collateral = maturity_itr->add_collateral;
          
          if (maturity_itr->ccr > 0) {
            // opening cdp
            
            // issue debt
            const int64_t debt_amount = (price * add_collateral.amount / maturity_itr->ccr);
            change_debt = asset(debt_amount, BUCK);
          }
          
          asset change_accrued_debt = ZERO_BUCK;
          if (change_debt.amount > 0) {
            
            add_balance(cdp_itr->account, change_debt, same_payer, true);
          }
          else {
            
            const uint64_t change_accrued_debt_amount = std::max(change_debt.amount, -cdp_itr->accrued_debt.amount);
            change_accrued_debt = asset(-change_accrued_debt_amount, BUCK); // positive
            change_debt += change_accrued_debt;
            
            add_savings_pool(change_accrued_debt);
            update_supply(-change_debt);
          }
          
          // stop being an insurer
          if (cdp_itr->debt.amount == 0 && change_debt.amount > 0) {
            withdraw_insurance_dividends(cdp_itr);
            update_excess_collateral(-cdp_itr->collateral);
          }
          else if (cdp_itr->debt.amount - change_debt.amount == 0) {
            update_excess_collateral(cdp_itr->collateral + add_collateral);
          }
          
          _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
            r.collateral += add_collateral;
            r.debt += change_debt;
            r.accrued_debt += change_accrued_debt;
            r.modified_round = _tax.begin()->current_round;
          });
          
          // to-do if removing debt, send accrued to savings pool
          
          maturity_itr = maturity_index.erase(maturity_itr); // remove request
          did_work = true;
        }
      }
      
      if (!did_work) {
        i--; // don't count this pass
        set_processing_status(ProcessingStatus::processing_redemption_requests);
        status = ProcessingStatus::processing_redemption_requests;
        continue;
      }
    }
    else if (status == ProcessingStatus::processing_redemption_requests) {

      // redeem request
      if (redeem_itr != _redeemreq.end() && redeem_itr->timestamp < oracle_timestamp) {
        
        // to-do sorting
        // to-do verify timestamp
        auto redeem_quantity = redeem_itr->quantity;
        asset rex_return = ZERO_REX;
        asset collateral_return = ZERO_EOS;
        
        asset burned_debt = ZERO_BUCK; // used up
        asset saved_debt = ZERO_BUCK; // to savings pool
        
        // loop through available debtors until all amount is redeemed or our of debtors
        while (redeem_quantity.amount > 0 && debtor_itr != debtor_index.end() && debtor_itr->debt.amount > 0) {
          
          const int32_t ccr = debtor_itr->collateral.amount * price / debtor_itr->debt.amount;
          
          // skip to the next debtor
          if (ccr < 100 - RF) { continue; }
          
          const int64_t total_debt_amount = debtor_itr->debt.amount + debtor_itr->accrued_debt.amount;
          const int64_t using_debt_amount = std::min(redeem_quantity.amount, total_debt_amount);
          const int64_t using_collateral_amount = using_debt_amount / (price + RF);
          
          const int64_t using_accrued_debt_amount = std::min(debtor_itr->accrued_debt.amount, using_debt_amount);
          const asset using_accrued_debt = asset(using_accrued_debt_amount, BUCK);
          const asset using_debt = asset(using_debt_amount, BUCK) - using_accrued_debt;
          const asset using_collateral = asset(using_collateral_amount, EOS);
          
          redeem_quantity -= using_debt + using_accrued_debt;
          collateral_return += using_collateral;
          
          debtor_index.modify(debtor_itr, same_payer, [&](auto& r) {
            r.debt -= using_debt;
            r.accrued_debt -= using_accrued_debt;
            r.collateral -= using_collateral;
          });
          
          saved_debt += using_accrued_debt;
          burned_debt += using_debt;
          
          // next best debtor will be the first in table (after this one changed)
          debtor_itr = debtor_index.begin();
        }
        
        if (redeem_quantity.amount > 0) {
          
          // return unredeemed amount
          add_balance(redeem_itr->account, redeem_quantity, same_payer, false);
        }
      
        add_savings_pool(saved_debt);
        update_supply(burned_debt);
        
        add_funds(redeem_itr->account, collateral_return, same_payer); // to-do receipt
      }
      else {
        // no more redemption requests
        set_processing_status(ProcessingStatus::processing_complete);
        break;
      }
    }
    else { break; }
  }
  
  auto accrual_index = _cdp.get_index<"accrued"_n>();
  auto accrual_item = accrual_index.begin();
  
  int i = 0;
  while (i < max && accrual_item != accrual_index.end() &&
        accrual_item->debt.amount > 0 &&
        time_point_sec(time_point(now - accrual_item->accrued_timestamp)).utc_seconds > ACCRUAL_PERIOD) {
    
    accrue_interest(_cdp.require_find(accrual_item->id));
    accrual_item = accrual_index.begin(); // take first element after index updated
    i++;
  }
}

void buck::run_liquidation(uint8_t max) {
  uint64_t processed = 0;
  const uint32_t price = get_eos_price();
  
  auto debtor_index = _cdp.get_index<"debtor"_n>();
  auto liquidator_index = _cdp.get_index<"liquidator"_n>();
  
  auto debtor_itr = debtor_index.begin();
  
  // loop through debtors
  while (debtor_itr != debtor_index.end() && processed < max) {
    
    int64_t debt_amount = debtor_itr->debt.amount + debtor_itr->accrued_debt.amount;
    int64_t collateral_amount = debtor_itr->collateral.amount;
    
    int64_t debtor_ccr = CR;
    if (debt_amount > 0) {
      debtor_ccr = collateral_amount * price / debt_amount;
    }
    
    PRINT("debtor id", debtor_itr->id)
    PRINT("total debt", debt_amount) 
    PRINT("col", debtor_itr->collateral)
    PRINT("ccr", debtor_ccr)
    
    // this and all further debtors don't have any bad debt
    if (debtor_ccr >= CR && max > processed) {
      
      PRINT_("DONE")
      set_liquidation_status(LiquidationStatus::liquidation_complete);
      run_requests(max - processed);
      return;
    }
        
    // loop through liquidators
    while (debtor_ccr < CR) {
      const auto liquidator_itr = liquidator_index.begin();
      
      int64_t liquidation_fee = LF;
      if (debtor_ccr >= 100 + LF) { liquidation_fee = LF; }
      else if (debtor_ccr < 75) { liquidation_fee = -25; }
      else { liquidation_fee = debtor_ccr - 100; }
      
      const int64_t x = (100 + liquidation_fee) 
                          * (750 * debt_amount - 5 * collateral_amount * price)
                          / (50000 - 1500 * liquidation_fee);
      
      const int64_t bad_debt = ((CR - debtor_ccr) * debt_amount) / 100 + x;
      
      const int64_t liquidator_collateral = liquidator_itr->collateral.amount;
      const int64_t liquidator_debt = liquidator_itr->debt.amount;
      const int64_t liquidator_acr = liquidator_itr->acr;
      
      int64_t liquidator_ccr = CR;
      if (liquidator_debt > 0) {
        liquidator_ccr = liquidator_collateral * price / liquidator_debt;
      }
      
      PRINT("liquidator", liquidator_itr->id)
      PRINT("ccr", liquidator_ccr)
      
      // this and all further liquidators can not bail out anymore bad debt
      if (liquidator_ccr < CR || liquidator_itr == liquidator_index.end()) {
        
        // to-do bailout pool?
        PRINT_("FAILED")
        set_liquidation_status(LiquidationStatus::failed);
        run_requests(max - processed);
        return;
      }
      
      // to-do check if right
      
      if (liquidator_itr->debt.amount == 0) {
        const auto cdp_itr = _cdp.require_find(liquidator_itr->id);
        withdraw_insurance_dividends(cdp_itr);
        update_excess_collateral(-cdp_itr->collateral);
      }
      
      
      const int64_t bailable = (liquidator_collateral * price - liquidator_debt * liquidator_acr)
                                    * (100 - liquidation_fee)
                                  / (liquidator_acr * (100 - liquidation_fee) - 10000);
      
      const int64_t used_debt_amount = std::min(std::min(bad_debt, bailable), debt_amount);
      const int64_t used_collateral_amount = std::min(collateral_amount, used_debt_amount * 10000 / (price * (100 - liquidation_fee)));
      
      // to-do check rounding
      const asset used_debt = asset(used_debt_amount, BUCK);
      const asset used_collateral = asset(used_collateral_amount, EOS);
      
      PRINT("bad debt", bad_debt)
      PRINT("bailable", bailable)
      PRINT("used_debt", used_debt)
      PRINT_("\n")
      
      debtor_index.modify(debtor_itr, same_payer, [&](auto& r) {
        r.collateral -= used_collateral;
        r.debt -= used_debt;
      });
      
      liquidator_index.modify(liquidator_itr, same_payer, [&](auto& r) {
        r.collateral += used_collateral;
        r.debt += used_debt;
      });
      
      // update values
      debt_amount = debtor_itr->debt.amount;
      collateral_amount = debtor_itr->collateral.amount;
      
      debtor_ccr = CR;
      if (debt_amount > 0) {
        debtor_ccr = collateral_amount * price / debt_amount;
      }
    }
    
    // continue to the next (first) debtor
    processed++;
    debtor_itr = debtor_index.begin();
  }
}