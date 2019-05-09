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
      uint64_t  e_balance;
    
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
      
      // actual processed taxes
      uint64_t insurance_pool;
      uint64_t savings_pool;
      
      // insurance
      uint64_t r_total;
      uint64_t r_aggregated;
      uint64_t r_collected; // BUCK this round
      
      // savings 
      uint64_t e_supply;
      uint64_t e_price;
      uint64_t e_collected; // REX this round
      
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
    
    struct processing {
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
      uint64_t  id;
      uint16_t  acr;
      name      account;
      asset     debt;
      asset     collateral;
      uint32_t  modified_round; // accrual round
      
      void p() const {
        eosio::print("#");eosio::print(id);
        eosio::print(" c: ");eosio::print(collateral);
        eosio::print(" d: ");eosio::print(debt);
        eosio::print(" acr: ");eosio::print(acr);
        eosio::print(" time: ");eosio::print(modified_round);
        eosio::print("\n");
      }
      
      uint64_t primary_key() const { return id; }
      uint64_t by_account() const { return account.value; }
      
      uint64_t by_accrued_time() const { 
        
        if (debt.amount == 0 || collateral.amount == 0) return UINT64_MAX;
        
        return modified_round;
      }
      
      // index to search for liquidators with the highest ability to bail out bad debt
      uint64_t liquidator() const {
        
        static const uint64_t MAX = 100'000'000'000'000;
        
        if (acr == 0 || collateral.amount == 0) return UINT64_MAX; // end of the table
        
        if (debt.amount == 0) return MAX + uint128_t(collateral.amount) * 100'000'000 / acr; // ascending c/acr
        
        const uint64_t cd = uint128_t(collateral.amount) * 100'000'000 / debt.amount;
        return MAX * 2 - cd / acr; // ascending cd/acr
      }
      
      // index to search for debtors with highest ccr
      uint64_t debtor() const {
        
        if (debt.amount == 0 || collateral.amount == 0) return UINT64_MAX; // end of the table
        
        const uint64_t cd = uint128_t(collateral.amount) * 100'000'000 / debt.amount;
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
    
    struct rex_pool {
      uint8_t    version;
      asset      total_lent; /// total amount of CORE_SYMBOL in open rex_loans
      asset      total_unlent; /// total amount of CORE_SYMBOL available to be lent (connector)
      asset      total_rent; /// fees received in exchange for lent  (connector)
      asset      total_lendable; /// total amount of CORE_SYMBOL that have been lent (total_unlent + total_lent)
      asset      total_rex; /// total number of REX shares allocated to contributors to total_lendable
      asset      namebid_proceeds; /// the amount of CORE_SYMBOL to be transferred from namebids to REX pool
      uint64_t   loan_num; /// increments with each new loan
      
      uint64_t primary_key() const { return 0; }
    };
  
    typedef multi_index<"rexbal"_n, rex_balance> rex_balance_i;
    typedef multi_index<"rexfund"_n, rex_fund> rex_fund_i;
    typedef multi_index<"rexpool"_n, rex_pool> rex_pool_i;
    
    // test time
    
    #if TEST_TIME
    TABLE time_test {
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
    void buy_r(const cdp_i::const_iterator& cdp_itr);
    void sell_r(const cdp_i::const_iterator& cdp_itr);
    void update_supply(const asset& quantity);
    
    void run_requests(uint8_t max);
    void run_liquidation(uint8_t max);
    void set_liquidation_status(LiquidationStatus status);
    void set_processing_status(ProcessingStatus status);
    
    inline void inline_transfer(const name& account, const asset& quantity, const std::string& memo, const name& contract);
    
    void buy_rex(const name& account, const asset& quantity);
    void sell_rex(const name& account, const asset& quantity);
    void process_maturities(const fund_i::const_iterator& fund_itr);
    
    // getters
    int64_t to_buck(int64_t quantity) const;
    int64_t to_rex(int64_t quantity, int64_t tax) const;
    uint32_t get_eos_usd_price() const;
    asset get_rex_balance() const;
    asset get_eos_rex_balance() const;
    ProcessingStatus get_processing_status() const;
    LiquidationStatus get_liquidation_status() const;
    time_point get_current_time_point() const;
    time_point_sec current_time_point_sec() const;
    time_point_sec get_maturity() const;
    bool is_mature(uint64_t cdp_id) const;
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