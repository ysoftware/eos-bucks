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
        PRINT_("close req")
        
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
        PRINT_("\nreparam req")
      
        // find cdp
        const auto cdp_itr = _cdp.require_find(reparam_itr->cdp_id);
        
        asset change_debt = ZERO_BUCK;
        asset change_collateral = ZERO_REX;
        
        int64_t ccr = CR; // used in to calculate relation with CR
        if (cdp_itr->debt.amount > 0) {
          ccr = convert_to_rex_usd(cdp_itr->collateral.amount) / cdp_itr->debt.amount;
        }
  
        // adding debt
        if (reparam_itr->change_debt.amount > 0) {
          
          const int64_t max_debt = ((ccr * 100 / CR) - 100) * cdp_itr->debt.amount / 100;
          const int64_t change_amount = std::min(max_debt, reparam_itr->change_debt.amount);

          change_debt = asset(change_amount, BUCK);
        }
        
        // removing debt
        else if (reparam_itr->change_debt.amount < 0) {
          change_debt = reparam_itr->change_debt; // add negative value
        }
        
        // adding collateral
        if (reparam_itr->change_collateral.amount > 0) {
          change_collateral = reparam_itr->change_collateral;
        }
        
        // removing collateral
        else if (reparam_itr->change_collateral.amount < 0) {
        
          const int64_t can_withdraw = (CR - 100) * cdp_itr->collateral.amount / ccr;
          const int64_t change_amount = std::max(-can_withdraw, reparam_itr->change_collateral.amount);
      
          const asset change = asset(change_amount, REX);
          change_collateral = change;
        }
        
        accrue_interest(cdp_itr);
        
        // to-do check new ccr parameters
        // don't give debt if ccr < CR
        
        
        if (change_debt.amount > 0) {
          add_balance(cdp_itr->account, change_debt, same_payer, true);
        }
        
        if (change_collateral.amount < 0) {
          
          sub_funds(cdp_itr->account, -change_collateral);
        }

        // stop being an insurer
        if (cdp_itr->debt.amount == 0 && change_debt.amount > 0) {
          withdraw_insurance_dividends(cdp_itr);
          update_excess_collateral(-cdp_itr->collateral); // remove old amount
        }
        // become an insurer
        else if (cdp_itr->debt.amount - change_debt.amount == 0) {
          update_excess_collateral(cdp_itr->collateral + change_collateral); // add new amount
        }
        
        PRINT("id", cdp_itr->id)
        PRINT("debt", cdp_itr->debt)
        PRINT("col", cdp_itr->collateral)
        PRINT("change_debt", change_debt)
        PRINT("change_collateral", change_collateral) 
        PRINT_("--------")
        
        _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
          r.collateral += change_collateral;
          r.debt += change_debt;
        });
      
        reparam_itr = _reparamreq.erase(reparam_itr);
        did_work = true;
      }
      
      // maturity requests (issue bucks, add/remove cdp debt, add collateral)
      // look for a first valid request
      while (maturity_itr != maturity_index.end() 
              && !(maturity_itr->maturity_timestamp < oracle_timestamp)) { maturity_itr++; }
      
      if (maturity_itr != maturity_index.end() && maturity_itr->maturity_timestamp < oracle_timestamp) {
        PRINT_("\nmaturity req")
        
        // to-do remove cdp if all collateral is 0 (and cdp was just created) ???
        const auto cdp_itr = _cdp.require_find(maturity_itr->cdp_id, "to-do: remove. no cdp for this maturity");
        
        // calculate new debt and collateral
        asset change_debt = maturity_itr->change_debt; // changing debt explicitly (or 0)
        const asset add_collateral = maturity_itr->add_collateral;
        
        // to-do validate new debt, new collateral, new ccr
        
        if (maturity_itr->ccr > 0) {
          // opening cdp, issue debt
          const int64_t debt_amount = (convert_to_rex_usd(add_collateral.amount) / maturity_itr->ccr);
          change_debt = asset(debt_amount, BUCK);
        }
        
        if (change_debt.amount > 0) {
          
          add_balance(cdp_itr->account, change_debt, same_payer, true);
        }
        else {
          change_debt += change_debt;
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
          r.modified_round = _tax.begin()->current_round;
        });
        
        maturity_itr = maturity_index.erase(maturity_itr); // remove request
        did_work = true;
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
        PRINT_("redeem req")
        
        // to-do sorting
        // to-do verify timestamp
        auto redeem_quantity = redeem_itr->quantity;
        asset rex_return = ZERO_REX;
        asset collateral_return = ZERO_REX;
        
        asset burned_debt = ZERO_BUCK; // used up
        
        // loop through available debtors until all amount is redeemed or our of debtors
        while (redeem_quantity.amount > 0 && debtor_itr != debtor_index.end() && debtor_itr->debt.amount > 0) {
          
          const int32_t ccr = convert_to_rex_usd(debtor_itr->collateral.amount) / debtor_itr->debt.amount;
          
          // skip to the next debtor
          if (ccr < 100 - RF) { continue; }
          
          const int64_t using_debt_amount = std::min(redeem_quantity.amount, debtor_itr->debt.amount);
          const int64_t using_collateral_amount = convert_to_usd_rex(using_debt_amount, RF);
        
          const asset using_debt = asset(using_debt_amount, BUCK);
          const asset using_collateral = asset(using_collateral_amount, REX);
          
          redeem_quantity -= using_debt;
          collateral_return += using_collateral;
          
          debtor_index.modify(debtor_itr, same_payer, [&](auto& r) {
            r.debt -= using_debt;
            r.collateral -= using_collateral;
          });
          
          burned_debt += using_debt;
          
          // next best debtor will be the first in table (after this one changed)
          debtor_itr = debtor_index.begin();
        }
        
        if (redeem_quantity.amount > 0) {
          
          // return unredeemed amount
          add_balance(redeem_itr->account, redeem_quantity, same_payer, false);
        }
      
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
  auto accrual_itr = accrual_index.begin();
  
  int i = 0;
  const time_point now = get_current_time_point();
  while (i < max && accrual_itr != accrual_index.end() &&
        accrual_itr->debt.amount > 0 && accrual_itr->modified_round > ACCRUAL_PERIOD) {
    
    accrue_interest(_cdp.require_find(accrual_itr->id));
    accrual_itr = accrual_index.begin(); // take first element after index updated
    i++;
  }
}

void buck::run_liquidation(uint8_t max) {
  uint64_t processed = 0;
  
  auto debtor_index = _cdp.get_index<"debtor"_n>();
  auto liquidator_index = _cdp.get_index<"liquidator"_n>();
  
  auto debtor_itr = debtor_index.begin();
  

  PRINT_("liquidation\n")

  // loop through debtors
  while (debtor_itr != debtor_index.end() && processed < max) {
    
    int64_t debt_amount = debtor_itr->debt.amount;
    int64_t collateral_amount = debtor_itr->collateral.amount;
    
    int64_t debtor_ccr = CR;
    if (debt_amount > 0) {
      PRINT("rex amount", convert_to_rex_usd(collateral_amount))
      debtor_ccr = convert_to_rex_usd(collateral_amount) / debt_amount;
    }
    
    PRINT("debtor id", debtor_itr->id)
    PRINT("debt", debt_amount) 
    PRINT("col", debtor_itr->collateral)
    PRINT("ccr", debtor_ccr)
    
    // this and all further debtors don't have any bad debt
    if (debtor_ccr >= CR && max > processed) {
      
      PRINT_("DONE")
      set_liquidation_status(LiquidationStatus::liquidation_complete);
      run_requests(max - processed);
      return;
    }
        
    const auto cdp_itr = _cdp.require_find(debtor_itr->id);
    accrue_interest(cdp_itr);
  
    // loop through liquidators
    while (debtor_ccr < CR) {
      const auto liquidator_itr = liquidator_index.begin();
      
      int64_t liquidation_fee = LF;
      if (debtor_ccr >= 100 + LF) { liquidation_fee = LF; }
      else if (debtor_ccr < 75) { liquidation_fee = -25; }
      else { liquidation_fee = debtor_ccr - 100; }
      
      const int64_t x = (100 + liquidation_fee) 
                          * (750 * debt_amount - convert_to_rex_usd(5 * collateral_amount))
                          / (50000 - 1500 * liquidation_fee);
      
      const int64_t bad_debt = ((CR - debtor_ccr) * debt_amount) / 100 + x;
      
      const int64_t liquidator_collateral = liquidator_itr->collateral.amount;
      const int64_t liquidator_debt = liquidator_itr->debt.amount;
      const int64_t liquidator_acr = liquidator_itr->acr;
      
      int64_t liquidator_ccr = CR;
      if (liquidator_debt > 0) {
        liquidator_ccr = convert_to_rex_usd(liquidator_collateral) / liquidator_debt;
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
      
      const auto cdp_itr = _cdp.require_find(liquidator_itr->id);
      accrue_interest(cdp_itr);
      
      // to-do check if right
      
      if (liquidator_itr->debt.amount == 0) {
        const auto cdp_itr = _cdp.require_find(liquidator_itr->id);
        withdraw_insurance_dividends(cdp_itr);
        update_excess_collateral(-cdp_itr->collateral);
      }
      
      const int64_t bailable = (convert_to_rex_usd(liquidator_collateral) - liquidator_debt * liquidator_acr)
                                    * (100 - liquidation_fee)
                                  / (liquidator_acr * (100 - liquidation_fee) - 10000);
      
      const int64_t used_debt_amount = std::min(std::min(bad_debt, bailable), debt_amount);
      const int64_t value2 = used_debt_amount * 10000 / (convert_to_rex_usd(100 - liquidation_fee));
      const int64_t used_collateral_amount = std::min(collateral_amount, value2);
      
      // to-do check rounding
      const asset used_debt = asset(used_debt_amount, BUCK);
      const asset used_collateral = asset(used_collateral_amount, REX);
      
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
        debtor_ccr = convert_to_rex_usd(collateral_amount) / debt_amount;
      }
    }
    
    // continue to the next (first) debtor
    processed++;
    debtor_itr = debtor_index.begin();
  }
}