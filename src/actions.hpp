// Copyright © Scruge 2019.
// This file is part of Scruge stable coin project.
// Created by Yaroslav Erohin.

void buck::inline_transfer(name account, asset quantity, std::string memo, name contract) {
	action(
		permission_level{ _self, "active"_n },
		contract, "transfer"_n,
		make_tuple(_self, account, quantity, memo)
	).send();
}

void buck::deferred_run(uint64_t max) {
  cancel_deferred("run"_n.value);
	
	transaction t;
	t.actions.emplace_back(permission_level(_self, "active"_n),
									 _self, "run"_n,
									 std::make_tuple(max));
	t.delay_sec = 1;
	t.send("run"_n.value, _self, false);
}