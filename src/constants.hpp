// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

// to-do remove all debug values

const eosio::symbol& EOS = eosio::symbol{"EOS", 4};
const eosio::symbol& BUCK = eosio::symbol{"BUCK", 4};

const double CR = 1.5;
const double IF = 0.005;
const double LF = 0.005;

const eosio::asset& MIN_COLLATERAL = eosio::asset(50000, EOS);