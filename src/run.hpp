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
  PRINT("run req", max)
  
  const time_point oracle_timestamp = _stat.begin()->oracle_timestamp;
  const uint32_t now = time_point_sec(oracle_timestamp).utc_seconds;
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
        
        // find cdp
        const auto cdp_itr = _cdp.find(reparam_itr->cdp_id);
        if (cdp_itr == _cdp.end()) {
          close_itr = _closereq.erase(close_itr);
          did_work = true;
          continue;
        }
        
        sell_r(cdp_itr);
        
        _cdp.erase(cdp_itr);
        add_funds(cdp_itr->account, cdp_itr->collateral, same_payer);
      
        close_itr = _closereq.erase(close_itr);
        did_work = true;
      }
      
      // reparam request
      if (reparam_itr != _reparamreq.end() && reparam_itr->timestamp < oracle_timestamp) {
        PRINT_("reparam")
      
        // find cdp
        const auto cdp_itr = _cdp.find(reparam_itr->cdp_id);
        if (cdp_itr == _cdp.end()) {
          PRINT("NO CDP FOR REPARAM", reparam_itr->cdp_id)
          reparam_itr = _reparamreq.erase(reparam_itr);
          did_work = true;
          continue;
        }
        
        accrue_interest(cdp_itr);
        sell_r(cdp_itr);
        
        cdp_itr->p();
        
        asset change_debt = ZERO_BUCK;
        asset change_collateral = ZERO_REX;
        
        if (reparam_itr->change_debt.amount < 0) { // removing debt
          if (reparam_itr->change_debt + cdp_itr->debt >= MIN_DEBT) {
            change_debt = reparam_itr->change_debt; // add negative value
          }
          else {
            PRINT_("not removing debt below min")
            PRINT("new debt", reparam_itr->change_debt + cdp_itr->debt)
          }
        }
        
        if (reparam_itr->change_collateral.amount > 0) { // adding collateral
          change_collateral = reparam_itr->change_collateral;
        }
        
        
        
        if (reparam_itr->change_collateral.amount < 0) { // removing collateral
          
          if (cdp_itr->collateral + reparam_itr->change_collateral >= MIN_COLLATERAL) {
            const auto new_debt_amount = change_debt.amount + cdp_itr->debt.amount;
  
            // check ccr with new collateral
            int32_t ccr = CR;
            if (new_debt_amount > 0) {
              ccr = to_buck(cdp_itr->collateral.amount) / new_debt_amount;
            }
            
            PRINT("reparam c<0, ccr", ccr)
            if (ccr >= CR) {
              const int64_t m1 = (CR - 100) * 100 * cdp_itr->collateral.amount / ccr / 100;
              const int64_t change_amount = std::max(-m1, reparam_itr->change_collateral.amount);
              const asset change = asset(change_amount, REX);
              change_collateral = change;
            }
            else {
              PRINT_("reparam quit 1")
            }
          }
          else {
            PRINT_("not removing collateral below min")
            PRINT("new c", cdp_itr->collateral + reparam_itr->change_collateral)
          }
        }
        
        if (reparam_itr->change_debt.amount > 0) { // adding debt
        
          const auto new_collateral_amount = change_collateral.amount + cdp_itr->collateral.amount;

          int32_t ccr = CR;
          if (cdp_itr->debt.amount > 0) {
            ccr = to_buck(new_collateral_amount) / cdp_itr->debt.amount;
          }
          
          PRINT("reparam d > 0, ccr", ccr)
          if (ccr >= CR) {
            
            const int64_t max_debt = (to_buck(new_collateral_amount * 100) / (CR * cdp_itr->debt.amount) - 100) * cdp_itr->debt.amount / 100;
            const int64_t change_amount = std::min(max_debt, reparam_itr->change_debt.amount);
            change_debt = asset(change_amount, BUCK);
          }
          else {
            PRINT_("reparam quit 2")
          }
        }
        
        PRINT("change d", change_debt.amount)
        PRINT("change c", change_collateral.amount)
        
        if (change_debt.amount > 0) {
          add_balance(cdp_itr->account, change_debt, same_payer, true);
        }
        
        if (change_collateral.amount < 0) {
          sub_funds(cdp_itr->account, -change_collateral);
        }
        
        _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
          r.collateral += change_collateral;
          r.debt += change_debt;
        });
        
        // sanity check
        check(cdp_itr->debt.amount >= 0, "programmer error, debt can't go below 0");
        check(cdp_itr->collateral.amount >= 0, "programmer error, collateral can't go below 0");
        
        cdp_itr->p();
      
        buy_r(cdp_itr);
        
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
         
        // find cdp
        const auto cdp_itr = _cdp.find(reparam_itr->cdp_id);
        if (cdp_itr == _cdp.end()) {
          maturity_itr = maturity_index.erase(maturity_itr);
          did_work = true;
          continue;
        }
        
        // calculate new debt and collateral
        asset change_debt = maturity_itr->change_debt; // changing debt explicitly (or 0)
        const asset add_collateral = maturity_itr->add_collateral;
        
        // to-do validate new debt, new collateral, new ccr
        
        if (maturity_itr->ccr > 0) {
          // opening cdp, issue debt
          const int64_t debt_amount = (to_buck(add_collateral.amount) / maturity_itr->ccr);
          change_debt = asset(debt_amount, BUCK);
        }
        
        if (change_debt.amount > 0) {
          add_balance(cdp_itr->account, change_debt, same_payer, true);
        }
        else {
          change_debt += change_debt;
          update_supply(-change_debt);
        }
        
        sell_r(cdp_itr);
        
        _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
          r.collateral += add_collateral;
          r.debt += change_debt;
          r.modified_round = now;
        });
        
        // sanity check
        check(cdp_itr->debt.amount >= 0, "programmer error, debt can't go below 0");
        check(cdp_itr->collateral.amount >= 0, "programmer error, collateral can't go below 0");
        
        
        buy_r(cdp_itr);
        
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
        PRINT("redeem", redeem_itr->quantity)
        
        // to-do sorting
        // to-do verify timestamp
        auto redeem_quantity = redeem_itr->quantity;
        asset rex_return = ZERO_REX;
        asset collateral_return = ZERO_REX;
        
        asset burned_debt = ZERO_BUCK; // used up
        debtor_itr = debtor_index.begin();
        
        // loop protection
        uint8_t debtors_failed = 0;
        
        // loop through available debtors until all amount is redeemed or our of debtors
        while (redeem_quantity.amount > 0 && debtor_itr != debtor_index.end() && debtors_failed < 20) {
          
          if (debtor_itr->collateral.amount == 0 || debtor_itr->debt.amount == 0) { // reached end of the table
            debtor_itr->p();
            break;
          }
          
          accrue_interest(_cdp.require_find(debtor_itr->id));
          
          if (debtor_itr->debt < MIN_DEBT) { // don't go below min debt
            debtor_itr++;
            debtors_failed++;
            debtor_itr->p();
            PRINT("debtor failed debt", debtor_itr->debt)
            continue;
          }
          
          const int32_t ccr = to_buck(debtor_itr->collateral.amount) / debtor_itr->debt.amount;
          
          // all further debtors have even worse ccr
          if (ccr < 100 - RF) { 
            debtor_itr++;
            debtors_failed++;
            PRINT("debtor failed ccr", ccr)
            continue;
          }
          
          const int64_t using_debt_amount = std::min(redeem_quantity.amount, debtor_itr->debt.amount);
          const int64_t using_collateral_amount = to_rex(using_debt_amount * 100, RF);
        
          const asset using_debt = asset(using_debt_amount, BUCK);
          const asset using_collateral = asset(using_collateral_amount, REX);
          
          PRINT("redeem_quantity", redeem_quantity)
          PRINT_("after tax") debtor_itr->p();
          PRINT("available debt", debtor_itr->debt)
          PRINT("using_debt", using_debt)
          PRINT("using_collateral", using_collateral)
          
          redeem_quantity -= using_debt;
          collateral_return += using_collateral;
          
          if (debtor_itr->debt == using_debt) {
            const asset left_over_collateral = debtor_itr->collateral - using_collateral;
            
            if (left_over_collateral.amount > 0) {
              add_funds(debtor_itr->account, left_over_collateral, same_payer);
            }
            
            PRINT_("redeem removing")
            debtor_itr->p();
            debtor_index.erase(debtor_itr); 
          }
          else {
            debtor_index.modify(debtor_itr, same_payer, [&](auto& r) {
              r.debt -= using_debt;
              r.collateral -= using_collateral;
            });
            
            // sanity check
            check(debtor_itr->debt.amount >= 0, "programmer error, debt can't go below 0");
            check(debtor_itr->collateral.amount >= 0, "programmer error, collateral can't go below 0");
            
            PRINT("redeem updating", using_debt)
            debtor_itr->p();
          }
          
          burned_debt += using_debt;
          
          // next best debtor will be the first in table (after this one changed)
          debtor_itr = debtor_index.begin();
        }
        
        if (redeem_quantity.amount > 0) {
          
          // return unredeemed amount
          add_balance(redeem_itr->account, redeem_quantity, same_payer, false);
        }
        
        if (redeem_itr->quantity != redeem_quantity) {
          
          // complete and remove redemption request only if anything was redeemed in the process
          update_supply(burned_debt);
          add_funds(redeem_itr->account, collateral_return, same_payer);
          redeem_itr = _redeemreq.erase(redeem_itr);
        }
        
        // in extreme situation, only do 1 request per run
        if (debtors_failed > 10) break;
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
  while (i < max && accrual_itr != accrual_index.end()
          && now - accrual_itr->modified_round > ACCRUAL_PERIOD) {
  
    // PRINT("i", i)
    accrue_interest(_cdp.require_find(accrual_itr->id));
    accrual_itr = accrual_index.begin(); // take first element after index updated
    i++;
    
    // PRINT("running accrual?", accrual_itr->id)
    // PRINT("modified", accrual_itr->modified_round)
  }
}

void buck::run_liquidation(uint8_t max) {
  PRINT("liquidation", max)
  uint64_t processed = 0;
  
  auto debtor_index = _cdp.get_index<"debtor"_n>();
  auto liquidator_index = _cdp.get_index<"liquidator"_n>();
  auto liquidator_itr = liquidator_index.begin();
  
  if (liquidator_itr == liquidator_index.end()) {
    return; // no cdp exist, quit
  }
  
  // loop through liquidators
  while (max > processed) {
    const auto liquidator_itr = liquidator_index.begin();
    const auto debtor_itr = debtor_index.begin();
    
    // PRINT_("LIQS")
    // for (auto& s: liquidator_index) { s.p(); }
    // PRINT_("---")
    
    // if (liquidator_itr == liquidator_index.end() || debtor_itr->id == liquidator_itr->id) {
    //   PRINT_("FAILED: END")
    //   set_liquidation_status(LiquidationStatus::failed);
    //   run_requests(max - processed);
    //   return;
    // }
    
    // PRINT_("\nliquidator")
    // liquidator_itr->p();
    
    // PRINT("liq sort", liquidator_itr->liquidator())
    
    // PRINT_("debtor")
    // debtor_itr->p();
    
    accrue_interest(_cdp.require_find(debtor_itr->id));
    
    if (debtor_itr->debt.amount == 0) {
      PRINT_("DONE1")
      set_liquidation_status(LiquidationStatus::liquidation_complete);
      run_requests(max - processed);
      return;
    }
    
    const int64_t debt_amount = debtor_itr->debt.amount;
    const int64_t collateral_amount = debtor_itr->collateral.amount;
    const int64_t debtor_ccr = to_buck(collateral_amount) / debt_amount;
    
    if (debtor_ccr >= CR) {
      
      PRINT_("DONE2")
      set_liquidation_status(LiquidationStatus::liquidation_complete);
      run_requests(max - processed);
      return;
    }
    
    // check acr
    if (liquidator_itr->acr == 0) {
      PRINT_("NO ACR")
      set_liquidation_status(LiquidationStatus::failed);
      run_requests(max - processed);
      return;
    }
    
    processed++;
    sell_r(_cdp.require_find(liquidator_itr->id));
    accrue_interest(_cdp.require_find(liquidator_itr->id));
    
    const int64_t liquidator_collateral = liquidator_itr->collateral.amount;
    const int64_t liquidator_debt = liquidator_itr->debt.amount;
    const int64_t liquidator_acr = liquidator_itr->acr;
    
    int64_t liquidator_ccr = 9999999;
    if (liquidator_debt > 0) {
      liquidator_ccr = to_buck(liquidator_collateral) / liquidator_debt;
    }
    
    PRINT("liq, ccr", liquidator_ccr)
    
    if (liquidator_ccr < CR || liquidator_debt > 0 && liquidator_ccr <= liquidator_acr) {
      PRINT_("FAILED: NO MORE GOOD LIQUIDATORS")
      set_liquidation_status(LiquidationStatus::failed);
      run_requests(max - processed);
      return;
    }
    
    int64_t liquidation_fee = LF;
    if (debtor_ccr >= 100 + LF) { liquidation_fee = LF; }
    else if (debtor_ccr < 75) { liquidation_fee = -25; }
    else { liquidation_fee = debtor_ccr - 100; }
    
    const int64_t x = (100 + liquidation_fee)
                        * (750 * debt_amount - to_buck(5 * collateral_amount))
                        / (50'000 - 1'500 * liquidation_fee);
    
    const int64_t bad_debt = ((CR - debtor_ccr) * debt_amount) / 100 + x;
    
    const int64_t bailable = (to_buck(liquidator_collateral) - (liquidator_debt * liquidator_acr)) 
        * (100 - liquidation_fee) / (liquidator_acr * (100 - liquidation_fee) - 10'000);
    
    const int64_t used_debt_amount = std::min(std::min(bad_debt, bailable), debt_amount);

    const int64_t value2 = used_debt_amount * 10'000 / to_buck(100 - liquidation_fee);
    const int64_t used_collateral_amount = std::min(collateral_amount, value2);
    
    const asset used_debt = asset(used_debt_amount, BUCK);
    const asset used_collateral = asset(used_collateral_amount, REX);
    
    // PRINT("\nbad debt", bad_debt)
    // PRINT("bailable", bailable)
    // PRINT("used d", used_debt_amount)
    // PRINT("use c", used_collateral_amount)
    // PRINT_("")
    
    // if (used_debt_amount <= 0) {
    //   PRINT_("L3")
    //   liquidator_itr->p();
    //   buy_r(_cdp.require_find(liquidator_itr->id));
    //   liquidator_itr++;
    //   continue;
    // }
    
    // PRINT("used_debt", used_debt_amount)
    
    const bool removed = debtor_itr->debt == used_debt;
    if (removed) {
      PRINT("removing debtor", debtor_itr->id)
      debtor_index.erase(debtor_itr);
    }
    else {
      PRINT_("updating debtor")
      debtor_index.modify(debtor_itr, same_payer, [&](auto& r) {
        r.collateral -= used_collateral;
        r.debt -= used_debt;
      });
      
      // sanity check
      check(debtor_itr->debt.amount >= 0, "programmer error, debt can't go below 0");
      check(debtor_itr->collateral.amount >= 0, "programmer error, collateral can't go below 0");
      
    }
    
    liquidator_index.modify(liquidator_itr, same_payer, [&](auto& r) {
      r.collateral += used_collateral;
      r.debt += used_debt;
    });
    
    // sanity check
    check(liquidator_itr->debt.amount >= 0, "programmer error, debt can't go below 0");
    check(liquidator_itr->collateral.amount >= 0, "programmer error, collateral can't go below 0");
    
    // PRINT_(".\n")
    // liquidator_itr = liquidator_index.begin();
  }
}