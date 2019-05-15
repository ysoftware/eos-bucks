# coding: utf8
# Copyright © Scruge 2019.
# This file is part of ScrugeX.
# Created by Yaroslav Erohin.

import unittest
from eosfactory.eosf import *
from methods import *
import eosfactory.core.setup as setup 
from time import sleep
import string
import datetime
import test

verbosity([Verbosity.INFO, Verbosity.OUT, Verbosity.TRACE, Verbosity.DEBUG])

CONTRACT_WORKSPACE = "eos-buck"

# methods

class Test(unittest.TestCase):

	# setup
	setup.is_raise_error = True

	@classmethod
	def tearDownClass(cls):
		stop()

	@classmethod
	def setUpClass(cls):
		reset()

		create_master_account("master")
		create_account("eosio_token", master, "eosio.token")
		create_account("rex", master, "rexrexrexrex")
		
		key = CreateKey(is_verbose=False)
		create_account("buck", master, "buck", key)
		perm(buck, key)

		deploy(Contract(eosio_token, "eosio_token"))
		deploy(Contract(buck, "eos-bucks/src"))
		deploy(Contract(rex, "eos-bucks/tests/rexmock"))

		# Distribute tokens

		create_issue(eosio_token, master, "EOS")

		# Users

		create_account("user1", master, "user1")

		transfer(eosio_token, master, user1, "1000000000000.0000 EOS", "")

	def run(self, result=None):
		super().run(result)

	# tests

	### check rex values when selling rex

	def test(self):
		SCENARIO("Test cdp reparametrization")

		time = 0
		maketime(buck, time)
		update(buck, 100)

		transfer(eosio_token, user1, buck, "1000000000.0000 EOS", "deposit")

		# mature rex
		time += 3_000_000
		maketime(buck, time)
		update(buck, 100)

		open(buck, user1, 0, 200, "10000.0000 REX") # 1000.0000 BUCK
		open(buck, user1, 0, 200, "10000.0000 REX")
		open(buck, user1, 0, 200, "10000.0000 REX")
		open(buck, user1, 0, 200, "10000.0000 REX")
		open(buck, user1, 0, 200, "10000.0000 REX")
		open(buck, user1, 1000, 0, "10000.0000 REX")
		open(buck, user1, 1000, 0, "10000.0000 REX")
		open(buck, user1, 1000, 0, "10000.0000 REX")
		open(buck, user1, 1000, 0, "10000.0000 REX")
		open(buck, user1, 1000, 0, "10000.0000 REX")
		

		##############################
		COMMENT("Reparam liquidator CDPs")

		# remove debt - error
		# reparam(buck, user1, 0, "-50.0000 BUCK", "0.0000 REX")

		# add debt
		reparam(buck, user1, 1, "50.0000 BUCK", "0.0000 REX")

		# remove collateral
		reparam(buck, user1, 2, "0.0000 BUCK", "-1000.0000 REX")

		# add collateral
		reparam(buck, user1, 3, "0.0000 BUCK", "1000.0000 REX")

		# both
		reparam(buck, user1, 4, "50.0000 BUCK", "-1000.0000 REX")

		##############################
		COMMENT("Reparam debtor CDPs")

		# remove debt
		reparam(buck, user1, 5, "-50.0000 BUCK", "0.0000 REX")

		# add debt
		reparam(buck, user1, 6, "50.0000 BUCK", "0.0000 REX")

		# remove collateral
		reparam(buck, user1, 7, "0.0000 BUCK", "-1000.0000 REX")

		# add collateral
		reparam(buck, user1, 8, "0.0000 BUCK", "1000.0000 REX")

		# both
		reparam(buck, user1, 9, "50.0000 BUCK", "-1000.0000 REX")

		table(buck, "reparamreq")

		table(buck, "stat")

		# mature rex
		time += 10_000
		maketime(buck, time)
		update(buck, 100)

		table(buck, "reparamreq")

		table(buck, "cdp")

		table(buck, "stat")



# main

if __name__ == "__main__":
	unittest.main()