// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

static constexpr uint32_t seconds_per_day = 24 * 3600;

/// defines if contract uses test rex environment. should be false when deployed on the main net
const bool REX_TESTING = true;

const symbol& EOS   = eosio::symbol{"EOS", 4};
const symbol& BUCK  = eosio::symbol{"BUCK", 4};
const symbol& REX   = eosio::symbol{"REX", 4};

const asset& MIN_COLLATERAL = eosio::asset(50000, EOS);
const asset& MIN_DEBT       = eosio::asset(500000, BUCK);

const name& EOSIO_TOKEN = "eosio.token"_n;
const name& EOSIO       = "eosio"_n;

const double CR = 1.5;
const double IF = 0.02;   // issuance tax
const double LF = 0.1;    // liquidation fee
const double LT = 0.025;  // liquidation tax
const double RF = 0.01;   // redemption fee
const double CF = 0.0125; // creator fee
const double TT = 0.00085;// transaction tax
