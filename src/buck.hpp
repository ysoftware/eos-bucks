// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

CONTRACT buck : public contract {
  public:
    using contract::contract;
    
    buck(eosio::name receiver, eosio::name code, datastream<const char*> ds);
    
    // user
    ACTION open(const name& account, double ccr, double acr);
    ACTION closecdp(uint64_t cdp_id);
    ACTION change(uint64_t cdp_id, const asset& change_debt, const asset& change_collateral);
    ACTION changeacr(uint64_t cdp_id, double acr);
    
    ACTION transfer(const name& from, const name& to, const asset& quantity, const std::string& memo);
    ACTION redeem(const name& account, const asset& quantity);
    
    ACTION run(uint64_t max);
    
    // admin
    ACTION update(double eos_price);
    ACTION process(uint8_t kind);
    
    #if DEBUG
    ACTION zdestroy();
    #endif
    
    [[eosio::on_notify("eosio.token::transfer")]]
    void notify_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo);
    
  private:
      
    enum ProcessKind: uint8_t { bought_rex = 0, sold_rex = 1, reparam = 2, 
                                closing = 3, redemption = 4 };

    TABLE account {
      asset balance;
    
      uint64_t primary_key() const { return balance.symbol.code().raw(); }
    };
    
    TABLE currency_stats {
      asset       supply;
      asset       max_supply;
      name        issuer;
      
      // oracle updates
      time_point  oracle_timestamp;
      double      oracle_eos_price;
      
      // liquidation
      time_point  liquidation_timestamp;
      
      // taxation
      asset   gathered_fees;
      asset   total_collateral;
      asset   aggregated_collateral;
      
      uint64_t primary_key() const { return supply.symbol.code().raw(); }
    };
    
    TABLE close_req {
      uint64_t    cdp_id;
      time_point  timestamp;
      
      uint64_t primary_key() const { return cdp_id; }
    };
    
    TABLE redeem_req {
      name        account;
      asset       quantity;
      time_point  timestamp;
      
      uint64_t primary_key() const { return account.value; }
    };
    
    TABLE reparam_req {
      uint64_t    cdp_id;
      asset       change_collateral;
      asset       change_debt;
      time_point  timestamp;
      bool        isPaid;
      
      uint64_t primary_key() const { return cdp_id; }
    };
    
    TABLE rex_processing {
      uint64_t  cdp_id; // can also contain redeemer account.value; to-do rename
      asset     current_balance;
      
      uint64_t primary_key() const { return cdp_id; }
    };
    
    TABLE redeem_processing {
      uint64_t  cdp_id;
      asset     collateral;
      asset     rex;
      name      account;
      
      uint64_t primary_key() const { return cdp_id; }
    };
    
    TABLE cdp_maturity_req {
      uint64_t        cdp_id;
      asset           change_debt;
      asset           add_collateral;
      double          ccr;
      time_point_sec  maturity_timestamp;
      
      uint64_t primary_key() const { return cdp_id; }
      uint64_t by_time() const { return maturity_timestamp.utc_seconds; }
    };
    
    TABLE cdp {
      uint64_t    id;
      double      acr;
      name        account;
      asset       debt;
      asset       rex_dividends;
      asset       collateral;
      asset       rex;
      time_point  timestamp;
      
      uint64_t primary_key() const { return id; }
      uint64_t by_account() const { return account.value; }
      
      // index to search for liquidators with the highest ability to bail out bad debt
      double liquidator() const {
        const double MAX = 100;
        
        if (acr == 0 || collateral.amount == 0 || rex.amount == 0) {
          return MAX * 3; // end of the table
        }
        
        double c = (double) collateral.amount;
        
        if (debt.amount == 0) {
          return MAX - c / acr; // descending c/acr
        }
        
        double cd = c / (double) debt.amount;
        return MAX * 2 - (cd - acr); // descending cd-acr 
      }
      
      // index to search for debtors with highest ccr
      double debtor() const {
        const double MAX = 100;
        
        if (debt.amount == 0 || rex.amount == 0) {
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
    
    typedef multi_index<"maturityreq"_n, cdp_maturity_req,
      indexed_by<"bytimestamp"_n, const_mem_fun<cdp_maturity_req, uint64_t, &cdp_maturity_req::by_time>>
        > cdp_maturity_req_i;
    
    typedef multi_index<"rexprocess"_n, rex_processing> rex_processing_i;
    typedef multi_index<"redprocess"_n, redeem_processing> red_processing_i;
    
    typedef multi_index<"cdp"_n, cdp,
      indexed_by<"debtor"_n, const_mem_fun<cdp, double, &cdp::debtor>>,
      indexed_by<"liquidator"_n, const_mem_fun<cdp, double, &cdp::liquidator>>,
      indexed_by<"byaccount"_n, const_mem_fun<cdp, uint64_t, &cdp::by_account>>
        > cdp_i;
    
    // rex 
    
   struct rex_fund {
      uint8_t version = 0;
      name    owner;
      asset   balance;

      uint64_t primary_key()const { return owner.value; }
   };

    struct rex_balance {
      uint8_t version = 0;
      name    owner;
      asset   vote_stake; /// the amount of CORE_SYMBOL currently included in owner's vote
      asset   rex_balance; /// the amount of REX owned by owner
      int64_t matured_rex = 0; /// matured REX available for selling
      std::deque<std::pair<time_point_sec, int64_t>> rex_maturities; /// REX daily maturity buckets

      uint64_t primary_key() const { return owner.value; }
   };

    typedef multi_index<"rexbal"_n, rex_balance> rex_balance_i;
    typedef multi_index<"rexfund"_n, rex_fund> rex_fund_i;
    
    // methods
    void init();
    void add_balance(const name& owner, const asset& value, const name& ram_payer, bool change_supply);
    void sub_balance(const name& owner, const asset& value, bool change_supply);
    void add_fee(const asset& value);
    void distribute_tax(uint64_t cdp_id);
    
    void run_requests(uint64_t max);
    void run_liquidation(uint64_t max);
    
    inline void inline_transfer(const name& account, const asset& quantity, const std::string& memo, const name& contract);
    inline void inline_process(ProcessKind kind);
    
    void buy_rex(uint64_t cdp_id, const asset& quantity);
    void sell_rex(uint64_t cdp_id, const asset& quantity, ProcessKind kind);
    void sell_rex_redeem(const asset& quantity);
    
    // getters
    double get_eos_price() const;
    double get_ccr(const asset& collateral, const asset& debt) const;
    bool is_mature(uint64_t cdp_id) const;
    time_point_sec get_maturity() const;
    asset get_rex_balance() const;
    asset get_eos_rex_balance() const;
    
    // tables
    cdp_i               _cdp;
    stats_i             _stat;
    close_req_i         _closereq;
    reparam_req_i       _reparamreq;
    redeem_req_i        _redeemreq;
    cdp_maturity_req_i  _maturityreq;
    rex_processing_i    _rexprocess;
    red_processing_i    _redprocess;
};