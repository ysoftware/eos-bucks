// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

const eosio::symbol& EOS = eosio::symbol{"EOS", 4};
const eosio::symbol& BUCK = eosio::symbol{"BUCK", 4};
const eosio::name& EOSIO_TOKEN = "eosio.token"_n;

const double CR = 1.5;
const double IF = 0.005;
const double LF = 0.005;
const double RF = 0.005;

const eosio::asset& MIN_COLLATERAL = eosio::asset(50000, EOS);
const eosio::asset& MIN_DEBT = eosio::asset(500000, BUCK);