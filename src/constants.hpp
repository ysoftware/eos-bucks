// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

const symbol& EOS = eosio::symbol{"EOS", 4};
const symbol& BUCK = eosio::symbol{"BUCK", 4};
const symbol& REX = eosio::symbol{"REX", 4};

const double CR = 1.5;
const double IF = 0.005;
const double LF = 0.005;
const double RF = 0.005;

const asset& MIN_COLLATERAL = eosio::asset(50000, EOS);
const asset& MIN_DEBT = eosio::asset(500000, BUCK);
const name& EOSIO_TOKEN = "eosio.token"_n;