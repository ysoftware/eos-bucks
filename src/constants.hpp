// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

/// defines if contract uses test rex environment
#define REX_TESTING true
/// defines if contract will print logs and enable debug features
#define DEBUG true

static constexpr uint32_t seconds_per_day = 24 * 3600;

static const symbol& EOS   = symbol("EOS", 4);
static const symbol& BUCK  = symbol("BUCK", 4);
static const symbol& REX   = symbol("REX", 4);

static const asset& ZERO_EOS   = asset(0, EOS);
static const asset& ZERO_BUCK  = asset(0, BUCK);
static const asset& ZERO_REX   = asset(0, REX);

/// minimal collateral is 5 EOS
static const asset& MIN_COLLATERAL = asset(50000, EOS);

/// minimal cdp debt is 50 BUCK
static const asset& MIN_DEBT       = asset(500000, BUCK);

static const name& EOSIO_TOKEN = "eosio.token"_n;
static const name& EOSIO       = "eosio"_n;
static const name& SCRUGE      = "scrugescruge"_n;

#if REX_TESTING
/// account on jungle/local test net
static const name& REX_ACCOUNT  = "rexrexrexrex"_n;
#else
static const name& REX_ACCOUNT  = EOSIO;
#endif

const double CR = 1.5;      /// minimal collateral ratio
const double IF = 0.02;     /// issuance tax
const double LF = 0.1;      /// liquidation fee
const double LT = 0.025;    /// liquidation tax
const double RF = 0.01;     /// redemption fee
const double CF = 0.0125;   /// creator fee
const double TT = 0.00085;  /// transaction tax
const double SP = 0.12;     /// part of taxes for scruge