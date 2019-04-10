// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::inline_transfer(name account, asset quantity, std::string memo, name contract) {
	action(permission_level{ _self, "active"_n },
		contract, "transfer"_n,
		std::make_tuple(_self, account, quantity, memo)
	).send();
}

void buck::inline_process(ProcessKind kind) {
  action(permission_level{ _self, "active"_n },
    _self, "process"_n, std::make_tuple(kind)
  ).send();
}