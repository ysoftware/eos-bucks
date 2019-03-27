// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

#define PRINT(x, y) eosio::print(x); eosio::print(": "); eosio::print(y); eosio::print("\n");
#define PRINT_(x) eosio::print(x); eosio::print("\n");

void buck::zdestroy() {
	{
    cdp_i table(_self, _self.value);
    auto item = table.begin();
    while (item != table.end()) {
      item = table.erase(item);
    }
	}
}