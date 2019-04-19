// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::run(uint64_t max) {
  // check liquidation is still going for this round
  if (get_liquidation_status() == LiquidationStatus::processing_liquidation) {
    run_liquidation(max);
  }
  else {
    run_requests(max);
  }
}

void buck::run_requests(uint64_t max) {
  const time_point now = current_time_point();
  const auto price = get_eos_price();
  const auto oracle_timestamp = _stat.begin()->oracle_timestamp;
  auto status = get_processing_status();
  
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
        sell_rex(cdp_itr->id, cdp_itr->collateral, ProcessKind::closing);
        close_itr++; // this request will be removed in process method
        did_work = true;
    }
      
      // reparam request
      if (reparam_itr != _reparamreq.end() && reparam_itr->timestamp < oracle_timestamp) {
      
        // find cdp
        const auto cdp_itr = _cdp.require_find(reparam_itr->cdp_id);
        bool shouldRemove = true;
        
        asset change_debt = ZERO_BUCK;
        asset new_collateral = cdp_itr->collateral;
        const double ccr = get_ccr(cdp_itr->collateral, cdp_itr->debt); // to-do use new ccr or old?
  
        // adding debt
        if (reparam_itr->change_debt.amount > 0) {
          
          const double ccr_cr = ((ccr / CR) - 1) * (double) cdp_itr->debt.amount;
          const double issue_debt = (double) reparam_itr->change_debt.amount;
          uint64_t change_amount = (uint64_t) ceil(fmin(ccr_cr, issue_debt));
          change_debt = asset(change_amount, BUCK);
          
          add_balance(cdp_itr->account, change_debt, same_payer, true);
        }
        
        // removing debt
        else if (reparam_itr->change_debt.amount < 0) {
          change_debt = reparam_itr->change_debt; // add negative value
        }
        
        // adding collateral
        if (reparam_itr->change_collateral.amount > 0) {
          new_collateral += reparam_itr->change_collateral;
          
          // open maturity request
          const auto maturity_itr = _maturityreq.require_find(cdp_itr->id, "to-do: remove. could not find maturity (run)");
          _maturityreq.modify(maturity_itr, same_payer, [&](auto& r) {
            r.maturity_timestamp = get_maturity();
            r.add_collateral = reparam_itr->change_collateral;
            r.cdp_id = cdp_itr->id;
            r.change_debt = change_debt;
            r.ccr = 0;
          });
          
          // buy rex for this cdp
          buy_rex(cdp_itr->id, reparam_itr->change_collateral);
        }
        
        // removing collateral
        else if (reparam_itr->change_collateral.amount < 0) {
        
          const auto cr_ccr = CR / ccr;
          const auto cwe = (double) -reparam_itr->change_collateral.amount / (double) cdp_itr->collateral.amount;
          const auto change = asset(ceil(fmin(cr_ccr, cwe) * cdp_itr->collateral.amount), EOS);
          new_collateral -= change;
          
          sell_rex(cdp_itr->id, -reparam_itr->change_collateral, ProcessKind::reparam);
          shouldRemove = false;
        }
        
        // to-do check new ccr parameters
        // don't give debt if ccr < CR
  
        // not removing collateral here, update immediately
        if (reparam_itr->change_collateral.amount == 0) {
          _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
            r.collateral = new_collateral;
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
          
          // remove cdp if all collateral is 0 (and cdp was just created)
          
          const auto cdp_itr = _cdp.require_find(maturity_itr->cdp_id, "to-do: remove. no cdp for this maturity");
          
          // to-do take fees
          
          // calculate new debt and collateral
          auto change_debt = maturity_itr->change_debt; // changing debt explicitly (or 0)
          const auto add_collateral = maturity_itr->add_collateral;
          
          if (maturity_itr->ccr > 0) {
            // opening cdp
            
            // issue debt
            const auto debt_amount = (price * (double) add_collateral.amount / maturity_itr->ccr);
            change_debt = asset(floor(debt_amount), BUCK);
          }
          
          update_excess_collateral(add_collateral);
          
          _cdp.modify(cdp_itr, same_payer, [&](auto& r) {
            r.collateral += add_collateral;
            r.debt += change_debt;
            r.modified_round = _tax.begin()->current_round;
          });
          
          if (change_debt.amount > 0) {
            
            add_balance(cdp_itr->account, change_debt, cdp_itr->account, true);
          }
          
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
        
        // loop through available debtors until all amount is redeemed or our of debtors
        while (redeem_quantity.amount > 0 && debtor_itr != debtor_index.end() && debtor_itr->debt.amount > 0) {
          const uint64_t using_debt_amount = std::min(redeem_quantity.amount, debtor_itr->debt.amount);
          const uint64_t using_collateral_amount = ceil((double) using_debt_amount / (price + RF));
          const uint64_t using_rex_amount =  debtor_itr->rex.amount * using_collateral_amount / debtor_itr->collateral.amount;
          
          const asset using_debt = asset(using_debt_amount, BUCK);
          const asset using_collateral = asset(using_collateral_amount, EOS);
          const asset using_rex = asset(using_rex_amount, REX);
          
          redeem_quantity -= using_debt;
          rex_return += using_rex;
          collateral_return += using_collateral;
          
          // to receive dividends after selling rex
          _redprocess.emplace(_self, [&](auto& r) {
            r.account = redeem_itr->account;
            r.cdp_id = debtor_itr->id;
            r.collateral = using_collateral;
            r.rex = using_rex;
          });
          
          auto cdp_itr = _cdp.require_find(debtor_itr->id);
          withdraw_insurance(cdp_itr);
          
          debtor_index.modify(debtor_itr, same_payer, [&](auto& r) {
            r.debt -= using_debt;
            r.collateral -= using_collateral;
            r.rex -= using_rex;
          });
  
          // next best debtor will be the first in table (after this one changed)
          debtor_itr = debtor_index.begin();
        }
        
        if (redeem_quantity.amount > 0) {
          
          // return unredeemed amount
          add_balance(redeem_itr->account, redeem_quantity, _self, true);
        }
        
        if (collateral_return.amount == 0) {
    
          // to-do completely failed to redeem. what to do?
          redeem_itr = _redeemreq.erase(redeem_itr);
        }
        else {
          update_excess_collateral(collateral_return);
          sell_rex(redeem_itr->account.value, rex_return, ProcessKind::redemption);
          add_funds(redeem_itr->account, collateral_return, same_payer); // to-do receipt
          redeem_itr++; // this request will be removed in process method
        }
      }
      else {
        // no more redemption requests
        set_processing_status(ProcessingStatus::processing_complete);
        break;
      }
    }
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

void buck::run_liquidation(uint64_t max) {
  uint64_t processed = 0;
  const auto price = get_eos_price();
  
  auto debtor_index = _cdp.get_index<"debtor"_n>();
  auto liquidator_index = _cdp.get_index<"liquidator"_n>();
  
  auto debtor_itr = debtor_index.begin();
  auto liquidator_itr = liquidator_index.begin();
  
  // loop through debtors
  while (debtor_itr != debtor_index.end() && processed < max) {
    
    const double debt = (double) debtor_itr->debt.amount;
    const double debtor_ccr = (double) debtor_itr->collateral.amount * price / debt;
    
    // this and all further debtors don't have any bad debt
    if (debtor_ccr >= CR) {
      
      set_liquidation_status(LiquidationStatus::liquidation_complete);
      run_requests(max - processed);
      return;
    }
    
    double bad_debt = (CR - debtor_ccr) * debt;
    
    // loop through liquidators
    while (bad_debt > 0) {
    
      // to-do check debt not 0
      const double liquidator_collateral = (double) liquidator_itr->collateral.amount;
      const double liquidator_debt = (double) liquidator_itr->debt.amount;
      const double liquidator_acr = liquidator_itr->acr;
      const double liquidator_ccr = liquidator_collateral * price / liquidator_debt;
      
      // this and all further liquidators can not bail out anymore bad debt 
      if (liquidator_acr > 0 && liquidator_ccr <= liquidator_acr || liquidator_itr == liquidator_index.end()) {
        
        set_liquidation_status(LiquidationStatus::failed);
        run_requests(max - processed);
        return;
      }
      
      const double bailable = liquidator_debt == 0 ? 
          liquidator_collateral / liquidator_acr * price :
          liquidator_collateral / ((liquidator_ccr / liquidator_acr) - 1) * price;
      
      const double used_debt_amount = fmin(bad_debt, bailable);
      const double used_collateral_amount = used_debt_amount / (price * (1 - LF));
      
      // to-do check rounding
      const asset used_debt = asset(ceil(used_debt_amount), BUCK);
      const asset used_collateral = asset(ceil(used_collateral_amount), EOS);
      
      debtor_index.modify(debtor_itr, same_payer, [&](auto& r) {
        r.collateral -= used_collateral;
        r.debt -= used_debt;
      });
      
      liquidator_index.modify(liquidator_itr, same_payer, [&](auto& r) {
        r.collateral += used_collateral;
        r.debt += used_debt;
      });
      
      // if liquidator did not bail out all of bad debt, continue with the next one
      if (bad_debt > bailable) { 
        liquidator_itr++;
      }
      
      // update values
      bad_debt -= used_debt_amount;
    }
    
    // continue to the next debtor
    processed++;
    debtor_itr++;
  }
}