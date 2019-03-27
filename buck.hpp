// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

#include <cmath>
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>

using namespace eosio;

CONTRACT buck : public contract {
  public:
    using contract::contract;
    buck(eosio::name receiver, eosio::name code, datastream<const char*> ds)
      :contract(receiver, code, ds) {}
    
    ACTION transfer(name from, name to, asset quantity, std::string memo);
    ACTION open(name account, double ccr, double acr);
    ACTION zdestroy();
    
    void receive_transfer(name from, name to, asset quantity, std::string memo);
    
  private:
  
   TABLE account {
      asset balance;
    
      uint64_t primary_key() const { return balance.symbol.code().raw(); }
    };
    
    TABLE currency_stats {
      asset     supply;
      asset     max_supply;
      name      issuer;
    
      uint64_t primary_key() const { return supply.symbol.code().raw(); }
    };
    
    TABLE cdp {
      uint64_t  id;
      double    acr;
      double    cr_sort;
      name      account;
      asset     debt;
      asset     collateral;
      uint64_t  timestamp;
      
      uint64_t primary_key() const { return id; }
      double by_cr() const { return cr_sort; }
      uint64_t by_account() const { return account.value; }
    };
    
    typedef multi_index<"accounts"_n, account> accounts_i;
    typedef multi_index<"stat"_n, currency_stats> stats_i;
    
    typedef multi_index<"cdp"_n, cdp,
      indexed_by<"bycr"_n, const_mem_fun<cdp, double, &cdp::by_cr>>,
      indexed_by<"byaccount"_n, const_mem_fun<cdp, uint64_t, &cdp::by_account>>
        > cdp_i;
    
    double get_eos_price();
    double get_buck_price();
    
    void add_balance(name owner, asset value, name ram_payer);
    void sub_balance(name owner, asset value);
};