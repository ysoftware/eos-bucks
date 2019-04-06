// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::distribute_tax(uint64_t cdp_id) {
  
}

void buck::add_fee(asset value) {
  _stat.modify(_stat.begin(), same_payer, [&](auto& r) {
    r.gathered_fees += value;
  });
}