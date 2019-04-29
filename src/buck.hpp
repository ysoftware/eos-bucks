// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

CONTRACT buck : public contract {
  public:
    using contract::contract;
         
          buck(eosio::name receiver, eosio::name code, datastream<const char*> ds);
    
    // user
    ACTION withdraw(const name& from, const asset& quantity);
    ACTION open(const name& account, const asset& quantity, uint16_t ccr, uint16_t acr);
    ACTION change(uint64_t cdp_id, const asset& change_debt, const asset& change_collateral);
    ACTION changeacr(uint64_t cdp_id, uint16_t acr);
    ACTION close(uint64_t cdp_id);
    ACTION redeem(const name& account, const asset& quantity);
    ACTION transfer(const name& from, const name& to, const asset& quantity, const std::string& memo);
    ACTION save(const name& account, const asset& value);
    ACTION take(const name& account, const asset& value);
    ACTION run(uint8_t max);
    
    // admin
    ACTION update(uint32_t eos_price);
    ACTION processrex();
  
    #if TEST_TIME
    ACTION zmaketime(uint64_t seconds);
    #endif
    
    #if DEBUG
    ACTION zdestroy();
    #endif
    
    [[eosio::on_notify("eosio.token::transfer")]]
    void notify_transfer(const name& from, const name& to, const asset& quantity, const std::string& memo);
    
  private:
    
    enum LiquidationStatus: uint8_t { liquidation_complete = 0, failed = 1, processing_liquidation = 2 }; 
    
    enum ProcessingStatus: uint8_t { processing_complete = 0, processing_redemption_requests = 1, processing_cdp_requests = 2 };

    TABLE account {
      asset     balance;
      
      asset     savings;
      uint32_t  withdrawn_round;
    
      uint64_t primary_key() const { return balance.symbol.code().raw(); }
    };
    
    TABLE currency_stats {
      asset       supply;
      asset       max_supply;
      name        issuer;
      
      // oracle updates
      time_point  oracle_timestamp;
      uint32_t    oracle_eos_price;
      
      // 2 bits for liquidation status
      // 2 bits for requests status
      uint8_t     processing_status;
      
      uint64_t primary_key() const { return supply.symbol.code().raw(); }
    };
    
    TABLE taxation_stats {
      uint32_t  current_round;
      
      // actual processed taxes
      asset     insurance_pool;
      asset     savings_pool;
      
      // taxes added in this round
      asset     collected_insurance;     
      asset     collected_savings;  
      
      // current amount of money in the system
      asset     total_excess;
      asset     total_bucks;
      
      // aggregated amount of money
      asset     aggregated_excess;
      asset     aggregated_bucks;
      
      // money added in this round
      asset     changed_excess;
      asset     changed_bucks;
      
      uint64_t primary_key() const { return 0; }
    };
    
    TABLE close_req {
      uint64_t    cdp_id;
      time_point  timestamp;
      
      uint64_t primary_key() const { return cdp_id; }
    };
    
    TABLE fund {
      name  account;
      asset balance;
      int64_t matured_rex = 0;
      std::deque<std::pair<time_point_sec, int64_t>> rex_maturities;
      
      uint64_t primary_key() const { return account.value; }
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
      
      uint64_t primary_key() const { return cdp_id; }
    };
    
    TABLE processing {
      asset current_balance;
      name  account;
      
      uint64_t primary_key() const { return 0; }
    };
    
    TABLE cdp_maturity_req {
      uint64_t        cdp_id;
      asset           change_debt;
      asset           add_collateral;
      uint16_t        ccr;
      time_point      maturity_timestamp;
      
      uint64_t primary_key() const { return cdp_id; }
      uint64_t by_time() const { return time_point_sec(maturity_timestamp).utc_seconds; }
    };
    
    TABLE cdp {
      uint64_t    id;
      uint16_t    acr;
      name        account;
      asset       debt;
      asset       accrued_debt;
      asset       collateral;
      time_point  accrued_timestamp;
      uint32_t    modified_round;
      
      uint64_t primary_key() const { return id; }
      uint64_t by_account() const { return account.value; }
      
      uint64_t by_accrued_time() const {
        if (debt.amount + accrued_debt.amount == 0) return UINT64_MAX;
        return time_point_sec(accrued_timestamp).utc_seconds; 
      }
      
      // index to search for liquidators with the highest ability to bail out bad debt
      uint64_t liquidator() const {
        
        static const uint64_t MAX = 100'000'000;
        const uint64_t c = collateral.amount;
        
        if (acr == 0 || c == 0) return MAX * 3; // end of the table
        
        const uint64_t td = (debt + accrued_debt).amount;
        if (td == 0) return MAX - c / acr; // descending c/acr
        
        const uint64_t cd = c * 10'000'000 / td;
        return MAX * 2 - cd; // descending cd
      }
      
      // index to search for debtors with highest ccr
      uint64_t debtor() const {
        
        const uint64_t td = (debt + accrued_debt).amount;
        if (td == 0) return UINT64_MAX; // end of the table
        
        const uint64_t cd = collateral.amount * 10'000'000 / td;
        return cd; // ascending cd
      }
    };
    
    typedef multi_index<"accounts"_n, account> accounts_i;
    typedef multi_index<"stat"_n, currency_stats> stats_i;
    typedef multi_index<"fund"_n, fund> fund_i;
    typedef multi_index<"taxation"_n, taxation_stats> taxation_i;
    
    typedef multi_index<"closereq"_n, close_req> close_req_i;
    typedef multi_index<"reparamreq"_n, reparam_req> reparam_req_i;
    typedef multi_index<"redeemreq"_n, redeem_req> redeem_req_i;
    
    typedef multi_index<"maturityreq"_n, cdp_maturity_req,
      indexed_by<"bytimestamp"_n, const_mem_fun<cdp_maturity_req, uint64_t, &cdp_maturity_req::by_time>>
        > cdp_maturity_req_i;
    
    typedef multi_index<"process"_n, processing> processing_i;
    
    typedef multi_index<"cdp"_n, cdp,
      indexed_by<"debtor"_n, const_mem_fun<cdp, uint64_t, &cdp::debtor>>,
      indexed_by<"liquidator"_n, const_mem_fun<cdp, uint64_t, &cdp::liquidator>>,
      indexed_by<"accrued"_n, const_mem_fun<cdp, uint64_t, &cdp::by_accrued_time>>,
      indexed_by<"byaccount"_n, const_mem_fun<cdp, uint64_t, &cdp::by_account>>
        > cdp_i;
    
    // rex 
    
    struct rex_fund {
      uint8_t version = 0;
      name    owner;
      asset   balance;

      uint64_t primary_key() const { return owner.value; }
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
    
    // test time
    
    #if TEST_TIME
    struct time_test {
      time_point_sec now;
     
      uint64_t primary_key() const { return 0; }
    };
    
    typedef multi_index<"testtime"_n, time_test> time_test_i;
    #endif
    
    // methods
    
    bool init();
    void add_balance(const name& owner, const asset& value, const name& ram_payer, bool change_supply);
    void sub_balance(const name& owner, const asset& value, bool change_supply);
    void add_funds(const name& from, const asset& quantity, const name& ram_payer);
    void sub_funds(const name& from, const asset& quantity);
    
    void process_taxes();
    void add_savings_pool(const asset& value);
    void accrue_interest(const cdp_i::const_iterator& cdp_itr);
    void update_excess_collateral(const asset& value);
    void withdraw_insurance_dividends(const cdp_i::const_iterator& cdp_itr);
    asset withdraw_savings_dividends(const name& account);
    void update_supply(const asset& quantity);
    
    void run_requests(uint8_t max);
    void run_liquidation(uint8_t max);
    void set_liquidation_status(LiquidationStatus status);
    void set_processing_status(ProcessingStatus status);
    
    inline void inline_transfer(const name& account, const asset& quantity, const std::string& memo, const name& contract);
    
    bool check_maturity(const asset& value, const name& account);
    void buy_rex(const name& account, const asset& quantity);
    void sell_rex(const name& account, const asset& quantity);
    void process_maturities(const fund_i::const_iterator& fund_itr);
    
    // getters
    uint32_t get_eos_price() const;
    time_point get_current_time_point() const;
    bool is_mature(uint64_t cdp_id) const;
    time_point_sec get_maturity() const;
    asset get_rex_balance() const;
    asset get_eos_rex_balance() const;
    ProcessingStatus get_processing_status() const;
    LiquidationStatus get_liquidation_status() const;
    time_point_sec current_time_point_sec() const;
    time_point_sec get_amount_maturity(const name& account, const asset& quantity) const;
    
    // tables
    cdp_i               _cdp;
    stats_i             _stat;
    taxation_i          _tax;
    fund_i              _fund;
    close_req_i         _closereq;
    reparam_req_i       _reparamreq;
    redeem_req_i        _redeemreq;
    cdp_maturity_req_i  _maturityreq;
    processing_i        _process;
};