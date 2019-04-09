#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/asset.hpp>
#include <eosio/asset.hpp>

#include <string>
#include <deque>

using namespace eosio;

CONTRACT rexmock : public contract {
  
  public:
    using contract::contract;
    rexmock(eosio::name receiver, eosio::name code, datastream<const char*> ds):contract(receiver, code, ds) {}

    ACTION deposit( const name& owner, const asset& amount );
    
    ACTION withdraw( const name& owner, const asset& amount );
    
    ACTION buyrex( const name& from, const asset& amount );
    
    ACTION sellrex( const name& from, const asset& rex );

  private:
  
    TABLE rex_balance {
      uint8_t version = 0;
      name    owner;
      asset   vote_stake; /// the amount of CORE_SYMBOL currently included in owner's vote
      asset   rex_balance; /// the amount of REX owned by owner
      int64_t matured_rex = 0; /// matured REX available for selling
      std::deque<std::pair<time_point_sec, int64_t>> rex_maturities; /// REX daily maturity buckets
    
      uint64_t primary_key()const { return owner.value; }
    };
    
    typedef eosio::multi_index< "rexbal"_n, rex_balance > rex_balance_table;
    
    TABLE rex_fund {
      uint8_t version = 0;
      name    owner;
      asset   balance;
    
      uint64_t primary_key()const { return owner.value; }
    };
    
    typedef eosio::multi_index< "rexfund"_n, rex_fund > rex_fund_table;
};