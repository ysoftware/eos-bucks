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
    
    // user
    ACTION transfer(name from, name to, asset quantity, std::string memo);
    ACTION open(name account, double ccr, double acr);
    
    // admin
    ACTION update(double eos_price, double buck_price);
    ACTION init();
    
    // debug 
    ACTION zdestroy();
    
    // notify
    void notify_transfer(name from, name to, asset quantity, std::string memo);
    
  private:
  
   TABLE account {
      asset balance;
      asset debt;
    
      uint64_t primary_key() const { return balance.symbol.code().raw(); }
    };
    
    TABLE currency_stats {
      asset     supply;
      asset     max_supply;
      name      issuer;
      
      uint64_t  oracle_timestamp;
      double    oracle_eos_price;
      double    oracle_buck_price;
    
      uint64_t primary_key() const { return supply.symbol.code().raw(); }
    };
    
    TABLE close_req {
      uint64_t cdp_id;
      uint64_t timestamp;
      
      uint64_t primary_key() const { return cdp_id; }
    };
    
    TABLE reparam_req {
      uint64_t cdp_id;
      asset change_collateral;
      asset change_debt;
      uint64_t timestamp;
      bool isPaid;
      
      uint64_t primary_key() const { return cdp_id; }
    };
    
    TABLE cdp {
      uint64_t  id;
      double    acr;
      double    temporary_ccr;
      name      account;
      asset     debt;
      asset     collateral;
      uint64_t  timestamp;
      
      uint64_t primary_key() const { return id; }
      uint64_t by_account() const { return account.value; }
      
      // index to search for liquidators with highest acr
      double liquidator() const {
        if (acr == 0) {
          return DOUBLE_MAX; // end of the table
        }
        
        if (debt.amount == 0) {
          return DOUBLE_MAX - acr; // descending acr 
        }
        
        double cd = (double) collateral.amount / (double) debt.amount;
        return DOUBLE_MAX - (cd - acr); // descending cd-acr 
      }
      
      // index to search for debtors with highest ccr
      double debtor() const {
        return (double) collateral.amount / (double) debt.amount; 
      }
    };
    
    typedef multi_index<"accounts"_n, account> accounts_i;
    typedef multi_index<"stat"_n, currency_stats> stats_i;
    
    typedef multi_index<"closereq"_n, close_req> close_req_i;
    typedef multi_index<"reparamreq"_n, reparam_req> reparam_req_i;
    
    typedef multi_index<"cdp"_n, cdp,
      indexed_by<"debtor"_n, const_mem_fun<cdp, double, &cdp::debtor>>,
      indexed_by<"liquidator"_n, const_mem_fun<cdp, double, &cdp::liquidator>>,
      indexed_by<"byaccount"_n, const_mem_fun<cdp, uint64_t, &cdp::by_account>>
        > cdp_i;
    
    // methods
    void add_balance(name owner, asset value, name ram_payer);
    void sub_balance(name owner, asset value);
    void run_requests(uint64_t max);
    void run_liquidation();
    void inline_transfer(name account, asset quantity, std::string memo, name contract);
    
    // getters
    double get_eos_price();
    double get_buck_price();
};