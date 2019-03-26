// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

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
      uint64_t  collateral_requirement;
    
      uint64_t primary_key() const { return supply.symbol.code().raw(); }
    };
    
    TABLE debt_positions {
      uint64_t  id;
      uint64_t  acceptable_ratio;
      name      account;
      asset     debt;
      asset     collateral;
      uint64_t  modify_timestamp;
      
      uint64_t primary_key() const { return id; }
      uint64_t by_acr() const { return acceptable_ratio; }
      uint64_t by_account() const { return account.value; }
    };
    
    typedef eosio::multi_index<"accounts"_n, account> accounts;
    typedef eosio::multi_index<"stat"_n, currency_stats> stats;
    
    typedef multi_index<"positions"_n, debt_positions,
      indexed_by<"byacr"_n, const_mem_fun<debt_positions, uint64_t, &debt_positions::by_acr>>,
      indexed_by<"byaccount"_n, const_mem_fun<debt_positions, uint64_t, &debt_positions::by_account>>
        > positions;
};