#include <rexmock.hpp>

const uint64_t rex_multiplier = 100;

ACTION rexmock::deposit( const name& owner, const asset& amount ) {
  rex_fund_table _rexfunds(_self, _self.value);
  auto itr = _rexfunds.find( owner.value );
  if ( itr == _rexfunds.end() ) {
     _rexfunds.emplace( owner, [&]( auto& fund ) {
        fund.owner   = owner;
        fund.balance = amount;
     });
  } else {
     _rexfunds.modify( itr, same_payer, [&]( auto& fund ) {
        fund.balance += amount;
     });
  }
}

ACTION rexmock::withdraw( const name& owner, const asset& amount ) {
  rex_fund_table _rexfunds(_self, _self.value);
  auto itr = _rexfunds.require_find( owner.value, "must deposit to REX fund first" );
  check(itr->balance >= amount, "overdrawn deposit balance");
  _rexfunds.modify( itr, same_payer, [&]( auto& fund ) {
     fund.balance -= amount;
  });
}

ACTION rexmock::buyrex( const name& from, const asset& amount ) {
  rex_balance_table _rexbalance(_self, _self.value);
  auto bitr = _rexbalance.find( from.value );
  if ( bitr == _rexbalance.end() ) {
     bitr = _rexbalance.emplace( from, [&]( auto& rb ) {
        rb.owner       = from;
        rb.rex_balance = asset(amount.amount * rex_multiplier, eosio::symbol{"REX", 4});
     });
  } else {
     _rexbalance.modify( bitr, same_payer, [&]( auto& rb ) {
        rb.rex_balance.amount += amount.amount * rex_multiplier;
     });
  }
  
  withdraw(from, amount);
}

ACTION rexmock::sellrex( const name& from, const asset& rex ) {
  rex_balance_table _rexbalance(_self, _self.value);
  auto bitr = _rexbalance.require_find( from.value, "must buy some REX first" );
  check(bitr->rex_balance >= rex, "overdrawn REX balance");
  
  _rexbalance.modify( bitr, same_payer, [&]( auto& rb ) {
    rb.rex_balance.amount -= rex.amount;
  });
  
  deposit(from, asset(rex.amount / rex_multiplier, eosio::symbol{"EOS", 4}));
}
