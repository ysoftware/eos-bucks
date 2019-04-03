// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

#include <cmath>
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/transaction.hpp>

using namespace eosio;

CONTRACT buck : public contract {
  public:
    using contract::contract;
    buck(eosio::name receiver, eosio::name code, datastream<const char*> ds)
      :contract(receiver, code, ds) {}
    
    // user
    ACTION open(name account, double ccr, double acr);
    ACTION closecdp(uint64_t cdp_id);
    ACTION change(uint64_t cdp_id, asset change_debt, asset change_collateral);
    ACTION changeacr(uint64_t cdp_id, double acr);
    
    ACTION transfer(name from, name to, asset quantity, std::string memo);
    ACTION redeem(name account, asset quantity);
    
    ACTION run(uint64_t max);
    
    // admin
    ACTION update(double eos_price);
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
      
      uint64_t  liquidation_timestamp;
      uint64_t  oracle_timestamp;
      double    oracle_eos_price;
    
      uint64_t primary_key() const { return supply.symbol.code().raw(); }
    };
    
    TABLE close_req {
      uint64_t cdp_id;
      uint64_t timestamp;
      
      uint64_t primary_key() const { return cdp_id; }
    };
    
    TABLE redeem_req {
      name      account;
      asset     quantity;
      uint64_t  timestamp;
      
      uint64_t primary_key() const { return account.value; }
    };
    
    TABLE reparam_req {
      uint64_t  cdp_id;
      asset     change_collateral;
      asset     change_debt;
      uint64_t  timestamp;
      bool      isPaid;
      
      uint64_t primary_key() const { return cdp_id; }
    };
    
    TABLE cdp {
      uint64_t  id;
      double    acr;
      name      account;
      asset     debt;
      asset     collateral;
      uint64_t  timestamp;
      
      uint64_t primary_key() const { return id; }
      uint64_t by_account() const { return account.value; }
      
      // index to search for liquidators with highest acr
      double liquidator() const {
        const double MAX = 100;
        
        if (acr == 0 || collateral.amount == 0) {
          return MAX * 3; // end of the table
        }
        
        if (debt.amount == 0) {
          return MAX - acr; // descending acr
        }
        
        double cd = (double) collateral.amount / (double) debt.amount;
        return MAX * 2 - (cd - acr); // descending cd-acr 
      }
      
      // index to search for debtors with highest ccr
      double debtor() const {
        const double MAX = 100;
        
        if (debt.amount == 0) {
          return MAX; // end of the table
        }
        
        double cd = (double) collateral.amount / (double) debt.amount;
        return cd; // ascending cd
      }
    };
    
    typedef multi_index<"accounts"_n, account> accounts_i;
    typedef multi_index<"stat"_n, currency_stats> stats_i;
    
    typedef multi_index<"closereq"_n, close_req> close_req_i;
    typedef multi_index<"reparamreq"_n, reparam_req> reparam_req_i;
    typedef multi_index<"redeemreq"_n, redeem_req> redeem_req_i;
    
    typedef multi_index<"cdp"_n, cdp,
      indexed_by<"debtor"_n, const_mem_fun<cdp, double, &cdp::debtor>>,
      indexed_by<"liquidator"_n, const_mem_fun<cdp, double, &cdp::liquidator>>,
      indexed_by<"byaccount"_n, const_mem_fun<cdp, uint64_t, &cdp::by_account>>
        > cdp_i;
    
    // methods
    void add_balance(name owner, asset value, name ram_payer, bool change_supply);
    void sub_balance(name owner, asset value, bool change_supply);
    
    void run_requests(uint64_t max);
    void run_liquidation(uint64_t max);
    
    void inline_transfer(name account, asset quantity, std::string memo, name contract);
    void deferred_run(uint64_t max);
    
    // getters
    double get_eos_price();
    double get_ccr(asset collateral, asset debt);
};