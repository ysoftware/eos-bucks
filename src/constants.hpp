// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

/// defines if contract will print logs and enable debug features
#define DEBUG true
/// defines if contract uses test rex environment
#define REX_TESTING true
/// defines if testing of time points is enabled
#define TEST_TIME true

static const uint32_t seconds_per_day     = 86'400;
static const uint32_t BASE_ROUND_DURATION = 1'000;
static const uint32_t YEAR                = 31'557'600;
static const uint32_t ACCRUAL_PERIOD      = 2'629'800;

static const symbol& EOS   = symbol("EOS", 4);
static const symbol& BUCK  = symbol("BUCK", 4);
static const symbol& REX   = symbol("REX", 4);

static const asset& ZERO_BUCK  = asset(0, BUCK);
static const asset& ZERO_REX   = asset(0, REX);

/// minimal collateral is 5 EOS
static const asset& MIN_COLLATERAL = asset(5'0000, REX);

/// minimal cdp debt is 50 BUCK
static const asset& MIN_DEBT       = asset(50'0000, BUCK);

static const name& EOSIO_TOKEN = "eosio.token"_n;
static const name& EOSIO       = "eosio"_n;
static const name& SCRUGE      = "scrugescruge"_n;

#if REX_TESTING
/// account on jungle/local test net
static const name& REX_ACCOUNT  = "rexrexrexrex"_n;
#else
static const name& REX_ACCOUNT  = EOSIO;
#endif

// constants in %

const uint8_t CR = 150; /// minimal collateral ratio
const uint8_t LF = 10;  /// liquidation fee
const uint8_t RF = 1;   /// redemption fee
const uint8_t SP = 20;  /// part of taxes for scruge
const uint8_t SR = 80;   /// savings ratio
const uint8_t IR = 20;   /// insurance ratio
const double AR = 0.05;   /// annual interest rate