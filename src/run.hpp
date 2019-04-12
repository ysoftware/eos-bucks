// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::run(uint64_t max) {
  // check if liquidation complete for this round
  if (_stat.begin()->liquidation_timestamp == _stat.begin()->oracle_timestamp) {
    run_requests(max);
  }
  else {
    run_liquidation(max);
  }
}

void buck::run_requests(uint64_t max) {
  PRINT_("running requests")
  PRINT("time", time_point_sec{ current_time_point() }.utc_seconds)
  
  uint64_t processed = 0;
  time_point_sec cts{ current_time_point() };
  auto price = get_eos_price();
  
  check(_stat.begin() != _stat.end(), "contract is not yet initiated");
  auto oracle_timestamp = _stat.begin()->oracle_timestamp;
  
  auto maturity_index = _maturityreq.get_index<"bytimestamp"_n>();
  auto debtor_index = _cdp.get_index<"debtor"_n>(); // to-do make sure index is correct here (liquidation debtor)!
  
  auto close_item = _closereq.begin();
  auto reparam_item = _reparamreq.begin();
  // to-do redeem sorting
  auto redeem_item = _redeemreq.begin();
  
  auto debtor_item = debtor_index.begin();
  auto maturity_item = maturity_index.begin();
  
  // loop until any requests exist and not over limit
  while (processed < max) {
    processed++;
    
    // close request
    if (close_item != _closereq.end() && close_item->timestamp < oracle_timestamp) {
      PRINT_("closing cdp")
      
      // find cdp, should always exist
      auto& cdp_item = _cdp.get(close_item->cdp_id, "to-do: remove. no cdp for this close request");
      
      // sell rex
      sell_rex(cdp_item.id, cdp_item.collateral, ProcessKind::closing);
      
      close_item++;
    }
    
    // redeem request
    if (redeem_item != _redeemreq.end() && redeem_item->timestamp < oracle_timestamp) {
      PRINT("redeeming", redeem_item->account)
      
      // to-do sorting
      // to-do verify timestamp
      auto redeem_item = _redeemreq.begin();
      auto redeem_quantity = redeem_item->quantity;
      asset rex_return = asset(0, REX);
      asset collateral_return = asset(0, EOS);
      
      while (redeem_quantity.amount > 0 && debtor_item != debtor_index.end() && debtor_item->debt.amount > 0) {
        auto next_debtor = debtor_item; // to-do remove this 
        next_debtor++; 
        
        uint64_t using_debt_amount = std::min(redeem_quantity.amount, debtor_item->debt.amount);
        uint64_t using_collateral_amount = ceil((double) using_debt_amount / (price + RF));
        uint64_t using_rex_amount =  debtor_item->rex.amount * using_collateral_amount / debtor_item->collateral.amount;
        
        asset using_debt = asset(using_debt_amount, BUCK);
        asset using_collateral = asset(using_collateral_amount, EOS);
        asset using_rex = asset(using_rex_amount, REX);
        
        redeem_quantity -= using_debt;
        rex_return += using_rex;
        collateral_return += using_collateral;
        
        // to receive dividends after selling rex
        _redprocess.emplace(_self, [&](auto& r) {
          r.account = redeem_item->account;
          r.cdp_id = debtor_item->id;
          r.collateral = using_collateral;
          r.rex = using_rex;
        });
        
        debtor_index.modify(debtor_item, same_payer, [&](auto& r) {
          r.debt -= using_debt;
          r.collateral -= using_collateral;
          r.rex -= using_rex;
        });

        // next best debtor will be the first in table (after this one changed)
        debtor_item = debtor_index.begin();
      }
      
      // check if all bucks have been redeemed
      if (redeem_quantity.amount > 0) {
        add_balance(redeem_item->account, redeem_quantity, _self, true);
      }
      
      if (collateral_return.amount == 0) {
  
        // to-do completely failed to redeem. what to do?
        redeem_item = _redeemreq.erase(redeem_item);
      }
      else {
        sell_rex(redeem_item->account.value, rex_return, ProcessKind::redemption);
        
        // transfer after selling rex
        inline_transfer(redeem_item->account, collateral_return, "bucks redemption", EOSIO_TOKEN);
        
        // next request (removed in process)
        redeem_item++;
      }
    }
    
    // reparam request
    if (reparam_item != _reparamreq.end() && reparam_item->timestamp < oracle_timestamp) {
      
      // to-do:
      // remove old unpaid requests in ~30 minutes. no need to return buck cuz unpaid
      // also remove maturity request if exists, and then skip
      
      // look for a first paid request
      while (reparam_item != _reparamreq.end() && !reparam_item->isPaid) { reparam_item++; }
      if (reparam_item != _reparamreq.end() && reparam_item->isPaid) {
        PRINT_("reparametrizing cdp")
        
        // find cdp
        auto cdp_item = _cdp.require_find(reparam_item->cdp_id);
        bool shouldRemove = true;
        
        asset change_debt = asset(0, BUCK);
        asset new_collateral = cdp_item->collateral;
        auto ccr = get_ccr(cdp_item->collateral, cdp_item->debt);
  
        // adding debt
        if (reparam_item->change_debt.amount > 0) {
          PRINT_("adding debt")
          
          double ccr_cr = ((ccr / CR) - 1) * (double) cdp_item->debt.amount;
          double issue_debt = (double) reparam_item->change_debt.amount;
          uint64_t change_amount = (uint64_t) ceil(fmin(ccr_cr, issue_debt));
          change_debt = asset(change_amount, BUCK);
          
          // take issuance fee
          uint64_t fee_amount = change_amount * IF;
          asset fee = asset(fee_amount, BUCK);
          add_fee(fee);
          
          PRINT("ccr", ccr)
          PRINT("cdp_item->debt.amount", cdp_item->debt.amount)
          PRINT("ccr_cr", ccr_cr)
          PRINT("issue_debt", issue_debt)
          PRINT("change_debt", change_debt)
          PRINT("fee", fee)
          
          add_balance(cdp_item->account, change_debt - fee, same_payer, true);
        }
        
        // removing debt
        else if (reparam_item->change_debt.amount < 0) {
          change_debt = reparam_item->change_debt; // add negative value
        }
        
        // adding collateral
        if (reparam_item->change_collateral.amount > 0) {
          new_collateral += reparam_item->change_collateral;
          
          // open maturity request
          auto& maturity_item = _maturityreq.get(cdp_item->id, "to-do: remove. could not find maturity (run)");
          _maturityreq.modify(maturity_item, same_payer, [&](auto& r) {
            r.maturity_timestamp = get_maturity();
            r.add_collateral = reparam_item->change_collateral;
            r.cdp_id = cdp_item->id;
            r.change_debt = change_debt;
            r.ccr = 0;
          });
          
          // buy rex for this cdp
          buy_rex(cdp_item->id, reparam_item->change_collateral);
        }
        
        // removing collateral
        else if (reparam_item->change_collateral.amount < 0) {
        
          auto cr_ccr = CR / ccr;
          auto cwe = (double) -reparam_item->change_collateral.amount / (double) cdp_item->collateral.amount;
          auto change = asset(ceil(fmin(cr_ccr, cwe) * cdp_item->collateral.amount), EOS);
          new_collateral -= change;
          
          sell_rex(cdp_item->id, -reparam_item->change_collateral, ProcessKind::reparam);
          shouldRemove = false;
        }
        
        // to-do check new ccr parameters
        // don't give debt if ccr < CR
  
        // not removing collateral here, update immediately
        if (reparam_item->change_collateral.amount == 0) {
          PRINT_("modifying cdp")
          _cdp.modify(cdp_item, same_payer, [&](auto& r) {
            r.collateral = new_collateral;
            r.debt += change_debt;
          });
        }
        
        // remove request
        if (shouldRemove) {
          PRINT_("removing request")
          reparam_item = _reparamreq.erase(reparam_item);
        }
        else {
          reparam_item++;
        }
        
        break; // done with reparam round 
      }
    }
    
    // maturity requests (issue bucks, add/remove cdp debt, add collateral)
    if (maturity_item != maturity_index.end()) {
      
      // look for a first valid request
      while (maturity_item != maturity_index.end() && !(maturity_item->maturity_timestamp < cts && maturity_item->maturity_timestamp.utc_seconds != 0)) { maturity_item++; }
      if (maturity_item != maturity_index.end() && maturity_item->maturity_timestamp < cts && maturity_item->maturity_timestamp.utc_seconds != 0) {
        PRINT_("running maturity request")
        
        // remove cdp if all collateral is 0 (and cdp was just created)
        
        auto& cdp_item = _cdp.get(maturity_item->cdp_id, "to-do: remove. no cdp for this maturity");
        
        // to-do take fees
        
        // calculate new debt and collateral
        auto change_debt = maturity_item->change_debt; // changing debt explicitly (or 0)
        auto add_collateral = maturity_item->add_collateral;
        
        if (maturity_item->ccr > 0) {
          // opening cdp 
          
          // issue debt
          auto price = get_eos_price();
          auto debt_amount = (price * (double) add_collateral.amount / maturity_item->ccr);
          change_debt = asset(floor(debt_amount), BUCK);
        }
        
        _cdp.modify(cdp_item, same_payer, [&](auto& r) {
          r.collateral += add_collateral;
          r.debt += change_debt;
        });
        
        if (change_debt.amount > 0) {
          
          // take issuance fee
          uint64_t fee_amount = change_debt.amount * IF;
          asset fee = asset(fee_amount, BUCK);
          add_fee(fee);
          
          add_balance(cdp_item.account, change_debt - fee, cdp_item.account, true);
        }
        
        // remove request
        maturity_item = maturity_index.erase(maturity_item);
        break; // done with maturity round
      }
    }
  }
}

void buck::run_liquidation(uint64_t max) {
  PRINT("running liquidation, max", max)
  uint64_t processed = 0;
  auto eos_price = get_eos_price();
  
  auto debtor_index = _cdp.get_index<"debtor"_n>();
  auto liquidator_index = _cdp.get_index<"liquidator"_n>();
  
  auto debtor_item = debtor_index.begin();
  auto liquidator_item = liquidator_index.begin();
  
  // loop through debtors
  while (debtor_item != debtor_index.end() && processed < max) {
    
    double debt = (double) debtor_item->debt.amount;
    double debtor_ccr = (double) debtor_item->collateral.amount * eos_price / debt;
    
    // this and all further debtors don't have any bad debt
    if (debtor_ccr >= CR) {
      
      _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
        r.liquidation_timestamp = _stat.begin()->oracle_timestamp;
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
        
        // to-do allow requests to run but still look for liquidation opportunity
        // to-do how about this? run_requests(max - processed);
        
        // to-do or instead of liquidation_timestamp use 
        // liquidation_status: complete / processing / no available liquidators
        
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
      
      // to-do change rounding
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