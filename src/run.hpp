// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::run_requests(uint64_t max) {
  
}

void buck::run_liquidation() {
  
  auto eos_price = get_eos_price();
  double liquidation_price = eos_price * (1 + LF);
  
  cdp_i positions(_self, _self.value);
  auto debtor_index = positions.get_index<"bycr"_n>();
  auto debtor_item = debtor_index.begin();
  auto liquidator_index = positions.get_index<"byacr"_n>();
  auto liquidator_item = liquidator_index.begin();
  
  // loop through debtors
  while (debtor_item != debtor_index.end()) {
    
    double debt = (double) debtor_item->debt.amount;
    double debtor_ccr = (double) debtor_item->collateral.amount * eos_price / debt;
    
    while (debtor_ccr < CR) {
    
      double liquidator_collateral = (double) liquidator_item->collateral.amount;
      double liquidator_ccr = liquidator_collateral * eos_price / (double) liquidator_item->debt.amount;
      
      double bad_debt = (CR - debtor_ccr) * debt;
      double bailable = (liquidator_collateral / (liquidator_ccr / liquidator_item->acr) - 1) * liquidation_price;
      
      uint64_t used_debt_amount = fmin(bad_debt, bailable);
      uint64_t used_collateral_amount = ceil((CR - debtor_ccr) * debt / liquidation_price);
      
      asset used_debt = asset(used_debt_amount, BUCK);
      asset used_collateral = asset(used_collateral_amount, EOS);
      
      debtor_index.modify(debtor_item, same_payer, [&](auto& r) {
        r.debt -= used_debt;
      });
      
      liquidator_index.modify(liquidator_item, same_payer, [&](auto& r) {
        r.collateral -= used_collateral;
        r.debt += used_debt;
      });
      
      // no more bad debt, continue with next debtor
      if (bad_debt <= bailable) {
        break;
      }
      
      // update values and continue with the next liquidator
      liquidator_item++;
      debt = (double) debtor_item->debt.amount;
      debtor_ccr = (double) debtor_item->collateral.amount * eos_price / debt;
    }
  }
}